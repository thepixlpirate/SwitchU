#pragma once

#include <nxui/widgets/Widget.hpp>
#include <nxui/core/Animation.hpp>
#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Texture.hpp>
#include <nxui/core/ThreadPool.hpp>

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

// Full-screen looping clip (or still picture) behind the HOME screen for the
// selected game.
// Clips are silent animated WebP files. Frames are decoded one at a time on a
// worker and shown through a single texture, so memory stays at roughly one
// frame no matter how long the clip is.
class VideoBackdrop final : public nxui::Widget {
public:
    VideoBackdrop(nxui::GpuDevice& gpu, nxui::Renderer& renderer, nxui::ThreadPool& pool);
    ~VideoBackdrop() override;

    // Starts `path` (fades in once the first frame is ready). An empty path,
    // or the clip already playing, is handled gracefully.
    void play(const std::string& path);
    // Fades out and releases the clip.
    void stop();
    // Pauses decoding while something else covers the screen.
    void setPaused(bool paused) { m_paused = paused; }
    // 0..1 darkening drawn over the clip so text stays readable.
    void setDim(float dim) { m_dim = dim; }
    const std::string& currentPath() const { return m_path; }
    bool isShowing() const { return m_alpha.value() > 0.01f; }

    // Texture uploads happen here, at the HOME update's safe point.
    void pump(float dt);

protected:
    void onRender(nxui::Renderer& ren) override;

private:
    struct Decoder;   // owns the WebP decoder; lives on the worker side
    struct FrameJob {
        std::vector<std::uint8_t> rgba;   // kOutW x kOutH
        int durationMs = 0;
        int width = 0;
        int height = 0;
        bool ok = false;
    };

    void requestNextFrame();
    void releaseClip();

    nxui::GpuDevice& m_gpu;
    nxui::Renderer& m_renderer;
    nxui::ThreadPool& m_pool;

    std::string m_path;
    std::shared_ptr<Decoder> m_decoder;
    std::future<void> m_job;
    std::shared_ptr<FrameJob> m_pending;     // frame being decoded
    std::shared_ptr<FrameJob> m_ready;       // decoded, waiting for its turn
    std::unique_ptr<nxui::Texture> m_texture;
    float m_frameTimeLeft = 0.f;
    bool m_paused = false;
    bool m_stopping = false;
    float m_dim = 0.25f;
    nxui::AnimatedFloat m_alpha{0.f};

    static constexpr int kOutW = 640;
    static constexpr int kOutH = 360;
    // Pictures are sharper; they are uploaded once, so size costs nothing per frame.
    static constexpr int kStillW = 1280;
    static constexpr int kStillH = 720;
};
