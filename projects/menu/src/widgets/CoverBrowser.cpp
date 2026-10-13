#include "CoverBrowser.hpp"

#include <nxui/core/I18n.hpp>
#include <nxui/third_party/stb/stb_image.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <system_error>

namespace {

bool isImageName(const std::string& name) {
    std::string ext = std::filesystem::path(name).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

std::string lowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Shrinks `text` with "..." until it fits `maxWidth` at `scale`.
std::string fitText(nxui::Font* font, const std::string& text, float maxWidth, float scale) {
    if (!font || font->measure(text).x * scale <= maxWidth)
        return text;
    std::string out = text;
    while (!out.empty()) {
        out.pop_back();
        while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0u) == 0x80u)
            out.pop_back();   // never cut a UTF-8 character in half
        if (font->measure(out + "...").x * scale <= maxWidth)
            return out + "...";
    }
    return "...";
}

// Splits a file name over two lines when it is too long for one.
std::pair<std::string, std::string> twoLines(nxui::Font* font, const std::string& text,
                                             float maxWidth, float scale) {
    if (!font || font->measure(text).x * scale <= maxWidth)
        return {text, {}};
    std::size_t cut = text.size();
    while (cut > 1 && font->measure(text.substr(0, cut)).x * scale > maxWidth) {
        --cut;
        while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0u) == 0x80u)
            --cut;
    }
    return {text.substr(0, cut), fitText(font, text.substr(cut), maxWidth, scale)};
}

// Decodes an image and scales it to fill w x h (centre crop), so thumbnails
// stay small in memory however big the original is.
void decodeThumbnail(const std::string& path, int w, int h,
                     std::vector<std::uint8_t>& out) {
    int sw = 0, sh = 0, channels = 0;
    stbi_uc* src = stbi_load(path.c_str(), &sw, &sh, &channels, 4);
    if (!src || sw <= 0 || sh <= 0) {
        if (src) stbi_image_free(src);
        return;
    }
    const float scale = std::max(static_cast<float>(w) / sw, static_cast<float>(h) / sh);
    const float offX = (sw * scale - w) * 0.5f;
    const float offY = (sh * scale - h) * 0.5f;
    out.assign(static_cast<std::size_t>(w) * h * 4, 0);
    // Average a small block of source pixels per output pixel (clean downscale).
    const int span = std::max(1, static_cast<int>(std::floor(1.f / scale)));
    for (int y = 0; y < h; ++y) {
        const int sy0 = std::clamp(static_cast<int>((y + offY) / scale), 0, sh - 1);
        for (int x = 0; x < w; ++x) {
            const int sx0 = std::clamp(static_cast<int>((x + offX) / scale), 0, sw - 1);
            unsigned r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int dy = 0; dy < span && sy0 + dy < sh; dy += std::max(1, span / 3)) {
                for (int dx = 0; dx < span && sx0 + dx < sw; dx += std::max(1, span / 3)) {
                    const stbi_uc* p = src + (static_cast<std::size_t>(sy0 + dy) * sw + (sx0 + dx)) * 4;
                    r += p[0]; g += p[1]; b += p[2]; a += p[3]; ++n;
                }
            }
            std::uint8_t* o = out.data() + (static_cast<std::size_t>(y) * w + x) * 4;
            o[0] = static_cast<std::uint8_t>(r / n);
            o[1] = static_cast<std::uint8_t>(g / n);
            o[2] = static_cast<std::uint8_t>(b / n);
            o[3] = static_cast<std::uint8_t>(a / n);
        }
    }
    stbi_image_free(src);
}

} // namespace

CoverBrowser::CoverBrowser(nxui::GpuDevice& gpu, nxui::Renderer& renderer, nxui::ThreadPool& pool)
    : m_gpu(gpu), m_renderer(renderer), m_pool(pool) {
    setVisible(false);
    setFocusable(true);
    setFrameworkTouchEnabled(false);
    setTag("cover_browser");
    setCornerRadius(26.f);
    setLiquidGlassEnabled(false);
    setForceLiquidGlass(false);
    setLiquidGlassShaderEnabled(false);
    setBlurEnabled(false);
    setPanelOpacity(0.98f);

    addDirectionAction(nxui::FocusDirection::LEFT, [this]() { moveSelection(-1, 0); });
    addDirectionAction(nxui::FocusDirection::RIGHT, [this]() { moveSelection(1, 0); });
    addDirectionAction(nxui::FocusDirection::UP, [this]() { moveSelection(0, -1); });
    addDirectionAction(nxui::FocusDirection::DOWN, [this]() { moveSelection(0, 1); });
    addAction(static_cast<std::uint64_t>(nxui::Button::A), [this]() { activateSelected(); });
    addAction(static_cast<std::uint64_t>(nxui::Button::B), [this]() { goBack(); });
    addAction(static_cast<std::uint64_t>(nxui::Button::Plus), [this]() { close(); });
}

CoverBrowser::~CoverBrowser() {
    if (m_job) m_job->cancelled.store(true);
    if (m_jobFuture.valid()) m_jobFuture.wait();
}

nxui::Rect CoverBrowser::panelRect() const {
    return {80.f, 24.f, 1120.f, 672.f};
}

nxui::Rect CoverBrowser::tileRect(int index) const {
    const nxui::Rect panel = panelRect();
    const float gridW = kColumns * kThumbW + (kColumns - 1) * kGapX;
    const float left = panel.x + (panel.width - gridW) * 0.5f;
    const float top = panel.y + 100.f;
    const int row = index / kColumns - m_scrollRow;
    const int col = index % kColumns;
    return {left + col * (kThumbW + kGapX),
            top + row * (kThumbH + kLabelH + kGapY),
            kThumbW, kThumbH + kLabelH};
}

void CoverBrowser::open(const std::string& heading, const std::string& startDirectory,
                        const std::string& currentChoice, bool coverChoices) {
    m_gameTitle = heading;
    m_coverChoices = coverChoices;
    m_currentChoice = currentChoice;
    m_active = true;
    setVisible(true);
    setRect(panelRect());
    if (m_theme) {
        setBaseColor(m_theme->panelBase.withAlpha(0.98f));
        setBorderColor(m_theme->panelBorder.withAlpha(0.5f));
        setHighlightColor(m_theme->panelHighlight.withAlpha(0.12f));
    }
    m_reveal.setImmediate(0.9f);
    m_reveal.set(1.f, 0.18f, nxui::Easing::outBack);
    loadDirectory(startDirectory, currentChoice);
}

void CoverBrowser::close() {
    if (!m_active) return;
    m_active = false;
    setVisible(false);
    releaseThumbnails(true);
    m_entries.clear();
    if (m_onClosed) m_onClosed();
}

void CoverBrowser::loadDirectory(const std::string& directory, const std::string& select) {
    namespace fs = std::filesystem;
    releaseThumbnails(true);
    m_failed.clear();
    m_entries.clear();
    m_selected = 0;
    m_scrollRow = 0;

    std::string dir = directory.empty() ? std::string("sdmc:/") : directory;
    if (dir.size() > 6 && dir.back() == '/') dir.pop_back();
    m_directory = dir;
    const bool atRoot = dir == "sdmc:/" || dir == "sdmc:";

    auto& i18n = nxui::I18n::instance();
    if (!atRoot) {
        const auto slash = dir.find_last_of('/');
        const std::string parent =
            (slash == std::string::npos || slash <= 5) ? std::string("sdmc:/") : dir.substr(0, slash);
        m_entries.push_back({Kind::Up, i18n.tr("browser.back_folder", "Back"), parent});
    }
    if (m_coverChoices) {
        m_entries.push_back({Kind::Automatic, i18n.tr("game.cover_auto", "Automatic"), ""});
        m_entries.push_back({Kind::Original, i18n.tr("game.cover_icon", "Original Icon"), "icon"});
    }

    std::vector<Entry> folders, images;
    std::error_code ec;
    fs::directory_iterator it(dir, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.empty() || name[0] == '.') continue;
        std::string path = dir;
        if (path.back() != '/') path += '/';
        path += name;
        std::error_code typeEc;
        if (it->is_directory(typeEc))
            folders.push_back({Kind::Folder, name, path});
        else if (it->is_regular_file(typeEc) && isImageName(name))
            images.push_back({Kind::Image, name, path});
    }
    auto byName = [](const Entry& a, const Entry& b) { return lowerCopy(a.label) < lowerCopy(b.label); };
    std::sort(folders.begin(), folders.end(), byName);
    std::sort(images.begin(), images.end(), byName);
    m_entries.insert(m_entries.end(), folders.begin(), folders.end());
    m_entries.insert(m_entries.end(), images.begin(), images.end());

    // Land on the current cover, a folder we came out of, or the first image.
    int pick = -1;
    for (int i = 0; i < static_cast<int>(m_entries.size()) && !select.empty(); ++i)
        if (m_entries[static_cast<std::size_t>(i)].path == select) pick = i;
    if (pick < 0) {
        for (int i = 0; i < static_cast<int>(m_entries.size()); ++i)
            if (m_entries[static_cast<std::size_t>(i)].kind == Kind::Image) { pick = i; break; }
    }
    if (pick < 0) {
        for (int i = 0; i < static_cast<int>(m_entries.size()); ++i)
            if (m_entries[static_cast<std::size_t>(i)].kind == Kind::Folder) { pick = i; break; }
    }
    m_selected = std::max(0, pick);
    keepSelectedVisible();
}

void CoverBrowser::keepSelectedVisible() {
    const int row = m_selected / kColumns;
    if (row < m_scrollRow) m_scrollRow = row;
    if (row >= m_scrollRow + kVisibleRows) m_scrollRow = row - kVisibleRows + 1;
    const int rows = (static_cast<int>(m_entries.size()) + kColumns - 1) / kColumns;
    m_scrollRow = std::clamp(m_scrollRow, 0, std::max(0, rows - kVisibleRows));
}

void CoverBrowser::moveSelection(int dx, int dy) {
    if (!m_active || m_entries.empty()) return;
    const int count = static_cast<int>(m_entries.size());
    int next = m_selected;
    if (dx != 0) {
        next = std::clamp(m_selected + dx, 0, count - 1);
    } else {
        next = m_selected + dy * kColumns;
        if (next >= count) {
            // Moving down onto a shorter last row: land on its last item.
            const int lastRow = (count - 1) / kColumns;
            if (m_selected / kColumns < lastRow) next = count - 1;
            else next = m_selected;
        }
        if (next < 0) next = m_selected;
    }
    if (next == m_selected) return;
    m_selected = next;
    keepSelectedVisible();
    if (m_onNavigate) m_onNavigate();
}

void CoverBrowser::activateSelected() {
    if (!m_active || m_selected < 0 || m_selected >= static_cast<int>(m_entries.size())) return;
    const Entry entry = m_entries[static_cast<std::size_t>(m_selected)];
    if (m_onActivate) m_onActivate();
    switch (entry.kind) {
        case Kind::Up: {
            const std::string from = m_directory;
            loadDirectory(entry.path, from);
            break;
        }
        case Kind::Folder:
            loadDirectory(entry.path);
            break;
        case Kind::Automatic:
        case Kind::Original:
        case Kind::Image: {
            auto cb = m_onChoose;
            const std::string choice = entry.path;
            close();
            if (cb) cb(choice);
            break;
        }
    }
}

void CoverBrowser::goBack() {
    if (!m_active) return;
    if (!m_entries.empty() && m_entries.front().kind == Kind::Up) {
        const std::string from = m_directory;
        loadDirectory(m_entries.front().path, from);
        if (m_onNavigate) m_onNavigate();
        return;
    }
    close();
}

void CoverBrowser::handleTouch(nxui::Input& input) {
    if (!m_active) return;
    if (input.touchDown()) {
        m_touchIndex = -1;
        m_touchStartY = input.touchY();
        m_touchScrolled = false;
        const int first = m_scrollRow * kColumns;
        const int last = std::min(static_cast<int>(m_entries.size()),
                                  first + kVisibleRows * kColumns);
        for (int i = first; i < last; ++i) {
            if (tileRect(i).contains(input.touchX(), input.touchY())) {
                m_touchIndex = i;
                break;
            }
        }
    }
    if (input.isTouching() && !m_touchScrolled) {
        // Drag up/down a row's height to scroll.
        const float dy = input.touchY() - m_touchStartY;
        const float rowH = kThumbH + kLabelH + kGapY;
        if (std::abs(dy) > rowH * 0.5f) {
            const int rows = (static_cast<int>(m_entries.size()) + kColumns - 1) / kColumns;
            m_scrollRow = std::clamp(m_scrollRow + (dy < 0 ? 1 : -1), 0,
                                     std::max(0, rows - kVisibleRows));
            m_touchStartY = input.touchY();
            m_touchScrolled = true;
            m_touchIndex = -1;
        }
    }
    if (input.touchUp()) {
        if (m_touchIndex >= 0 && !m_touchScrolled) {
            if (m_touchIndex == m_selected) {
                activateSelected();
            } else {
                m_selected = m_touchIndex;
                if (m_onNavigate) m_onNavigate();
            }
        } else if (!m_touchScrolled && !panelRect().contains(input.touchX(), input.touchY())) {
            close();
        }
        m_touchIndex = -1;
    }
}

void CoverBrowser::releaseThumbnails(bool all) {
    if (all) {
        if (m_job) m_job->cancelled.store(true);
    }
    if (m_thumbs.empty()) return;
    std::vector<std::string> release;
    if (all) {
        for (const auto& entry : m_thumbs) release.push_back(entry.first);
    } else {
        if (m_thumbs.size() <= kMaxThumbs) return;
        // Keep what is on screen plus a row either side.
        std::unordered_set<std::string> keep;
        const int first = std::max(0, (m_scrollRow - 1) * kColumns);
        const int last = std::min(static_cast<int>(m_entries.size()),
                                  (m_scrollRow + kVisibleRows + 1) * kColumns);
        for (int i = first; i < last; ++i) keep.insert(m_entries[static_cast<std::size_t>(i)].path);
        for (const auto& entry : m_thumbs)
            if (!keep.count(entry.first)) release.push_back(entry.first);
    }
    if (release.empty()) return;
    m_gpu.waitIdle();
    for (const auto& path : release) m_thumbs.erase(path);
#ifdef NXUI_BACKEND_DEKO3D
    m_renderer.reclaimReleasedTextureSlotsAfterIdle();
#endif
}

void CoverBrowser::pumpThumbnails() {
    // Finished decode -> GPU texture.
    if (m_jobFuture.valid() &&
        m_jobFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        m_jobFuture.get();
        auto job = std::move(m_job);
        if (job && !job->cancelled.load()) {
            constexpr std::uint64_t kReserve = 4u * 1024u * 1024u;
            auto tex = std::make_unique<nxui::Texture>();
            if (!job->rgba.empty() &&
                m_gpu.imageMemoryAvailable() > kReserve + job->rgba.size() &&
                tex->loadFromPixels(m_gpu, m_renderer, job->rgba.data(), job->width, job->height))
                m_thumbs[job->path] = std::move(tex);
            else
                m_failed.insert(job->path);
        }
    }
    if (m_jobFuture.valid()) return;

    releaseThumbnails(false);

    // Next thumbnail: the selected one first, then the rest on screen, then
    // the next row so scrolling down is already loaded.
    const int first = m_scrollRow * kColumns;
    const int last = std::min(static_cast<int>(m_entries.size()),
                              (m_scrollRow + kVisibleRows + 1) * kColumns);
    std::vector<int> order;
    if (m_selected >= 0) order.push_back(m_selected);
    for (int i = first; i < last; ++i) order.push_back(i);
    for (int i : order) {
        if (i < 0 || i >= static_cast<int>(m_entries.size())) continue;
        const Entry& entry = m_entries[static_cast<std::size_t>(i)];
        if (entry.kind != Kind::Image) continue;
        if (m_thumbs.count(entry.path) || m_failed.count(entry.path)) continue;
        auto job = std::make_shared<DecodeJob>();
        job->path = entry.path;
        job->width = static_cast<int>(kThumbW);
        job->height = static_cast<int>(kThumbH);
        m_job = job;
        m_jobFuture = m_pool.submit([job]() {
            if (job->cancelled.load()) return;
            decodeThumbnail(job->path, job->width, job->height, job->rgba);
        });
        break;
    }
}

void CoverBrowser::update(float dt) {
    if (!m_active) return;
    m_reveal.update(dt);
    pumpThumbnails();
    nxui::GlassWidget::update(dt);
}

void CoverBrowser::drawEntry(nxui::Renderer& ren, const Entry& entry, const nxui::Rect& tile,
                             bool selected, float alpha) {
    const nxui::Color primary = m_theme ? m_theme->textPrimary : nxui::Color::white();
    const nxui::Color secondary = m_theme ? m_theme->textSecondary
                                          : nxui::Color(0.78f, 0.82f, 0.88f, 1.f);
    const nxui::Color card = m_theme ? m_theme->panelBorder.withAlpha(0.22f)
                                     : nxui::Color(1.f, 1.f, 1.f, 0.12f);
    const nxui::Color purple(0.48f, 0.12f, 0.66f, 1.f);
    const nxui::Rect thumb{tile.x, tile.y, kThumbW, kThumbH};
    constexpr float kRadius = 12.f;
    nxui::Font* big = m_font;
    nxui::Font* small = m_smallFont ? m_smallFont : m_font;

    auto centredText = [&](const std::string& text, nxui::Font* font, float scale,
                           float y, const nxui::Color& color) {
        if (!font) return;
        const std::string fitted = fitText(font, text, thumb.width - 12.f, scale);
        const float w = font->measure(fitted).x * scale;
        ren.drawText(fitted, {thumb.x + (thumb.width - w) * 0.5f, y}, font, color, scale);
    };

    switch (entry.kind) {
        case Kind::Image: {
            auto found = m_thumbs.find(entry.path);
            if (found != m_thumbs.end() && found->second && found->second->valid()) {
                ren.drawTextureRounded(found->second.get(), thumb, kRadius,
                                       nxui::Color::white().withAlpha(alpha));
            } else {
                ren.drawRoundedRect(thumb, card.withAlpha(card.a * alpha), kRadius);
                centredText(m_failed.count(entry.path) ? "?" : "...", big, 0.9f,
                            thumb.y + thumb.height * 0.42f, secondary.withAlpha(alpha));
            }
            break;
        }
        case Kind::Folder:
        case Kind::Up: {
            ren.drawRoundedRect(thumb, card.withAlpha(card.a * alpha), kRadius);
            // A simple folder: tab + body.
            const float fw = thumb.width * 0.62f;
            const float fh = fw * 0.72f;
            const float fx = thumb.x + (thumb.width - fw) * 0.5f;
            const float fy = thumb.y + (thumb.height - fh) * 0.5f - 6.f;
            const nxui::Color folder(0.96f, 0.78f, 0.32f, alpha);
            ren.drawRoundedRect({fx, fy - fh * 0.14f, fw * 0.42f, fh * 0.3f}, folder, 5.f);
            ren.drawRoundedRect({fx, fy, fw, fh}, folder, 7.f);
            if (entry.kind == Kind::Up)
                centredText("<", big, 1.0f, fy + fh * 0.18f,
                            nxui::Color(0.25f, 0.18f, 0.05f, alpha));
            break;
        }
        case Kind::Automatic:
            ren.drawRoundedRect(thumb, card.withAlpha(card.a * alpha), kRadius);
            centredText("AUTO", big, 0.9f, thumb.y + thumb.height * 0.40f, primary.withAlpha(alpha));
            break;
        case Kind::Original:
            ren.drawRoundedRect(thumb, card.withAlpha(card.a * alpha), kRadius);
            ren.drawRoundedRectOutline(thumb.shrunk(30.f), primary.withAlpha(0.6f * alpha), 14.f, 3.f);
            centredText("ICON", small, 0.8f, thumb.y + thumb.height * 0.43f, primary.withAlpha(alpha));
            break;
    }

    if (entry.path == m_currentChoice && entry.kind != Kind::Up && entry.kind != Kind::Folder)
        centredText("(current)", small, 0.6f, thumb.y + thumb.height - 24.f,
                    nxui::Color(0.3f, 0.95f, 0.55f, alpha));

    if (selected)
        ren.drawRoundedRectOutline(thumb.expanded(5.f), purple.withAlpha(alpha), kRadius + 5.f, 5.f);

    // File name under the picture, up to two lines.
    if (small) {
        constexpr float kScale = 0.62f;
        auto [line1, line2] = twoLines(small, entry.label, kThumbW + kGapX - 8.f, kScale);
        const nxui::Color color = selected ? primary : secondary;
        auto drawLine = [&](const std::string& line, float y) {
            if (line.empty()) return;
            const float w = small->measure(line).x * kScale;
            ren.drawText(line, {tile.x + (kThumbW - w) * 0.5f, y}, small,
                         color.withAlpha(alpha), kScale);
        };
        drawLine(line1, thumb.y + thumb.height + 6.f);
        drawLine(line2, thumb.y + thumb.height + 24.f);
    }
}

void CoverBrowser::render(nxui::Renderer& renderer) {
    if (!m_active || !isVisible()) return;
    const float alpha = std::clamp(m_reveal.value(), 0.f, 1.f);

    // Dim everything behind the window.
    renderer.drawRect({0.f, 0.f, 1280.f, 720.f}, nxui::Color(0.f, 0.f, 0.f, 0.55f * alpha));
    setRect(panelRect());
    nxui::GlassWidget::render(renderer);

    const nxui::Rect panel = panelRect();
    const nxui::Color primary = m_theme ? m_theme->textPrimary : nxui::Color::white();
    const nxui::Color secondary = m_theme ? m_theme->textSecondary
                                          : nxui::Color(0.78f, 0.82f, 0.88f, 1.f);
    auto& i18n = nxui::I18n::instance();

    if (m_font) {
        const std::string title = m_gameTitle;
        renderer.drawText(fitText(m_font, title, panel.width - 60.f, 0.8f),
                          {panel.x + 30.f, panel.y + 20.f}, m_font, primary.withAlpha(alpha), 0.8f);
    }
    if (m_smallFont) {
        std::string shown = m_directory.rfind("sdmc:", 0) == 0 ? m_directory.substr(5) : m_directory;
        if (shown.empty()) shown = "/";
        renderer.drawText(fitText(m_smallFont, shown, panel.width - 60.f, 0.7f),
                          {panel.x + 30.f, panel.y + 60.f}, m_smallFont,
                          secondary.withAlpha(alpha), 0.7f);
    }

    const int first = m_scrollRow * kColumns;
    const int last = std::min(static_cast<int>(m_entries.size()),
                              first + kVisibleRows * kColumns);
    for (int i = first; i < last; ++i)
        drawEntry(renderer, m_entries[static_cast<std::size_t>(i)], tileRect(i),
                  i == m_selected, alpha);

    // More above / below.
    nxui::Font* hintFont = m_smallFont ? m_smallFont : m_font;
    if (hintFont) {
        if (m_scrollRow > 0)
            renderer.drawText("▲", {panel.x + panel.width - 44.f, panel.y + 92.f}, hintFont,
                              secondary.withAlpha(alpha), 0.7f);
        if (last < static_cast<int>(m_entries.size()))
            renderer.drawText("▼", {panel.x + panel.width - 44.f, panel.y + panel.height - 40.f},
                              hintFont, secondary.withAlpha(alpha), 0.7f);
        const std::string help = i18n.tr("browser.help", "A  Choose / Open     B  Back     +  Close");
        const float w = hintFont->measure(help).x * 0.62f;
        renderer.drawText(help, {panel.x + (panel.width - w) * 0.5f, panel.y + panel.height - 30.f},
                          hintFont, secondary.withAlpha(alpha), 0.62f);
    }
}
