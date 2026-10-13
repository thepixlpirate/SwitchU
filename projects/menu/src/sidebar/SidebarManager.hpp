#pragma once
#include "SidebarAnimation.hpp"
#include "widgets/AppletButton.hpp"
#include <nxui/core/Texture.hpp>
#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/Theme.hpp>
#include <nxui/widgets/Widget.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class SidebarManager {
public:
    struct Actions {
        std::function<void()> onAlbum;
        std::function<void()> onMiiEditor;
        std::function<void()> onControllers;
        std::function<void()> onSleep;
        std::function<void()> onSettings;
        std::function<void()> onMiiverse;
    };

    void build(nxui::GpuDevice& gpu, nxui::Renderer& ren,
               const std::string& assetsBase, const Actions& actions);

    void reloadAssets(nxui::GpuDevice& gpu, nxui::Renderer& ren,
                      const std::string& assetsBase,
                      const std::string& customIconsBase = std::string());
    void invalidateAssetsCache();

    void update(float dt, nxui::Widget* focusedWidget);

    // Hide individual shortcuts. Hidden buttons stay alive (other code may
    // still hold a pointer) but are invisible, unfocusable and left out of
    // leftButtons()/rightButtons(). Call configure*Navigation afterwards.
    struct Visibility {
        bool homebrew = true;     // Album / homebrew
        bool miiEditor = true;
        bool settings = true;
        bool controllers = true;
        bool sleep = true;
        bool themes = true;
        bool allShown() const {
            return homebrew && miiEditor && settings && controllers && sleep && themes;
        }
    };
    void setButtonVisibility(const Visibility& visibility);
    // Horizontal centre of the trimmed top strip (single-row layout).
    void setCompactStripCenterX(float x) { m_compactCenterX = x; }
    void setCompactStripY(float y) { m_compactY = y; }
    // UP on a strip button (used when the strip sits below the games).
    void setDynamicLineUpAction(std::function<void()> action);
    // Every button, hidden ones included (for adding to the widget tree).
    const std::vector<std::shared_ptr<AppletButton>>& allLeftButtons() const  { return m_allLeft; }
    const std::vector<std::shared_ptr<AppletButton>>& allRightButtons() const { return m_allRight; }

    // The dynamic-line HOME layout turns the two side columns into one
    // horizontal control strip between the profile bar and the applications.
    void setDynamicLineLayout(bool enabled);
    void setDynamicLineDownAction(std::function<void()> action);
    void setDynamicLineUpTarget(nxui::Widget* target);
    void setDynamicLineProfileTarget(nxui::Widget* profile);

    void applyTheme(const nxui::Theme& theme);

    std::vector<std::shared_ptr<AppletButton>>& leftButtons()  { return m_leftButtons; }
    std::vector<std::shared_ptr<AppletButton>>& rightButtons() { return m_rightButtons; }
    const std::vector<std::shared_ptr<AppletButton>>& leftButtons() const  { return m_leftButtons; }
    const std::vector<std::shared_ptr<AppletButton>>& rightButtons() const { return m_rightButtons; }

    AppletButton*  albumButton()    const { return m_albumButton; }
    nxui::Widget*  settingsButton() const { return m_settingsButton; }
    nxui::Widget*  themeShopButton() const { return m_themeShopButton; }

private:
    void loadAssets(nxui::GpuDevice& gpu, nxui::Renderer& ren,
                    const std::string& assetsBase,
                    const std::string& customIconsBase);
    void tryLoadAnimation(nxui::GpuDevice& gpu, nxui::Renderer& ren,
                          const std::string& webpPath, AppletButton* button,
                          nxui::Texture* staticIcon);

    std::vector<nxui::Texture> m_icons;
    std::vector<std::shared_ptr<AppletButton>> m_leftButtons;
    std::vector<std::shared_ptr<AppletButton>> m_rightButtons;
    std::vector<std::shared_ptr<AppletButton>> m_allLeft;
    std::vector<std::shared_ptr<AppletButton>> m_allRight;
    AppletButton* m_miiEditorButton = nullptr;
    AppletButton* m_controllersButton = nullptr;
    AppletButton* m_sleepButton = nullptr;
    Visibility m_visibility;
    float m_compactCenterX = 640.f;
    float m_compactY = 18.f;
    bool isShown(const AppletButton* button) const;
    void placeColumns();
    AppletButton*  m_albumButton    = nullptr;
    nxui::Widget*  m_settingsButton = nullptr;
    nxui::Widget*  m_themeShopButton = nullptr;

    struct AnimEntry {
        SidebarAnimation anim;
        AppletButton*    button    = nullptr;
        nxui::Texture*   staticTex = nullptr;
    };
    std::vector<AnimEntry> m_anims;
    std::string m_loadedAssetsBase;
    std::string m_loadedCustomIconsBase;
    bool m_assetsLoaded = false;
};
