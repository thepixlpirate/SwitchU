#pragma once
#include <nxui/Activity.hpp>
#include <nxui/Application.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/core/Texture.hpp>
#include <nxui/core/I18n.hpp>
#include <nxui/Theme.hpp>
#include "widgets/IconGrid.hpp"
#include "core/GridModel.hpp"
#include "widgets/SelectionCursor.hpp"
#include "widgets/WaraWaraBackground.hpp"
#include "widgets/DateTimeWidget.hpp"
#include "widgets/BatteryWidget.hpp"
#include "widgets/TitlePillWidget.hpp"
#include "core/AudioManager.hpp"
#include "core/AccessibilityManager.hpp"
#include "widgets/LaunchAnimation.hpp"
#include "widgets/OverlayDialog.hpp"
#include "widgets/QuickSettingsOverlay.hpp"
#include "widgets/ContextMenu.hpp"
#include "widgets/CoverBrowser.hpp"
#include "widgets/ColorPicker.hpp"
#include "widgets/HomeBackdrop.hpp"
#include "widgets/VideoBackdrop.hpp"
#include "widgets/ProgressDialog.hpp"
#include "widgets/AppletButton.hpp"
#include "widgets/PageIndicator.hpp"
#include "widgets/ProfileCarouselScreen.hpp"
#include "widgets/ProfileLockButton.hpp"
#include "widgets/FolderBackdrop.hpp"
#include "widgets/SteamGridDbBackdrop.hpp"
#include "widgets/FolderZoom.hpp"
#include "steamgriddb/SteamGridDbManager.hpp"
#include "steamgriddb/ArtworkCache.hpp"
#include "settings/SettingsScreen.hpp"
#include "settings/GameOptionsScreen.hpp"
#include "settings/SteamGridDbPickerScreen.hpp"
#include "settings/FolderOptionsScreen.hpp"
#include "settings/AutoThemeScreen.hpp"
#include "settings/ControllerTestScreen.hpp"
#include "settings/TextEntryScreen.hpp"
#include "themeshop/ThemeShopScreen.hpp"
#include "core/Config.hpp"
#include "core/FolderStore.hpp"
#include "core/WidgetStore.hpp"
#include "core/ThemePreset.hpp"
#include "core/LeaveFrameCache.hpp"
#include "sidebar/SidebarManager.hpp"
#include "launcher/AppletLauncher.hpp"
#include "launcher/AppListLoader.hpp"
#include "launcher/IconStreamer.hpp"
#include "core/SystemMessages.hpp"
#include "navigation/MenuNavigator.hpp"
#include "services/ClockService.hpp"
#include "services/AutoThemeService.hpp"
#ifdef SWITCHU_DEBUG_UI
#include "debug/DebugImGuiOverlay.hpp"
#endif
#include <nxui/widgets/Background.hpp>
#include <nxui/widgets/Box.hpp>
#include <nxui/widgets/GlassPanel.hpp>
#include <nxui/widgets/Label.hpp>
#include <cstdint>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <future>
#include <utility>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <switch.h>
#ifdef SWITCHU_MENU
#include <switchu/smi_protocol.hpp>
#endif


#ifdef SWITCHU_HOMEBREW
static constexpr const char* SD_ASSETS = "romfs:";
#else
static constexpr const char* SD_ASSETS = "sdmc:/switch/SwitchU";
#endif

class WiiUMenuApp : public nxui::Activity {
public:
    WiiUMenuApp();
    ~WiiUMenuApp();

    void setTutorialStartupFade(bool enabled);

#ifdef SWITCHU_MENU
    void setStartupStatus(const switchu::smi::SystemStatus& status, bool fastReturn = false);
#endif

    bool onCreate() override;
    void onDestroy() override;
    void onUpdate(float dt) override;
    void onRender(nxui::Renderer& ren) override;
    bool presentInitialFrame(nxui::Renderer& ren) override;
    void onAfterPresent(nxui::Renderer& ren) override;

    nxui::Widget* focusRoot() override;

private:
    struct GridLayoutMetrics {
        float cellW = 150.f;
        float cellH = 150.f;
        float padX = 20.f;
        float padY = 16.f;
    };

    void loadResources();
    void loadStaticTextures();
    GridLayoutMetrics computeGridLayoutMetrics() const;
    GridLayoutMetrics computeGridLayoutMetrics(int columns, int rows) const;
    std::pair<int, int> folderGridDimensions(std::uint32_t folderId) const;
    void reflowHomeGrid();
    void buildGrid();
    void loadProfiles(bool loadImmediately = true);
    void loadNextProfile();
    void finishProfileLoading();
    void refreshProfileLockButton();
    void wireProfileLockNavigation();
    void createProfileCarousel();
    void openProfileCarousel();
    void setLockedProfile(std::optional<AccountUid> uid);
    void composeRootPending(std::vector<PendingApp>& apps);
    GridModel buildRootFolderModel();
    GridModel buildOpenFolderModel(std::uint32_t folderId) const;
    void applyDisplayModel(GridModel model, std::uint64_t focusId, bool animate,
                           const IconAppearOptions& appear = {},
                           bool instantFocus = false);
    nxui::Rect folderTileRect(std::uint32_t folderId) const;
    void snapCursorToFocus();
    void syncEditJiggle();
    void syncPageIndicator();
    void flipPageFromEdge(int dir);
    void requestOpenFolder(std::uint32_t folderId, std::uint64_t focusTitleId = 0);
    void openCapturedFolder();
    // `animated` plays the mirrored close (icons fly back into the tile, then
    // the root grid is rebuilt); otherwise the root grid is rebuilt right away.
    void closeFolder(bool preserveEditMode = false, bool animated = false);
    void finishCloseFolder(std::uint32_t oldId, bool preserveEditMode);
    nxui::Rect folderPanelRect() const;
    // Re-anchor open-folder glass + title after leave-splash / layout settles.
    void refreshOpenFolderChrome();
    void placeFolderHeader(const nxui::Rect& panel);
    void syncSteamGridDbLogoArea();
    void syncFolderHeader();
    void createFolder(int targetSlot = -1);
    void showAddContextMenu(int targetSlot, const nxui::Rect& anchor);
    void showWidgetTypeMenu(int targetSlot, const nxui::Rect& anchor);
    void showWidgetSizeMenu(int targetSlot, const nxui::Rect& anchor,
                            switchu::widgets::WidgetType type);
    void showWidgetAssetMenu(int targetSlot, const nxui::Rect& anchor,
                             switchu::widgets::WidgetType type,
                             switchu::widgets::WidgetSize size);
    void showWidgetOptionsMenu(std::uint32_t widgetId, int slot,
                               const nxui::Rect& anchor);
    void createWidget(int targetSlot, switchu::widgets::WidgetType type,
                      switchu::widgets::WidgetSize size,
                      const std::string& assetRef = {});
    bool saveWidgetsOrReport(const char* operation);
    bool canPlaceWidget(int targetSlot, switchu::widgets::WidgetSize size,
                        std::uint32_t ignoringWidgetId = 0) const;
    void normalizeWidgetPlacements();
    std::vector<std::pair<std::string, std::string>> listWidgetAssets(bool screenshotsOnly) const;
    std::string resolveWidgetAssetRef(const std::string& assetRef) const;
    void syncWidgetPageAssets();
    std::string randomScreenshotPath(std::uint32_t widgetId) const;
    std::string widgetTypeLabel(switchu::widgets::WidgetType type) const;
    std::string widgetDurationLabel(std::uint64_t seconds) const;
    void refreshRecentActivityDuration();
    void ensureRecentWidgetAssets(std::uint64_t titleId);
    void pollRecentWidgetAssets();
    void syncRecentWidgetTextures();
    void ensureGameArtwork(std::uint64_t titleId);
    void startNextGameArtworkDecode();
    void pollGameArtworkAssets();
    void syncGameArtworkTextures(std::uint64_t titleId);
    switchu::widgets::WidgetSize gameGridSize(std::uint64_t titleId,
                                               AppLayoutMode mode) const;
    bool canPlaceGridItem(int targetSlot, switchu::widgets::WidgetSize size,
                          std::uint64_t ignoringTitleId = 0,
                          std::uint64_t alsoIgnoringTitleId = 0) const;
#ifdef SWITCHU_MENU
    void activateApplication(GlossyIcon* source, AppEntry* entry,
                             std::uint64_t titleId,
                             const std::string& launchTitle);
    void resumeSuspendedApplication(std::uint64_t titleId,
                                    const std::string& launchTitle);
#endif
    void scheduleLeaveCapture(std::function<void()> afterCapture,
                              std::uint64_t previewSuspendedTitleId = 0);
    bool hasActiveLeaveCaptureOverlay() const;
    void pollDeferredLeaveCapture();
    /// Visual-only suspended outline for leave-frame capture (no focus move).
    void setSuspendedIconVisuals(std::uint64_t titleId);
    /// Align green pulse + blue cursor + leave-session focus to the launching
    /// title before the splash frame is captured (does not open folders).
    void previewLeaveCaptureTitle(std::uint64_t titleId);
    LeaveFrameSession captureLeaveSession() const;
    bool saveLeaveFrame(nxui::Renderer& ren);
    void restoreLeaveSession();
    void setLeaveMotionFrozen(bool frozen);
    void updateLeaveSplashHandoff(float dt);
    bool leaveSplashActive() const;
    void renameFolder(std::uint32_t folderId);
    void showFolderContextMenu(std::uint32_t folderId);
    bool saveFoldersOrReport(const char* operation);
    void createTextEntry();
    void requestTextEntry(const std::string& title, const std::string& guide,
                          const std::string& initial, int maxLength, bool password,
                          std::function<void(const std::string&)> onAccept);
    void editSteamGridDbApiKey();
    void startSteamGridDbScrape();
    void cancelSteamGridDbScrape();
    void openSteamGridDbPicker(GameOptionsScreen::ArtworkKind kind,
                               const std::string& query = std::string());
    void openSteamGridDbPickerForTitle(std::uint64_t titleId,
                                       const std::string& title,
                                       GameOptionsScreen::ArtworkKind kind,
                                       const std::string& query = std::string());
    void clearSteamGridDbArtwork(std::uint64_t titleId,
                                 GameOptionsScreen::ArtworkKind kind);
    void editSteamGridDbPickerQuery();
    void applySteamGridDbCandidate(const SteamGridDbManager::BrowseResult& browse,
                                   const SteamGridDbManager::Candidate& candidate);
    void syncSteamGridDb();
    void showFocusedSteamGridDbArtwork(bool forceReload = false);
    bool steamGridDbArtworkAllowedHere() const;
    void applyTheme();
    void applyThemeResources(const ThemePreset& preset);
    void retryPendingBackgroundImage();
    void applyUiLanguage();
    void rebuildThemeFromColors();
    ThemePreset buildEffectiveThemePreset();
    std::string resolveThemeAssetPath(const ThemePreset& preset, const std::string& rawPath) const;
    ThemePreset* findPresetPtr(const std::string& name);
    void deletePreset(const std::string& presetId);
    void startSoftwareDeletion(uint64_t titleId, const std::string& title,
                               bool closeGameOptionsOnSuccess);
    void syncSoftwareDeletion();
    void updateCursor();
    struct ActionHint {
        std::string icon;
        std::string label;
    };
    std::vector<ActionHint> buildActionHints();

    struct HintCapsule {
        std::string icon;
        std::string label;
        std::string outgoing;              // label fading out during a swap
        float width = 0.f;
        float widthFrom = 0.f, widthTo = 0.f;
        float widthT = 1.f;                // 0..1
        float swapT  = 1.f;                // 0..1
    };
    std::vector<HintCapsule> m_hintCapsules;
    void  syncHintCapsules(float dt);
    float hintCapsuleWidth(const std::string& icon, const std::string& label);
    void renderActionHintBar(nxui::Renderer& ren);
    void renderActionHintPanel(nxui::Renderer& ren);
    void renderPageArrows(nxui::Renderer& ren);
    bool pagingAvailable();

    struct PageArrowAnim {
        float show  = 0.f;   // 0..1
        float press = 0.f;   // 1 -> 0
    };
    PageArrowAnim m_arrowAnimLeft, m_arrowAnimRight;
    bool m_touchArrowLeft = false, m_touchArrowRight = false;
    bool m_touchBattery = false;

    nxui::Rect pageArrowRect(bool left);
    void kickPageArrow(int dir);
    bool flipPage(int dir);
    bool addPageAvailable();
    bool currentPageEmpty() const;
    bool deletePageAvailable() const;
    void createPage();
    void showDeletePageDialog();
    void deleteCurrentPage();
    void deleteAllUnusedPages();
    float m_addPageHold = 0.f;
    bool  m_addPageMode = false;
    bool  m_addPageTouchHold = false;
    int findTitleIndex(uint64_t titleId) const;
    bool focusTitle(uint64_t titleId, bool instantFocus = false);
    void markSuspendedIcon(uint64_t titleId);
    void closeActiveOverlays(bool closeFolders = true);
    void handleTouch();
    std::shared_ptr<GlossyIcon> makeIcon(const AppEntry& entry);
    nxui::Texture* folderCoverTexture(std::uint64_t titleId);
    void applyFolderCoversToIcons();
    void wireFocusCallback();
    void wireGlobalActions();
    void toggleAccessibilitySpeech();
    bool handleAccessibilityToggleCombo();
    bool isCurrentFocusableWidget(nxui::Widget* w) const;
    std::string accessibilityPositionFor(nxui::Widget* w) const;
    void createSettings();
    void createQuickSettings();
    void openQuickSettings();
    void persistMusicPlaybackState(bool updateEnabledFlag = false);
    void resumeMenuMusicAfterReturn();
    // Console sleep: the daemon announces the sleep before the system goes
    // under, because an applet left running keeps presenting frames, playing
    // music and doing its frame work for the whole sleep.
    void enterSystemSleep();
    void leaveSystemSleep();
    void pollSleepNotifications();
    void onUpdateAwake(float dt);
    void closeQuickSettings();
    void createThemeShop();
    void createGameOptions();
    void createFolderOptions();
    void createAutoThemeScreen();
    void openAutoThemeSettings();
    void refreshAutoThemeSummary();
    void pushGeoDisplayToScreen();
    void createControllerTest();
    void reloadThemePresets();
    void refreshThemeShopState();
    std::vector<ThemeShopScreen::ThemeShopEntry> buildThemeShopEntries();
    void startThemePackageTransfer(const ThemeCatalogClient::Entry& entry, bool installMode);
    void syncThemePackageTransfer();
    void activateThemePreset(ThemePreset* preset, bool applyBundledSound);
    void applyAutoThemeConfig();
    void evaluateAutoTheme(bool force);
    void maybeFetchGeoLocation();
    void pollGeoLocationFetch();
    std::string resolveSoundPresetId(const std::string& preset) const;
    void loadSoundPreset(const std::string& preset);
    void changeSoundPreset(const std::string& preset);
    std::vector<std::string> scanAvailablePresets();
    void loadMenuLayout();
    void saveMenuLayout();
    bool quiesceWritersForPowerAction();
    void applyMenuLayoutToPending(std::vector<PendingApp>& apps);
    void startEditGhost(GlossyIcon* sourceIcon);
    nxui::Texture* adoptEditGhostTexture(GlossyIcon* sourceIcon);
    void detachEditSourceIcon();
    void reattachEditSourceIcon();
    void stopEditGhost();
    void updateEditGhost(float dt);
    // Re-anchor move-mode placement after the grid model changes (folder
    // open/close, rebuild). preferEmptySlot is used when dropping into a
    // folder so the cursor starts on a free cell instead of a stale root index.
    void syncEditPlacementAfterModelChange(bool preferEmptySlot);
    bool commitEditModePlacement();
    bool activateEditModeTarget();
    bool moveFocusedIcon(nxui::FocusDirection dir);
    void enterEditMode();
    void exitEditMode();
    void bindEditActions(GlossyIcon* icon);
    void unbindEditActions();
    bool isEditableIcon(nxui::Widget* w) const;
    void announceFocusedWidget(nxui::Widget* w);
    std::string accessibilityContextFor(nxui::Widget* w) const;
    std::string accessibilityActionsFor(nxui::Widget* w) const;

    // Settings > Home Screen: show/hide the optional home chrome and apply the
    // single-row style. Safe to call any time after the HOME widgets exist.
    void applyHomeChromeOptions();
    void applyHomeOption(const std::string& key, int value);
    void syncPortraitTitlePlacement();
    // Portrait (2:3) covers for the Switch-style row, decoded off-thread and
    // kept only for the covers near the focused game.
    // Every cover image found for a game, best first: /iconswap (by title ID
    // or name), /config/SwitchU/portraits, then the SteamGridDB download.
    std::vector<std::string> coverCandidates(std::uint64_t titleId, const std::string& title);
    std::string portraitSourcePath(std::uint64_t titleId, const std::string& title);
    void rebuildIconSwapIndex();
    void reloadCover(std::uint64_t titleId);
    void hideTitle(std::uint64_t titleId);
    void unhideTitle(std::uint64_t titleId);
    void dropHiddenTitles(GridModel& model) const;
    bool isArchivedEntry(const AppEntry& entry) const;
    bool isArchivedTitle(std::uint64_t titleId, std::uint32_t viewFlags) const;
    std::vector<std::pair<std::uint64_t, std::string>> hiddenGamesForSettings() const;
    void pushHomeOptionsToSettings();
    void layoutTopHud();
    void applyBottomBarNavigation();
    void openThemeShopScreen();
    // Small pop-ups: + on a game, - on HOME, and a file browser for covers.
    void showQuickGameMenu(GlossyIcon* icon);
    void showCoverBrowser(std::uint64_t titleId, const std::string& title,
                          const std::string& directory);
    // `includeSettings`: the - button version also offers Settings.
    void showPowerMenu(bool includeSettings = true);
    // ZL/ZR in the single row: jump a whole screen of games. False when not
    // in the single row (the grid then flips pages as before).
    bool jumpLine(int direction);
    // Holding ZL/ZR keeps jumping until released.
    void updateTriggerRepeat(float dt);
    float m_triggerHoldTime = 0.f;
    float m_triggerRepeatTimer = 0.f;
    // HOME pressed while already on the menu: sort pop-up.
    void showSortMenu();
    void applySortMode(int mode);
    // Gives newly installed games a "first seen" number for the Newest sort.
    void noteNewTitles();
    // Rebuild deferred while an overlay owned focus (e.g. a game deleted).
    void processPendingHomeRebuild();
    // Themes > Options: highlight colour and HOME background.
    void handleHomeStyleAction(const std::string& action, int value);
    void openColorPicker(const std::string& target);
    void pushHomeStyleToThemeShop();
    void applyHomeBackground();
    void pollHomeBackground();
    nxui::Color highlightColor() const;
    // Silent looping clip of the selected game behind HOME.
    void updateVideoBackground(float dt);
    std::string videoPathFor(std::uint64_t titleId, const std::string& title);
    void rebuildVideoIndex();
    void confirmDeleteSoftware(std::uint64_t titleId, const std::string& title);
    void syncPortraitCovers();
    void pollPortraitCovers();
    void releasePortraitCovers(bool all);

    void toggleAppLayoutMode();
    void setAppLayoutMode(AppLayoutMode mode);
    void configureDynamicLineNavigation();
    void cycleSortMode();
    std::string sortModeLabel() const;
    AppLayoutMode appLayoutMode() const { return m_appLayoutMode; }

#ifdef SWITCHU_MENU
    void refreshAppList();
    void finalizeRefresh();
    void handleSystemAction(SysAction a);
    void showGameContextMenu(GlossyIcon* icon);
#endif

    nxui::Font  m_fontNormal;
    nxui::Font  m_fontSmall;
    nxui::Font  m_fontIcons;

    GridModel    m_model;
    nxui::Theme  m_theme;
    switchu::services::ClockService m_clockService;
    switchu::services::AutoThemeService m_autoTheme;
    float                    m_autoThemeCheckTimer = 0.f;

    struct GeoFetchResult {
        bool ok = false;
        double lat = 0.0;
        double lon = 0.0;
        std::string city;
    };
    std::future<void>        m_geoFetchFuture;
    std::shared_ptr<GeoFetchResult> m_geoFetchResult;
    bool                     m_geoFetchInFlight = false;

    std::string              m_activePresetName = "Default Light";
    ThemeColorSet            m_activeColors;
    nxui::ThemeMode          m_activeMode = nxui::ThemeMode::Light;
    std::vector<ThemePreset> m_allPresets;
    ThemePreset              m_effectivePreset;

    std::shared_ptr<WaraWaraBackground> m_background;
    std::shared_ptr<IconGrid>          m_grid;
    std::shared_ptr<SelectionCursor>   m_cursor;
    std::shared_ptr<SelectionCursor>   m_pointerCursor;
    std::shared_ptr<DateTimeWidget>    m_clock;
    std::shared_ptr<BatteryWidget>     m_battery;
    std::shared_ptr<TitlePillWidget>   m_titlePill;
    std::shared_ptr<PageIndicator>     m_pageIndicator;
    std::shared_ptr<LaunchAnimation>   m_launchAnim;
    std::shared_ptr<OverlayDialog>     m_userSelect;
    std::shared_ptr<OverlayDialog>     m_dialog;
    std::shared_ptr<QuickSettingsOverlay> m_quickSettings;
    std::shared_ptr<ContextMenu>       m_contextMenu;
    std::shared_ptr<CoverBrowser>      m_coverBrowser;
    std::uint64_t                      m_coverBrowserTitleId = 0;
    bool                               m_softwareDeleteFromHome = false;
    std::unordered_set<std::uint64_t>  m_deletedTitles;   // deleted this run
    bool                               m_browserForBackground = false;
    std::shared_ptr<ColorPicker>       m_colorPicker;
    std::string                        m_colorPickerTarget;   // "highlight" | "background"
    std::shared_ptr<HomeBackdrop>      m_homeBackdrop;
    std::shared_ptr<VideoBackdrop>     m_videoBackdrop;
    std::unordered_map<std::string, std::vector<std::string>> m_videoIndex;
    bool                               m_videoIndexed = false;
    float                              m_videoIndexAge = 0.f;
    std::uint64_t                      m_videoFocusTitleId = 0;
    float                              m_videoSettle = 0.f;
    std::unique_ptr<nxui::Texture>     m_homeBackgroundTexture;
    std::string                        m_homeBackgroundLoadedPath;
    struct BackgroundDecode {
        std::string path;
        steamgriddb::artwork::DecodedImage image;
    };
    std::shared_ptr<BackgroundDecode>  m_homeBackgroundDecode;
    std::future<void>                  m_homeBackgroundFuture;
    std::shared_ptr<ProgressDialog>    m_progressDialog;
    std::shared_ptr<SettingsScreen>    m_settings;
    std::shared_ptr<ThemeShopScreen>   m_themeShop;
    std::shared_ptr<GameOptionsScreen> m_gameOptions;
    std::shared_ptr<SteamGridDbPickerScreen> m_steamGridDbPicker;
    std::shared_ptr<FolderOptionsScreen> m_folderOptions;
    std::shared_ptr<AutoThemeScreen>   m_autoThemeScreen;
    std::shared_ptr<ControllerTestScreen> m_controllerTest;
    std::shared_ptr<ProfileCarouselScreen> m_profileCarousel;
    std::shared_ptr<TextEntryScreen>      m_textEntry;

    nxui::Texture m_gameCardTex;
    nxui::Texture m_arrowTexLeft;
    nxui::Texture m_arrowTexRight;
    nxui::Texture m_batteryConsoleTex;
    nxui::Texture m_batteryJoyconLeftTex;
    nxui::Texture m_batteryJoyconRightTex;
    nxui::AnimatedFloat m_arrowCenterY;
    bool m_arrowCenterInit = false;

    std::shared_ptr<nxui::Box> m_bgLayer;
    std::shared_ptr<nxui::Box> m_contentLayer;
    std::shared_ptr<nxui::Box> m_overlayLayer;
    std::shared_ptr<nxui::Box> m_topHud;
    std::shared_ptr<nxui::Box> m_leftSidebar;
    std::shared_ptr<nxui::Box> m_rightSidebar;
    std::shared_ptr<ProfileLockButton> m_profileLock;
    std::shared_ptr<FolderBackdrop> m_folderBackdrop;
    std::shared_ptr<SteamGridDbBackdrop> m_steamGridDbBackdrop;
    std::shared_ptr<FolderZoom>     m_folderZoom;
    nxui::Rect                      m_folderZoomOriginRect{};
    std::shared_ptr<nxui::GlassPanel> m_folderHeader;
    std::shared_ptr<nxui::Label> m_folderHeaderLabel;
    std::vector<HomeProfile> m_profiles;
    bool m_profilesComplete = false;
    nxui::AnimatedFloat m_folderHeaderAnim{0.f};
    nxui::Rect m_folderHeaderRest{410.f, 78.f, 460.f, 58.f};

    AudioManager m_audio;
    AccessibilityManager m_accessibility;
    std::future<void>    m_audioFuture;
    std::future<void>    m_configSaveFuture;
    std::future<void>    m_themeDeleteFuture;
    bool                 m_audioStarted = false;
    bool  m_audioPlaybackRestorePending = false;
    bool  m_audioSeekAfterHoldoff = false;
    int   m_audioPlaybackRestoreDelayFrames = 0;
    bool                 m_musicFadeActive = false;
    std::vector<std::string> m_availablePresets;
    bool                 m_presetChangePending = false;
    std::string          m_loadedSoundPreset;
    std::string          m_pendingSoundPreset;

    struct ThemePackageTransferShared {
        std::mutex mutex;
        ThemeTransferState state;
        std::string themeId;
        bool installMode = false;
        std::string destinationPath;
        std::uint64_t revision = 0;
    };

    struct SteamGridDbApplyProgressShared {
        std::mutex mutex;
        std::string message;
        float progress01 = 0.f;
        std::uint64_t revision = 0;
    };

    nxui::ThreadPool m_threadPool{2};
    SidebarManager  m_sidebar;
    AppletLauncher  m_launcher;
    AppListLoader   m_appLoader;
    IconStreamer    m_iconStreamer;
    SystemMessages  m_sysMsg;
    switchu::navigation::MenuNavigator m_navigator;

    bool m_showDebugOverlay  = false;
#ifdef SWITCHU_DEBUG_UI
    std::unique_ptr<DebugImGuiOverlay> m_debugOverlay;
#endif
    bool m_showWireframe     = false;
    // Console sleep: set by the daemon's sleep notification, cleared on wake.
    bool m_systemAsleep      = false;
    bool m_musicWasPlayingBeforeSleep = false;
    bool m_editMode          = false;
    int  m_editSourceIndex   = -1;
    int  m_editTargetIndex   = -1;
    int  m_editOriginRootSlot = -1;
    int  m_editOriginFolderIndex = -1;
    std::uint32_t m_editOriginFolderId = 0;
    std::uint64_t m_editHeldTitleId = 0;
    std::string m_editHeldTitle;
    GlossyIcon* m_editBoundIcon = nullptr;
    GlossyIcon* m_editSourceIcon = nullptr;
    std::shared_ptr<GlossyIcon> m_editGhostIcon;
    std::unique_ptr<nxui::Texture> m_editGhostTexture;
    nxui::Rect m_editGhostTargetRect {0.f, 0.f, 0.f, 0.f};
    nxui::AnimatedRect m_editGhostRect;
    bool m_editGhostRectInit = false;
    float m_editGhostPulse = 0.f;
    // Touch drag: the ghost follows the finger instead of snapping to slots.
    bool m_editGhostTouchFollow = false;
    nxui::Vec2 m_editGhostTouchPos {0.f, 0.f};
    // Dragging past the grid edge flips pages; holding keeps flipping.
    float m_editDragEdgeHold = 0.f;
    int   m_editDragEdgeDir = 0;
    // Dragging a game over a folder tile opens it after a short hover.
    float m_editDragFolderHold = 0.f;
    // Countdown while the hover-opened folder swaps in.
    float m_editDragFolderOpenWait = 0.f;
    std::vector<uint64_t> m_layoutSlots;
    std::unordered_map<std::uint64_t, switchu::widgets::WidgetSize> m_gameSizes;
    bool m_layoutDirty = false;
    std::unordered_map<std::uint64_t, std::unique_ptr<nxui::Texture>> m_folderCoverCache;
    switchu::folders::FolderStore m_folderStore;
    switchu::widgets::WidgetStore m_widgetStore;
    std::uint64_t m_recentWidgetAssetTitleId = 0;
    std::uint64_t m_recentWidgetLoadedTitleId = 0;
    std::unique_ptr<nxui::Texture> m_recentWidgetHero;
    std::unique_ptr<nxui::Texture> m_recentWidgetLogo;
    std::unique_ptr<nxui::Texture> m_recentWidgetIcon;
    struct RecentWidgetAssetDecodeState {
        std::uint64_t titleId = 0;
        steamgriddb::artwork::DecodedImage hero;
        steamgriddb::artwork::DecodedImage logo;
        IconStreamer::DecodedIcon icon;
        std::atomic<bool> cancelled{false};
        std::int64_t elapsedMs = 0;
    };
    std::shared_ptr<RecentWidgetAssetDecodeState> m_recentWidgetAssetDecode;
    std::shared_ptr<RecentWidgetAssetDecodeState> m_recentWidgetAssetReady;
    std::future<void> m_recentWidgetAssetFuture;
    int m_recentWidgetAssetUploadStage = 0;
    struct GameArtworkTextures {
        std::unique_ptr<nxui::Texture> hero;
        std::unique_ptr<nxui::Texture> logo;
    };
    std::unordered_map<std::uint64_t, GameArtworkTextures> m_gameArtwork;
    struct GameArtworkDecodeState {
        std::uint64_t titleId = 0;
        steamgriddb::artwork::DecodedImage hero;
        steamgriddb::artwork::DecodedImage logo;
        std::atomic<bool> cancelled{false};
        std::int64_t elapsedMs = 0;
    };
    struct PortraitDecodeState {
        std::uint64_t titleId = 0;
        steamgriddb::artwork::DecodedImage image;
        std::atomic<bool> cancelled{false};
    };
    std::unordered_map<std::uint64_t, std::unique_ptr<nxui::Texture>> m_portraitCovers;
    // /iconswap contents keyed by normalised name or title ID.
    std::unordered_map<std::string, std::vector<std::string>> m_iconSwapIndex;
    bool m_iconSwapIndexed = false;
    bool m_homeRebuildPending = false;   // e.g. a game was unhidden in Settings
    bool m_startupFocusSettled = false;  // first rebuild after start picks the first game
    std::vector<std::string> m_gameOptionsCoverPaths;   // matches the Cover selector
    std::function<void()> m_openThemeShopAction;
    std::function<void()> m_openSettingsAction;
    // Titles already looked up with no cover file, so they are not retried
    // every frame. Cleared when the covers are reloaded.
    std::unordered_map<std::uint64_t, bool> m_portraitMissing;
    std::shared_ptr<PortraitDecodeState> m_portraitDecode;
    std::future<void> m_portraitFuture;
    float m_portraitRescanTimer = 0.f;
    std::vector<std::uint64_t> m_gameArtworkDecodeQueue;
    std::shared_ptr<GameArtworkDecodeState> m_gameArtworkDecode;
    std::shared_ptr<GameArtworkDecodeState> m_gameArtworkReady;
    std::future<void> m_gameArtworkFuture;
    GameArtworkTextures m_gameArtworkUploadTextures;
    int m_gameArtworkUploadStage = 0;
    struct RetainedImagePin {
        std::string assetRef;
        std::string assetPath;
        std::shared_ptr<GlossyIcon> icon;
    };
    std::unordered_map<std::uint64_t, RetainedImagePin> m_retainedImagePins;
    int m_widgetAssetPage = -1;
    bool m_widgetAssetsWereSliding = false;
    std::vector<std::uint8_t> m_widgetAssetCurrentScratch;
    std::vector<std::uint8_t> m_widgetAssetKeepScratch;
    int m_consoleBatteryPercent = 0;
    bool m_consoleBatteryCharging = false;
    std::vector<AppEntry> m_allApps;
    std::uint32_t m_openFolderId = 0;
    std::uint32_t m_requestedFolderId = 0;
    std::uint64_t m_folderOpenFocusTitleId = 0;
    /// Leave-splash folder restore deferred until HUD/layers exist (frosted capture).
    std::uint32_t m_leaveRestoreFolderId = 0;
    std::uint64_t m_leaveRestoreFolderFocus = 0;
    bool m_folderCaptureRequested = false;
    bool m_folderCaptureReady = false;
    bool m_folderClosing = false;
    bool m_folderOriginStale = false;
    bool m_gridSliding = false;

    int  m_touchHitIndex     = -1;
    bool m_touchOnFocused    = false;
    bool m_touchEditDragActive = false;
    bool m_touchProfileLock = false;
    bool m_touchProfileLockWasFocused = false;
    int  m_deferredRefreshFrames = 0;
    bool m_refreshQueued         = false;
    int  m_refreshCooldownFrames = 0;
    bool m_asyncRefreshPending   = false;
    int  m_refreshPrevPage       = 0;
    std::uint64_t m_refreshPrevFocusTitleId = 0;

    AppConfig m_config;
    SteamGridDbManager m_steamGridDb;
    std::future<SteamGridDbManager::BrowseResult> m_steamGridDbBrowseFuture;
    std::future<SteamGridDbManager::ApplyResult> m_steamGridDbApplyFuture;
    std::shared_ptr<SteamGridDbApplyProgressShared> m_steamGridDbApplyProgress;
    std::uint64_t m_steamGridDbApplyProgressUiRevision = 0;
    std::uint64_t m_steamGridDbUiRevision = 0;
    std::uint64_t m_steamGridDbLastCompletedTitleId = 0;
    bool m_steamGridDbWasRunning = false;
    AppLayoutMode m_appLayoutMode = AppLayoutMode::Grid;
    bool m_settingsNeedRefresh        = false;
    std::string m_loadedRegularFontPath;
    std::string m_loadedSmallFontPath;
    std::string m_loadedGameCardPath;
    std::string m_loadedBackgroundImagePath;
    bool m_backgroundImageLoaded      = false;
    std::string m_pendingBackgroundImagePath;
    int m_backgroundImageRetryFrames = 0;
    int m_backgroundImageRetryAttempts = 0;
    bool m_forceThemeResourceReload   = false;
    std::uint64_t m_gameOptionsTitleId = 0;
    std::uint32_t m_folderOptionsId = 0;
    nxui::Widget* m_contextMenuReturnFocus = nullptr;
    nxui::Widget* m_dialogReturnFocus = nullptr;
    bool m_dialogWasActive            = false;
    bool m_suppressNextNavigateSfx    = false;
    bool m_pendingNetConnect          = false;
    int  m_deferredInitialAssetFrames = 0;
    bool m_deferredStaticTextures = false;
    int  m_deferredProfileFrames = 0;
    struct DeferredProfileList {
        std::vector<AccountUid> uids;
        Result result = 0;
    };
    std::shared_ptr<DeferredProfileList> m_deferredProfileList;
    std::future<void> m_deferredProfileListFuture;
    std::vector<AccountUid> m_pendingProfileUids;
    std::size_t m_pendingProfileIndex = 0;
    std::future<void> m_accessibilityFuture;
    bool m_accessibilityReady = false;
    std::uint64_t m_fastReturnStartupTick = 0;

    bool m_leaveCapturePending = false;
    std::function<void()> m_leaveCaptureAfter;
    bool m_leaveCaptureDeferred = false;
    std::function<void()> m_leaveCaptureDeferredAfter;
    std::uint64_t m_leaveCaptureDeferredSuspendedTitleId = 0;
    std::uint64_t m_leaveCaptureFocusTitleId = 0;
    LeaveFrameSession m_leaveSession;
    nxui::Texture m_leaveSplashTex;
    enum class LeaveSplashPhase { None, Hold, Fade };
    LeaveSplashPhase m_leaveSplashPhase = LeaveSplashPhase::None;
    float m_leaveSplashHoldRemaining = 0.f;
    float m_leaveSplashFade = 0.f;
    bool m_leaveSplashDrawn = false;
    bool m_leaveMotionFrozen = false;
    bool m_leaveMotionFreezePending = false;
    static constexpr float kLeaveSplashHoldDur = 0.06f;
    static constexpr float kLeaveSplashFadeDur = 0.25f;

    bool  m_audioInitPending = false;
    bool  m_audioHeldLogged  = false;
    bool m_fastReturnRequested = false;
    std::future<void> m_themePackageTransferFuture;
    std::future<void> m_softwareDeleteFuture;
    Result m_softwareDeleteResult = 0;
    std::uint64_t m_softwareDeleteTitleId = 0;
    std::string m_softwareDeleteTitle;
    bool m_softwareDeleteClosesGameOptions = false;
    std::shared_ptr<ThemePackageTransferShared> m_themePackageTransfer;
    std::uint64_t m_themePackageTransferUiRevision = 0;
    std::uint64_t m_themePackageTransferHandledRevision = 0;
#ifdef SWITCHU_MENU
    switchu::smi::OperationOutcome m_startupFailure{};
#endif
    int m_themeRenderDebugFrames = 0;

    float m_returnFadeTimer = 0.f;
    float m_tutorialStartupFadeTimer = 0.f;
    std::uint64_t m_tutorialStartupFadeDeadlineTick = 0;
    bool  m_tutorialStartupFade = false;
    bool m_hintPanelInitialized = false;
    bool m_hintCapsulesInitialized = false;
    bool m_accessibilityToggleComboHeld = false;
    bool m_plusExitPending = false;
    float m_plusExitPendingTimer = 0.f;
    nxui::AnimatedFloat m_hintPanelW{0.f};
    nxui::AnimatedFloat m_hintPanelH{0.f};
    nxui::AnimatedFloat m_hintContentReveal{1.f};
    std::string m_hintSignature;
    static constexpr float kReturnFadeInDur = 0.22f;
    static constexpr float kTutorialStartupFadeDur = 0.34f;
};
