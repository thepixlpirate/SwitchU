// Settings > Home Screen: optional home chrome, hidden games, and the portrait
// single row (2:3 covers). Everything here is opt-in/opt-out from the settings
// tab; with every option on, Grid or the Wii U carousel selected, HOME looks
// exactly as it did before these options existed.

#include "WiiUMenuApp.hpp"
#include "widgets/GlossyIcon.hpp"
#include "DebugLog.hpp"
#include <nxui/core/I18n.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

namespace {

// The player's own covers (any naming: title ID or game name, file or folder).
constexpr const char* kIconSwapFolder = "sdmc:/iconswap";
constexpr const char* kPortraitFolder = "sdmc:/config/SwitchU/portraits";
// Decoded size of one cover. The focused card is at most ~290x432 on screen.
constexpr int kPortraitDecodeW = 300;
constexpr int kPortraitDecodeH = 450;
// Covers kept around the focused game: a little behind, more ahead.
constexpr int kPortraitKeepBehind = 3;
constexpr int kPortraitKeepAhead = 8;
// Released in batches so the GPU is not drained on every scroll step.
constexpr std::size_t kPortraitReleaseBatch = 16;

bool fileExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

bool isImageFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

// "Quake II" -> "quakeii": letters and digits only, lower case.
std::string normaliseName(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text)
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

// Keys a file or folder name can be found under: its normalised name, plus any
// 16-digit title ID inside it ("Quake II [0100ABCD12345000]").
std::vector<std::string> indexKeysFor(const std::string& name) {
    std::vector<std::string> keys;
    const std::string norm = normaliseName(name);
    if (!norm.empty()) keys.push_back(norm);
    int run = 0;
    for (std::size_t i = 0; i < name.size(); ++i) {
        if (std::isxdigit(static_cast<unsigned char>(name[i]))) {
            ++run;
            if (run >= 16) {
                std::string id = name.substr(i + 1 - 16, 16);
                std::transform(id.begin(), id.end(), id.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (std::find(keys.begin(), keys.end(), id) == keys.end())
                    keys.push_back(id);
            }
        } else {
            run = 0;
        }
    }
    return keys;
}

std::string titleIdKey(std::uint64_t titleId) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(titleId));
    return buf;
}

} // namespace

// ---------------------------------------------------------------------------
// Home chrome
// ---------------------------------------------------------------------------

void WiiUMenuApp::layoutTopHud() {
    if (!m_topHud)
        return;
    m_topHud->layout();
    if (!m_clock || !m_battery)
        return;
    if (m_config.homeClockPosition == 0) {
        // Top bar: clock on the left (after the profile button when shown).
        nxui::Rect clock = m_clock->rect();
        clock.x = m_config.homeShowProfileButton ? 24.f + 62.f + 16.f : 24.f;
        m_clock->setRect(clock);
    } else if (m_config.homeClockPosition == 1) {
        // Clock just left of the battery, like the Switch's top-right corner.
        nxui::Rect clock = m_clock->rect();
        const nxui::Rect battery = m_battery->rect();
        clock.x = battery.x - clock.width + 12.f;
        clock.y = battery.y;
        m_clock->setRect(clock);
    } else if (m_config.homeClockPosition == 2) {
        // Bottom bar: clock bottom-left, battery bottom-right (Settings sits
        // in the middle, placed by the sidebar strip).
        constexpr float kBarY = 648.f;
        nxui::Rect clock = m_clock->rect();
        nxui::Rect battery = m_battery->rect();
        clock.x = 24.f;
        clock.y = kBarY;
        battery.x = 1280.f - 24.f - battery.width;
        battery.y = kBarY;
        m_clock->setRect(clock);
        m_battery->setRect(battery);
    }
}

void WiiUMenuApp::applyBottomBarNavigation() {
    const bool bottom = m_config.homeClockPosition == 2
                     && m_appLayoutMode == AppLayoutMode::DynamicLine;
    if (!bottom) {
        m_sidebar.setDynamicLineUpAction({});
        return;
    }
    nxui::Widget* first = nullptr;
    if (!m_sidebar.leftButtons().empty()) first = m_sidebar.leftButtons().front().get();
    else if (!m_sidebar.rightButtons().empty()) first = m_sidebar.rightButtons().front().get();
    if (m_grid) {
        // Games: DOWN goes to Settings below, UP to the profile (if shown).
        m_grid->setDynamicLineUpTarget(
            m_config.homeShowProfileButton && m_profileLock ? m_profileLock.get() : nullptr);
        m_grid->setDynamicLineDownTarget(first);
    }
    m_sidebar.setDynamicLineDownAction({});
    m_sidebar.setDynamicLineUpAction([this]() {
        if (!m_grid || m_appLayoutMode != AppLayoutMode::DynamicLine ||
            m_navigator.route() != switchu::navigation::Route::Home)
            return;
        if (auto* target = m_grid->focusManager().current())
            focusManager().setFocus(target);
    });
}

void WiiUMenuApp::applyHomeChromeOptions() {
    const bool bubbles = m_config.homeShowHudBubbles;
    if (m_clock) {
        m_clock->setBubbleVisible(bubbles);
        m_clock->setShowDate(m_config.homeShowDate);
        m_clock->setMonthFirst(m_config.homeDateFormat == 0);
        m_clock->setUse12HourClock(m_config.clockUse12Hour);
    }
    if (m_battery) m_battery->setBubbleVisible(bubbles);
    // Hidden ones stay in the layout (so nothing else moves) but draw nothing.
    if (m_clock) m_clock->setOpacity(m_config.homeShowClock ? 1.f : 0.f);
    if (m_battery) m_battery->setOpacity(m_config.homeShowBattery ? 1.f : 0.f);
    if (m_titlePill) m_titlePill->setBubbleVisible(bubbles);

    if (m_profileLock) {
        // Kept in the top row (so the layout stays balanced) but invisible,
        // unfocusable and untouchable while hidden.
        const bool show = m_config.homeShowProfileButton;
        m_profileLock->setOpacity(show ? 1.f : 0.f);
        m_profileLock->setFocusable(show);
        if (!show && focusManager().current() == m_profileLock.get() && m_grid) {
            if (auto* target = m_grid->focusManager().current())
                focusManager().setFocus(target);
        }
    }
    layoutTopHud();

    nxui::Widget* focused = focusManager().current();
    SidebarManager::Visibility visibility;
    visibility.homebrew = m_config.homeShowHomebrewButton;
    visibility.miiEditor = m_config.homeShowMiiButton;
    // Settings may only disappear while - opens the power menu with Settings.
    visibility.settings = m_config.homeShowSettingsButton || m_config.homeMinusAction != 0;
    visibility.controllers = m_config.homeShowControllersButton;
    visibility.sleep = m_config.homeShowSleepButton;
    visibility.themes = m_config.homeShowThemesButton;
    // With the clock in the top middle, the shortcuts move left of it.
    // Only the original layout keeps the clock in the middle; the shortcuts
    // then sit left of it. Everywhere else Settings is in the middle.
    m_sidebar.setCompactStripCenterX(m_config.homeClockPosition == 3 ? 330.f : 640.f);
    // Bottom bar: Settings in the bottom middle, level with clock and battery.
    m_sidebar.setCompactStripY(m_config.homeClockPosition == 2 ? 656.f : 18.f);
    m_sidebar.setButtonVisibility(visibility);
    if (focused && !focused->isVisible() && m_grid) {
        if (auto* target = m_grid->focusManager().current())
            focusManager().setFocus(target);
    }

    if (m_grid) {
        m_grid->setLineStyle(m_config.homeLineStyle);
        m_grid->setCenteredLayout(m_config.homeCoversShown, m_config.homeClockPosition == 2);
    }
    configureDynamicLineNavigation();
    if (m_grid && !m_grid->isPortraitLine())
        releasePortraitCovers(true);
    syncPortraitTitlePlacement();
    updateCursor();
}

void WiiUMenuApp::pushHomeOptionsToSettings() {
    if (!m_settings)
        return;
    SettingsScreen::HomeOptionsState home;
    home.singleRow = m_appLayoutMode == AppLayoutMode::DynamicLine;
    home.lineStyle = m_config.homeLineStyle;
    home.clockPosition = m_config.homeClockPosition;
    home.showClock = m_config.homeShowClock;
    home.showBattery = m_config.homeShowBattery;
    home.coversShownIndex = m_config.homeCoversShown >= 5 ? 1 : 0;
    home.showDate = m_config.homeShowDate;
    home.dateFormat = m_config.homeDateFormat;
    home.plusAction = m_config.homePlusAction;
    home.minusAction = m_config.homeMinusAction;
    home.showProfileButton = m_config.homeShowProfileButton;
    home.showHudBubbles = m_config.homeShowHudBubbles;
    home.showHomebrewButton = m_config.homeShowHomebrewButton;
    home.showMiiButton = m_config.homeShowMiiButton;
    home.showControllersButton = m_config.homeShowControllersButton;
    home.showSleepButton = m_config.homeShowSleepButton;
    home.showThemesButton = m_config.homeShowThemesButton;
    home.showSettingsButton = m_config.homeShowSettingsButton;
    home.showButtonHints = m_config.homeShowButtonHints;
    home.hiddenGames = hiddenGamesForSettings();
    m_settings->setHomeOptionsState(home);
    m_settings->setClockUse12HourState(m_config.clockUse12Hour);
}

void WiiUMenuApp::openThemeShopScreen() {
    if (m_openThemeShopAction)
        m_openThemeShopAction();
}

void WiiUMenuApp::applyHomeOption(const std::string& key, int value) {
    const bool on = value != 0;
    if (key == "singleRow") {
        const AppLayoutMode mode = on ? AppLayoutMode::DynamicLine : AppLayoutMode::Grid;
        if (mode != m_appLayoutMode)
            setAppLayoutMode(mode);   // saves the config itself
        applyHomeChromeOptions();
        return;
    }
    if (key == "openThemes") {
        openThemeShopScreen();
        return;
    }
    if (key.rfind("unhide:", 0) == 0) {
        unhideTitle(std::strtoull(key.c_str() + 7, nullptr, 16));
        return;
    }
    if (key == "lineStyle")                  m_config.homeLineStyle = std::clamp(value, 0, 2);
    else if (key == "clockPosition")         m_config.homeClockPosition = std::clamp(value, 0, 3);
    else if (key == "showClock")             m_config.homeShowClock = on;
    else if (key == "showBattery")           m_config.homeShowBattery = on;
    else if (key == "coversShown")           m_config.homeCoversShown = value >= 1 ? 5 : 3;
    else if (key == "showDate")              m_config.homeShowDate = on;
    else if (key == "dateFormat")            m_config.homeDateFormat = std::clamp(value, 0, 1);
    else if (key == "plusAction")            m_config.homePlusAction = std::clamp(value, 0, 1);
    else if (key == "minusAction")           m_config.homeMinusAction = std::clamp(value, 0, 1);
    else if (key == "showProfileButton")     m_config.homeShowProfileButton = on;
    else if (key == "showHudBubbles")        m_config.homeShowHudBubbles = on;
    else if (key == "showHomebrewButton")    m_config.homeShowHomebrewButton = on;
    else if (key == "showMiiButton")         m_config.homeShowMiiButton = on;
    else if (key == "showControllersButton") m_config.homeShowControllersButton = on;
    else if (key == "showSleepButton")       m_config.homeShowSleepButton = on;
    else if (key == "showThemesButton")      m_config.homeShowThemesButton = on;
    else if (key == "showSettingsButton")    m_config.homeShowSettingsButton = on;
    else if (key == "showButtonHints")       m_config.homeShowButtonHints = on;
    else return;

    applyHomeChromeOptions();
    if (m_configSaveFuture.valid())
        m_configSaveFuture.wait();
    m_configSaveFuture = m_threadPool.submit([config = m_config]() {
        config.save();
    });
}

void WiiUMenuApp::syncPortraitTitlePlacement() {
    if (!m_titlePill || !m_grid)
        return;
    const bool portrait = m_grid->isPortraitLine();
    nxui::Widget* cur = focusManager().current();
    const bool onGame = cur && cur->tag() == "glossy_icon";
    if (portrait && onGame) {
        // The name sits centred under the focused cover.
        const nxui::Rect card = m_grid->portraitFocusRestRect();
        m_titlePill->setAnchorCenterX(card.x + card.width * 0.5f);
        const float y = card.y + card.height + 14.f;
        if (std::abs(m_titlePill->rect().y - y) > 0.5f)
            m_titlePill->setPosition(m_titlePill->rect().x, y);
    } else {
        m_titlePill->setAnchorCenterX(-1.f);
        if (std::abs(m_titlePill->rect().y - 630.f) > 0.5f)
            m_titlePill->setPosition(m_titlePill->rect().x, 630.f);
    }
}

// ---------------------------------------------------------------------------
// Hidden games
// ---------------------------------------------------------------------------

void WiiUMenuApp::dropHiddenTitles(GridModel& model) const {
    if (m_config.hiddenTitles.empty())
        return;
    for (int i = 0; i < model.count(); ++i) {
        const AppEntry& entry = model.at(i);
        if (entry.isApplication() && m_config.isTitleHidden(entry.titleId))
            model.at(i) = AppEntry{};
    }
}

std::vector<std::pair<std::uint64_t, std::string>> WiiUMenuApp::hiddenGamesForSettings() const {
    std::vector<std::pair<std::uint64_t, std::string>> out;
    for (const auto titleId : m_config.hiddenTitles) {
        std::string name;
        for (const auto& app : m_allApps) {
            if (app.titleId == titleId) {
                name = app.title;
                break;
            }
        }
        if (name.empty()) {
            char buf[17];
            std::snprintf(buf, sizeof(buf), "%016llX", static_cast<unsigned long long>(titleId));
            name = buf;
        }
        out.emplace_back(titleId, std::move(name));
    }
    return out;
}

void WiiUMenuApp::hideTitle(std::uint64_t titleId) {
    if (titleId == 0 || m_config.isTitleHidden(titleId))
        return;

    // Land on the game next to it afterwards.
    std::uint64_t next = 0;
    const int index = findTitleIndex(titleId);
    if (index >= 0) {
        for (int i = index + 1; i < m_model.count() && next == 0; ++i)
            if (m_model.at(i).titleId != 0) next = m_model.at(i).titleId;
        for (int i = index - 1; i >= 0 && next == 0; --i)
            if (m_model.at(i).titleId != 0) next = m_model.at(i).titleId;
    }

    m_config.hiddenTitles.push_back(titleId);
    if (m_configSaveFuture.valid())
        m_configSaveFuture.wait();
    m_configSaveFuture = m_threadPool.submit([config = m_config]() { config.save(); });

    if (m_openFolderId != 0)
        applyDisplayModel(buildOpenFolderModel(m_openFolderId), next, false);
    else
        applyDisplayModel(buildRootFolderModel(), next, false);
    if (m_grid) {
        if (auto* target = m_grid->focusManager().current())
            focusManager().setFocus(target);
    }
    updateCursor();
    pushHomeOptionsToSettings();
    if (m_settings) m_settingsNeedRefresh = true;
}

void WiiUMenuApp::unhideTitle(std::uint64_t titleId) {
    auto& hidden = m_config.hiddenTitles;
    const auto it = std::find(hidden.begin(), hidden.end(), titleId);
    if (it == hidden.end())
        return;
    hidden.erase(it);
    if (m_configSaveFuture.valid())
        m_configSaveFuture.wait();
    m_configSaveFuture = m_threadPool.submit([config = m_config]() { config.save(); });

    // Rebuilding the row moves focus to it, so wait until Settings is closed.
    m_homeRebuildPending = true;
    pushHomeOptionsToSettings();
    if (m_settings) m_settingsNeedRefresh = true;
}

// ---------------------------------------------------------------------------
// Portrait covers
// ---------------------------------------------------------------------------

void WiiUMenuApp::rebuildIconSwapIndex() {
    m_iconSwapIndex.clear();
    m_iconSwapIndexed = true;
    std::error_code ec;
    if (!std::filesystem::is_directory(kIconSwapFolder, ec))
        return;

    auto addFile = [this](const std::filesystem::path& file,
                          const std::vector<std::string>& extraKeys) {
        auto keys = indexKeysFor(file.stem().string());
        keys.insert(keys.end(), extraKeys.begin(), extraKeys.end());
        for (const auto& key : keys) {
            auto& list = m_iconSwapIndex[key];
            const std::string path = file.string();
            if (std::find(list.begin(), list.end(), path) == list.end())
                list.push_back(path);
        }
    };

    std::filesystem::directory_iterator top(kIconSwapFolder, ec), end;
    for (; !ec && top != end; top.increment(ec)) {
        const auto& entry = *top;
        std::error_code typeEc;
        if (entry.is_regular_file(typeEc) && isImageFile(entry.path())) {
            addFile(entry.path(), {});
        } else if (entry.is_directory(typeEc)) {
            // A folder per game ("0100ABCD12345000/icon.jpg", "Quake II/cover.png"):
            // every image inside belongs to that game. One extra level is read.
            const auto folderKeys = indexKeysFor(entry.path().filename().string());
            std::error_code innerEc;
            std::filesystem::recursive_directory_iterator inner(entry.path(), innerEc), innerEnd;
            for (; !innerEc && inner != innerEnd; inner.increment(innerEc)) {
                if (inner.depth() > 1) { inner.disable_recursion_pending(); continue; }
                std::error_code fileEc;
                if (inner->is_regular_file(fileEc) && isImageFile(inner->path()))
                    addFile(inner->path(), folderKeys);
            }
        }
    }
    for (auto& [key, list] : m_iconSwapIndex)
        std::sort(list.begin(), list.end());
    DebugLog::log("[portrait] iconswap index keys=%zu", m_iconSwapIndex.size());
}

std::vector<std::string> WiiUMenuApp::coverCandidates(std::uint64_t titleId,
                                                      const std::string& title) {
    std::vector<std::string> out;
    if (titleId == 0)
        return out;
    if (!m_iconSwapIndexed)
        rebuildIconSwapIndex();
    auto add = [&out](const std::string& path) {
        if (!path.empty() && std::find(out.begin(), out.end(), path) == out.end())
            out.push_back(path);
    };
    // 1) /iconswap, by title ID first, then by game name.
    if (auto found = m_iconSwapIndex.find(titleIdKey(titleId)); found != m_iconSwapIndex.end())
        for (const auto& path : found->second) add(path);
    const std::string name = normaliseName(title);
    if (!name.empty())
        if (auto found = m_iconSwapIndex.find(name); found != m_iconSwapIndex.end())
            for (const auto& path : found->second) add(path);
    // 2) /config/SwitchU/portraits/<TITLEID>.png|.jpg
    char upper[17];
    std::snprintf(upper, sizeof(upper), "%016llX", static_cast<unsigned long long>(titleId));
    const std::string lower = titleIdKey(titleId);
    for (const std::string& id : {std::string(upper), lower})
        for (const char* ext : {".png", ".jpg", ".jpeg"}) {
            std::string path = std::string(kPortraitFolder) + "/" + id + ext;
            if (fileExists(path)) add(path);
        }
    // 3) The cover the SteamGridDB scan downloaded.
    const std::string downloaded = SteamGridDbManager::portraitPath(titleId);
    if (fileExists(downloaded)) add(downloaded);
    return out;
}

std::string WiiUMenuApp::portraitSourcePath(std::uint64_t titleId, const std::string& title) {
    if (titleId == 0)
        return {};
    const std::string choice = m_config.coverChoiceFor(titleId);
    if (choice == "icon")
        return {};   // the player picked the plain square icon
    if (!choice.empty() && fileExists(choice))
        return choice;
    const auto candidates = coverCandidates(titleId, title);
    return candidates.empty() ? std::string() : candidates.front();
}

void WiiUMenuApp::releasePortraitCovers(bool all) {
    if (all) {
        if (m_portraitDecode)
            m_portraitDecode->cancelled.store(true);
        m_portraitMissing.clear();
    }
    if (m_portraitCovers.empty())
        return;

    std::vector<std::uint64_t> release;
    if (all) {
        for (const auto& entry : m_portraitCovers)
            release.push_back(entry.first);
    } else {
        if (m_portraitCovers.size() < kPortraitReleaseBatch || !m_grid)
            return;
        const auto& icons = m_grid->allIcons();
        const int focus = std::max(0, m_grid->focusedGlobalIndex());
        const int lo = focus - kPortraitKeepBehind;
        const int hi = focus + kPortraitKeepAhead;
        std::unordered_map<std::uint64_t, bool> keep;
        for (int i = std::max(0, lo); i <= hi && i < (int)icons.size(); ++i)
            if (icons[(std::size_t)i]) keep[icons[(std::size_t)i]->titleId()] = true;
        for (const auto& entry : m_portraitCovers)
            if (!keep.count(entry.first))
                release.push_back(entry.first);
    }
    if (release.empty())
        return;

    // Nothing may still point at a texture once it is gone.
    if (m_grid) {
        for (const auto& icon : m_grid->allIcons()) {
            if (!icon || !icon->portraitTexture()) continue;
            if (std::find(release.begin(), release.end(), icon->titleId()) != release.end())
                icon->setPortraitTexture(nullptr);
        }
    }
    app().gpu().waitIdle();
    for (const std::uint64_t titleId : release)
        m_portraitCovers.erase(titleId);
#ifdef NXUI_BACKEND_DEKO3D
    app().renderer().reclaimReleasedTextureSlotsAfterIdle();
#endif
    DebugLog::log("[portrait] released=%zu kept=%zu", release.size(), m_portraitCovers.size());
}

void WiiUMenuApp::reloadCover(std::uint64_t titleId) {
    if (m_portraitDecode && m_portraitDecode->titleId == titleId)
        m_portraitDecode->cancelled.store(true);
    m_portraitMissing.erase(titleId);
    if (!m_portraitCovers.count(titleId))
        return;
    if (m_grid) {
        for (const auto& icon : m_grid->allIcons())
            if (icon && icon->titleId() == titleId)
                icon->setPortraitTexture(nullptr);
    }
    app().gpu().waitIdle();
    m_portraitCovers.erase(titleId);
#ifdef NXUI_BACKEND_DEKO3D
    app().renderer().reclaimReleasedTextureSlotsAfterIdle();
#endif
}

void WiiUMenuApp::pollPortraitCovers() {
    if (!m_portraitFuture.valid())
        return;
    if (m_portraitFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    try {
        m_portraitFuture.get();
    } catch (...) {
        DebugLog::log("[portrait] decode failed");
    }
    auto state = std::move(m_portraitDecode);
    if (!state || state->cancelled.load())
        return;
    if (state->image.rgba.empty()) {
        m_portraitMissing[state->titleId] = true;
        return;
    }
    constexpr std::uint64_t kImageReserve = 4u * 1024u * 1024u;
    auto texture = std::make_unique<nxui::Texture>();
    if (app().gpu().imageMemoryAvailable() > kImageReserve + state->image.rgba.size() &&
        texture->loadFromPixels(app().gpu(), app().renderer(), state->image.rgba.data(),
                                state->image.width, state->image.height)) {
        m_portraitCovers[state->titleId] = std::move(texture);
    } else {
        // Out of image memory: try again on a later pass.
        DebugLog::log("[portrait] upload skipped title=0x%016llX",
                      static_cast<unsigned long long>(state->titleId));
    }
}

void WiiUMenuApp::syncPortraitCovers() {
    if (!m_grid || !m_grid->isPortraitLine()) {
        if (!m_portraitCovers.empty())
            releasePortraitCovers(true);
        return;
    }

    pollPortraitCovers();

    // Look for missing covers again now and then, so images copied to the SD
    // card or downloaded by a SteamGridDB scan show up without a restart.
    m_portraitRescanTimer += 1.f / 60.f;
    if (m_portraitRescanTimer > 12.f) {
        m_portraitRescanTimer = 0.f;
        m_portraitMissing.clear();
        m_iconSwapIndexed = false;
    }

    const auto& icons = m_grid->allIcons();
    if (icons.empty())
        return;
    const int focus = std::max(0, m_grid->focusedGlobalIndex());

    // Hand loaded covers to the icons in view (icons are rebuilt often).
    const int lo = std::max(0, focus - kPortraitKeepBehind);
    const int hi = std::min((int)icons.size() - 1, focus + kPortraitKeepAhead);
    for (int i = lo; i <= hi; ++i) {
        const auto& icon = icons[(std::size_t)i];
        if (!icon) continue;
        auto found = m_portraitCovers.find(icon->titleId());
        nxui::Texture* tex = found != m_portraitCovers.end() ? found->second.get() : nullptr;
        if (icon->portraitTexture() != tex)
            icon->setPortraitTexture(tex);
    }

    releasePortraitCovers(false);

    if (m_portraitFuture.valid())
        return;   // one decode at a time

    // Next cover to load: the focused one first, then its neighbours.
    static constexpr int kOrder[] = {0, 1, -1, 2, -2, 3, 4, -3, 5, 6, 7, 8};
    for (int offset : kOrder) {
        const int i = focus + offset;
        if (i < 0 || i >= (int)icons.size()) continue;
        const auto& icon = icons[(std::size_t)i];
        if (!icon || icon->entryKind() != GridEntryKind::Application) continue;
        const std::uint64_t titleId = icon->titleId();
        if (titleId == 0 || m_portraitCovers.count(titleId) || m_portraitMissing.count(titleId))
            continue;
        const std::string path = portraitSourcePath(titleId, icon->title());
        if (path.empty()) {
            m_portraitMissing[titleId] = true;
            continue;
        }
        auto state = std::make_shared<PortraitDecodeState>();
        state->titleId = titleId;
        m_portraitDecode = state;
        m_portraitFuture = m_threadPool.submit([state, path]() {
            if (state->cancelled.load()) return;
            state->image = steamgriddb::artwork::decode(path, kPortraitDecodeW,
                                                        kPortraitDecodeH, true);
        });
        break;
    }
}

// ---------------------------------------------------------------------------
// Small pop-ups: + on a game, - on HOME, and the cover file browser
// ---------------------------------------------------------------------------

namespace {

// Centres a ContextMenu on screen (it opens to the right of its anchor).
nxui::Rect centredMenuAnchor() {
    return {640.f - 156.f - 16.f - 1.f, 340.f, 1.f, 40.f};
}


} // namespace

void WiiUMenuApp::confirmDeleteSoftware(std::uint64_t titleId, const std::string& title) {
    if (!m_dialog) return;
    auto& i18n = nxui::I18n::instance();
    m_dialogReturnFocus = focusManager().current();
    m_dialog->show(
        i18n.tr("game.uninstall_title", "Delete software"),
        i18n.tr("game.delete_confirmation", "The software will be deleted. Save data will remain.")
            + std::string("\n") + title,
        {
            {i18n.tr("button.cancel", "Cancel"), []() {}, true},
            {i18n.tr("button.delete", "Delete"), [this, titleId, title]() {
                startSoftwareDeletion(titleId, title, false);
            }, true},
        }, 0, {});
    focusManager().setFocus(m_dialog.get());
}

void WiiUMenuApp::showQuickGameMenu(GlossyIcon* icon) {
    if (!icon || !m_contextMenu || icon->titleId() == 0)
        return;
    auto& i18n = nxui::I18n::instance();
    const std::uint64_t titleId = icon->titleId();
    const std::string title = icon->title();
    const bool gameCard = icon->isGameCard();
    if (!m_contextMenu->isActive())
        m_contextMenuReturnFocus = focusManager().current();

    std::vector<ContextMenu::Item> items;
    items.push_back({i18n.tr("game.quick.change_icon", "Change Icon"),
                     [this, titleId, title]() {
                         // Start in /iconswap when it exists, else the SD root.
                         std::error_code ec;
                         const std::string start =
                             std::filesystem::is_directory(kIconSwapFolder, ec)
                                 ? std::string(kIconSwapFolder) : std::string("sdmc:/");
                         showCoverBrowser(titleId, title, start);
                     }});
    items.push_back({i18n.tr("game.hide", "Hide from Home"),
                     [this, titleId]() {
                         m_contextMenu->hide(false);
                         m_contextMenuReturnFocus = nullptr;
                         hideTitle(titleId);
                     }});
    items.push_back({gameCard
                         ? i18n.tr("game.quick.delete_card", "Delete (not for game cards)")
                         : i18n.tr("game.quick.delete", "Delete Game"),
                     [this, titleId, title]() {
                         m_contextMenu->hide();
                         confirmDeleteSoftware(titleId, title);
                     },
                     !gameCard});
#ifdef SWITCHU_MENU
    items.push_back({i18n.tr("game.quick.more", "More Options"),
                     [this, titleId]() {
                         m_contextMenu->hide();
                         if (focusTitle(titleId)) {
                             if (auto* focused = m_grid->focusManager().current()) {
                                 focusManager().setFocus(focused);
                                 if (focused->tag() == "glossy_icon")
                                     showGameContextMenu(static_cast<GlossyIcon*>(focused));
                             }
                         }
                     }});
#endif

    nxui::Rect anchor = icon->focusRect();
    if (m_grid && m_grid->isPortraitLine())
        anchor = m_grid->portraitFocusRestRect();
    m_contextMenu->show(anchor, title, std::move(items));
    m_audio.playSfx(Sfx::ModalShow);
    focusManager().setFocus(m_contextMenu.get());
}

void WiiUMenuApp::showCoverBrowser(std::uint64_t titleId, const std::string& title,
                                   const std::string& directory) {
    if (!m_coverBrowser || titleId == 0) return;
    if (m_contextMenu && m_contextMenu->isActive())
        m_contextMenu->hide(false);
    m_contextMenuReturnFocus = nullptr;
    m_coverBrowserTitleId = titleId;
    // Open in the folder of the current cover when it is a file, else `directory`.
    std::string start = directory;
    const std::string choice = m_config.coverChoiceFor(titleId);
    if (!choice.empty() && choice != "icon" && fileExists(choice)) {
        const auto slash = choice.find_last_of('/');
        if (slash != std::string::npos && slash > 5) start = choice.substr(0, slash);
    }
    m_navigator.navigate(switchu::navigation::Route::GameOptions);
    m_coverBrowser->open(title, start, choice);
    m_audio.playSfx(Sfx::ModalShow);
    focusManager().setFocus(m_coverBrowser.get());
}

void WiiUMenuApp::showPowerMenu() {
    if (!m_contextMenu) return;
    auto& i18n = nxui::I18n::instance();
    if (!m_contextMenu->isActive())
        m_contextMenuReturnFocus = focusManager().current();
    std::vector<ContextMenu::Item> items;
    items.push_back({i18n.tr("power.shutdown_full", "Power Off"), [this]() {
        m_contextMenu->hide(false);
        m_audio.playSfx(Sfx::ConfirmPositive);
#ifdef SWITCHU_MENU
        m_launcher.shutdown();
#else
        app().requestExit();
#endif
    }});
    items.push_back({i18n.tr("power.reboot", "Restart"), [this]() {
        m_contextMenu->hide(false);
        m_audio.playSfx(Sfx::ConfirmPositive);
#ifdef SWITCHU_MENU
        m_launcher.reboot();
#else
        app().requestExit();
#endif
    }});
    items.push_back({i18n.tr("sidebar.settings", "Settings"), [this]() {
        m_contextMenu->hide();
        if (m_openSettingsAction) m_openSettingsAction();
    }});
    m_contextMenu->show(centredMenuAnchor(), i18n.tr("power.title", "Power"), std::move(items));
    m_audio.playSfx(Sfx::ModalShow);
    focusManager().setFocus(m_contextMenu.get());
}
