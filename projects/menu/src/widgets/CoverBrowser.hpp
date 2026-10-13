#pragma once

#include <nxui/widgets/GlassWidget.hpp>
#include <nxui/core/Animation.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Input.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Texture.hpp>
#include <nxui/core/ThreadPool.hpp>
#include <nxui/Theme.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Large centred picker for a game's cover: browse the SD card folder by
// folder and see a thumbnail of every image before choosing it.
class CoverBrowser final : public nxui::GlassWidget {
public:
    // "" = automatic cover, "icon" = original square icon, otherwise a path.
    using ChooseCallback = std::function<void(const std::string& choice)>;
    using VoidCallback = std::function<void()>;

    CoverBrowser(nxui::GpuDevice& gpu, nxui::Renderer& renderer, nxui::ThreadPool& pool);
    ~CoverBrowser() override;

    void setFont(nxui::Font* font) { m_font = font; }
    void setSmallFont(nxui::Font* font) { m_smallFont = font; }
    void setTheme(const nxui::Theme* theme) { m_theme = theme; }
    void onChoose(ChooseCallback cb) { m_onChoose = std::move(cb); }
    void onClosed(VoidCallback cb) { m_onClosed = std::move(cb); }
    void onNavigate(VoidCallback cb) { m_onNavigate = std::move(cb); }
    void onActivate(VoidCallback cb) { m_onActivate = std::move(cb); }

    // `currentChoice` is highlighted when it is in the opened folder.
    // `heading` is the window title. `coverChoices` adds the Automatic and
    // Original Icon tiles (off when picking a background picture).
    void open(const std::string& heading, const std::string& startDirectory,
              const std::string& currentChoice, bool coverChoices = true);
    void close();
    bool isActive() const { return m_active; }

    void handleTouch(nxui::Input& input);
    void update(float dt) override;
    void render(nxui::Renderer& renderer) override;

private:
    enum class Kind { Up, Folder, Image, Automatic, Original };
    struct Entry {
        Kind kind = Kind::Image;
        std::string label;
        std::string path;
    };
    struct DecodeJob {
        std::string path;
        std::vector<std::uint8_t> rgba;
        int width = 0;
        int height = 0;
        std::atomic<bool> cancelled{false};
    };

    void loadDirectory(const std::string& directory, const std::string& select = {});
    void moveSelection(int dx, int dy);
    void activateSelected();
    void goBack();
    void keepSelectedVisible();
    nxui::Rect panelRect() const;
    nxui::Rect tileRect(int index) const;   // screen rect (scroll applied)
    int columns() const { return kColumns; }
    void pumpThumbnails();
    void releaseThumbnails(bool all);
    void drawEntry(nxui::Renderer& ren, const Entry& entry, const nxui::Rect& tile,
                   bool selected, float alpha);

    nxui::GpuDevice& m_gpu;
    nxui::Renderer& m_renderer;
    nxui::ThreadPool& m_pool;
    nxui::Font* m_font = nullptr;
    nxui::Font* m_smallFont = nullptr;
    const nxui::Theme* m_theme = nullptr;

    bool m_active = false;
    std::string m_gameTitle;
    bool m_coverChoices = true;
    std::string m_directory;
    std::string m_currentChoice;
    std::vector<Entry> m_entries;
    int m_selected = 0;
    int m_scrollRow = 0;
    nxui::AnimatedFloat m_reveal{0.f};

    std::unordered_map<std::string, std::unique_ptr<nxui::Texture>> m_thumbs;
    std::unordered_set<std::string> m_failed;
    std::shared_ptr<DecodeJob> m_job;
    std::future<void> m_jobFuture;

    int m_touchIndex = -1;
    float m_touchStartY = 0.f;
    bool m_touchScrolled = false;

    ChooseCallback m_onChoose;
    VoidCallback m_onClosed;
    VoidCallback m_onNavigate;
    VoidCallback m_onActivate;

    static constexpr int kColumns = 6;
    static constexpr int kVisibleRows = 2;
    static constexpr float kThumbW = 140.f;
    static constexpr float kThumbH = 210.f;
    static constexpr float kLabelH = 46.f;
    static constexpr float kGapX = 30.f;
    static constexpr float kGapY = 16.f;
    static constexpr std::size_t kMaxThumbs = 36;
};
