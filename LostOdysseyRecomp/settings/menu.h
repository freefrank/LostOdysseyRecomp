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
    bool operator==(const BrightnessCalibration &) const = default;
};
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
//   1 Audio (voice, volumes, audio output), 2 Graphics (display, rendering,
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
// - Graphics order: display -> resolution and shadows -> anti-aliasing and
//   upscaling -> effects -> frame rate -> HDR and brightness -> Save.
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
inline constexpr int AudioRowCount = 4;
// System tab (3).
inline constexpr int SystemUiLanguageRow = 0;
inline constexpr int SystemGameLanguageRow = 1;
inline constexpr int SystemUpdatesRow = 2;
inline constexpr int SystemCollectionRow = 3;
inline constexpr int SystemImportRow = 4;
inline constexpr int SystemSaveRow = 5;
inline constexpr int SystemRowCount = 6;
static_assert(GamePromptRow == GameRetailRowCount && GameMainMenuRow + 1 == GameRowCount);
static_assert(AudioOutputRow + 1 == AudioRowCount && SystemSaveRow + 1 == SystemRowCount);
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
    AmbientOcclusion = 10,
    DlssQuality = 11,
    FsrSharpness = 12,
    AnisotropicFiltering = 13,
    DepthOfField = 14,
    Bloom = 15,
    MotionBlur = 16,
    ScalingQuality = 17,
    RgbRange = 18,
    FrameRate = 19,
    FrameGeneration = 20,
    FrameGenerationMultiplier = 21,
    VariableRefreshRate = 22,
    Hdr = 23,
    HdrPaperWhite = 24,
    HdrPeak = 25,
    Brightness = 26,
    Save = 27,
    Count = 28,
};
inline constexpr int MenuTabWidth = 640 / MenuTabCount;
// Called by input polling before returning the guest-facing controller state.
bool FilterInput(uint16_t &buttons, int16_t leftX, int16_t leftY);
// Snapshot rendered on the presentation thread, never accessing guest memory.
bool DrawMenu(std::vector<uint32_t> &pixels, uint64_t &revision, uint32_t width = 1280, uint32_t height = 720);
void PointerClick(float x, float y, bool reverse);
bool IsOpen();
} // namespace settings
