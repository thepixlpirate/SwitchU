#include "TabBuilders.hpp"
#include <nxui/core/I18n.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

// Every home-screen look option lives here so the HOME page itself can stay
// minimal. Turning everything on (and picking Grid / Carousel) gives the
// original SwitchU home screen back.
SettingsScreen::Tab settings::tabs::HomeScreenTab::build(SettingsScreen& screen) {
    using Tab = SettingsScreen::Tab;
    using SettingItem = SettingsScreen::SettingItem;
    using ItemType = SettingsScreen::ItemType;
    using S = SettingsScreen::HomeOptionsState;
    auto& i18n = nxui::I18n::instance();
    Tab t;
    t.name = i18n.tr("settings.tabs.home_screen", "Home Screen");

    auto notify = [&screen](const std::string& key, int value) {
        if (screen.m_homeOptionCb) screen.m_homeOptionCb(key, value);
    };
    auto addSection = [&](const std::string& label) {
        SettingItem sec;
        sec.label = label;
        sec.type = ItemType::Section;
        t.items.push_back(std::move(sec));
    };
    auto addToggle = [&](const std::string& label, const std::string& desc,
                         const char* key, bool S::*field) {
        SettingItem it;
        it.label = label;
        it.description = desc;
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_homeOptions.*field;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen, notify, key, field](SettingItem& self) {
            screen.m_homeOptions.*field = self.boolVal;
            notify(key, self.boolVal ? 1 : 0);
        };
        t.items.push_back(std::move(it));
    };
    auto addSelector = [&](const std::string& label, const std::string& desc,
                           std::vector<std::string> options, const char* key,
                           int S::*field) {
        SettingItem it;
        it.label = label;
        it.description = desc;
        it.type = ItemType::Selector;
        it.options = std::move(options);
        it.intVal = std::clamp(screen.m_homeOptions.*field, 0, (int)it.options.size() - 1);
        it.onChange = [&screen, notify, key, field](SettingItem& self) {
            screen.m_homeOptions.*field = self.intVal;
            notify(key, self.intVal);
        };
        t.items.push_back(std::move(it));
    };

    // --- Games ------------------------------------------------------------
    addSection(i18n.tr("settings.home.section_layout", "Games"));
    {
        SettingItem it;
        it.label = i18n.tr("settings.home.layout", "Game Layout");
        it.description = i18n.tr("settings.home.layout_desc",
                                 "Single row shows one scrolling row of games. Grid shows pages of icons.");
        it.type = ItemType::Selector;
        it.options = {
            i18n.tr("settings.home.layout_row", "Single Row"),
            i18n.tr("settings.home.layout_grid", "Grid"),
        };
        it.intVal = screen.m_homeOptions.singleRow ? 0 : 1;
        it.onChange = [&screen, notify](SettingItem& self) {
            screen.m_homeOptions.singleRow = self.intVal == 0;
            notify("singleRow", self.intVal == 0 ? 1 : 0);
        };
        t.items.push_back(std::move(it));
    }
    addSelector(i18n.tr("settings.home.row_style", "Single Row Style"),
                i18n.tr("settings.home.row_style_desc",
                        "Centered: 3 tall covers, the selected one in the middle. "
                        "Shelf: covers in a line, selected on the left. Carousel: original Wii U look."),
                {i18n.tr("settings.home.row_style_carousel", "Carousel"),
                 i18n.tr("settings.home.row_style_shelf", "Shelf"),
                 i18n.tr("settings.home.row_style_centered", "Centered")},
                "lineStyle", &S::lineStyle);
    addSelector(i18n.tr("settings.home.covers_shown", "Covers on Screen"),
                i18n.tr("settings.home.covers_shown_desc",
                        "For the Centered style: the selected game plus 1 or 2 on each side."),
                {"3", "5"}, "coversShown", &S::coversShownIndex);
    addSelector(i18n.tr("settings.home.cover_shape", "Cover Shape"),
                i18n.tr("settings.home.cover_shape_desc",
                        "Portrait: tall 2:3 covers from /iconswap and SteamGridDB. "
                        "Square: the games' normal square icons."),
                {i18n.tr("settings.home.shape_portrait", "Portrait"),
                 i18n.tr("settings.home.shape_square", "Square")},
                "coverShape", &S::coverShape);
    {
        SettingItem it;
        it.label = i18n.tr("settings.display.grid_rows", "Home Grid Rows");
        it.description = i18n.tr("settings.home.grid_rows_desc", "Used when Game Layout is Grid.");
        it.type = ItemType::Slider;
        it.sliderSteps = 4;
        const int rows = std::clamp(screen.m_gridRows, 1, 5);
        it.floatVal = (float)(rows - 1) / 4.f;
        it.anim01 = it.floatVal;
        it.infoText = std::to_string(rows);
        it.onChange = [&screen](SettingItem& self) {
            const int rows = std::clamp(
                1 + (int)std::round(std::clamp(self.floatVal, 0.f, 1.f) * 4.f), 1, 5);
            self.floatVal = (float)(rows - 1) / 4.f;
            self.anim01 = self.floatVal;
            self.infoText = std::to_string(rows);
            screen.m_gridRows = rows;
            if (screen.m_gridRowsCb) screen.m_gridRowsCb(rows);
        };
        t.items.push_back(std::move(it));
    }
    {
        SettingItem it;
        it.label = i18n.tr("settings.display.grid_columns", "Home Grid Columns");
        it.description = i18n.tr("settings.home.grid_columns_desc", "Used when Game Layout is Grid.");
        it.type = ItemType::Slider;
        it.sliderSteps = 7;
        const int cols = std::clamp(screen.m_gridColumns, 1, 8);
        it.floatVal = (float)(cols - 1) / 7.f;
        it.anim01 = it.floatVal;
        it.infoText = std::to_string(cols);
        it.onChange = [&screen](SettingItem& self) {
            const int cols = std::clamp(
                1 + (int)std::round(std::clamp(self.floatVal, 0.f, 1.f) * 7.f), 1, 8);
            self.floatVal = (float)(cols - 1) / 7.f;
            self.anim01 = self.floatVal;
            self.infoText = std::to_string(cols);
            screen.m_gridColumns = cols;
            if (screen.m_gridColumnsCb) screen.m_gridColumnsCb(cols);
        };
        t.items.push_back(std::move(it));
    }

    // --- Buttons ----------------------------------------------------------
    addSection(i18n.tr("settings.home.section_buttons", "Buttons"));
    addSelector(i18n.tr("settings.home.plus_action", "+ Button on a Game"),
                i18n.tr("settings.home.plus_action_desc",
                        "Quick Menu: a small pop-up to change the icon, hide or delete. "
                        "Full Options: the original game options screen."),
                {i18n.tr("settings.home.plus_quick", "Quick Menu"),
                 i18n.tr("settings.home.plus_full", "Full Options")},
                "plusAction", &S::plusAction);
    addSelector(i18n.tr("settings.home.minus_action", "- Button"),
                i18n.tr("settings.home.minus_action_desc",
                        "Power Menu: Power Off, Restart or Settings. "
                        "Switch View: change between single row and grid (original)."),
                {i18n.tr("settings.home.minus_power", "Power Menu"),
                 i18n.tr("settings.home.minus_view", "Switch View")},
                "minusAction", &S::minusAction);

    addSelector(i18n.tr("settings.home.sort", "Sort Games"),
                i18n.tr("settings.home.sort_desc",
                        "Order of the games. Newest/Oldest Added count from when this version "
                        "of SwitchU first saw each game."),
                {i18n.tr("settings.home.sort_custom", "My Order"),
                 i18n.tr("settings.home.sort_az", "Title A-Z"),
                 i18n.tr("settings.home.sort_recent", "Recently Played"),
                 i18n.tr("settings.home.sort_za", "Title Z-A"),
                 i18n.tr("settings.home.sort_newest", "Newest Added"),
                 i18n.tr("settings.home.sort_oldest", "Oldest Added")},
                "sortMode", &S::sortMode);
    addToggle(i18n.tr("settings.home.home_sort", "HOME Button Opens Sort Menu"),
              i18n.tr("settings.home.home_sort_desc",
                      "Press HOME on the Home screen to change how games are sorted."),
              "homeButtonSortMenu", &S::homeButtonSortMenu);

    // --- Clock ------------------------------------------------------------
    addSection(i18n.tr("settings.home.section_clock", "Clock"));
    {
        SettingItem it;
        it.label = i18n.tr("settings.system.clock_12h", "12-Hour Clock");
        it.description = i18n.tr("settings.home.clock_12h_desc",
                                 "Shows 4:25 PM instead of 16:25. The time zone comes from the console's System Settings.");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_clockUse12Hour;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_clockUse12Hour = self.boolVal;
            if (screen.m_clockUse12HourCb) screen.m_clockUse12HourCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }
    addSelector(i18n.tr("settings.home.clock_position", "Clock and Battery Position"),
                i18n.tr("settings.home.clock_position_desc",
                        "Top/Bottom Bar: clock on the left, Settings in the middle, battery on the right. "
                        "Original: clock in the top middle."),
                {i18n.tr("settings.home.clock_top_bar", "Top Bar"),
                 i18n.tr("settings.home.clock_right", "Top Right Together"),
                 i18n.tr("settings.home.clock_bottom", "Bottom Bar"),
                 i18n.tr("settings.home.clock_original", "Original")},
                "clockPosition", &S::clockPosition);
    addToggle(i18n.tr("settings.home.show_clock", "Show Clock"),
              i18n.tr("settings.home.show_clock_desc", "The time on the Home screen."),
              "showClock", &S::showClock);
    addToggle(i18n.tr("settings.home.show_battery", "Show Battery"),
              i18n.tr("settings.home.show_battery_desc", "Battery level and charging status on the Home screen."),
              "showBattery", &S::showBattery);
    addToggle(i18n.tr("settings.home.show_date", "Show Date"),
              i18n.tr("settings.home.show_date_desc", "A small date line under the time."),
              "showDate", &S::showDate);
    addSelector(i18n.tr("settings.home.date_format", "Date Format"),
                i18n.tr("settings.home.date_format_desc", "How the date under the clock is written."),
                {i18n.tr("settings.home.date_mdy", "Month/Day/Year"),
                 i18n.tr("settings.home.date_dmy", "Day/Month/Year")},
                "dateFormat", &S::dateFormat);

    // --- What is on the HOME screen ---------------------------------------
    addSection(i18n.tr("settings.home.section_show", "Show on Home Screen"));
    addToggle(i18n.tr("settings.home.profile_button", "Profile Button"),
              i18n.tr("settings.home.profile_button_desc",
                      "The user picker. With only one user on the console, games start with it automatically."),
              "showProfileButton", &S::showProfileButton);
    addToggle(i18n.tr("settings.home.hud_bubbles", "Bubbles Behind Text"),
              i18n.tr("settings.home.hud_bubbles_desc",
                      "See-through bubbles behind the clock, battery and game name."),
              "showHudBubbles", &S::showHudBubbles);
    addToggle(i18n.tr("settings.home.settings_button", "Settings Button"),
              i18n.tr("settings.home.settings_button_desc",
                      "The Settings icon on the Home screen. When hidden, open Settings with the - button "
                      "(only while - is set to Power Menu)."),
              "showSettingsButton", &S::showSettingsButton);
    addToggle(i18n.tr("settings.home.homebrew_button", "Homebrew Button"),
              i18n.tr("settings.home.homebrew_button_desc", "Shortcut to the Album / homebrew menu."),
              "showHomebrewButton", &S::showHomebrewButton);
    addToggle(i18n.tr("settings.home.mii_button", "Mii Editor Button"),
              i18n.tr("settings.home.mii_button_desc", "Shortcut to the Mii Editor."),
              "showMiiButton", &S::showMiiButton);
    addToggle(i18n.tr("settings.home.controllers_button", "Controllers Button"),
              i18n.tr("settings.home.controllers_button_desc",
                      "Shortcut for pairing controllers. Also in Settings > Controllers."),
              "showControllersButton", &S::showControllersButton);
    addToggle(i18n.tr("settings.home.sleep_button", "Power Button"),
              i18n.tr("settings.home.sleep_button_desc",
                      "Sleep / shut down / restart shortcut. Holding the console's power button does the same."),
              "showSleepButton", &S::showSleepButton);
    addToggle(i18n.tr("settings.home.themes_button", "Themes Button"),
              i18n.tr("settings.home.themes_button_desc", "Shortcut to themes. Also under Themes below."),
              "showThemesButton", &S::showThemesButton);
    addToggle(i18n.tr("settings.home.button_hints", "Button Hints"),
              i18n.tr("settings.home.button_hints_desc",
                      "The control hints in the bottom corner. The buttons work either way."),
              "showButtonHints", &S::showButtonHints);

    // --- Themes -------------------------------------------------------------
    addSection(i18n.tr("settings.home.section_themes", "Themes"));
    {
        SettingItem it;
        it.label = i18n.tr("settings.home.themes", "Themes and Theme Options");
        it.buttonLabel = i18n.tr("button.open", "Open");
        it.description = i18n.tr("settings.home.themes_desc",
                                 "Installed themes, theme downloads, music, sounds and automatic day/night themes.");
        it.type = ItemType::Action;
        it.onChange = [notify](SettingItem&) { notify("openThemes", 1); };
        t.items.push_back(std::move(it));
    }

    // --- Hidden games --------------------------------------------------------
    addSection(i18n.tr("settings.home.section_hidden", "Hidden Games"));
    if (screen.m_homeOptions.hiddenGames.empty()) {
        SettingItem it;
        it.label = i18n.tr("settings.home.no_hidden", "No hidden games");
        it.type = ItemType::Info;
        it.description = i18n.tr("settings.home.no_hidden_desc",
                                 "Press + on a game and choose Hide from Home to hide it.");
        t.items.push_back(std::move(it));
    } else {
        for (const auto& game : screen.m_homeOptions.hiddenGames) {
            SettingItem it;
            it.label = game.second;
            it.buttonLabel = i18n.tr("settings.home.unhide", "Show");
            it.description = i18n.tr("settings.home.unhide_desc", "Put this game back on the Home screen.");
            it.type = ItemType::Action;
            char key[32];
            std::snprintf(key, sizeof(key), "unhide:%016llX",
                          static_cast<unsigned long long>(game.first));
            const std::string keyStr = key;
            it.onChange = [notify, keyStr](SettingItem&) { notify(keyStr, 1); };
            t.items.push_back(std::move(it));
        }
    }

    // --- Covers ---------------------------------------------------------------
    addSection(i18n.tr("settings.home.section_covers", "Portrait Covers"));
    {
        SettingItem it;
        it.label = i18n.tr("settings.home.covers_info", "Where Covers Come From");
        it.type = ItemType::Info;
        it.description = i18n.tr("settings.home.covers_info_text",
            "The /iconswap folder on the SD card (named by title ID or game name), "
            "then /config/SwitchU/portraits, then the SteamGridDB scan. "
            "Press + on a game to pick a different cover.");
        t.items.push_back(std::move(it));
    }

    return t;
}
