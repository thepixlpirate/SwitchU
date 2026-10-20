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
// Big enough for the largest card (360x540) to stay sharp.
constexpr int kPortraitDecodeW = 400;
constexpr int kPortraitDecodeH = 600;
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
    // Holding - always opens Settings, so the icon can be hidden.
    visibility.settings = m_config.homeShowSettingsButton;
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
        m_grid->setSquareCovers(m_config.homeSquareCovers);
    }
    configureDynamicLineNavigation();
    if (m_grid) {
        m_grid->setGridPortrait(gridPortraitActive());
        const bool covers = (m_grid->isPortraitLine() && !m_config.homeSquareCovers) ||
                            m_grid->isPortraitGrid();
        if (!covers) releasePortraitCovers(true);
    }
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
    home.coverShape = m_config.homeSquareCovers ? 1 : 0;
    home.gridShape = m_config.homeGridShape;
    home.sortMode = m_config.sortMode;
    home.homeButtonSortMenu = m_config.homeButtonSortMenu;
    home.showArchived = m_config.homeShowArchived;
    home.longPressPowerMenu = m_config.homeLongPressPowerMenu;
    home.debugWindow = m_showDebugOverlay;
    home.videoBackgrounds = m_config.homeVideoBackgrounds;
    home.videoDim = m_config.homeVideoDim;
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
    if (key == "sortMode") {
        applySortMode(value);
        return;
    }
    if (key == "debugWindow") {
        // Not saved: a restart always comes back with it off.
        m_showDebugOverlay = on;
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
    else if (key == "coverShape")            m_config.homeSquareCovers = value == 1;
    else if (key == "gridShape") {
        m_config.homeGridShape = std::clamp(value, 0, 1);
        m_homeRebuildPending = true;   // new cell size, applied when Settings closes
    }
    else if (key == "homeButtonSortMenu")    m_config.homeButtonSortMenu = on;
    else if (key == "videoBackgrounds") {
        m_config.homeVideoBackgrounds = on;
        if (!on && m_videoBackdrop) m_videoBackdrop->stop();
        m_videoFocusTitleId = 0;
    }
    else if (key == "longPressPowerMenu")    m_config.homeLongPressPowerMenu = on;
    else if (key == "videoDim")              m_config.homeVideoDim = std::clamp(value, 0, 2);
    else if (key == "showArchived") {
        m_config.homeShowArchived = on;
        m_homeRebuildPending = true;   // applied when Settings closes
    }
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

bool WiiUMenuApp::isArchivedEntry(const AppEntry& entry) const {
    if (!entry.isApplication())
        return false;
    return isArchivedTitle(entry.titleId, entry.viewFlags);
}

bool WiiUMenuApp::isArchivedTitle(std::uint64_t titleId, std::uint32_t flags) const {
    if (titleId == 0)
        return false;
    // Deleted on this run: hide straight away, before the console's list
    // catches up.
    if (m_deletedTitles.count(titleId))
        return true;
    if (flags == 0)
        return false;   // status unknown: keep it
    if (switchu::ns::viewHasFlag(flags, switchu::ns::AppViewFlag_IsGameCard) ||
        switchu::ns::viewHasFlag(flags, switchu::ns::AppViewFlag_IsDownloading))
        return false;
    return !switchu::ns::viewHasFlag(flags, switchu::ns::AppViewFlag_HasMainContents);
}

void WiiUMenuApp::dropHiddenTitles(GridModel& model) const {
    const bool dropArchived = !m_config.homeShowArchived;
    if (m_config.hiddenTitles.empty() && !dropArchived)
        return;
    for (int i = 0; i < model.count(); ++i) {
        const AppEntry& entry = model.at(i);
        if (!entry.isApplication()) continue;
        if (m_config.isTitleHidden(entry.titleId) || (dropArchived && isArchivedEntry(entry)))
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

// Which icons need their cover: around the focus in the single row; the whole
// visible page (plus the next one) in the grid.
static void portraitWindow(const IconGrid& grid, int& lo, int& hi) {
    const int count = static_cast<int>(grid.allIcons().size());
    if (grid.isPortraitGrid()) {
        const int perPage = std::max(1, grid.iconsPerPage());
        lo = grid.currentPage() * perPage;
        hi = std::min(count - 1, lo + perPage * 2 - 1);
    } else {
        const int focus = std::max(0, grid.focusedGlobalIndex());
        lo = std::max(0, focus - 3);
        hi = std::min(count - 1, focus + 8);
    }
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
        int lo = 0, hi = -1;
        portraitWindow(*m_grid, lo, hi);
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
    // Square cards use each game's own icon, so no cover art is loaded.
    const bool wanted = m_grid &&
        ((m_grid->isPortraitLine() && !m_config.homeSquareCovers) || m_grid->isPortraitGrid());
    if (!wanted) {
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
    int lo = 0, hi = -1;
    portraitWindow(*m_grid, lo, hi);
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

    // Next cover to load: the focused one first, then its neighbours (single
    // row) or the rest of the page (grid).
    static constexpr int kOrder[] = {0, 1, -1, 2, -2, 3, 4, -3, 5, 6, 7, 8};
    std::vector<int> order;
    if (m_grid->isPortraitGrid()) {
        order.push_back(focus);
        for (int i = lo; i <= hi; ++i) order.push_back(i);
    } else {
        for (int offset : kOrder) order.push_back(focus + offset);
    }
    for (int i : order) {
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
                m_softwareDeleteFromHome = true;
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
    // Last row: the same Sort Games pop-up the HOME button opens.
    items.push_back({i18n.tr("game.quick.sort", "Sort Games") + "  (" + sortModeLabel() + ")",
                     [this]() { showSortMenu(); }});

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
    m_coverBrowser->open(nxui::I18n::instance().tr("game.quick.change_icon", "Change Icon")
                             + (title.empty() ? std::string() : "  -  " + title),
                         start, choice, true);
    m_audio.playSfx(Sfx::ModalShow);
    focusManager().setFocus(m_coverBrowser.get());
}

void WiiUMenuApp::showPowerMenu(bool includeSettings) {
    if (!m_contextMenu) return;
    auto& i18n = nxui::I18n::instance();
    if (!m_contextMenu->isActive())
        m_contextMenuReturnFocus = focusManager().current();
    std::vector<ContextMenu::Item> items;
    items.push_back({i18n.tr("power.reboot", "Restart"), [this]() {
        m_contextMenu->hide(false);
        m_audio.playSfx(Sfx::ConfirmPositive);
#ifdef SWITCHU_MENU
        m_launcher.reboot();
#else
        app().requestExit();
#endif
    }});
    items.push_back({i18n.tr("power.sleep", "Sleep"), [this]() {
        m_contextMenu->hide(false);
        m_audio.playSfx(Sfx::ConfirmPositive);
#ifdef SWITCHU_MENU
        m_launcher.enterSleep();
#else
        app().requestExit();
#endif
    }});
    items.push_back({i18n.tr("power.shutdown_full", "Power Off"), [this]() {
        m_contextMenu->hide(false);
        m_audio.playSfx(Sfx::ConfirmPositive);
#ifdef SWITCHU_MENU
        m_launcher.shutdown();
#else
        app().requestExit();
#endif
    }});
    if (includeSettings) {
        items.push_back({i18n.tr("sidebar.settings", "Settings"), [this]() {
            m_contextMenu->hide();
            if (m_openSettingsAction) m_openSettingsAction();
        }});
    }
    m_contextMenu->show(centredMenuAnchor(), i18n.tr("power.title", "Power"), std::move(items));
    m_audio.playSfx(Sfx::ModalShow);
    focusManager().setFocus(m_contextMenu.get());
}

bool WiiUMenuApp::jumpLine(int direction) {
    if (!m_grid || m_appLayoutMode != AppLayoutMode::DynamicLine || m_editMode)
        return false;
    const auto& icons = m_grid->allIcons();
    int first = -1, last = -1;
    for (int i = 0; i < static_cast<int>(icons.size()); ++i) {
        if (icons[static_cast<std::size_t>(i)] && icons[static_cast<std::size_t>(i)]->isFocusable()) {
            if (first < 0) first = i;
            last = i;
        }
    }
    if (first < 0)
        return true;
    const int current = std::max(first, m_grid->focusedGlobalIndex());
    // One screenful: the covers on screen in the centred style, else 5.
    const int step = m_grid->lineStyle() == 2 ? std::max(3, m_config.homeCoversShown)
                   : m_grid->lineStyle() == 1 ? 4 : 5;
    int target = std::clamp(current + direction * step, first, last);
    while (target != current && !(icons[static_cast<std::size_t>(target)] &&
                                  icons[static_cast<std::size_t>(target)]->isFocusable()))
        target -= direction;
    if (target == current)
        return true;   // already at the start/end
    m_grid->focusGlobalIndex(target, false);
    if (auto* focused = m_grid->focusManager().current())
        focusManager().setFocus(focused);
    m_audio.playSfx(Sfx::Navigate);
    updateCursor();
    return true;
}

// ---------------------------------------------------------------------------
// Sorting (HOME button pop-up)
// ---------------------------------------------------------------------------

void WiiUMenuApp::noteNewTitles() {
    // First run of this feature: everything already installed shares 0, so
    // "newest" only moves games installed from now on ahead of the rest.
    bool changed = false;
    for (const auto& app : m_allApps) {
        if (!app.isApplication() || app.titleId == 0) continue;
        bool known = false;
        for (const auto& entry : m_config.firstSeen)
            if (entry.first == app.titleId) { known = true; break; }
        if (known) continue;
        m_config.firstSeen.emplace_back(app.titleId, m_config.firstSeenSequence);
        changed = true;
    }
    if (changed) {
        // Later arrivals get a higher number than everything seen before.
        ++m_config.firstSeenSequence;
        if (m_configSaveFuture.valid())
            m_configSaveFuture.wait();
        m_configSaveFuture = m_threadPool.submit([config = m_config]() { config.save(); });
    }
}

void WiiUMenuApp::applySortMode(int mode) {
    mode = std::clamp(mode, 0, 5);
    if (m_config.sortMode == mode)
        return;
    m_config.sortMode = mode;
    if (m_configSaveFuture.valid())
        m_configSaveFuture.wait();
    m_configSaveFuture = m_threadPool.submit([config = m_config]() { config.save(); });
    if (m_navigator.route() == switchu::navigation::Route::Home && m_openFolderId == 0) {
        reflowHomeGrid();
        if (m_grid) {
            // Start from the first game of the new order.
            const auto& icons = m_grid->allIcons();
            for (int i = 0; i < static_cast<int>(icons.size()); ++i) {
                if (icons[static_cast<std::size_t>(i)] &&
                    icons[static_cast<std::size_t>(i)]->isFocusable()) {
                    m_grid->focusGlobalIndex(i, false);
                    break;
                }
            }
            if (auto* focused = m_grid->focusManager().current())
                focusManager().setFocus(focused);
        }
        updateCursor();
    } else {
        m_homeRebuildPending = true;
    }
    pushHomeOptionsToSettings();
}

void WiiUMenuApp::showSortMenu() {
    if (!m_contextMenu) return;
    auto& i18n = nxui::I18n::instance();
    if (!m_contextMenu->isActive())
        m_contextMenuReturnFocus = focusManager().current();
    const std::pair<int, std::string> modes[] = {
        {0, i18n.tr("settings.home.sort_custom", "My Order")},
        {4, i18n.tr("settings.home.sort_newest", "Newest Added")},
        {5, i18n.tr("settings.home.sort_oldest", "Oldest Added")},
        {2, i18n.tr("settings.home.sort_recent", "Recently Played")},
        {1, i18n.tr("settings.home.sort_az", "Title A-Z")},
        {3, i18n.tr("settings.home.sort_za", "Title Z-A")},
    };
    std::vector<ContextMenu::Item> items;
    int selected = 0;
    for (const auto& [mode, label] : modes) {
        if (mode == m_config.sortMode) selected = static_cast<int>(items.size());
        const std::string shown = mode == m_config.sortMode ? label + "  (current)" : label;
        items.push_back({shown, [this, m = mode]() {
            m_contextMenu->hide();
            applySortMode(m);
        }});
    }
    m_contextMenu->show(centredMenuAnchor(), i18n.tr("settings.home.sort", "Sort Games"),
                        std::move(items), selected);
    m_audio.playSfx(Sfx::ModalShow);
    focusManager().setFocus(m_contextMenu.get());
}

void WiiUMenuApp::processPendingHomeRebuild() {
    if (!m_homeRebuildPending || !m_grid || m_openFolderId != 0)
        return;
    if (m_navigator.route() != switchu::navigation::Route::Home ||
        (m_contextMenu && m_contextMenu->isActive()) ||
        (m_dialog && m_dialog->isActive()) ||
        (m_progressDialog && m_progressDialog->isActive()) ||
        (m_coverBrowser && m_coverBrowser->isActive()) ||
        (m_settings && m_settings->isActive()))
        return;
    m_homeRebuildPending = false;
    std::uint64_t focusId = 0;
    if (!m_startupFocusSettled) {
        // First rebuild after the menu starts: this is where the chosen sort
        // order is applied. Keeping the focused game here meant whatever game
        // was first in "My Order" ended up selected halfway down the sorted
        // list. Start on the first game instead (or the game left open).
        m_startupFocusSettled = true;
        focusId = m_launcher.suspendedTitleId();
    } else if (auto* cur = m_grid->focusManager().current();
               cur && cur->tag() == "glossy_icon") {
        focusId = static_cast<GlossyIcon*>(cur)->titleId();
    }
    applyDisplayModel(buildRootFolderModel(), focusId, false, {}, true);
    if (focusId == 0) {
        const auto& icons = m_grid->allIcons();
        for (int i = 0; i < static_cast<int>(icons.size()); ++i) {
            if (icons[static_cast<std::size_t>(i)] &&
                icons[static_cast<std::size_t>(i)]->isFocusable()) {
                m_grid->focusGlobalIndex(i, true);
                break;
            }
        }
    }
    if (auto* focused = m_grid->focusManager().current())
        focusManager().setFocus(focused);
    updateCursor();
}

// ---------------------------------------------------------------------------
// Themes > Options: highlight colour and HOME background
// ---------------------------------------------------------------------------

nxui::Color WiiUMenuApp::highlightColor() const {
    nxui::Color color(0.48f, 0.12f, 0.66f, 1.f);
    ColorPicker::parseHex(m_config.homeHighlightHex, color);
    return color;
}

void WiiUMenuApp::pushHomeStyleToThemeShop() {
    if (!m_themeShop) return;
    m_themeShop->setHomeStyleState({m_config.homeHighlightHex, m_config.homeBackgroundMode,
                                    m_config.homeBackgroundHex,
                                    std::filesystem::path(m_config.homeBackgroundImage)
                                        .filename().string()});
    if (m_themeShop->isActive())
        m_themeShop->rebuildCurrentTab();
}

void WiiUMenuApp::openColorPicker(const std::string& target) {
    if (!m_colorPicker) return;
    auto& i18n = nxui::I18n::instance();
    m_colorPickerTarget = target;
    if (m_overlayLayer) {   // above the Themes window
        m_overlayLayer->removeChild(m_colorPicker.get());
        m_overlayLayer->addChild(m_colorPicker);
    }
    if (target == "background")
        m_colorPicker->open(i18n.tr("themeshop.options.bg_color_pick", "Background Color"),
                            m_config.homeBackgroundHex, ColorPicker::Preview::Fill);
    else
        m_colorPicker->open(i18n.tr("themeshop.options.highlight", "Game Highlight Color"),
                            m_config.homeHighlightHex, ColorPicker::Preview::Outline);
    m_audio.playSfx(Sfx::ModalShow);
    focusManager().setFocus(m_colorPicker.get());
}

void WiiUMenuApp::handleHomeStyleAction(const std::string& action, int value) {
    if (action == "highlight") {
        openColorPicker("highlight");
    } else if (action == "backgroundColor") {
        openColorPicker("background");
    } else if (action == "backgroundImage") {
        if (!m_coverBrowser) return;
        std::error_code ec;
        std::string start = "sdmc:/config/SwitchU/backgrounds";
        std::filesystem::create_directories(start, ec);
        if (!m_config.homeBackgroundImage.empty()) {
            const auto slash = m_config.homeBackgroundImage.find_last_of('/');
            if (slash != std::string::npos && slash > 5)
                start = m_config.homeBackgroundImage.substr(0, slash);
        }
        m_browserForBackground = true;
        if (m_overlayLayer) {
            m_overlayLayer->removeChild(m_coverBrowser.get());
            m_overlayLayer->addChild(m_coverBrowser);
        }
        m_coverBrowser->open(nxui::I18n::instance().tr("themeshop.options.bg_image_pick",
                                                       "Background Picture"),
                             start, m_config.homeBackgroundImage, false);
        m_audio.playSfx(Sfx::ModalShow);
        focusManager().setFocus(m_coverBrowser.get());
    } else if (action == "backgroundMode") {
        m_config.homeBackgroundMode = std::clamp(value, 0, 2);
        if (m_configSaveFuture.valid())
            m_configSaveFuture.wait();
        m_configSaveFuture = m_threadPool.submit([config = m_config]() { config.save(); });
        applyHomeBackground();
    }
}

void WiiUMenuApp::applyHomeBackground() {
    if (!m_homeBackdrop) return;
    switch (m_config.homeBackgroundMode) {
        case 1: {
            nxui::Color color(0.f, 0.f, 0.f, 1.f);
            ColorPicker::parseHex(m_config.homeBackgroundHex, color);
            m_homeBackdrop->setColor(color);
            m_homeBackdrop->setMode(HomeBackdrop::Mode::Solid);
            break;
        }
        case 2: {
            const std::string path = m_config.homeBackgroundImage;
            if (path.empty() || !fileExists(path)) {
                m_homeBackdrop->setMode(HomeBackdrop::Mode::Theme);   // nothing chosen yet
                break;
            }
            m_homeBackdrop->setMode(HomeBackdrop::Mode::Image);
            if (path != m_homeBackgroundLoadedPath && !m_homeBackgroundFuture.valid()) {
                auto job = std::make_shared<BackgroundDecode>();
                job->path = path;
                m_homeBackgroundDecode = job;
                m_homeBackgroundFuture = m_threadPool.submit([job]() {
                    job->image = steamgriddb::artwork::decode(job->path, 1280, 720, true);
                });
            }
            m_homeBackdrop->setTexture(path == m_homeBackgroundLoadedPath
                                           ? m_homeBackgroundTexture.get() : nullptr);
            break;
        }
        default:
            m_homeBackdrop->setMode(HomeBackdrop::Mode::Theme);
            break;
    }
}

void WiiUMenuApp::pollHomeBackground() {
    if (!m_homeBackgroundFuture.valid() ||
        m_homeBackgroundFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    try { m_homeBackgroundFuture.get(); } catch (...) {}
    auto job = std::move(m_homeBackgroundDecode);
    if (!job) return;
    // Retire the old picture before uploading the new one.
    if (m_homeBackgroundTexture) {
        if (m_homeBackdrop) m_homeBackdrop->setTexture(nullptr);
        app().gpu().waitIdle();
        m_homeBackgroundTexture.reset();
#ifdef NXUI_BACKEND_DEKO3D
        app().renderer().reclaimReleasedTextureSlotsAfterIdle();
#endif
    }
    m_homeBackgroundLoadedPath = job->path;
    if (!job->image.rgba.empty()) {
        auto texture = std::make_unique<nxui::Texture>();
        if (texture->loadFromPixels(app().gpu(), app().renderer(), job->image.rgba.data(),
                                    job->image.width, job->image.height))
            m_homeBackgroundTexture = std::move(texture);
    }
    // The choice may have changed again while this was decoding.
    applyHomeBackground();
}

void WiiUMenuApp::updateTriggerRepeat(float dt) {
    auto& input = app().input();
    const bool left = input.isHeld(nxui::Button::ZL);
    const bool right = input.isHeld(nxui::Button::ZR);
    const bool usable = (left != right) && m_grid &&
        m_appLayoutMode == AppLayoutMode::DynamicLine && !m_editMode &&
        m_navigator.route() == switchu::navigation::Route::Home &&
        focusRoot() == &rootBox();
    if (!usable) {
        m_triggerHoldTime = 0.f;
        m_triggerRepeatTimer = 0.f;
        return;
    }
    // The first jump comes from the button press itself; after a short pause
    // keep jumping while the trigger stays held, a little faster over time.
    constexpr float kDelay = 0.35f;
    m_triggerHoldTime += dt;
    if (m_triggerHoldTime < kDelay)
        return;
    const float interval = m_triggerHoldTime > 2.0f ? 0.09f : 0.16f;
    m_triggerRepeatTimer -= dt;
    if (m_triggerRepeatTimer > 0.f)
        return;
    m_triggerRepeatTimer = interval;
    jumpLine(right ? +1 : -1);
}

// ---------------------------------------------------------------------------
// Video backgrounds
// ---------------------------------------------------------------------------

void WiiUMenuApp::rebuildVideoIndex() {
    m_videoIndex.clear();
    m_videoIndexed = true;
    m_videoIndexAge = 0.f;
    constexpr const char* kVideoFolder = "sdmc:/config/SwitchU/videos";
    std::error_code ec;
    std::filesystem::create_directories(kVideoFolder, ec);
    ec.clear();
    auto addFile = [this](const std::filesystem::path& file,
                          const std::vector<std::string>& extraKeys) {
        std::string ext = file.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".webp" && ext != ".png" && ext != ".jpg" && ext != ".jpeg") return;
        auto keys = indexKeysFor(file.stem().string());
        keys.insert(keys.end(), extraKeys.begin(), extraKeys.end());
        for (const auto& key : keys) m_videoIndex[key].push_back(file.string());
    };
    std::filesystem::directory_iterator it(kVideoFolder, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        std::error_code typeEc;
        if (it->is_regular_file(typeEc)) {
            addFile(it->path(), {});
        } else if (it->is_directory(typeEc)) {
            // A folder per game works too: videos/<name or id>/anything.webp
            const auto folderKeys = indexKeysFor(it->path().filename().string());
            std::error_code innerEc;
            std::filesystem::directory_iterator inner(it->path(), innerEc), innerEnd;
            for (; !innerEc && inner != innerEnd; inner.increment(innerEc)) {
                std::error_code fileEc;
                if (inner->is_regular_file(fileEc)) addFile(inner->path(), folderKeys);
            }
        }
    }
    // A clip (.webp) wins over a still picture for the same game.
    auto isClip = [](const std::string& p) {
        std::string lower = p;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower.size() > 5 && lower.compare(lower.size() - 5, 5, ".webp") == 0;
    };
    for (auto& [key, list] : m_videoIndex)
        std::sort(list.begin(), list.end(), [&](const std::string& a, const std::string& b) {
            if (isClip(a) != isClip(b)) return isClip(a);
            return a < b;
        });
}

std::string WiiUMenuApp::videoPathFor(std::uint64_t titleId, const std::string& title) {
    if (!m_videoIndexed) rebuildVideoIndex();
    if (auto found = m_videoIndex.find(titleIdKey(titleId)); found != m_videoIndex.end())
        return found->second.front();
    const std::string name = normaliseName(title);
    if (!name.empty())
        if (auto found = m_videoIndex.find(name); found != m_videoIndex.end())
            return found->second.front();
    return {};
}

void WiiUMenuApp::updateVideoBackground(float dt) {
    if (!m_videoBackdrop) return;
    m_videoBackdrop->setDim(m_config.homeVideoDim == 0 ? 0.f
                            : m_config.homeVideoDim == 1 ? 0.22f : 0.42f);

    // Look for newly copied clips now and then.
    m_videoIndexAge += dt;
    if (m_videoIndexAge > 15.f) m_videoIndexed = false;

    const bool launching = (m_launchAnim && m_launchAnim->isPlaying()) ||
                           m_leaveCapturePending || m_leaveCaptureDeferred;
    if (!m_config.homeVideoBackgrounds || launching || !m_grid) {
        m_videoBackdrop->stop();
        m_videoFocusTitleId = 0;
        m_videoBackdrop->pump(dt);
        return;
    }
    // Keep the clip but stop decoding while a menu or window covers HOME.
    const bool covered = m_navigator.route() != switchu::navigation::Route::Home;
    m_videoBackdrop->setPaused(covered);

    // Follow the selected game. Moving off games (e.g. to Settings) keeps the
    // last clip playing.
    nxui::Widget* cur = m_grid->focusManager().current();
    if (cur && cur->tag() == "glossy_icon") {
        auto* icon = static_cast<GlossyIcon*>(cur);
        const std::uint64_t titleId =
            icon->entryKind() == GridEntryKind::Application ? icon->titleId() : 0;
        if (titleId != m_videoFocusTitleId) {
            m_videoFocusTitleId = titleId;
            m_videoSettle = 0.f;
            m_videoBackdrop->stop();   // fade out while moving
        }
        const bool scrolling = m_grid->isDynamicLineScrolling() || m_grid->isTransitioning();
        if (scrolling) m_videoSettle = 0.f;
        else m_videoSettle += dt;
        // Start once you rest on a game for a moment (scrolling fast stays smooth).
        if (titleId != 0 && m_videoSettle >= 0.45f && !covered) {
            const std::string path = videoPathFor(titleId, icon->title());
            if (!path.empty()) m_videoBackdrop->play(path);
        }
    }
    m_videoBackdrop->pump(dt);
}

bool WiiUMenuApp::gridPortraitActive(int rows) const {
    if (rows < 0) rows = std::clamp(m_config.gridRows, 1, 5);
    // Tall covers only fit on the HOME grid with 1-2 rows; folders and
    // anything taller always use the square icons.
    return m_config.homeGridShape == 1 && rows <= 2 && m_openFolderId == 0 &&
           m_appLayoutMode == AppLayoutMode::Grid;
}
