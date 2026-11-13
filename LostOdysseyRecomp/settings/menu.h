#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace settings
{
struct HdrDisplayInfo
{
    bool active = false;
    uint32_t peakNits = 0; // Zero means the platform has no usable peak report.
    bool relative = false; // EDR headroom estimate, not measured panel nits.
};
struct HdrCalibration
{
    bool open = false;
    bool automatic = true;
    bool detectedValid = false;
    bool relative = false;
    bool hdrActive = false;
    bool sceneAvailable = false;
    bool scenePreview = true;
    bool numericEditing = false;
    uint32_t manualNits = 1000;
    uint32_t detectedNits = 0;
    uint32_t effectiveNits = 1000;
    uint32_t paperWhiteNits = 203;
    int focus = 0;
    std::wstring numericText;
    bool operator==(const HdrCalibration &) const = default;
};
// Brightness / gamma page. Presentation compares the game image with these
// values applied, on the frozen scene or a test pattern.
struct BrightnessCalibration
{
    bool open = false;
    bool sceneAvailable = false;
    bool scenePreview = true;
    bool expandRgbRange = false;
    int brightness = 0;
    uint32_t gamma = 100;
    int focus = 0;
    // The retail calibration screen ("Original pattern") can open.
    bool originalPattern = true;
    bool operator==(const BrightnessCalibration &) const = default;
};
// DLSS 5 neural rendering page, opened from its Graphics row. Presentation
// shows the NGX controller's preview of the last DLSS frame with these values
// inside the NrPreview rectangle: DLSS on the left, DLSS + NR on the right.
// Percents are the model's 0..2 strengths, as in settings::Config.
struct NeuralRenderingTuning
{
    bool open = false;
    bool sceneAvailable = false; // a DLSS frame is held for the preview
    // Why the preview cannot appear even in a DLSS scene.
    enum class Blocker : uint8_t { None, MissingRuntime, Unsupported };
    Blocker blocker = Blocker::None;
    uint32_t passes = 0;
    uint32_t preset = 0, style = 0;
    uint32_t intensity = 100, globalTone = 100, localTone = 100, structure = 100;
    int skin = -100;
    bool autoMask = true;
    int focus = 0;
    bool operator==(const NeuralRenderingTuning &) const = default;
};
// The page's preview rectangle in the 1280x720 menu layout; each half is 16:9.
inline constexpr int NrPreviewLeft = 160, NrPreviewTop = 80, NrPreviewRight = 1120, NrPreviewBottom = 350;
// Page layout (1280x720) shared by the renderer and pointer hit-testing. Focus
// 0-4 fill the left column, 5-8 the right one; 9 Default, 10 Done, 11 Cancel.
inline constexpr int NrControlCount = 9, NrFocusCount = 12;
inline constexpr int NrColumnX[2] = {160, 650}, NrColumnWidth = 470;
inline constexpr int NrRowTop = 365, NrRowHeight = 44;
inline constexpr int NrControlOffset = 155; // label width inside a column
inline constexpr int NrSliderWidth = 215;   // percent slider; the value text follows it
inline constexpr int NrButtonTop = 600, NrButtonHeight = 45, NrButtonWidth = 300, NrButtonGap = 30;
// Called by presentation when a DLSS frame for the preview appears or goes.
void SetNeuralRenderingPreviewAvailable(bool available);
// Unsaved values while the settings menu is open, saved values otherwise.
NeuralRenderingTuning GetNeuralRenderingTuning();
void SetHdrDisplayInfo(HdrDisplayInfo info);
// Called by presentation when its owned, frozen game scene (HDR or SDR)
// becomes available.
void SetHdrCalibrationSceneAvailable(bool available);
HdrCalibration GetHdrCalibration();
// Unsaved values while the settings menu is open, saved values otherwise.
BrightnessCalibration GetBrightnessCalibration();
// ASCII digits, Backspace (8), Enter (13), and Escape (27). Returns true when
// the calibration page consumes this host key before game input mapping.
bool CalibrationKey(uint32_t key);
void PointerDrag(float x, float y, bool held);
// Settings menu layout rules. Follow them when adding or moving a row:
// - Tabs: 0 Gameplay (retail game options, Controls page, game actions),
//   1 Audio (voice language, volumes, audio output, rear angle), 2 Graphics (display, rendering,
//   frame rate, HDR), 3 System (interface and game language, updates, Discord status, debug log,
//   Achievements, TAA shader collection, Mods, Import discs & DLC, Shader bundle,
//   Prepare shaders again, Android diagnostics on Android, Save).
// - Gameplay starts with the seven retail guest settings in their retail order;
//   host rows follow them.
// - Host rows are grouped by topic. Controller and keyboard settings (button
//   prompts, vibration, button and key layouts, future ones) go on the
//   Controls page, opened from Gameplay's Controls row; not on Audio.
// - Actions come last on their tab: Controls / Restore / Quit to Main Menu on
//   Gameplay, Brightness / Save on Graphics, Mods / Import / Shader bundle /
//   Prepare shaders again / Save on System.
// - Gameplay, Audio and System fit the visible rows and never scroll. System
//   uses 12 compact rows; other tabs use 11. Only Graphics scrolls. Pages (Mods,
//   Controls, Achievements) may scroll.
// - Graphics order: display -> graphics preset -> resolution and shadows -> anti-aliasing,
//   upscaling (quality, DLSS model, FSR sharpness), DLSS neural rendering and
//   frame generation -> effects -> frame rate (loading pace, VSync, performance overlay) ->
//   HDR and brightness -> Save. A divider opens each group after the first
//   (menu_render.cpp).
// - Rows that only apply to one choice (DLSS model, HDR levels) are hidden
//   while that choice is off (GraphicsRowHidden in menu.cpp).
// - A page opened from the menu has its own entry row (MenuRow::pageEntry: one
//   wide cell, the confirm button and a chevron) right under the setting it
//   belongs to, shown whenever that setting is: NR tuning & preview under DLSS 5
//   neural rendering, HDR calibration under HDR peak brightness, Brightness
//   before Save; Controls, Mods and Achievements on their tabs. Confirm on the
//   setting row itself stays a shortcut.
// - Help text is one line and shrinks when it is too long. Graphics help shares
//   the box with the upscaler status, so it is drawn small already: keep it to
//   what a player needs, with the longest status it can carry, in all five
//   languages. LoMenuFlowTest fails a line below 85% of its size (run it with
//   the game folder to measure the game's own fonts).
// Input, help text, pointer hits and the tests use these constants, never
// literal row numbers.
inline constexpr int MenuTabCount = 4;
// Gameplay tab (0).
inline constexpr int GameRetailRowCount = 7;
inline constexpr int GameControlsRow = 7; // opens the Controls page
inline constexpr int GameRestoreRow = 8;
inline constexpr int GameMainMenuRow = 9;
inline constexpr int GameRowCount = 10;
// Audio tab (1).
inline constexpr int AudioVoiceRow = 0;
inline constexpr int AudioMusicRow = 1;
inline constexpr int AudioEffectsRow = 2;
inline constexpr int AudioVoiceVolumeRow = 3; // host setting, kept in settings.ini
inline constexpr int AudioOutputRow = 4;
inline constexpr int AudioRearAngleRow = 5; // enabled with Matrix surround
inline constexpr int AudioRowCount = 6;
// System tab (3).
inline constexpr int SystemUiLanguageRow = 0;
inline constexpr int SystemGameLanguageRow = 1;
inline constexpr int SystemUpdatesRow = 2;
inline constexpr int SystemDiscordStatusRow = 3; // hidden on platforms without Discord IPC
inline constexpr int SystemDebugLogRow = 4;
inline constexpr int SystemAchievementsRow = 5; // opens the Achievements page
inline constexpr int SystemCollectionRow = 6;
inline constexpr int SystemModsRow = 7; // opens the Mods page
inline constexpr int SystemImportRow = 8;
inline constexpr int SystemAddToSteamRow = 9; // desktop shortcut with library artwork
inline constexpr int SystemShaderBundleRow = 10; // downloads the shader bundle again on the next start
inline constexpr int SystemShaderPreparationRow = 11; // requests startup pipeline preparation
inline constexpr int SystemAndroidDiagnosticsRow = 12; // Android-only page
inline constexpr int SystemSaveRow = 13;
inline constexpr int SystemRowCount = 14;
static_assert(GameControlsRow == GameRetailRowCount && GameMainMenuRow + 1 == GameRowCount);
static_assert(AudioRearAngleRow + 1 == AudioRowCount && SystemSaveRow + 1 == SystemRowCount);
// Logical ids for the graphics tab (2). MenuSnapshot::row stores these as int.
// Count is the tab length, not the on-screen viewport.
enum class GraphicsRow : int
{
    Backend = 0,
    Gpu = 1,
    DisplayMode = 2,
    Display = 3,
    AspectRatio = 4,
    OutputResolution = 5,
    Preset = 6, // derived from the rows below; not saved
    RenderResolution = 7,
    ShadowResolution = 8,
    DynamicShadows = 9,
    AntiAliasing = 10,
    DlssQuality = 11,
    DlssModel = 12, // shown with DLSS
    FsrSharpness = 13,
    DlssNeuralRendering = 14,
    NeuralRenderingTuning = 15, // opens the NR page; shown with the NR row
    FrameGeneration = 16,
    FrameGenerationMultiplier = 17,
    AmbientOcclusion = 18,
    AnisotropicFiltering = 19,
    DepthOfField = 20,
    Bloom = 21,
    MotionBlur = 22,
    SceneFades = 23,
    Culling = 24,
    ScalingQuality = 25,
    RgbRange = 26,
    FrameRate = 27,
    LoadingTransitions = 28,
    VariableRefreshRate = 29,
    PerformanceOverlay = 30,
    Hdr = 31,
    HdrPaperWhite = 32, // shown with HDR on
    HdrPeak = 33, // shown with HDR on
    HdrCalibration = 34, // opens the HDR peak page; shown with HDR on
    Brightness = 35, // opens the brightness page
    Save = 36,
    Count = 37,
};
inline constexpr int MenuTabWidth = 640 / MenuTabCount;
// Mods page (System → Mods): a list page in the tab layout. A row with a detail
// text keeps its choice cells in the last ModsCellsWidth of the value column;
// the focused mod has move up / move down cells beside the value column.
inline constexpr int ModsCellsWidth = 240;
inline constexpr int ModsMoveLeft = 1112, ModsMoveWidth = 64, ModsMoveGap = 8;
// Achievements page (System → Achievements): a list page. Notifications (On /
// Silent / Off, applied and saved at once), a summary title that is never
// focused, then one row per achievement: icon, name, text, gamerscore and date.
inline constexpr int AchievementsNotificationsRow = 0;
inline constexpr int AchievementsSummaryRow = 1;
inline constexpr int AchievementsFirstRow = 2;
// Controls page (Gameplay → Controls): a list page. Button prompts and
// Vibration, then a titled section per device with a row for each Xbox 360
// input in hid/input_map.h order (the keyboard adds the left stick), then
// Restore and Save. The section titles are never focused.
inline constexpr int ControlsPromptRow = 0;
inline constexpr int ControlsVibrationRow = 1;
inline constexpr int ControlsControllerTitle = 2;
inline constexpr int ControlsControllerFirst = 3;  // 16 rows: A .. D-pad right
inline constexpr int ControlsKeyboardTitle = 19;
inline constexpr int ControlsKeyboardFirst = 20;   // 20 rows: A .. left stick right
inline constexpr int ControlsRestoreRow = 40;
inline constexpr int ControlsSaveRow = 41;
inline constexpr int ControlsRowCount = 42;
// A capture waits this long for a button or key.
inline constexpr int ControlsCaptureSeconds = 5;
// Called by input polling before returning the guest-facing controller state.
// defaultButtons is the same input on the default layouts, which the Controls
// page uses.
bool FilterInput(uint16_t &buttons, int16_t leftX, int16_t leftY, uint16_t defaultButtons);
// Key presses (SDL scancodes) before shortcuts and game input. True when a
// capture on the Controls page takes the key: Escape cancels a capture, a
// keyboard capture takes every key.
bool CaptureKey(uint32_t scancode, bool repeat);
// Snapshot rendered on the presentation thread, never accessing guest memory.
bool DrawMenu(std::vector<uint32_t> &pixels, uint64_t &revision, uint32_t width = 1280, uint32_t height = 720);
// Straight-alpha title confirmation, composited over the live title picture.
// Stays drawable through its closing fade; revision also tracks fade frames.
bool DrawTitleQuit(std::vector<uint32_t> &pixels, uint64_t &revision, uint32_t width, uint32_t height);
void PointerClick(float x, float y, bool reverse);
bool IsOpen();
bool IsTitleQuitOpen();
// The menu is drawn over the live game frame, which shows the retail header
// above MenuGameHeaderBottom (layout pixels): presentation clears the menu's
// alpha there. Otherwise the menu replaces the frame.
bool MenuOverGame();
inline constexpr int MenuGameHeaderBottom = 100;
// Title menu entry (title_entry.cpp): Y on the idle title menu opens Settings
// before a game is loaded. The title tick reports whether its menu is idle and
// takes a fresh press made there, if any.
inline constexpr uint16_t TitleSettingsButton = 0x8000; // XINPUT_GAMEPAD_Y
inline constexpr uint16_t TitleQuitButton = 0x0020; // XINPUT_GAMEPAD_BACK (keyboard Backspace)
bool ConsumeTitleShortcut(bool titleMenuIdle);
// Tick the host Yes/No dialog before the title's input handler. True while it
// owns input, including its dismissal tick. The retail Settings task stays idle.
bool UpdateTitleQuit(uint8_t* base, bool titleMenuIdle);
// The retail Settings task was just opened from the title menu. Until that task
// is idle again, the per-save options are hidden (Gameplay's retail rows,
// Restore game defaults and Quit to Main Menu; Audio's Voice language, Music and
// Sound effects) and the brightness page cannot open the retail calibration screen.
void MarkTitleEntry();
} // namespace settings
