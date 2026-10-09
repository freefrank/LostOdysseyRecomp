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
// - Tabs: 0 Gameplay (retail game options, controller options, game actions),
//   1 Audio (voice, volumes, audio output, rear angle), 2 Graphics (display, rendering,
//   frame rate, HDR), 3 System (interface and game language, updates, TAA
//   shader collection, Import discs & DLC, Save).
// - Gameplay starts with the seven retail guest settings in their retail order;
//   host rows follow them.
// - Host rows are grouped by topic. Controller rows (button prompts, vibration,
//   future ones) go to Gameplay, not Audio.
// - Actions come last on their tab: Restore / Quit to Main Menu on Gameplay,
//   Brightness / Save on Graphics, Import / Save on System.
// - Gameplay, Audio and System fit the visible rows (kMenuVisibleRows) and never
//   scroll; only Graphics scrolls.
// - Graphics order: display -> resolution and shadows -> anti-aliasing,
//   upscaling (quality, DLSS model, FSR sharpness), DLSS neural rendering and
//   frame generation -> effects -> frame rate -> HDR and brightness -> Save. A
//   divider opens each group after the first (menu_render.cpp).
// - Rows that only apply to one choice (DLSS model, HDR levels) are hidden
//   while that choice is off (GraphicsRowHidden in menu.cpp).
// Input, help text, pointer hits and the tests use these constants, never
// literal row numbers.
inline constexpr int MenuTabCount = 4;
// Gameplay tab (0).
inline constexpr int GameRetailRowCount = 7;
inline constexpr int GamePromptRow = 7;
inline constexpr int GameVibrationRow = 8;
inline constexpr int GameRestoreRow = 9;
inline constexpr int GameMainMenuRow = 10;
inline constexpr int GameRowCount = 11;
// Audio tab (1).
inline constexpr int AudioVoiceRow = 0;
inline constexpr int AudioMusicRow = 1;
inline constexpr int AudioEffectsRow = 2;
inline constexpr int AudioOutputRow = 3;
inline constexpr int AudioRearAngleRow = 4; // enabled with Matrix surround
inline constexpr int AudioRowCount = 5;
// System tab (3).
inline constexpr int SystemUiLanguageRow = 0;
inline constexpr int SystemGameLanguageRow = 1;
inline constexpr int SystemUpdatesRow = 2;
inline constexpr int SystemDebugLogRow = 3;
inline constexpr int SystemCollectionRow = 4;
inline constexpr int SystemImportRow = 5;
inline constexpr int SystemSaveRow = 6;
inline constexpr int SystemRowCount = 7;
static_assert(GamePromptRow == GameRetailRowCount && GameMainMenuRow + 1 == GameRowCount);
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
    RenderResolution = 6,
    ShadowResolution = 7,
    DynamicShadows = 8,
    AntiAliasing = 9,
    DlssQuality = 10,
    DlssModel = 11, // shown with DLSS
    FsrSharpness = 12,
    DlssNeuralRendering = 13,
    FrameGeneration = 14,
    FrameGenerationMultiplier = 15,
    AmbientOcclusion = 16,
    AnisotropicFiltering = 17,
    DepthOfField = 18,
    Bloom = 19,
    MotionBlur = 20,
    Culling = 21,
    ScalingQuality = 22,
    RgbRange = 23,
    FrameRate = 24,
    VariableRefreshRate = 25,
    Hdr = 26,
    HdrPaperWhite = 27, // shown with HDR on
    HdrPeak = 28, // shown with HDR on
    Brightness = 29,
    Save = 30,
    Count = 31,
};
inline constexpr int MenuTabWidth = 640 / MenuTabCount;
// Called by input polling before returning the guest-facing controller state.
bool FilterInput(uint16_t &buttons, int16_t leftX, int16_t leftY);
// Snapshot rendered on the presentation thread, never accessing guest memory.
bool DrawMenu(std::vector<uint32_t> &pixels, uint64_t &revision, uint32_t width = 1280, uint32_t height = 720);
void PointerClick(float x, float y, bool reverse);
bool IsOpen();
// Title menu entry (title_entry.cpp): Y on the idle title menu opens Settings
// before a game is loaded. The title tick reports whether its menu is idle and
// takes a fresh press made there, if any.
inline constexpr uint16_t TitleSettingsButton = 0x8000; // XINPUT_GAMEPAD_Y
bool ConsumeTitleShortcut(bool titleMenuIdle);
// The retail Settings task was just opened from the title menu. Until that task
// is idle again, the per-save options are hidden (Gameplay's retail rows,
// Restore game defaults and Quit to Main Menu; Audio's Voice, Music and Sound
// effects) and the brightness page cannot open the retail calibration screen.
void MarkTitleEntry();
} // namespace settings
