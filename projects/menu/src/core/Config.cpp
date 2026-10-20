#include "Config.hpp"
#include "FolderStore.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <mutex>
#include <system_error>

namespace {

static constexpr const char* kLegacyConfigPath = "sdmc:/config/SwitchU/config.json";
static constexpr const char* kLegacyBackupPath = "sdmc:/config/SwitchU/config.json.bak";
std::mutex g_configSaveMutex;

template <typename T>
void readJsonOpt(const nlohmann::json& j, const char* key, T& out) {
    auto it = j.find(key);
    if (it != j.end() && !it->is_null()) {
        try {
            out = it->get<T>();
        } catch (...) {
        }
    }
}

} // namespace

bool AppConfig::load() {
    // The previous copy is tried when the current one is missing or unreadable.
    // A crash used to cost the player every setting they had ever chosen --
    // including tutorialCompleted, which is why the tutorial greeted them again
    // after the theme crash. Losing the last change is acceptable; losing the
    // whole file is not. Legacy paths remain last so existing installations
    // migrate without taking precedence over the new settings file.
    nlohmann::json j;
    bool parsed = false;
    for (const char* path : {kConfigPath, kBackupPath,
                             kLegacyConfigPath, kLegacyBackupPath}) {
        std::ifstream f(path);
        if (!f.is_open()) continue;
        try {
            f >> j;
            parsed = true;
            break;
        } catch (...) {
            // Truncated or half-written: fall through to the backup.
        }
    }
    if (!parsed) return false;

    readJsonOpt(j, "musicEnabled", musicEnabled);
    readJsonOpt(j, "musicVolume", musicVolume);
    readJsonOpt(j, "sfxVolume", sfxVolume);
    readJsonOpt(j, "musicTrackIndex", musicTrackIndex);
    readJsonOpt(j, "musicPositionSeconds", musicPositionSeconds);
    readJsonOpt(j, "musicShuffle", musicShuffle);
    readJsonOpt(j, "musicRepeatMode", musicRepeatMode);
    musicPlaylistOrder.clear();
    if (auto it = j.find("musicPlaylistOrder"); it != j.end() && it->is_array()) {
        for (const auto& item : *it) {
            if (item.is_string())
                musicPlaylistOrder.push_back(item.get<std::string>());
        }
    }
    readJsonOpt(j, "gridColumns", gridColumns);
    readJsonOpt(j, "gridRows", gridRows);
    {
        std::string modeStr;
        readJsonOpt(j, "appLayoutMode", modeStr);
        if (modeStr == "dynamic_line" || modeStr == "line")
            appLayoutMode = AppLayoutMode::DynamicLine;
        else if (modeStr == "grid")
            appLayoutMode = AppLayoutMode::Grid;
    }
    readJsonOpt(j, "actionHintStyle", actionHintStyle);
    readJsonOpt(j, "cursorMotionMode", cursorMotionMode);
    readJsonOpt(j, "pageTransitionMode", pageTransitionMode);
    readJsonOpt(j, "uiLanguageOverride", uiLanguageOverride);
    readJsonOpt(j, "soundPreset", soundPreset);
    readJsonOpt(j, "defaultProfileEnabled", defaultProfileEnabled);
    readJsonOpt(j, "defaultProfileUid", defaultProfileUid);
    readJsonOpt(j, "tutorialCompleted", tutorialCompleted);
    readJsonOpt(j, "clockUse12Hour", clockUse12Hour);
    readJsonOpt(j, "accessibilityEnabled", accessibilityEnabled);
    readJsonOpt(j, "accessibilitySpeakHints", accessibilitySpeakHints);
    readJsonOpt(j, "accessibilitySpeakContextEveryFocus", accessibilitySpeakContextEveryFocus);
    readJsonOpt(j, "accessibilitySpeakPosition", accessibilitySpeakPosition);
    readJsonOpt(j, "accessibilitySpeechRate", accessibilitySpeechRate);
    readJsonOpt(j, "steamGridDbEnabled", steamGridDbEnabled);
    readJsonOpt(j, "steamGridDbShowInGrid", steamGridDbShowInGrid);
    readJsonOpt(j, "steamGridDbShowInDynamicLine", steamGridDbShowInDynamicLine);
    readJsonOpt(j, "steamGridDbShowInFolders", steamGridDbShowInFolders);
    readJsonOpt(j, "steamGridDbApiKey", steamGridDbApiKey);
    readJsonOpt(j, "sortMode", sortMode);
    readJsonOpt(j, "homeButtonSortMenu", homeButtonSortMenu);
    readJsonOpt(j, "firstSeenSequence", firstSeenSequence);
    firstSeen.clear();
    if (auto it = j.find("firstSeen"); it != j.end() && it->is_object()) {
        for (auto& [key, value] : it->items()) {
            if (!value.is_number_unsigned()) continue;
            const auto titleId = std::strtoull(key.c_str(), nullptr, 16);
            if (titleId != 0)
                firstSeen.emplace_back(titleId, value.get<std::uint64_t>());
        }
    }
    readJsonOpt(j, "lastOpenedSequence", lastOpenedSequence);
    lastOpened.clear();
    if (auto it = j.find("lastOpened"); it != j.end() && it->is_object()) {
        for (auto& [key, value] : it->items()) {
            if (!value.is_number_unsigned()) continue;
            const auto titleId = std::strtoull(key.c_str(), nullptr, 16);
            if (titleId != 0)
                lastOpened.emplace_back(titleId, value.get<std::uint64_t>());
        }
    }
    readJsonOpt(j, "homeShowProfileButton", homeShowProfileButton);
    readJsonOpt(j, "homeShowHudBubbles", homeShowHudBubbles);
    readJsonOpt(j, "homeShowMiiButton", homeShowMiiButton);
    readJsonOpt(j, "homeShowControllersButton", homeShowControllersButton);
    readJsonOpt(j, "homeShowHomebrewButton", homeShowHomebrewButton);
    readJsonOpt(j, "homeShowSleepButton", homeShowSleepButton);
    readJsonOpt(j, "homeShowThemesButton", homeShowThemesButton);
    readJsonOpt(j, "homeShowSettingsButton", homeShowSettingsButton);
    readJsonOpt(j, "homeShowButtonHints", homeShowButtonHints);
    readJsonOpt(j, "homeShowDate", homeShowDate);
    readJsonOpt(j, "homeDateFormat", homeDateFormat);
    readJsonOpt(j, "homeClockPosition", homeClockPosition);
    readJsonOpt(j, "homeShowClock", homeShowClock);
    readJsonOpt(j, "homeShowBattery", homeShowBattery);
    readJsonOpt(j, "homeCoversShown", homeCoversShown);
    readJsonOpt(j, "homeSquareCovers", homeSquareCovers);
    readJsonOpt(j, "homeGridShape", homeGridShape);
    readJsonOpt(j, "homeShowArchived", homeShowArchived);
    readJsonOpt(j, "homeVideoBackgrounds", homeVideoBackgrounds);
    readJsonOpt(j, "homeVideoDim", homeVideoDim);
    readJsonOpt(j, "pirateFoilAddress", pirateFoilAddress);
    readJsonOpt(j, "pirateFoilApiKey", pirateFoilApiKey);
    readJsonOpt(j, "pirateFoilLastSync", pirateFoilLastSync);
    readJsonOpt(j, "homeHighlightHex", homeHighlightHex);
    readJsonOpt(j, "homeBackgroundMode", homeBackgroundMode);
    readJsonOpt(j, "homeBackgroundHex", homeBackgroundHex);
    readJsonOpt(j, "homeBackgroundImage", homeBackgroundImage);
    readJsonOpt(j, "homePlusAction", homePlusAction);
    readJsonOpt(j, "homeMinusAction", homeMinusAction);
    readJsonOpt(j, "homeLongPressPowerMenu", homeLongPressPowerMenu);
    readJsonOpt(j, "homeLineStyle", homeLineStyle);
    // A settings file written by an older build has no such key.
    homeDefaultsVersion = 0;
    readJsonOpt(j, "homeDefaultsVersion", homeDefaultsVersion);
    hiddenTitles.clear();
    if (auto it = j.find("hiddenTitles"); it != j.end() && it->is_array()) {
        for (const auto& item : *it) {
            if (!item.is_string()) continue;
            const auto titleId = std::strtoull(item.get<std::string>().c_str(), nullptr, 16);
            if (titleId != 0 && !isTitleHidden(titleId)) hiddenTitles.push_back(titleId);
        }
    }
    coverChoices.clear();
    if (auto it = j.find("coverChoices"); it != j.end() && it->is_object()) {
        for (auto& [key, value] : it->items()) {
            if (!value.is_string()) continue;
            const auto titleId = std::strtoull(key.c_str(), nullptr, 16);
            if (titleId != 0) coverChoices.emplace_back(titleId, value.get<std::string>());
        }
    }
    readJsonOpt(j, "themePreset", themePreset);
    readJsonOpt(j, "autoThemeMode", autoThemeMode);
    readJsonOpt(j, "autoThemeDayPreset", autoThemeDayPreset);
    readJsonOpt(j, "autoThemeNightPreset", autoThemeNightPreset);
    readJsonOpt(j, "autoThemeDayStartHour", autoThemeDayStartHour);
    readJsonOpt(j, "autoThemeNightStartHour", autoThemeNightStartHour);
    readJsonOpt(j, "autoThemeGeoResolved", autoThemeGeoResolved);
    readJsonOpt(j, "autoThemeGeoLat", autoThemeGeoLat);
    readJsonOpt(j, "autoThemeGeoLon", autoThemeGeoLon);
    readJsonOpt(j, "autoThemeGeoCity", autoThemeGeoCity);
    readJsonOpt(j, "folderStyle", folderStyle);
    const bool hasShowCoverKey = j.find("folderShowCover") != j.end();
    readJsonOpt(j, "folderShowCover", folderShowCover);

    if (musicVolume < 0.f) musicVolume = 0.f;
    if (musicVolume > 1.f) musicVolume = 1.f;
    if (sfxVolume   < 0.f) sfxVolume   = 0.f;
    if (sfxVolume   > 1.f) sfxVolume   = 1.f;
    gridColumns = std::clamp(gridColumns, 1, 8);
    gridRows = std::clamp(gridRows, 1, 5);
    if (actionHintStyle != "panel" && actionHintStyle != "capsules")
        actionHintStyle = "capsules";
    cursorMotionMode = std::clamp(cursorMotionMode, 0, 1);
    pageTransitionMode = std::clamp(pageTransitionMode, 0, 1);
    if (uiLanguageOverride.empty()) uiLanguageOverride = "auto";
    if (soundPreset.empty()) soundPreset = "wiiu";
    if (!defaultProfileEnabled) defaultProfileUid.clear();
    accessibilitySpeechRate = std::clamp(accessibilitySpeechRate, 120, 320);
    sortMode = std::clamp(sortMode, 0, 5);
    homeLineStyle = std::clamp(homeLineStyle, 0, 2);
    homeClockPosition = std::clamp(homeClockPosition, 0, 3);
    homeCoversShown = homeCoversShown >= 5 ? 5 : 3;
    homeDateFormat = std::clamp(homeDateFormat, 0, 1);
    homeBackgroundMode = std::clamp(homeBackgroundMode, 0, 2);
    homePlusAction = std::clamp(homePlusAction, 0, 1);
    homeMinusAction = std::clamp(homeMinusAction, 0, 1);
    if (homeDefaultsVersion < kHomeDefaultsVersion) {
        const int from = homeDefaultsVersion;
        if (from < 3) {
            // First start on an existing install: the minimal single-row look
            // (centred portrait covers, 12-hour clock, bottom bar, 5 covers).
            appLayoutMode = AppLayoutMode::DynamicLine;
            homeLineStyle = 2;
            clockUse12Hour = true;
            homeClockPosition = 2;
            homeCoversShown = 5;
        }
        if (from < 4) {
            // - switches the layout again; HOME behaves as originally; holding
            // HOME opens the power pop-up.
            homeMinusAction = 1;
            homeButtonSortMenu = false;
            homeLongPressPowerMenu = true;
        }
        if (from < 5) {
            // Holding - now opens Settings, so the icon is no longer needed.
            homeShowSettingsButton = false;
        }
        homeDefaultsVersion = kHomeDefaultsVersion;
    }
    if (themePreset.empty()) themePreset = "Default Light";
    if (!hasShowCoverKey) {
        // Local 9-style table used while vetting Cover/Plate as separate styles.
        switch (folderStyle) {
            case 0: // Classic
            case 1: // Simple
                break;
            case 2: // Cover → Simple + cover
                folderStyle = switchu::folders::kFolderStyleSimple;
                folderShowCover = true;
                break;
            case 3: folderStyle = switchu::folders::kFolderStyleMinimal; break;
            case 4: folderStyle = switchu::folders::kFolderStyleTab; break;
            case 5: folderStyle = switchu::folders::kFolderStyleRing; break;
            case 6: folderStyle = switchu::folders::kFolderStyleManila; break;
            case 7: folderStyle = switchu::folders::kFolderStyleClassic; break; // Plate
            case 8: folderStyle = switchu::folders::kFolderStyleLabel; break;
            default:
                folderStyle = switchu::folders::kDefaultFolderStyle;
                break;
        }
    }
    folderStyle = std::clamp(folderStyle, 0, switchu::folders::kFolderStyleCount - 1);

    if (autoThemeMode != "off" && autoThemeMode != "manual" && autoThemeMode != "geo")
        autoThemeMode = "off";
    autoThemeDayStartHour = std::clamp(autoThemeDayStartHour, 0, 23);
    autoThemeNightStartHour = std::clamp(autoThemeNightStartHour, 0, 23);
    if (autoThemeGeoLat < -90.0 || autoThemeGeoLat > 90.0
        || autoThemeGeoLon < -180.0 || autoThemeGeoLon > 180.0) {
        autoThemeGeoResolved = false;
        autoThemeGeoLat = 0.0;
        autoThemeGeoLon = 0.0;
        autoThemeGeoCity.clear();
    }

    return true;
}

bool AppConfig::save() const {
    // Settings closes are persisted by a worker while launch actions
    // can save on the UI thread. Both use the same staging filename, so those
    // writes must never overlap.
    const std::lock_guard<std::mutex> saveLock(g_configSaveMutex);
    std::error_code ec;
    std::filesystem::create_directory("sdmc:/config", ec);
    ec.clear();
    std::filesystem::create_directory(kConfigDir, ec);

    nlohmann::json j;
    j["musicEnabled"] = musicEnabled;
    j["musicVolume"] = musicVolume;
    j["sfxVolume"] = sfxVolume;
    j["musicTrackIndex"] = musicTrackIndex;
    j["musicPositionSeconds"] = musicPositionSeconds;
    j["musicShuffle"] = musicShuffle;
    j["musicRepeatMode"] = std::clamp(musicRepeatMode, 0, 2);
    j["musicPlaylistOrder"] = musicPlaylistOrder;
    j["gridColumns"] = std::clamp(gridColumns, 1, 8);
    j["gridRows"] = std::clamp(gridRows, 1, 5);
    j["appLayoutMode"] = (appLayoutMode == AppLayoutMode::DynamicLine) ? "dynamic_line" : "grid";
    j["actionHintStyle"] = actionHintStyle == "panel" ? "panel" : "capsules";
    j["cursorMotionMode"] = std::clamp(cursorMotionMode, 0, 1);
    j["pageTransitionMode"] = std::clamp(pageTransitionMode, 0, 1);
    j["uiLanguageOverride"] = uiLanguageOverride;
    j["soundPreset"] = soundPreset;
    j["defaultProfileEnabled"] = defaultProfileEnabled;
    j["defaultProfileUid"] = defaultProfileEnabled ? defaultProfileUid : std::string();
    j["tutorialCompleted"] = tutorialCompleted;
    j["clockUse12Hour"] = clockUse12Hour;
    j["accessibilityEnabled"] = accessibilityEnabled;
    j["accessibilitySpeakHints"] = accessibilitySpeakHints;
    j["accessibilitySpeakContextEveryFocus"] = accessibilitySpeakContextEveryFocus;
    j["accessibilitySpeakPosition"] = accessibilitySpeakPosition;
    j["accessibilitySpeechRate"] = std::clamp(accessibilitySpeechRate, 120, 320);
    j["steamGridDbEnabled"] = steamGridDbEnabled;
    j["steamGridDbShowInGrid"] = steamGridDbShowInGrid;
    j["steamGridDbShowInDynamicLine"] = steamGridDbShowInDynamicLine;
    j["steamGridDbShowInFolders"] = steamGridDbShowInFolders;
    j["steamGridDbApiKey"] = steamGridDbApiKey;
    j["sortMode"] = std::clamp(sortMode, 0, 5);
    j["homeButtonSortMenu"] = homeButtonSortMenu;
    j["firstSeenSequence"] = firstSeenSequence;
    {
        nlohmann::json seen = nlohmann::json::object();
        char key[17];
        for (const auto& entry : firstSeen) {
            std::snprintf(key, sizeof(key), "%016llX",
                          static_cast<unsigned long long>(entry.first));
            seen[key] = entry.second;
        }
        j["firstSeen"] = std::move(seen);
    }
    j["lastOpenedSequence"] = lastOpenedSequence;
    {
        nlohmann::json opened = nlohmann::json::object();
        char key[17];
        for (const auto& entry : lastOpened) {
            std::snprintf(key, sizeof(key), "%016llX",
                          static_cast<unsigned long long>(entry.first));
            opened[key] = entry.second;
        }
        j["lastOpened"] = std::move(opened);
    }
    j["homeShowProfileButton"] = homeShowProfileButton;
    j["homeShowHudBubbles"] = homeShowHudBubbles;
    j["homeShowMiiButton"] = homeShowMiiButton;
    j["homeShowControllersButton"] = homeShowControllersButton;
    j["homeShowHomebrewButton"] = homeShowHomebrewButton;
    j["homeShowSleepButton"] = homeShowSleepButton;
    j["homeShowThemesButton"] = homeShowThemesButton;
    j["homeShowSettingsButton"] = homeShowSettingsButton;
    j["homeShowButtonHints"] = homeShowButtonHints;
    j["homeShowDate"] = homeShowDate;
    j["homeDateFormat"] = std::clamp(homeDateFormat, 0, 1);
    j["homeClockPosition"] = std::clamp(homeClockPosition, 0, 3);
    j["homeShowClock"] = homeShowClock;
    j["homeShowBattery"] = homeShowBattery;
    j["homeCoversShown"] = homeCoversShown >= 5 ? 5 : 3;
    j["homeSquareCovers"] = homeSquareCovers;
    j["homeGridShape"] = std::clamp(homeGridShape, 0, 1);
    j["homeShowArchived"] = homeShowArchived;
    j["homeVideoBackgrounds"] = homeVideoBackgrounds;
    j["homeVideoDim"] = std::clamp(homeVideoDim, 0, 2);
    j["pirateFoilAddress"] = pirateFoilAddress;
    j["pirateFoilApiKey"] = pirateFoilApiKey;
    j["pirateFoilLastSync"] = pirateFoilLastSync;
    j["homeHighlightHex"] = homeHighlightHex;
    j["homeBackgroundMode"] = std::clamp(homeBackgroundMode, 0, 2);
    j["homeBackgroundHex"] = homeBackgroundHex;
    j["homeBackgroundImage"] = homeBackgroundImage;
    j["homePlusAction"] = std::clamp(homePlusAction, 0, 1);
    j["homeMinusAction"] = std::clamp(homeMinusAction, 0, 1);
    j["homeLongPressPowerMenu"] = homeLongPressPowerMenu;
    j["homeLineStyle"] = std::clamp(homeLineStyle, 0, 2);
    j["homeDefaultsVersion"] = homeDefaultsVersion;
    {
        nlohmann::json hidden = nlohmann::json::array();
        char key[17];
        for (const auto titleId : hiddenTitles) {
            std::snprintf(key, sizeof(key), "%016llX", static_cast<unsigned long long>(titleId));
            hidden.push_back(key);
        }
        j["hiddenTitles"] = std::move(hidden);
        nlohmann::json covers = nlohmann::json::object();
        for (const auto& entry : coverChoices) {
            std::snprintf(key, sizeof(key), "%016llX", static_cast<unsigned long long>(entry.first));
            covers[key] = entry.second;
        }
        j["coverChoices"] = std::move(covers);
    }
    j["themePreset"] = themePreset;
    j["autoThemeMode"] = (autoThemeMode == "manual" || autoThemeMode == "geo") ? autoThemeMode : "off";
    j["autoThemeDayPreset"] = autoThemeDayPreset;
    j["autoThemeNightPreset"] = autoThemeNightPreset;
    j["autoThemeDayStartHour"] = std::clamp(autoThemeDayStartHour, 0, 23);
    j["autoThemeNightStartHour"] = std::clamp(autoThemeNightStartHour, 0, 23);
    j["autoThemeGeoResolved"] = autoThemeGeoResolved;
    j["autoThemeGeoLat"] = autoThemeGeoLat;
    j["autoThemeGeoLon"] = autoThemeGeoLon;
    j["autoThemeGeoCity"] = autoThemeGeoCity;
    j["folderStyle"] = std::clamp(folderStyle, 0, switchu::folders::kFolderStyleCount - 1);
    j["folderShowCover"] = folderShowCover;

    // Written beside the real file and swapped in, never over it. Truncating
    // the live config and then dying mid-write is how a crash used to reset
    // every setting to its default: load() found a half-written file, failed to
    // parse it, and started from scratch. The staging file absorbs that risk.
    const std::string staging = std::string(kConfigPath) + ".part";

    {
        std::ofstream f(staging, std::ios::trunc);
        if (!f.is_open()) return false;
        f << j.dump(2);
        f.flush();
        if (!f.good()) {
            f.close();
            std::remove(staging.c_str());
            return false;
        }
    }

    // The outgoing copy becomes the backup rather than being deleted, so the
    // window in which neither file is complete costs nothing.
    std::remove(kBackupPath);
    std::rename(kConfigPath, kBackupPath);   // absent on the very first save
    if (std::rename(staging.c_str(), kConfigPath) != 0) {
        std::remove(staging.c_str());
        std::rename(kBackupPath, kConfigPath);
        return false;
    }
    // No commit here: save() is submitted to the thread pool, so this ran on a
    // worker while the main thread was also writing. Committing an fs session
    // from two threads at once is its own hazard, and the author's build -
    // which does not corrupt - commits nowhere.
    return true;
}
