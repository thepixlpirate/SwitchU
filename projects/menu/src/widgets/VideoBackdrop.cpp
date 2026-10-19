#include "VideoBackdrop.hpp"
#include "core/DebugLog.hpp"

#include <webp/demux.h>
#include <nxui/third_party/stb/stb_image.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <mutex>

namespace {
// Clips larger than this are refused (they would not play smoothly anyway).
constexpr std::size_t kMaxFileBytes = 24u * 1024u * 1024u;
constexpr int kMaxCanvasSide = 1920;
}

struct VideoBackdrop::Decoder {
    std::mutex mutex;                  // one frame at a time
    std::vector<std::uint8_t> bytes;   // the whole .webp file
    WebPAnimDecoder* dec = nullptr;
    int width = 0;
    int height = 0;
    int lastTimestamp = 0;
    bool failed = false;
    // A plain picture (PNG/JPG, or a one-frame WebP): decoded once, shown
    // until the selection changes.
    bool still = false;
    std::vector<std::uint8_t> stillRgba;
    std::atomic<bool> cancelled{false};

    ~Decoder() {
        if (dec) WebPAnimDecoderDelete(dec);
    }

    bool open(const std::string& path) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input.is_open()) return false;
        const auto size = static_cast<std::size_t>(input.tellg());
        if (size == 0 || size > kMaxFileBytes) {
            DebugLog::log("[video-bg] %s: size %zu not usable", path.c_str(), size);
            return false;
        }
        bytes.resize(size);
        input.seekg(0);
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
        if (!input.good()) return false;

        std::string ext = path.substr(path.find_last_of('.') + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != "webp") {
            int w = 0, h = 0, channels = 0;
            stbi_uc* raw = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                                 &w, &h, &channels, 4);
            bytes.clear();
            bytes.shrink_to_fit();
            if (!raw || w <= 0 || h <= 0 || w > 4096 || h > 4096) {
                if (raw) stbi_image_free(raw);
                return false;
            }
            width = w;
            height = h;
            stillRgba.assign(raw, raw + static_cast<std::size_t>(w) * h * 4);
            stbi_image_free(raw);
            still = true;
            return true;
        }

        WebPAnimDecoderOptions opts;
        if (!WebPAnimDecoderOptionsInit(&opts)) return false;
        opts.color_mode = MODE_RGBA;
        opts.use_threads = 0;
        WebPData data{bytes.data(), bytes.size()};
        dec = WebPAnimDecoderNew(&data, &opts);
        if (!dec) return false;
        WebPAnimInfo info;
        if (!WebPAnimDecoderGetInfo(dec, &info)) return false;
        width = static_cast<int>(info.canvas_width);
        height = static_cast<int>(info.canvas_height);
        still = info.frame_count <= 1;
        if (width <= 0 || height <= 0 || width > kMaxCanvasSide || height > kMaxCanvasSide) {
            DebugLog::log("[video-bg] %s: canvas %dx%d not usable", path.c_str(), width, height);
            return false;
        }
        return true;
    }

    // Decodes the next frame (looping at the end) scaled to fill outW x outH.
    bool next(std::vector<std::uint8_t>& out, int outW, int outH, int& durationMs) {
        if (failed) return false;
        if (!stillRgba.empty()) {
            durationMs = 500;
            scaleInto(stillRgba.data(), out, outW, outH);
            stillRgba.clear();
            stillRgba.shrink_to_fit();
            return true;
        }
        if (!dec) return false;
        if (!WebPAnimDecoderHasMoreFrames(dec)) {
            WebPAnimDecoderReset(dec);
            lastTimestamp = 0;
        }
        std::uint8_t* rgba = nullptr;
        int timestamp = 0;
        if (!WebPAnimDecoderGetNext(dec, &rgba, &timestamp) || !rgba) {
            failed = true;
            return false;
        }
        durationMs = std::clamp(timestamp - lastTimestamp, 20, 500);
        lastTimestamp = timestamp;
        scaleInto(rgba, out, outW, outH);
        return true;
    }

    void scaleInto(const std::uint8_t* rgba, std::vector<std::uint8_t>& out, int outW, int outH) {

        // Centre-crop to 16:9 and scale (nearest pixel; the result is shown
        // full screen and slightly dimmed, so this is plenty).
        out.resize(static_cast<std::size_t>(outW) * outH * 4);
        const float scale = std::max(static_cast<float>(outW) / width,
                                     static_cast<float>(outH) / height);
        const float offX = (width * scale - outW) * 0.5f;
        const float offY = (height * scale - outH) * 0.5f;
        for (int y = 0; y < outH; ++y) {
            const int sy = std::clamp(static_cast<int>((y + offY) / scale), 0, height - 1);
            const std::uint8_t* srcRow = rgba + static_cast<std::size_t>(sy) * width * 4;
            std::uint8_t* dst = out.data() + static_cast<std::size_t>(y) * outW * 4;
            for (int x = 0; x < outW; ++x) {
                const int sx = std::clamp(static_cast<int>((x + offX) / scale), 0, width - 1);
                const std::uint8_t* s = srcRow + static_cast<std::size_t>(sx) * 4;
                dst[0] = s[0]; dst[1] = s[1]; dst[2] = s[2]; dst[3] = 255;
                dst += 4;
            }
        }
    }
};

VideoBackdrop::VideoBackdrop(nxui::GpuDevice& gpu, nxui::Renderer& renderer,
                             nxui::ThreadPool& pool)
    : m_gpu(gpu), m_renderer(renderer), m_pool(pool) {
    setRect({0.f, 0.f, 1280.f, 720.f});
    setFocusable(false);
}

VideoBackdrop::~VideoBackdrop() {
    if (m_decoder) m_decoder->cancelled.store(true);
    if (m_job.valid()) m_job.wait();
}

void VideoBackdrop::play(const std::string& path) {
    if (path.empty()) { stop(); return; }
    if (path == m_path && !m_stopping) return;
    // Switching clips: drop the old one now (no cross-fade, to keep memory
    // at a single clip).
    releaseClip();
    m_path = path;
    m_stopping = false;
    auto decoder = std::make_shared<Decoder>();
    m_decoder = decoder;
    auto job = std::make_shared<FrameJob>();
    m_pending = job;
    m_job = m_pool.submit([decoder, job, path]() {
        std::lock_guard<std::mutex> lock(decoder->mutex);
        if (decoder->cancelled.load()) return;
        if (!decoder->open(path)) {
            decoder->failed = true;
            DebugLog::log("[video-bg] could not open %s", path.c_str());
            return;
        }
        const int w = decoder->still ? kStillW : kOutW;
        const int h = decoder->still ? kStillH : kOutH;
        job->width = w;
        job->height = h;
        job->ok = decoder->next(job->rgba, w, h, job->durationMs);
    });
}

void VideoBackdrop::stop() {
    if (m_path.empty()) return;
    m_stopping = true;
    m_alpha.set(0.f, 0.18f, nxui::Easing::outCubic);
}

void VideoBackdrop::releaseClip() {
    // Never wait here (opening a clip reads it from the SD card): the worker
    // holds its own references and simply finishes into nothing.
    if (m_decoder) m_decoder->cancelled.store(true);
    m_job = {};
    m_decoder.reset();
    m_pending.reset();
    m_ready.reset();
    m_path.clear();
    m_frameTimeLeft = 0.f;
    m_alpha.setImmediate(0.f);
    // The texture is kept for reuse (same size every clip); it is simply not
    // drawn while alpha is 0.
}

void VideoBackdrop::requestNextFrame() {
    if (!m_decoder || m_job.valid() || m_pending || m_decoder->failed) return;
    if (m_decoder->still) return;   // a picture needs only its one frame
    auto decoder = m_decoder;
    auto job = std::make_shared<FrameJob>();
    m_pending = job;
    m_job = m_pool.submit([decoder, job]() {
        std::lock_guard<std::mutex> lock(decoder->mutex);
        if (decoder->cancelled.load()) return;
        job->width = kOutW;
        job->height = kOutH;
        job->ok = decoder->next(job->rgba, kOutW, kOutH, job->durationMs);
    });
}

void VideoBackdrop::pump(float dt) {
    m_alpha.update(dt);
    if (m_stopping) {
        if (m_alpha.value() <= 0.01f) {
            releaseClip();
            m_stopping = false;
        }
        return;
    }
    if (!m_decoder) return;

    // Collect a finished decode.
    if (m_job.valid() &&
        m_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        m_job.get();
        if (m_pending && m_pending->ok) m_ready = std::move(m_pending);
        m_pending.reset();
        if (m_decoder->failed && !m_texture) {
            // Unreadable clip: behave as if there were none.
            releaseClip();
            return;
        }
    }
    if (m_paused) return;

    m_frameTimeLeft -= dt * 1000.f;
    if (m_ready && m_frameTimeLeft <= 0.f) {
        auto frame = std::move(m_ready);
        if (!m_texture) m_texture = std::make_unique<nxui::Texture>();
        // Same size every time, so this reuses the texture's memory and slot.
        if (m_texture->loadFromPixels(m_gpu, m_renderer, frame->rgba.data(),
                                      frame->width, frame->height)) {
            if (m_alpha.target() < 1.f)
                m_alpha.set(1.f, 0.35f, nxui::Easing::outCubic);
        }
        m_frameTimeLeft = static_cast<float>(frame->durationMs);
    }
    requestNextFrame();
}

void VideoBackdrop::onRender(nxui::Renderer& ren) {
    const float a = m_alpha.value() * m_opacity;
    if (a <= 0.01f || !m_texture || !m_texture->valid()) return;
    ren.drawTexture(m_texture.get(), m_rect, nxui::Color::white().withAlpha(a));
    if (m_dim > 0.001f)
        ren.drawRect(m_rect, nxui::Color(0.f, 0.f, 0.f, m_dim * a));
}
