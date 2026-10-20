#pragma once
#include "core/AppLayoutMode.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

struct AppConfig {
    bool  musicEnabled = true;
    float musicVolume  = 0.4f;
    float sfxVolume    = 0.7f;
    int   musicTrackIndex = 0;
    float musicPositionSeconds = 0.f;
    bool  musicShuffle = false;
    int   musicRepeatMode = 1; // MusicRepeatMode::All
    std::vector<std::string> musicPlaylistOrder;
    int   gridColumns  = 5;
    int   gridRows     = 3;
    AppLayoutMode appLayoutMode = AppLayoutMode::DynamicLine;
    std::string actionHintStyle = "capsules";
    // 0 = animated translation, 1 = immediate jump.
    int cursorMotionMode = 0;
    // 0 = pages slide sideways, 1 = pages swap instantly.
    int pageTransitionMode = 0;
    std::string uiLanguageOverride = "auto";
    std::string soundPreset = "wiiu";
    bool  defaultProfileEnabled = false;
    std::string defaultProfileUid;
    bool  tutorialCompleted = false;
    bool  clockUse12Hour = true;
    bool  accessibilityEnabled = true;
    bool  accessibilitySpeakHints = true;
    bool  accessibilitySpeakContextEveryFocus = false;
    bool  accessibilitySpeakPosition = true;
    int   accessibilitySpeechRate = 190;
    bool  steamGridDbEnabled = true;
    // Sub-toggles used when steamGridDbEnabled is on. Defaults keep prior
    // behaviour (artwork everywhere the focused title allows).
    bool  steamGridDbShowInGrid = true;
    bool  steamGridDbShowInDynamicLine = true;
    bool  steamGridDbShowInFolders = true;
    std::string steamGridDbApiKey;

    // 0 keeps the hand-made layout. The other modes are display-only
    // projections and never overwrite layout.json.
    // 0 = my order, 1 = A-Z, 2 = recently played, 3 = Z-A,
    // 4 = newest added, 5 = oldest added.
    int sortMode = 0;
    // When each game was first seen by SwitchU (a running number), for the
    // "newest added" sort. Games present on the first run share 0.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> firstSeen;
    std::uint64_t firstSeenSequence = 0;
    std::uint64_t firstSeenAt(std::uint64_t titleId) const {
        for (const auto& entry : firstSeen)
            if (entry.first == titleId) return entry.second;
        return 0;
    }
    // HOME button while already on the menu opens the sort pop-up.
    bool homeButtonSortMenu = false;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> lastOpened;
    std::uint64_t lastOpenedSequence = 0;

    std::uint64_t lastOpenedAt(std::uint64_t titleId) const {
        for (const auto& entry : lastOpened)
            if (entry.first == titleId) return entry.second;
        return 0;
    }
    void noteOpened(std::uint64_t titleId) {
        for (const auto& entry : lastOpened)
            lastOpenedSequence = std::max(lastOpenedSequence, entry.second);
        if (lastOpenedSequence != std::numeric_limits<std::uint64_t>::max())
            ++lastOpenedSequence;
        for (auto& entry : lastOpened) {
            if (entry.first == titleId) {
                entry.second = lastOpenedSequence;
                return;
            }
        }
        lastOpened.emplace_back(titleId, lastOpenedSequence);
    }

    // --- Home screen look (Settings > Home Screen) -------------------------
    // Defaults are the minimal look. Turning every toggle on restores the
    // original SwitchU home screen.
    bool  homeShowProfileButton = false;      // top-left profile picker
    bool  homeShowHudBubbles = false;         // glass behind clock, battery, game name
    bool  homeShowMiiButton = false;          // Mii Editor shortcut
    bool  homeShowControllersButton = false;  // Controllers shortcut
    bool  homeShowHomebrewButton = false;     // Album / homebrew shortcut
    bool  homeShowSleepButton = false;        // Power menu shortcut
    bool  homeShowThemesButton = false;       // Theme shop shortcut
    // Settings shortcut. Only hidden while - opens the power menu, so
    // Settings can always be reached.
    bool  homeShowSettingsButton = false;   // hold - to open Settings
    bool  homeShowButtonHints = false;        // bottom-right hints on the home screen
    bool  homeShowDate = false;               // date line under the clock
    // 0 = month/day/year (US), 1 = day/month/year.
    int   homeDateFormat = 0;
    // 0 = top bar: clock top-left, Settings top-middle, battery top-right,
    // 1 = clock beside the battery (top right),
    // 2 = bottom bar: clock bottom-left, Settings bottom-middle, battery
    //     bottom-right,
    // 3 = original: clock in the top middle.
    int   homeClockPosition = 2;
    bool  homeShowClock = true;
    bool  homeShowBattery = true;
    // Centred single row: covers on screen, 3 or 5.
    int   homeCoversShown = 5;
    // Card shape in the portrait styles: false = tall 2:3 covers,
    // true = square 1:1 game icons.
    bool  homeSquareCovers = false;
    // Grid icons: 0 = square game icons, 1 = tall covers (1-2 rows only).
    int   homeGridShape = 0;
    // Deleted games stay on the console as "archived" records (the Switch's
    // own HOME shows them with a cloud). Off = leave them out of the row.
    bool  homeShowArchived = false;
    // Looping silent clip of the selected game behind HOME
    // (sdmc:/config/SwitchU/videos/<title id or name>.webp).
    bool  homeVideoBackgrounds = true;
    int   homeVideoDim = 1;   // 0 = none, 1 = light, 2 = medium
    // Outline around the selected game (Themes > Options).
    std::string homeHighlightHex = "#7A1FA8";
    // HOME background: 0 = the theme's own, 1 = solid colour, 2 = picture.
    int   homeBackgroundMode = 0;
    std::string homeBackgroundHex = "#000000";
    std::string homeBackgroundImage;
    // + on a game: 0 = small popup (change icon / hide / delete),
    //              1 = the full game options screen (original).
    int   homePlusAction = 0;
    // - on HOME: 0 = small power popup (power off / restart / settings),
    //            1 = switch between grid and single row (original).
    int   homeMinusAction = 1;
    // Holding HOME on the menu opens Restart / Sleep / Power Off.
    bool  homeLongPressPowerMenu = true;
    // Single-row look: 0 = Wii U carousel, 1 = portrait shelf (focus on the
    // left), 2 = portrait, focused cover centred with half-size neighbours.
    int   homeLineStyle = 2;
    // Bumped when new minimal defaults should be applied once to an existing
    // settings file. load() reads 0 for files written by older builds.
    int   homeDefaultsVersion = 5;
    static constexpr int kHomeDefaultsVersion = 5;

    // Games hidden from HOME (Settings > Home Screen lists them to unhide).
    std::vector<std::uint64_t> hiddenTitles;
    bool isTitleHidden(std::uint64_t titleId) const {
        return std::find(hiddenTitles.begin(), hiddenTitles.end(), titleId) != hiddenTitles.end();
    }
    // Per-game cover choice: a file path, or "icon" for the original square
    // icon. Missing = automatic.
    std::vector<std::pair<std::uint64_t, std::string>> coverChoices;
    std::string coverChoiceFor(std::uint64_t titleId) const {
        for (const auto& entry : coverChoices)
            if (entry.first == titleId) return entry.second;
        return {};
    }
    void setCoverChoice(std::uint64_t titleId, const std::string& choice) {
        for (auto it = coverChoices.begin(); it != coverChoices.end(); ++it) {
            if (it->first == titleId) {
                if (choice.empty()) coverChoices.erase(it);
                else it->second = choice;
                return;
            }
        }
        if (!choice.empty()) coverChoices.emplace_back(titleId, choice);
    }

    std::string themePreset = "Default Light";
    // See switchu::folders::kFolderStyle*. Applies to every folder tile.
    int folderStyle = 0;
    // First-game icon overlay. Ignored by Classic (the mosaic is the cover).
    bool folderShowCover = false;

    // Automatic day/night theme switching.
    std::string autoThemeMode = "off";       // "off" | "manual" | "geo"
    std::string autoThemeDayPreset;          // preset id/name used during the day
    std::string autoThemeNightPreset;        // preset id/name used during the night
    int         autoThemeDayStartHour = 7;   // manual boundary [0..23]
    int         autoThemeNightStartHour = 19;// manual boundary [0..23]
    // Cached IP-geolocated position for the geolocation mode.
    bool        autoThemeGeoResolved = false;
    double      autoThemeGeoLat = 0.0;       // degrees, north positive
    double      autoThemeGeoLon = 0.0;       // degrees, east positive
    std::string autoThemeGeoCity;            // human-readable resolved location

    bool load();

    bool save() const;

    static constexpr const char* kConfigDir  = "sdmc:/config/SwitchU";
    static constexpr const char* kConfigPath = "sdmc:/config/SwitchU/settings.json";
    // The copy that was current before the last save. Read only when the live
    // settings file is missing or does not parse.
    static constexpr const char* kBackupPath = "sdmc:/config/SwitchU/settings.json.bak";
};
