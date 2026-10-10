#include <os/log_collection.h>
#include "menu.h"
#include "menu_render.h"
#include "menu_assets.h"
#include "config.h"
#include "graphics_menu.h"
#include <gpu/frame_rate.h>
#include <gpu/dlss_nr_state.h>
#include "restart.h"
#include "translations.h"
#include <gpu/video.h>
#include <gpu/display_choice.h>
#include <gpu/frame_plan.h>
#include <gpu/frame_generation_settings.h>
#include <kernel/io/file_system.h>
#include <os/logger.h>
#include <stdafx.h>
#include "language_trace.h"
#include <apu/audio.h>
#include <hid/hid.h>
extern "C" PPC_FUNC(__imp__sub_822F19B0);
extern "C" PPC_FUNC(__imp__sub_82481BE8);
extern "C" PPC_FUNC(__imp__sub_82870E38);
extern "C" PPC_FUNC(__imp__sub_828710A0);
extern "C" PPC_FUNC(__imp__sub_82889E50);
namespace settings { void RequestMainMenuAfterSettingsClose(PPCContext& ctx, uint8_t* base, uint32_t settingsMenu); }
namespace settings
{
namespace
{
std::atomic<bool> active{false};
std::atomic<uint16_t> pending{0};
std::atomic<unsigned> cancelPolls{0};
std::atomic<uint16_t> cancelButton{0x2000};
std::atomic<bool> swapConfirm{false};
std::atomic<bool> waitForRelease{true};
std::atomic<bool> releaseToParent{false};
std::atomic<int> mouseTab{-1}, mouseRow{-1};
std::atomic<int> mouseDialog{-1};
std::atomic<uint16_t> mouseAction{0};
std::atomic<uint64_t> hdrDisplayInfo{0};
std::atomic<bool> calibrationOpen{false};
std::atomic<bool> calibrationSceneAvailable{false}, calibrationScenePreview{true};
std::atomic<int> calibrationClick{-1}, calibrationDragNits{-1};
std::mutex calibrationKeyMutex;
std::vector<uint32_t> calibrationKeys;
bool calibrationNumberEditing = false;
std::wstring calibrationNumber;
int calibrationFocus = 0;
bool calibrationStartAutomatic = true;
uint32_t calibrationStartPeakNits = 1000;
// Brightness / gamma page. Focus: 0 brightness, 1 gamma, 2-5 buttons.
std::atomic<bool> brightnessOpen{false};
std::atomic<int> brightnessClick{-1};
std::atomic<int> brightnessDragBrightness{INT_MIN}, brightnessDragGamma{-1};
int brightnessFocus = 0;
int brightnessStart = 0;
uint32_t gammaStart = 100;
// The original calibration screen returns to the page with the unsaved edit.
bool returnToBrightness = false;
constexpr int kBrightnessSliderX = 380, kBrightnessSliderWidth = 620;
// DLSS 5 neural rendering page. Focus: 0 passes, 1 model, 2 preset, 3-6 tones and
// structure, 7 skin, 8 character mask, 9-11 Default / Done / Cancel.
std::atomic<bool> nrOpen{false};
std::atomic<bool> nrSceneAvailable{false};
// Pending pointer action: valid bit, kind (1 focus, 2 step down, 3 step up,
// 4 set value, 5 focus + confirm), focus in bits 4-7, value + 100 from bit 8.
std::atomic<uint32_t> nrAction{0};
int nrFocus = 0;
Config nrStart; // NR fields (and passes) as the page opened
std::mutex snapshotMutex;
using Row = MenuRow;
using Snapshot = MenuSnapshot;
Snapshot snapshot;
Config edit;
Config previousDisplay;
uint64_t displayTicket = 0;
bool displayRollback = false, rollbackSaveFailed = false;
// Keep-or-revert prompt after a saved display choice moved the window. Without
// an answer the previous display returns when the countdown ends.
constexpr auto kDisplayConfirmTime = std::chrono::seconds(5);
bool displayConfirm = false, displayReverting = false;
int displayConfirmChoice = 0;
uint64_t displayMovesBefore = 0;
std::chrono::steady_clock::time_point displayConfirmDeadline;
std::atomic<bool> displayConfirmOpen{false}, displayConfirmEscape{false};
std::chrono::steady_clock::duration menuClockOffset{}; // Tests advance the menu clock.
// Saved display choice the menu last saw; the window thread changes it when the
// player moves the window (Win+Shift+arrow).
std::string syncedDisplayName;
uint32_t syncedDisplayIndex = 0;
std::chrono::steady_clock::time_point MenuNow() { return std::chrono::steady_clock::now() + menuClockOffset; }
bool collectionPrompt = false;
int collectionChoice = 1;
bool restartPrompt = false, savedRestartPrompt = false, restartSaveFailed = false;
bool restartForFgProvider = false;
int restartChoice = 0;
bool mainMenuPrompt = false;
int mainMenuChoice = 1;
bool mainMenuRequested = false;
bool importPrompt = false;
int importChoice = 1;
bool importLaunchPending = false;
Config restartAfter;
int tab = 0, row = 0;
bool bypass = false, sawModal = false;
bool closing = false;
uint32_t lastMenu = 0;
std::wstring status;
HdrDisplayInfo CurrentHdrDisplayInfo()
{
    const uint64_t bits = hdrDisplayInfo.load(std::memory_order_relaxed);
    return {bool(bits & (1ull << 32)), uint32_t(bits), bool(bits & (1ull << 33))};
}
HdrCalibration MakeHdrCalibration(const Config &config, bool open)
{
    const auto display = CurrentHdrDisplayInfo();
    HdrCalibration result;
    result.open = open;
    result.automatic = config.hdrPeakAutomatic;
    result.manualNits = config.hdrPeakNits;
    result.paperWhiteNits = config.hdrPaperWhiteNits;
    result.detectedValid = display.peakNits > 0;
    result.detectedNits = display.peakNits;
    result.relative = display.relative;
    result.hdrActive = display.active;
    result.sceneAvailable = calibrationSceneAvailable.load(std::memory_order_relaxed);
    result.scenePreview = calibrationScenePreview.load(std::memory_order_relaxed);
    result.effectiveNits = std::clamp(result.automatic && result.detectedValid
        ? result.detectedNits : result.automatic ? 1000u : result.manualNits,
        result.paperWhiteNits, 10000u);
    result.numericEditing = open && calibrationNumberEditing;
    if (open) result.focus = calibrationFocus;
    if (result.numericEditing) result.numericText = calibrationNumber;
    return result;
}
BrightnessCalibration MakeBrightnessCalibration(const Config &config, bool open)
{
    BrightnessCalibration result;
    result.open = open;
    result.brightness = config.displayBrightness;
    result.gamma = config.displayGamma;
    result.expandRgbRange = config.expandRgbRange;
    result.sceneAvailable = calibrationSceneAvailable.load(std::memory_order_relaxed);
    result.scenePreview = calibrationScenePreview.load(std::memory_order_relaxed);
    if (open) result.focus = brightnessFocus;
    return result;
}
float BrightnessSliderFraction(float x)
{
    return std::clamp((x - kBrightnessSliderX) / float(kBrightnessSliderWidth), 0.0f, 1.0f);
}
std::wstring BrightnessValue(int brightness)
{
    return brightness > 0 ? L"+" + std::to_wstring(brightness) : std::to_wstring(brightness);
}
constexpr uint32_t NrAction(int kind, int focus, int value = 0)
{
    return 0x80000000u | uint32_t(kind) | (uint32_t(focus) << 4) | (uint32_t(value + 100) << 8);
}
NeuralRenderingTuning MakeNeuralRenderingTuning(const Config &config, bool open)
{
    NeuralRenderingTuning result;
    result.open = open;
    result.sceneAvailable = nrSceneAvailable.load(std::memory_order_relaxed);
    result.passes = std::min(config.dlssNeuralRendering, DlssNeuralRenderingMaxPasses);
    result.preset = config.dlssNrPreset;
    result.style = config.dlssNrStyle;
    result.intensity = config.dlssNrIntensity;
    result.globalTone = config.dlssNrGlobalTone;
    result.localTone = config.dlssNrLocalTone;
    result.structure = config.dlssNrStructure;
    result.skin = config.dlssNrSkin;
    result.autoMask = config.dlssNrAutoMask;
    if (open) result.focus = nrFocus;
    return result;
}
void ResetNeuralRenderingTuning(Config &config)
{
    const Config defaults;
    config.dlssNrPreset = defaults.dlssNrPreset;
    config.dlssNrStyle = defaults.dlssNrStyle;
    config.dlssNrIntensity = defaults.dlssNrIntensity;
    config.dlssNrGlobalTone = defaults.dlssNrGlobalTone;
    config.dlssNrLocalTone = defaults.dlssNrLocalTone;
    config.dlssNrStructure = defaults.dlssNrStructure;
    config.dlssNrSkin = defaults.dlssNrSkin;
    config.dlssNrAutoMask = defaults.dlssNrAutoMask;
}
void RestoreNeuralRenderingTuning(Config &config, const Config &start)
{
    config.dlssNeuralRendering = start.dlssNeuralRendering;
    config.dlssNrPreset = start.dlssNrPreset;
    config.dlssNrStyle = start.dlssNrStyle;
    config.dlssNrIntensity = start.dlssNrIntensity;
    config.dlssNrGlobalTone = start.dlssNrGlobalTone;
    config.dlssNrLocalTone = start.dlssNrLocalTone;
    config.dlssNrStructure = start.dlssNrStructure;
    config.dlssNrSkin = start.dlssNrSkin;
    config.dlssNrAutoMask = start.dlssNrAutoMask;
}
// Percent sliders (focus 3-6, 0-200) and skin (focus 7, -100..100) step by 5.
bool NrIsSlider(int focus) { return focus >= 3 && focus <= 7; }
uint32_t *NrPercentField(Config &config, int focus)
{
    switch (focus)
    {
    case 3: return &config.dlssNrIntensity;
    case 4: return &config.dlssNrGlobalTone;
    case 5: return &config.dlssNrLocalTone;
    case 6: return &config.dlssNrStructure;
    default: return nullptr;
    }
}
void SetNrValue(Config &config, int focus, int value)
{
    if (uint32_t *percent = NrPercentField(config, focus))
        *percent = uint32_t(std::clamp(value, 0, 200));
    else if (focus == 7)
        config.dlssNrSkin = std::clamp(value, -100, 100);
}
// Left/right on a control: combos cycle with wrap, sliders step by 5, the toggle flips.
void StepNrControl(Config &config, int focus, int delta)
{
    auto wrap = [&](uint32_t value, uint32_t count) {
        return uint32_t((int(value) + delta + int(count)) % int(count));
    };
    if (focus == 0)
        config.dlssNeuralRendering = wrap(std::min(config.dlssNeuralRendering, DlssNeuralRenderingMaxPasses),
                                          DlssNeuralRenderingMaxPasses + 1);
    else if (focus == 1) config.dlssNrStyle = wrap(std::min(config.dlssNrStyle, 2u), 3);
    else if (focus == 2) config.dlssNrPreset = wrap(std::min(config.dlssNrPreset, 3u), 4);
    else if (const uint32_t *percent = NrPercentField(config, focus))
        SetNrValue(config, focus, int(*percent) + delta * 5);
    else if (focus == 7) SetNrValue(config, focus, config.dlssNrSkin + delta * 5);
    else if (focus == 8) config.dlssNrAutoMask = !config.dlssNrAutoMask;
}
// Pointer position to the control row it lands on (focus 0-8). `value` is the
// stepped slider value for x; `controlSide` is 0 over the label, else 2 / 3 for
// the left / right half of a combo or toggle.
bool NrControlAt(float x, float y, int &focus, bool &onTrack, int &value, int &controlSide)
{
    const int column = x >= NrColumnX[1] - 10 ? 1 : 0;
    const int slot = int(std::floor((y - NrRowTop) / float(NrRowHeight)));
    if (y < NrRowTop || slot < 0 || slot >= (column ? NrControlCount - 5 : 5)) return false;
    if (x < NrColumnX[column] || x >= NrColumnX[column] + NrColumnWidth) return false;
    focus = column ? 5 + slot : slot;
    const float controlX = float(NrColumnX[column] + NrControlOffset);
    onTrack = NrIsSlider(focus) && x >= controlX - 6 && x < controlX + NrSliderWidth + 6;
    const float fraction = std::clamp((x - controlX) / float(NrSliderWidth), 0.0f, 1.0f);
    value = int(std::lround(fraction * 40.0f)) * 5 - (focus == 7 ? 100 : 0);
    controlSide = x < controlX ? 0 : x < controlX + (NrColumnWidth - NrControlOffset) * 0.5f ? 2 : 3;
    return true;
}
std::wstring GammaValue(uint32_t gamma)
{
    const std::wstring hundredths = std::to_wstring(gamma % 100);
    return std::to_wstring(gamma / 100) + L"." + (hundredths.size() < 2 ? L"0" : L"") + hundredths;
}
uint32_t CalibrationSliderValue(float x, uint32_t paperWhiteNits)
{
    const float fraction = std::clamp((x - 260.0f) / 760.0f, 0.0f, 1.0f);
    const float minimum = float(std::clamp(paperWhiteNits, 80u, 400u));
    // Logarithmic travel leaves useful precision around common HDR peaks while
    // still permitting manual values up to 10,000 nits.
    return uint32_t(std::clamp(int(std::lround(minimum * std::exp(std::log(10000.0f / minimum) * fraction))),
        int(paperWhiteNits), 10000));
}
constexpr uint32_t resolutions16_9[][2] = {
    {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
constexpr uint32_t resolutions21_9[][2] = {
    {1720, 720}, {2560, 1080}, {3440, 1440}, {3840, 1600}, {5120, 2160}};
constexpr uint32_t resolutions4_3[][2] = {
    {960, 720}, {1440, 1080}, {1920, 1440}, {2880, 2160}};
using AspectMode = gpu::aspect_ratio::Mode;
// Menu positions are independent of the persisted quality IDs (Quality=0, Balanced=1, Performance=2, native AA=3).
constexpr uint32_t qualityMenuIds[] = {2, 1, 0, 3};
constexpr uint32_t QualityMenuIndex(uint32_t id)
{
    return id < 3 ? 2 - id : 3;
}
inline bool IsUltrawideAspect(uint32_t width, uint32_t height)
{
    return width && height && (uint64_t(width) * 9 > uint64_t(height) * 16);
}
// Output resolution choices follow the Aspect ratio setting. Auto keeps the
// list of the current resolution's shape.
std::span<const uint32_t[2]> OutputResolutions(AspectMode mode, uint32_t width, uint32_t height)
{
    if (mode == AspectMode::Auto)
        mode = IsUltrawideAspect(width, height) ? AspectMode::Ultrawide
             : uint64_t(width) * 2 < uint64_t(height) * 3 ? AspectMode::Standard : AspectMode::Wide;
    if (mode == AspectMode::Ultrawide) return resolutions21_9;
    if (mode == AspectMode::Standard) return resolutions4_3;
    return resolutions16_9;
}
inline uint32_t FindNearestResolutionIndex(const uint32_t list[][2], size_t count, uint32_t currentWidth, uint32_t currentHeight)
{
    for (size_t i = 0; i < count; ++i)
        if (list[i][0] == currentWidth && list[i][1] == currentHeight)
            return uint32_t(i);
    // Closest match by height; for equidistant heights (such as 900 vs 720/1080), <= chooses the higher option
    uint32_t bestIndex = 0;
    uint32_t bestDiff = UINT32_MAX;
    for (size_t i = 0; i < count; ++i)
    {
        uint32_t diff = list[i][1] >= currentHeight ? (list[i][1] - currentHeight) : (currentHeight - list[i][1]);
        if (diff <= bestDiff)
        {
            bestDiff = diff;
            bestIndex = uint32_t(i);
        }
    }
    return bestIndex;
}
const wchar_t *Tr(const wchar_t *en, const wchar_t *zh)
{
    return Translate(edit.uiLanguage, en, zh);
}
// Adapter and display names arrive as UTF-8.
std::wstring Widen(std::string_view text)
{
    std::wstring result;
    for (size_t i = 0; i < text.size();)
    {
        const auto lead = uint8_t(text[i]);
        const size_t length = lead < 0x80 ? 1 : (lead >> 5) == 6 ? 2 : (lead >> 4) == 14 ? 3 : (lead >> 3) == 30 ? 4 : 0;
        if (!length || i + length > text.size())
        {
            result.push_back(L'?');
            ++i;
            continue;
        }
        uint32_t code = length == 1 ? lead : lead & (0x7f >> length);
        for (size_t k = 1; k < length; ++k)
            code = (code << 6) | (uint8_t(text[i + k]) & 0x3f);
        i += length;
        if (sizeof(wchar_t) == 2 && code >= 0x10000)
        {
            code -= 0x10000;
            result.push_back(wchar_t(0xd800 + (code >> 10)));
            result.push_back(wchar_t(0xdc00 + (code & 0x3ff)));
        }
        else
            result.push_back(wchar_t(code));
    }
    return result;
}
// Menu choice 0 is Automatic; choice i names[i - 1]. A saved name that is not
// listed stays as the last choice, so the row shows the saved value.
struct NameChoices
{
    std::vector<std::string> names;
    uint32_t selected = 0;
};
NameChoices GpuChoices()
{
    NameChoices result{gpu::video::GpuDeviceNames()};
    if (!edit.gpuDevice.empty())
    {
        auto found = std::find(result.names.begin(), result.names.end(), edit.gpuDevice);
        if (found == result.names.end()) found = result.names.insert(result.names.end(), edit.gpuDevice);
        result.selected = uint32_t(found - result.names.begin()) + 1;
    }
    return result;
}
// Choice 0 is Automatic, choice i the connected display i - 1. Monitors of one
// model share a name, so each label also carries its number, size and position.
// A saved display that is not connected stays as the last choice.
struct DisplayChoiceList
{
    std::vector<gpu::display_choice::Display> displays;
    std::vector<std::wstring> labels;
    uint32_t selected = 0;
};
DisplayChoiceList DisplayChoices()
{
    DisplayChoiceList result{gpu::video::Displays()};
    result.labels.push_back(Tr(L"Automatic", L"自動"));
    for (size_t i = 0; i < result.displays.size(); ++i)
    {
        const auto &display = result.displays[i];
        std::wstring label = std::to_wstring(i + 1) + L": " + Widen(display.name);
        if (display.width > 0 && display.height > 0)
            label += L" · " + std::to_wstring(display.width) + L"×" + std::to_wstring(display.height) +
                     L" (" + std::to_wstring(display.x) + L", " + std::to_wstring(display.y) + L")";
        result.labels.push_back(std::move(label));
    }
    if (!edit.displayName.empty())
    {
        const int found = gpu::display_choice::Resolve(result.displays, edit.displayName, edit.displayIndex);
        if (found < 0) result.labels.push_back(Widen(edit.displayName));
        result.selected = found < 0 ? uint32_t(result.labels.size() - 1) : uint32_t(found) + 1;
    }
    return result;
}
void KeepDisplayChoice()
{
    displayConfirm = false;
    displayConfirmOpen = false;
    status = Tr(L"Display kept.", L"已保留這台顯示器。");
    LOG_INFO("settings: display choice kept: \"{}\"#{}", edit.displayName, edit.displayIndex);
}
// Restores the previous display choice in settings.ini and moves the window
// back to where it was, in the current mode.
void RevertDisplayChoice(const char *reason)
{
    displayConfirm = false;
    displayConfirmOpen = false;
    Config reverted = GetConfig();
    reverted.displayName = previousDisplay.displayName;
    reverted.displayIndex = previousDisplay.displayIndex;
    rollbackSaveFailed = !SaveConfig(reverted);
    if (rollbackSaveFailed) PreviewConfig(reverted);
    edit.displayName = reverted.displayName;
    edit.displayIndex = reverted.displayIndex;
    displayReverting = true;
    displayTicket = gpu::video::BeginDisplayRevert(reverted);
    status = Tr(L"Returning to the previous display…", L"正在回到之前的顯示器……");
    LOG_INFO("settings: display choice reverted ({}): back to \"{}\"#{}", reason, reverted.displayName, reverted.displayIndex);
}
uint32_t ConfigAddress(uint8_t *base)
{
    uint32_t object = PPC_LOAD_U32(0x8326A068);
    if (!object)
        return 0;
    uint32_t storage = PPC_LOAD_U32(object + 4);
    return storage ? PPC_LOAD_U32(storage + 0x18) : 0;
}
uint32_t VoiceCount(uint8_t *base)
{
    // The original 82482028/82482038 access this resource-populated list.
    return language::VoiceCount(PPC_LOAD_U8(language::Registry + 419));
}
uint32_t VoiceLanguage(uint8_t *base, uint32_t index)
{
    return index < VoiceCount(base) ? PPC_LOAD_U16(language::Registry + 288 + index * 2) : 0;
}
const wchar_t *VoiceName(uint8_t *base, uint32_t index)
{
    constexpr const wchar_t *names[] = {L"English", L"English", L"日本語", L"Deutsch", L"Français",
                                       L"Español", L"Italiano", L"한국어", L"繁體中文", L"简体中文"};
    const auto language = VoiceLanguage(base, index);
    return language >= 1 && language < std::size(names) ? names[language] : L"Unknown";
}
std::wstring ExecutionSizeSuffix(const gpu::frame_plan::FramePlan &plan)
{
    if (!plan.width || !plan.height || !plan.output.width || !plan.output.height)
        return {};
    return L" " + std::to_wstring(plan.width) + L"×" + std::to_wstring(plan.height) +
           L" - " + std::to_wstring(plan.output.width) + L"×" + std::to_wstring(plan.output.height);
}
const wchar_t *SubmittedModeSentence(gpu::upscaling::DlssQuality quality)
{
    switch (gpu::upscaling::NormalizeDlssQuality(quality))
    {
    case gpu::upscaling::DlssQuality::Balanced:
        return Tr(L"Submitted DLSS Balanced output.", L"已提交 DLSS 平衡輸出。");
    case gpu::upscaling::DlssQuality::Performance:
        return Tr(L"Submitted DLSS Performance output.", L"已提交 DLSS 效能輸出。");
    case gpu::upscaling::DlssQuality::Dlaa:
        return Tr(L"Submitted DLAA output.", L"已提交 DLAA 輸出。");
    default:
        return Tr(L"Submitted DLSS Quality output.", L"已提交 DLSS 品質輸出。");
    }
}
// Classified reason from DlssEffectSnapshot. The menu does not infer GPU eligibility.
const wchar_t *ReasonSentence(gpu::frame_plan::DlssEffectReason reason)
{
    using gpu::frame_plan::DlssEffectReason;
    switch (reason)
    {
    case DlssEffectReason::AwaitingGpuFrame:
        return Tr(L"Waiting for the first DLSS result.", L"正在等待第一次 DLSS 結果。");
    case DlssEffectReason::DeviceNotReady:
        return Tr(L"DLSS is waiting for the graphics device. Normal rendering is used for now.",
                  L"DLSS 正在等待圖形裝置。目前先使用常規渲染。");
    case DlssEffectReason::NeedsVulkanRestart:
        return Tr(L"DLSS needs Vulkan and a restart.", L"DLSS 需要 Vulkan，並在重新啟動後才會使用。");
    case DlssEffectReason::CapabilityUnavailable:
        return Tr(L"DLSS is not available on this device.", L"這台裝置無法使用 DLSS。");
    case DlssEffectReason::SizingPending:
        return Tr(L"DLSS is querying the render resolution. Normal rendering is used for now.",
                  L"DLSS 正在查詢渲染解析度。目前先使用常規渲染。");
    case DlssEffectReason::SizingUnavailable:
        return Tr(L"Render resolution is unavailable.", L"渲染解析度無法使用。");
    case DlssEffectReason::SizingError:
        return Tr(L"Render resolution query failed.", L"渲染解析度查詢失敗。");
    case DlssEffectReason::InputProbeOnly:
        return Tr(L"Input capture only. DLSS is not run.", L"只採集輸入，不執行 DLSS。");
    case DlssEffectReason::NoEligibleScene:
        return Tr(L"No eligible scene this frame.", L"這一幀沒有合格場景。");
    case DlssEffectReason::MotionPipelinePending:
        return Tr(L"Motion data is still being prepared.", L"正在準備運動資料。");
    case DlssEffectReason::UnknownColorEncoding:
        return Tr(L"Color conditions do not support DLSS.", L"色彩條件不支援 DLSS。");
    case DlssEffectReason::FeatureReconfigurePending:
        return Tr(L"Waiting to rebuild DLSS.", L"正在等待重建 DLSS。");
    case DlssEffectReason::PromotionUnavailable:
        return Tr(L"DLSS output was not adopted.", L"未能採用 DLSS 輸出。");
    case DlssEffectReason::RequestFailure:
        return Tr(L"DLSS request failed and fell back. No automatic retry.",
                  L"DLSS 請求失敗並已回退，不會自動重試。");
    case DlssEffectReason::GpuWorkStopped:
        return Tr(L"GPU work has stopped.", L"GPU 工作已停止。");
    case DlssEffectReason::None:
        break;
    }
    return nullptr;
}
const wchar_t *PhaseSentence(gpu::frame_plan::DlssEffectPhase phase)
{
    using gpu::frame_plan::DlssEffectPhase;
    using gpu::frame_plan::DlssEffectReason;
    switch (phase)
    {
    case DlssEffectPhase::AwaitingExecution:
        return ReasonSentence(DlssEffectReason::AwaitingGpuFrame);
    case DlssEffectPhase::InputProbeOnly:
        return ReasonSentence(DlssEffectReason::InputProbeOnly);
    case DlssEffectPhase::GpuStopped:
        return ReasonSentence(DlssEffectReason::GpuWorkStopped);
    case DlssEffectPhase::NeedsVulkanRestart:
        return ReasonSentence(DlssEffectReason::NeedsVulkanRestart);
    case DlssEffectPhase::DeviceUnavailable:
        return ReasonSentence(DlssEffectReason::CapabilityUnavailable);
    case DlssEffectPhase::TemporaryFallback:
        return Tr(L"DLSS is not running. Normal rendering is used for now.",
                  L"DLSS 沒有在執行。目前先使用常規渲染。");
    case DlssEffectPhase::Inactive:
    case DlssEffectPhase::Active:
        break;
    }
    return Tr(L"DLSS is not in use.", L"DLSS 目前未啟用。");
}
std::wstring SubmittedSentence(const gpu::frame_plan::DlssEffectSnapshot &running)
{
    return std::wstring(SubmittedModeSentence(running.execution->plan.dlssQuality)) +
           ExecutionSizeSuffix(running.execution->plan);
}
const wchar_t *BackendPendingSentence(gpu::backend::Backend backend)
{
    if (backend == gpu::backend::Backend::Vulkan)
        return Tr(L"Still using Vulkan until restart. DLSS is checked after restart.",
                  L"重新啟動前仍使用 Vulkan。DLSS 會在重新啟動後再確認。");
    if (backend == gpu::backend::Backend::D3D11)
        return Tr(L"Still using Direct3D 11 until restart. DLSS is checked after restart.",
                  L"重新啟動前仍使用 Direct3D 11。DLSS 會在重新啟動後再確認。");
    if (backend == gpu::backend::Backend::D3D12)
        return Tr(L"Still using Direct3D 12 until restart. DLSS is checked after restart.",
                  L"重新啟動前仍使用 Direct3D 12。DLSS 會在重新啟動後再確認。");
    return Tr(L"Graphics backend change is not applied. DLSS is checked after restart.",
              L"圖形後端變更尚未套用。DLSS 會在重新啟動後再確認。");
}
// XeSS shares the FSR status sentences and their translations; only the
// provider name differs.
std::wstring WithProvider(std::wstring text, std::wstring_view name)
{
    for (size_t at = text.find(L"FSR"); at != std::wstring::npos; at = text.find(L"FSR", at + name.size()))
        text.replace(at, 3, name);
    return text;
}
const wchar_t* FsrFallbackSentence(gpu::frame_plan::DlssEffectReason reason)
{
    using gpu::frame_plan::DlssEffectReason;
    switch (reason) {
    case DlssEffectReason::NoEligibleScene:
        return Tr(L"FSR: no eligible scene in the latest frame. Menus and transitions may use normal rendering.",
                  L"FSR：最近一幀沒有合格場景。選單和過場可能使用常規渲染。");
    case DlssEffectReason::UnknownColorEncoding:
        return Tr(L"FSR: scene color source is not verified. Normal rendering is used for this frame.",
                  L"FSR：場景色彩來源尚未確認，這一幀使用常規渲染。");
    case DlssEffectReason::MotionPipelinePending:
        return Tr(L"FSR: motion data is still being prepared.", L"FSR：正在準備運動資料。");
    case DlssEffectReason::FeatureReconfigurePending:
        return Tr(L"FSR: waiting to rebuild the upscaler.", L"FSR：正在等待重建縮放器。");
    case DlssEffectReason::PromotionUnavailable:
        return Tr(L"FSR: output was not adopted this frame.", L"FSR：這一幀未能採用輸出。");
    case DlssEffectReason::RequestFailure:
        return Tr(L"FSR request failed. Change the upscaler setting or restart to retry.",
                  L"FSR 請求失敗。請變更縮放設定或重新啟動後重試。");
    case DlssEffectReason::GpuWorkStopped:
        return Tr(L"FSR: GPU work has stopped. Restart is required.", L"FSR：GPU 工作已停止，需要重新啟動。");
    default:
        return Tr(L"FSR has not submitted output for the current request. Normal rendering is in use.",
                  L"FSR 目前的請求尚無已提交輸出，正在使用常規渲染。");
    }
}
// The first sentence is the classified result. Active with an execution record
// reads quality and size from that submitted frame, not from an unsaved edit or
// the CPU plan. Submitted means that output was accepted for submission. It does
// not mean the GPU finished, the image was presented, or the whole frame used DLSS.
// Active without an execution record stays on the waiting sentence.
// An unsaved Off, quality, or backend edit is only a following note.
std::wstring DlssNotice()
{
    const auto running = gpu::frame_plan::CurrentDlssEffect();
    const auto execution = gpu::frame_plan::CurrentUpscalerExecution();
    if (GetConfig().upscaler == gpu::upscaling::Upscaler::MetalFx) {
        const bool matchingExecution = running.hasPlan && running.plannedRequest == gpu::upscaling::Upscaler::MetalFx &&
            execution && execution->actualProvider == gpu::upscaling::Upscaler::MetalFx &&
            execution->plan.deviceEpoch == running.device.deviceEpoch &&
            execution->plan.requestSignature == running.requestSignature &&
            execution->plan.geometryEpoch == running.geometryEpoch;
        std::wstring text;
        if (!running.device.deviceReady || running.device.gpuWorkStopped)
            text = Tr(L"MetalFX: graphics device is not ready.", L"MetalFX：圖形裝置尚未就緒。");
        else if (!running.device.metalFxAvailable)
            text = Tr(L"MetalFX Temporal is unavailable on this Mac.", L"這台 Mac 無法使用 MetalFX Temporal。");
        else if (running.hasPlan && running.failure)
            text = Tr(L"MetalFX request failed. Change the upscaler setting or restart to retry.",
                      L"MetalFX 請求失敗。請變更縮放設定或重新啟動後重試。");
        else if (matchingExecution && execution->submissionSerial &&
            execution->outcome == gpu::frame_plan::DlssExecutionOutcome::Submitted) {
            const wchar_t* modes[] = {Tr(L"Quality", L"品質"), Tr(L"Balanced", L"平衡"), Tr(L"Performance", L"效能"), L"Native AA"};
            text = std::wstring(L"MetalFX ") + modes[uint32_t(gpu::upscaling::NormalizeFsrQuality(execution->plan.fsrQuality))] +
                Tr(L" output submitted.", L" 輸出已提交。") + ExecutionSizeSuffix(execution->plan);
        } else
            text = Tr(L"MetalFX: no upscaled scene in the latest frame. Menus and transitions use normal rendering.",
                      L"MetalFX：最近一幀沒有縮放場景。選單和過場使用常規渲染。");
        if (edit.upscaler != gpu::upscaling::Upscaler::MetalFx)
            text += Tr(L" The selected upscaler is not applied yet.", L" 選取的縮放技術尚未套用。");
        return text;
    }
    for (const auto provider : {gpu::upscaling::Upscaler::Fsr, gpu::upscaling::Upscaler::Xess}) {
        if (GetConfig().upscaler != provider && !(execution && execution->actualProvider == provider)) continue;
        const bool xess = provider == gpu::upscaling::Upscaler::Xess;
        const std::wstring_view name = xess ? L"XeSS" : L"FSR";
        std::wstring fsrText;
        const bool matchingRequest = running.hasPlan && running.plannedRequest == provider;
        const bool matchingExecution = matchingRequest && execution &&
            execution->actualProvider == provider &&
            execution->plan.deviceEpoch == running.device.deviceEpoch &&
            execution->plan.requestSignature == running.requestSignature &&
            execution->plan.geometryEpoch == running.geometryEpoch;
        if (running.device.gpuWorkStopped)
            fsrText = FsrFallbackSentence(gpu::frame_plan::DlssEffectReason::GpuWorkStopped);
        else if (!running.device.deviceReady)
            fsrText = Tr(L"FSR: graphics device is not ready.", L"FSR：圖形裝置尚未就緒。");
        else if (xess && !running.device.xessAvailable)
            fsrText = Tr(L"XeSS needs Direct3D 12 and an XeSS-enabled build.", L"XeSS 需要 Direct3D 12 與包含 XeSS 的版本。");
        else if (!xess && !running.device.fsrAvailable)
            fsrText = Tr(L"FSR is unavailable on this device or build.", L"目前的裝置或版本無法使用 FSR。");
        else if (matchingRequest && running.failure)
            fsrText = FsrFallbackSentence(gpu::frame_plan::DlssEffectReason::RequestFailure);
        else if (matchingExecution && execution->submissionSerial &&
            execution->plan.consumer == gpu::upscaling::FsrQualityConsumer(provider) &&
            execution->outcome == gpu::frame_plan::DlssExecutionOutcome::Submitted) {
            const wchar_t* modes[] = {Tr(L"Quality", L"品質"), Tr(L"Balanced", L"平衡"), Tr(L"Performance", L"效能"), L"Native AA"};
            fsrText = std::wstring(L"FSR ") + modes[uint32_t(gpu::upscaling::NormalizeFsrQuality(execution->plan.fsrQuality))] +
                Tr(L" output submitted.", L" 輸出已提交。") + ExecutionSizeSuffix(execution->plan);
            if (edit.upscaler == provider && edit.fsrQuality != execution->plan.fsrQuality)
                fsrText += Tr(L" The selected FSR quality is not applied yet.", L" 選取的 FSR 品質尚未套用。");
        } else {
            fsrText = FsrFallbackSentence(matchingExecution ? execution->reason :
                gpu::frame_plan::DlssEffectReason::AwaitingGpuFrame);
        }
        if (edit.graphicsBackend != running.device.backend)
            fsrText += Tr(L" The selected graphics backend applies after restart.", L" 選取的圖形後端會在重新啟動後套用。");
        if (edit.upscaler != provider)
            fsrText += Tr(L" The selected upscaler is not applied yet.", L" 選取的縮放技術尚未套用。");
        return xess ? WithProvider(std::move(fsrText), name) : fsrText;
    }
    std::wstring text;
    if (running.phase == gpu::frame_plan::DlssEffectPhase::Active && running.execution)
        text = SubmittedSentence(running);
    else if (running.phase == gpu::frame_plan::DlssEffectPhase::Active)
        text = ReasonSentence(gpu::frame_plan::DlssEffectReason::AwaitingGpuFrame);
    else if (const wchar_t *classified = ReasonSentence(running.reason))
        text = classified;
    else
        text = PhaseSentence(running.phase);
    const bool backendPending = edit.graphicsBackend != running.device.backend;
    const bool runningDlss = running.phase != gpu::frame_plan::DlssEffectPhase::Inactive;
    std::optional<gpu::upscaling::DlssQuality> appliedQuality;
    if (running.phase == gpu::frame_plan::DlssEffectPhase::Active)
    {
        if (running.execution)
            appliedQuality = gpu::upscaling::NormalizeDlssQuality(running.execution->plan.dlssQuality);
    }
    else if (runningDlss)
        appliedQuality = gpu::upscaling::NormalizeDlssQuality(running.plannedQuality);
    if (runningDlss && edit.upscaler != gpu::upscaling::Upscaler::Dlss)
        text += std::wstring(L" ") + (edit.upscaler == gpu::upscaling::Upscaler::Fsr ? Tr(L"The FSR choice is not applied yet.", L"FSR 選項尚未套用。") :
            edit.upscaler == gpu::upscaling::Upscaler::Xess ? WithProvider(Tr(L"The FSR choice is not applied yet.", L"FSR 選項尚未套用。"), L"XeSS") :
            Tr(L"The Off choice is not applied yet.", L"關閉選項尚未套用。"));
    else if (!runningDlss && edit.upscaler == gpu::upscaling::Upscaler::Dlss && !backendPending)
        text += std::wstring(L" ") + Tr(L"The DLSS choice is not applied yet.", L"DLSS 選項尚未套用。");
    else if (runningDlss && edit.upscaler == gpu::upscaling::Upscaler::Dlss && appliedQuality &&
             gpu::upscaling::NormalizeDlssQuality(edit.dlssQuality) != *appliedQuality)
        text += std::wstring(L" ") + Tr(L"The selected DLSS quality is not applied yet.", L"選取的 DLSS 品質尚未套用。");
    if (backendPending)
        text += std::wstring(L" ") + BackendPendingSentence(running.device.backend);
    return text;
}
bool GraphicsRowHidden(int r)
{
#if LO_PLATFORM_SWITCH
    // The console owns the display and output size, and settings::Validate pins
    // the Xbox 360 image (720p scene, no host AA, AO, filtering, upscaling, HDR).
    switch (GraphicsRow(r))
    {
    case GraphicsRow::Backend: case GraphicsRow::Gpu: case GraphicsRow::DisplayMode:
    case GraphicsRow::Display: case GraphicsRow::AspectRatio: case GraphicsRow::OutputResolution:
    case GraphicsRow::RenderResolution: case GraphicsRow::ShadowResolution: case GraphicsRow::AntiAliasing:
    case GraphicsRow::DlssQuality: case GraphicsRow::FsrSharpness: case GraphicsRow::DlssNeuralRendering:
    case GraphicsRow::FrameGeneration: case GraphicsRow::FrameGenerationMultiplier:
    case GraphicsRow::AmbientOcclusion: case GraphicsRow::AnisotropicFiltering:
    case GraphicsRow::VariableRefreshRate: case GraphicsRow::Hdr: case GraphicsRow::HdrPaperWhite:
    case GraphicsRow::HdrPeak:
        return true;
    default:
        return false;
    }
#endif
#if LO_PLATFORM_ANDROID
    // Android owns the native surface; the renderer derives aspect from its drawable.
    // NGX and frame generation have no Android providers in this build.
    if (r == int(GraphicsRow::Backend) || r == int(GraphicsRow::Gpu) || r == int(GraphicsRow::DisplayMode) ||
        r == int(GraphicsRow::Display) || r == int(GraphicsRow::AspectRatio) || r == int(GraphicsRow::OutputResolution) ||
        r == int(GraphicsRow::VariableRefreshRate) || r == int(GraphicsRow::FrameGeneration) ||
        r == int(GraphicsRow::FrameGenerationMultiplier))
        return true;
    if (r == int(GraphicsRow::DlssQuality) || r == int(GraphicsRow::FsrSharpness))
        return !graphics_menu::AndroidFsrAvailable || edit.upscaler != gpu::upscaling::Upscaler::Fsr;
#endif
    // A single adapter or display leaves nothing to choose.
    if (r == int(GraphicsRow::Gpu))
        return gpu::video::GpuDeviceNames().size() <= 1;
    if (r == int(GraphicsRow::Display))
        return gpu::video::Displays().size() <= 1;
#ifdef _WIN32
    if (r == int(GraphicsRow::DlssNeuralRendering))
        return edit.upscaler != gpu::upscaling::Upscaler::Dlss;
#else
    // nvngx_dlssnr.dll is Windows-only.
    if (r == int(GraphicsRow::DlssNeuralRendering))
        return true;
#endif
    return (r == int(GraphicsRow::DlssQuality) && edit.upscaler == gpu::upscaling::Upscaler::Off) ||
           (r == int(GraphicsRow::FsrSharpness) && edit.upscaler != gpu::upscaling::Upscaler::Fsr) ||
           (r == int(GraphicsRow::FrameGenerationMultiplier) && edit.frameGenerationProvider != framegen::Provider::Dlss);
}
std::vector<framegen::Provider> FgProviders()
{
    std::vector<framegen::Provider> providers{framegen::Provider::Off};
    for (auto provider : {framegen::Provider::Dlss, framegen::Provider::Fsr, framegen::Provider::MetalFx, framegen::Provider::Xess})
        if (gpu::frame_generation::CompiledProvider(edit.graphicsBackend, provider) || edit.frameGenerationProvider == provider)
            providers.push_back(provider);
    return providers;
}
std::wstring FgNotice()
{
    const auto running = gpu::video::GetFrameGenerationStatus();
    if (gpu::video::SelectedBackend() == GraphicsBackend::D3D12 &&
        running.sessionProvider == framegen::Provider::Dlss &&
        (running.requested == framegen::Provider::Fsr || running.requested == framegen::Provider::Xess))
        return WithProvider(Tr(L"FSR FG requires a restart after DLSS FG. Frame generation is off until then.",
                  L"從 DLSS 影格生成切換到 FSR 影格生成需要重新啟動；在此之前影格生成會關閉。"),
            running.requested == framegen::Provider::Xess ? L"XeSS" : L"FSR");
    std::wstring text;
    using gpu::video::FrameGenerationPhase;
    switch (running.phase) {
    case FrameGenerationPhase::RestartRequired:
        text = Tr(L"Restart to enable Vulkan frame generation.", L"請重新啟動以啟用 Vulkan 影格生成。"); break;
    case FrameGenerationPhase::Pending:
        text = Tr(L"Applying FG settings…", L"正在套用影格生成設定……"); break;
    case FrameGenerationPhase::Ready:
        text = running.applied == framegen::Provider::Dlss ? L"DLSS" :
            running.applied == framegen::Provider::MetalFx ? L"MetalFX" :
            running.applied == framegen::Provider::Xess ? L"XeSS" : L"FSR";
        text += Tr(L" FG ready. Generation depends on the current scene.", L" 影格生成已就緒，是否補幀取決於目前場景。"); break;
    case FrameGenerationPhase::Unavailable:
        text = Tr(L"FG is unavailable for this request. Normal rendering is in use.",
                  L"目前的影格生成設定無法使用，正在使用常規渲染。"); break;
    default:
        text = Tr(L"Frame generation is off.", L"影格生成已關閉。"); break;
    }
    if (running.environmentOverride)
        text += Tr(L" A diagnostic override controls FG.", L" 影格生成由診斷覆寫控制。");
    return text;
}
static_assert(int(GraphicsRow::Save) + 1 == int(GraphicsRow::Count));
// Only Graphics scrolls; the other tabs fit the visible rows (menu.h layout rules).
static_assert(GameRowCount <= kMenuVisibleRows && AudioRowCount <= kMenuVisibleRows && SystemRowCount <= kMenuVisibleRows);
void Publish(uint8_t *base, uint32_t config)
{
#if LO_PLATFORM_ANDROID
    // The first graphics ids are hidden; enter the tab on a visible row.
    if (tab == 2)
        for (int i = 0; i < int(GraphicsRow::Count) && GraphicsRowHidden(row); ++i)
            row = (row + 1) % int(GraphicsRow::Count);
#endif
    Snapshot next;
    next.tab = tab;
    next.row = row;
    next.language = edit.uiLanguage;
    next.calibration = MakeHdrCalibration(edit, calibrationOpen.load());
    next.brightness = MakeBrightnessCalibration(edit, brightnessOpen.load());
    next.neuralRendering = MakeNeuralRenderingTuning(edit, nrOpen.load());
    const uint32_t flags = PPC_LOAD_U32(config + 4);
    auto makeChoices = [&](const wchar_t *en, const wchar_t *zh, std::vector<std::wstring> choices,
                           uint32_t selected, bool enabled = true) {
        selected = choices.empty() ? 0 : std::min(selected, uint32_t(choices.size() - 1));
        const std::wstring value = choices.empty() ? std::wstring{} : choices[selected];
        return Row{Tr(en, zh), value, enabled, std::move(choices), int(selected)};
    };
    auto addChoices = [&](const wchar_t *en, const wchar_t *zh, std::vector<std::wstring> choices,
                          uint32_t selected, bool enabled = true) {
        next.rows.push_back(makeChoices(en, zh, std::move(choices), selected, enabled));
    };
    auto addAction = [&](const wchar_t *en, const wchar_t *zh, const wchar_t *value) {
        addChoices(en, zh, {value}, 0);
    };
    auto addSlider = [&](const wchar_t *en, const wchar_t *zh, uint32_t percent) {
        next.rows.push_back({Tr(en, zh), std::to_wstring(percent) + L"%", true, {}, 0,
                             int(std::min(percent, 100u))});
    };
    auto onOff = [&]() {
        return std::vector<std::wstring>{Tr(L"On", L"開"), Tr(L"Off", L"關")};
    };
    if (tab == 0)
    {
        addChoices(L"Text speed", L"文字速度", {Tr(L"Fast", L"快"), Tr(L"Normal", L"正常"),
                                                    Tr(L"Slow", L"慢")},
                   std::min(PPC_LOAD_U32(config), 2u));
        addChoices(L"Captions", L"字幕", onOff(), (flags & 0x40000000) ? 0 : 1);
        addChoices(L"Remember battle cursor", L"記住戰鬥游標", onOff(),
                   (flags & 0x10000000) ? 0 : 1);
        addChoices(L"Automatic back-row input", L"後排自動輸入", onOff(),
                   (flags & 0x00800000) ? 0 : 1);
        addChoices(L"Invert camera vertically", L"反轉鏡頭上下", onOff(),
                   (flags & 0x08000000) ? 0 : 1);
        addChoices(L"Invert camera horizontally", L"反轉鏡頭左右", onOff(),
                   (flags & 0x04000000) ? 0 : 1);
        addChoices(L"Confirmation button", L"確認按鍵", {L"A / B", L"B / A"},
                   (flags & 0x02000000) ? 1 : 0);
        next.rows.back().controllerButtons = true;
        addChoices(L"Button prompts", L"按鍵提示", {Tr(L"Auto", L"自動"), L"Xbox", L"PlayStation"},
                   std::min(edit.buttonPrompts, 2u));
        addSlider(L"Vibration", L"震動", edit.vibrationPercent);
        addAction(L"Restore game defaults", L"恢復遊戲預設設定", Tr(L"Restore", L"恢復"));
        addAction(L"Quit to Main Menu", L"退出到主選單", Tr(L"Return", L"返回"));
    }
    else if (tab == 1)
    {
        std::vector<std::wstring> voices;
        for (uint32_t i = 0; i < VoiceCount(base); ++i)
            voices.emplace_back(VoiceName(base, i));
        const auto index = PPC_LOAD_U32(config + 24);
        const bool valid = index < voices.size();
        addChoices(L"Voice language", L"語音語言", std::move(voices), index, valid);
        if (!valid)
        {
            next.rows.back().value = L"—";
            next.rows.back().choices.clear();
            next.rows.back().selectedChoice = -1;
        }
        addSlider(L"Music", L"音樂音量", PPC_LOAD_U32(config + 8));
        addSlider(L"Sound effects", L"音效音量", PPC_LOAD_U32(config + 12));
        addChoices(L"Audio output", L"音訊輸出", {Tr(L"Stereo", L"立體聲"), Tr(L"5.1 surround", L"5.1 環繞聲")},
                   edit.audioOutput);
    }
    else if (tab == 2)
    {
        next.rows.resize(int(GraphicsRow::Count));
        auto placeGraphics = [&](GraphicsRow id, Row value) {
            value.hidden = GraphicsRowHidden(int(id));
            next.rows[int(id)] = std::move(value);
        };
#ifdef _WIN32
        placeGraphics(GraphicsRow::Backend, makeChoices(L"Graphics backend", L"圖形後端", {L"Direct3D 12", L"Vulkan", Tr(L"Direct3D 11 (unsupported)", L"Direct3D 11（尚未支援）")},
                   uint32_t(edit.graphicsBackend)));
#elif LO_PLATFORM_MACOS
        placeGraphics(GraphicsRow::Backend, makeChoices(L"Graphics backend", L"圖形後端", {L"Metal"}, 0));
#else
        placeGraphics(GraphicsRow::Backend, makeChoices(L"Graphics backend", L"圖形後端", {L"Vulkan"}, 0));
#endif
        {
            const auto gpus = GpuChoices();
            std::vector<std::wstring> labels{Tr(L"Automatic", L"自動")};
            for (const auto& name : gpus.names) labels.push_back(Widen(name));
            auto gpuRow = makeChoices(L"GPU", L"GPU", std::move(labels), gpus.selected);
            gpuRow.singleValue = true;
            placeGraphics(GraphicsRow::Gpu, std::move(gpuRow));
        }
        placeGraphics(GraphicsRow::DisplayMode, makeChoices(L"Display mode", L"顯示模式",
                   {Tr(L"Windowed", L"視窗"), Tr(L"Fullscreen", L"全螢幕")},
                   uint32_t(edit.windowMode)));
        {
            auto displays = DisplayChoices();
            auto displayRow = makeChoices(L"Display", L"顯示器", std::move(displays.labels), displays.selected);
            displayRow.singleValue = true;
            placeGraphics(GraphicsRow::Display, std::move(displayRow));
        }
        placeGraphics(GraphicsRow::AspectRatio, makeChoices(L"Aspect ratio", L"畫面比例",
                   {Tr(L"Auto", L"自動"), L"16:9", L"21:9", L"4:3"}, uint32_t(edit.aspectRatio)));
        std::vector<std::wstring> outputChoices;
        uint32_t outputChoice = 0;
        const auto resolutions = OutputResolutions(edit.aspectRatio, edit.width, edit.height);
        for (uint32_t i = 0; i < resolutions.size(); ++i)
        {
            outputChoices.push_back(std::to_wstring(resolutions[i][0]) + L" × " + std::to_wstring(resolutions[i][1]));
            if (edit.width == resolutions[i][0] && edit.height == resolutions[i][1]) outputChoice = i;
        }
        placeGraphics(GraphicsRow::OutputResolution, makeChoices(L"Output resolution", L"輸出解析度", std::move(outputChoices), outputChoice));
        std::vector<std::wstring> renderChoices;
        for (const int height : graphics_menu::RenderResolutions)
            renderChoices.push_back(height == 0 ? Tr(L"Follow output", L"跟隨輸出") :
                height == InternalResolutionNative ? Tr(L"Native (Retina)", L"原生（Retina）") :
                std::to_wstring(height) + L"p");
        placeGraphics(GraphicsRow::RenderResolution, makeChoices(L"Render resolution", L"渲染解析度",
                   std::move(renderChoices), graphics_menu::RenderResolutionChoice(edit)));
        placeGraphics(GraphicsRow::ShadowResolution, makeChoices(L"Shadow resolution", L"陰影解析度",
                   {L"1×", L"2×", L"4×"}, graphics_menu::ShadowResolutionChoice(edit)));
        placeGraphics(GraphicsRow::DynamicShadows, makeChoices(L"Dynamic shadows", L"動態陰影", onOff(), edit.dynamicShadows ? 0 : 1));
#if LO_PLATFORM_MACOS
        std::vector<std::wstring> aaChoices{Tr(L"Off", L"關"), L"FXAA", L"SMAA", L"TAA", L"MetalFX Temporal"};
#elif LO_PLATFORM_ANDROID
        std::vector<std::wstring> aaChoices{Tr(L"Off", L"關"), L"FXAA", L"SMAA", L"TAA"};
        if (graphics_menu::AndroidFsrAvailable) aaChoices.emplace_back(L"FSR 3.1");
#else
        std::vector<std::wstring> aaChoices{Tr(L"Off", L"關"), L"FXAA", L"SMAA", L"TAA", L"DLSS", L"FSR 3.1", L"XeSS"};
#endif
        aaChoices.resize(graphics_menu::AaChoiceCount);
        placeGraphics(GraphicsRow::AntiAliasing, makeChoices(L"Anti-aliasing / Upscaling", L"抗鋸齒 / 超解析度",
                   std::move(aaChoices), std::min(graphics_menu::AaChoice(edit), graphics_menu::AaChoiceCount - 1)));
        placeGraphics(GraphicsRow::AmbientOcclusion, makeChoices(L"Ambient occlusion", L"環境光遮蔽",
                   {Tr(L"Off", L"關"), L"SSAO", L"GTAO"}, std::min(edit.ambientOcclusion, 2u)));
        // FSR and MetalFX share the FSR quality ratios and IDs.
        const bool savedFsr = gpu::upscaling::UsesFsrQuality(edit.upscaler);
        const bool savedMetalFx = edit.upscaler == gpu::upscaling::Upscaler::MetalFx;
        const bool savedXess = edit.upscaler == gpu::upscaling::Upscaler::Xess;
        auto dlssQuality = makeChoices(savedMetalFx ? L"MetalFX quality" : savedXess ? L"XeSS quality" : savedFsr ? L"FSR quality" : L"DLSS quality",
                   savedMetalFx ? L"MetalFX 品質" : savedXess ? L"XeSS 品質" : savedFsr ? L"FSR 品質" : L"DLSS 品質",
                   {Tr(L"Performance", L"效能"), Tr(L"Balanced", L"平衡"), Tr(L"Quality", L"品質"), savedFsr ? L"Native AA" : L"DLAA"},
                   QualityMenuIndex(savedFsr ? uint32_t(edit.fsrQuality) : uint32_t(edit.dlssQuality)));
        // Hidden instead of removed so this logical id stays stable for input, drawing and hit-testing.
        dlssQuality.hidden = GraphicsRowHidden(int(GraphicsRow::DlssQuality));
        placeGraphics(GraphicsRow::DlssQuality, std::move(dlssQuality));
        // Reuse the existing many-choice control: it shows the selected percentage
        // between arrows without adding another renderer layout or shifting row IDs.
        std::vector<std::wstring> sharpnessChoices;
        sharpnessChoices.reserve(101);
        sharpnessChoices.emplace_back(Tr(L"Off", L"關"));
        for (uint32_t percent = 1; percent <= 100; ++percent)
            sharpnessChoices.push_back(std::to_wstring(percent) + L"%");
        auto fsrSharpness = makeChoices(L"FSR sharpness", L"FSR 銳化", std::move(sharpnessChoices),
                                        std::min(edit.fsrSharpnessPercent, 100u));
        fsrSharpness.hidden = GraphicsRowHidden(int(GraphicsRow::FsrSharpness));
        placeGraphics(GraphicsRow::FsrSharpness, std::move(fsrSharpness));
        auto neuralRendering = makeChoices(L"DLSS 5 neural rendering", L"DLSS 5 神經渲染",
                   {Tr(L"Off", L"關"), L"1×", L"2×", L"3×", L"4×"},
                   std::min(edit.dlssNeuralRendering, DlssNeuralRenderingMaxPasses));
        neuralRendering.hidden = GraphicsRowHidden(int(GraphicsRow::DlssNeuralRendering));
        placeGraphics(GraphicsRow::DlssNeuralRendering, std::move(neuralRendering));
        const uint32_t afChoice = edit.anisotropicFiltering == 16 ? 4 : edit.anisotropicFiltering == 8 ? 3 :
                                  edit.anisotropicFiltering == 4 ? 2 : edit.anisotropicFiltering == 2 ? 1 : 0;
        placeGraphics(GraphicsRow::AnisotropicFiltering, makeChoices(L"Anisotropic filtering", L"各向異性過濾",
                   {Tr(L"Off", L"關"), L"2×", L"4×", L"8×", L"16×"}, afChoice));
        std::vector<std::wstring> dofChoices{Tr(L"Off", L"關")};
        for (uint32_t percent = 10; percent <= 100; percent += 10)
            dofChoices.push_back(std::to_wstring(percent) + L"%");
        placeGraphics(GraphicsRow::DepthOfField, makeChoices(L"Depth of field", L"景深", std::move(dofChoices),
                   (std::min(edit.depthOfFieldPercent, 100u) + 5) / 10));
        placeGraphics(GraphicsRow::Bloom, makeChoices(L"Bloom", L"光暈", onOff(), edit.bloom ? 0 : 1));
        placeGraphics(GraphicsRow::MotionBlur, makeChoices(L"Motion blur", L"動態模糊", onOff(), edit.motionBlur ? 0 : 1));
        std::vector<std::wstring> cullingChoices;
        for (uint32_t percent = 0; percent <= 200; percent += 10)
            cullingChoices.push_back(std::to_wstring(percent) + L"%");
        placeGraphics(GraphicsRow::Culling, makeChoices(L"Culling", L"剔除", std::move(cullingChoices),
                   (std::min(edit.cullingPercent, 200u) + 5) / 10));
#if LO_PLATFORM_MACOS
        placeGraphics(GraphicsRow::ScalingQuality, makeChoices(L"Scaling filter", L"縮放濾鏡",
                   {Tr(L"Standard", L"標準"), Tr(L"High", L"高"), L"MetalFX"},
                   std::min(edit.scalingQuality, ScalingMetalFx)));
#else
        placeGraphics(GraphicsRow::ScalingQuality, makeChoices(L"Scaling filter", L"縮放濾鏡",
                   {Tr(L"Standard", L"標準"), Tr(L"High", L"高")},
                   std::min(edit.scalingQuality, 1u)));
#endif
        placeGraphics(GraphicsRow::RgbRange, makeChoices(L"RGB Range", L"RGB 範圍",
                   {Tr(L"Off", L"關"), Tr(L"Expanded", L"擴展")}, edit.expandRgbRange ? 1 : 0));
        std::vector<std::wstring> frameRates;
        for (const auto fps : gpu::frame_rate::kNativeRates)
            frameRates.push_back(std::to_wstring(fps) + L" FPS");
        placeGraphics(GraphicsRow::FrameRate, makeChoices(L"Frame rate", L"影格率",
                   std::move(frameRates), gpu::frame_rate::MenuIndex(edit.frameRate)));
#if LO_PLATFORM_MACOS
        placeGraphics(GraphicsRow::VariableRefreshRate, makeChoices(L"Adaptive sync (ProMotion)",
            L"自適應同步（ProMotion）", onOff(), edit.variableRefreshRate ? 0 : 1));
#else
        placeGraphics(GraphicsRow::VariableRefreshRate, makeChoices(L"FreeSync / G-SYNC Compatible",
            L"FreeSync / G-SYNC Compatible", onOff(), edit.variableRefreshRate ? 0 : 1));
#endif
        std::vector<std::wstring> providers;
        uint32_t selected = 0;
        for (auto provider : FgProviders()) {
            if (provider == edit.frameGenerationProvider) selected = uint32_t(providers.size());
            providers.emplace_back(provider == framegen::Provider::Off ? Tr(L"Off", L"關") :
                provider == framegen::Provider::Dlss ? L"DLSS" : provider == framegen::Provider::MetalFx ? L"MetalFX" :
                provider == framegen::Provider::Xess ? L"XeSS" : L"FSR");
        }
        auto frameGeneration = makeChoices(L"Frame generation", L"影格生成", std::move(providers), selected);
        frameGeneration.hidden = GraphicsRowHidden(int(GraphicsRow::FrameGeneration));
        placeGraphics(GraphicsRow::FrameGeneration, std::move(frameGeneration));
        std::vector<std::wstring> multipliers;
        for (uint32_t multiplier = 2; multiplier <= framegen::kMaxMultiplier; ++multiplier)
            multipliers.push_back(std::to_wstring(multiplier) + L"×");
        auto fgMultiplier = makeChoices(L"FG multiplier", L"影格生成倍數", std::move(multipliers),
            std::clamp(edit.frameGenerationMultiplier, 2u, framegen::kMaxMultiplier) - 2);
        fgMultiplier.hidden = GraphicsRowHidden(int(GraphicsRow::FrameGenerationMultiplier));
        placeGraphics(GraphicsRow::FrameGenerationMultiplier, std::move(fgMultiplier));
        const bool hdrAvailable = graphics_menu::HdrAvailable(edit.graphicsBackend);
        placeGraphics(GraphicsRow::Hdr, makeChoices(L"HDR output", L"HDR 輸出", onOff(), edit.hdr ? 0 : 1, hdrAvailable));
        auto hdrLevel = [&](const wchar_t *en, const wchar_t *zh, uint32_t nits) {
            const auto value = std::to_wstring(nits) + L" nits";
            return Row{Tr(en, zh), value, hdrAvailable, {L"◀", value, L"▶"}, 1};
        };
        placeGraphics(GraphicsRow::HdrPaperWhite, hdrLevel(L"HDR paper white", L"HDR 參考白位", edit.hdrPaperWhiteNits));
        const auto peak = next.calibration;
        const std::wstring peakValue = peak.automatic
            ? Tr(L"Auto", L"自動") + std::wstring(L" (") + std::to_wstring(peak.effectiveNits) + L" nits)"
            : std::to_wstring(peak.manualNits) + L" nits";
        placeGraphics(GraphicsRow::HdrPeak,
            Row{Tr(L"HDR peak brightness", L"HDR 最高亮度"), peakValue, hdrAvailable,
                {L"◀", peakValue, L"▶"}, 1});
        placeGraphics(GraphicsRow::Brightness, makeChoices(L"Brightness / Gamma", L"亮度 / Gamma",
                   {BrightnessValue(edit.displayBrightness) + L" · " + GammaValue(edit.displayGamma)}, 0));
        placeGraphics(GraphicsRow::Save, makeChoices(L"Save graphics settings", L"儲存圖形設定", {Tr(L"Save", L"儲存")}, 0));
    }
    else
    {
        std::vector<std::wstring> uiLanguages(std::begin(UiLanguageNames), std::end(UiLanguageNames));
        addChoices(L"Settings language", L"設定界面語言", std::move(uiLanguages), edit.uiLanguage);
        std::vector<std::wstring> gameLanguages;
        for (const auto name : GameLanguageNames) gameLanguages.emplace_back(name);
        addChoices(L"Game language", L"遊戲語言", std::move(gameLanguages), GameLanguageIndex(edit.gameLanguage));
        addChoices(L"Automatic updates", L"自動更新", onOff(), edit.automaticUpdates ? 0 : 1);
        addChoices(L"Debug log", L"除錯日誌", onOff(), edit.debugLog ? 0 : 1);
        const bool logUpload = os::log_collection::Supported();
        next.rows.push_back({os::log_collection::Label(edit.uiLanguage),
            !logUpload ? Tr(L"Windows only", L"僅限 Windows") : os::log_collection::Enabled() ? Tr(L"On", L"開") : Tr(L"Off", L"關"),
            logUpload, {}, 0});
        addAction(L"Import discs & DLC", L"匯入光碟與 DLC", Tr(L"Open", L"開啟"));
        addAction(L"Save settings", L"儲存設定", Tr(L"Save", L"儲存"));
    }
    // Keep the focused row inside the visible window. Scroll persists per tab
    // so returning to a long list restores its position.
    if (tab >= 0 && tab < MenuTabCount)
    {
        static int scrollByTab[MenuTabCount] = {};
        int visibleCount = 0, visibleFocus = 0;
        for (size_t i = 0; i < next.rows.size(); ++i)
        {
            if (next.rows[i].hidden) continue;
            if (int(i) <= row) visibleFocus = visibleCount;
            ++visibleCount;
        }
        int &scroll = scrollByTab[tab];
        scroll = std::clamp(scroll, 0, std::max(0, visibleCount - kMenuVisibleRows));
        if (visibleFocus < scroll) scroll = visibleFocus;
        if (visibleFocus >= scroll + kMenuVisibleRows) scroll = visibleFocus - kMenuVisibleRows + 1;
        next.scroll = scroll;
    }
    next.help = status.empty() ? Tr(L"LB / RB: category     D-pad: select / change     A: confirm     B: back",
                                    L"LB / RB：分類     方向鍵：選擇 / 調整     A：確認     B：返回")
                               : status;
    if (status.empty() && (flags & 0x02000000))
        next.help = Tr(L"LB / RB: category     D-pad: select / change     B: confirm     A: back",
                       L"LB / RB：分類     方向鍵：選擇 / 調整     B：確認     A：返回");
    if (tab == 0 && row == GameVibrationRow)
        next.help = Tr(L"Controller vibration strength. Min turns it off. Applies immediately.",
                       L"控制器震動強度。調到最小即關閉。立即套用。");
    if (tab == 3 && row == SystemGameLanguageRow)
        next.help = Tr(L"Game language takes effect after restarting. Requires matching language assets.",
                       L"遊戲語言重新啟動後生效，需要對應語言資源。中文遊戲文本需要亞洲版資源。");
    if (tab == 0 && row == GamePromptRow)
        next.help = Tr(L"Which button icons the game shows. Auto follows the controller you use.",
                       L"遊戲顯示的按鍵圖示。自動會跟隨你使用的控制器。");
    if (tab == 3 && row == SystemDebugLogRow)
        next.help = Tr(L"Writes every message to the log in logs/. Turn it on when you report a problem. Applies when saved.",
                       L"把所有訊息寫入 logs/ 中的日誌。回報問題時請開啟。儲存後生效。");
    if (tab == 3 && row == SystemImportRow)
        next.help = Tr(L"Close the game to import selected discs or DLC again. Other content and saves stay intact.",
                       L"關閉遊戲並重新匯入所選光碟或 DLC；其他內容與存檔保留。");
    if (tab == 1 && row == AudioOutputRow && status.empty())
        next.help = edit.audioOutput == AudioOutputSurround && apu::OutputChannels() == 2
#ifdef _WIN32
            // The speaker layout is only in the classic Sound control panel.
            ? Tr(L"Windows reports a stereo device. Open Control Panel → Sound → Playback, select the device, click Configure and choose 5.1 or 7.1 Surround, then select 5.1 again.",
                 L"Windows 將此裝置視為立體聲。開啟 控制台 → 音效 → 播放，選取裝置，按「設定」並選擇 5.1 或 7.1 環繞，再重新選擇 5.1。")
#else
            ? Tr(L"The output device is not set to 5.1, so the stereo mix is in use. Set the system speakers to 5.1 or 7.1 and select 5.1 again.",
                 L"輸出裝置未設定為 5.1，正在使用立體聲混音。請將系統喇叭設定為 5.1 或 7.1 後重新選擇 5.1。")
#endif
            : Tr(L"5.1 sends the game's surround mix to a 5.1 or 7.1 speaker setup. Applies immediately.",
                 L"5.1 會將遊戲的環繞聲混音輸出到 5.1 或 7.1 喇叭，立即套用。");
    if (tab == 2)
    {
        switch (GraphicsRow(row))
        {
        case GraphicsRow::Backend: {
            next.help = Tr(L"The graphics backend is changed after restarting. LO_GRAPHICS_API remains a diagnostic override.",
                           L"圖形後端重新啟動後變更；LO_GRAPHICS_API 仍可作為診斷覆寫。 ");
            const auto selected = gpu::video::SelectedBackend();
            next.help += Tr(L" Running: ", L" 目前使用：");
            next.help += selected == gpu::backend::Backend::Vulkan ? L"Vulkan" :
                selected == gpu::backend::Backend::D3D12 ? L"Direct3D 12" :
                selected == gpu::backend::Backend::Metal ? L"Metal" : L"-";
            break;
        }
        case GraphicsRow::Gpu: {
            next.help = Tr(L"Applies after restarting. Lists the GPUs of the running graphics backend.",
                           L"重新啟動後套用。列出目前圖形後端的 GPU。");
            next.help += Tr(L" Running: ", L" 目前使用：");
            const auto active = gpu::video::ActiveGpuDeviceName();
            next.help += active.empty() ? std::wstring(L"-") : Widen(active);
            break;
        }
        case GraphicsRow::Display:
            next.help = Tr(L"Moves the game window to this display when saved; fullscreen uses it too. Automatic leaves the window where it is.",
                           L"儲存後將遊戲視窗移到這台顯示器，全螢幕也會使用它。自動則讓視窗留在原處。");
            break;
        case GraphicsRow::AspectRatio:
            next.help = Tr(L"Auto fills the window. 16:9, 21:9 and 4:3 keep that shape with black bars; frame generation is off while bars show. Applies immediately after saving.",
                           L"自動會填滿視窗。16:9、21:9 與 4:3 保持該比例並加上黑邊；有黑邊時影格生成關閉。儲存後立即套用。");
            break;
        case GraphicsRow::OutputResolution:
            next.help = Tr(L"Sets the output size. Fullscreen uses the desktop size.",
                           L"設定輸出尺寸；全螢幕使用桌面尺寸。");
            break;
        case GraphicsRow::RenderResolution:
#if LO_PLATFORM_MACOS
            next.help = Tr(L"Scene resolution before scaling. Follow output uses the window size in points; Native uses every Retina pixel (4x the work).",
                           L"縮放前的場景解析度。跟隨輸出使用視窗的點尺寸；原生使用全部 Retina 像素（4 倍工作量）。");
#else
            next.help = Tr(L"Scene resolution before scaling to the output. Follow output matches the output size.",
                           L"縮放至輸出前的場景解析度。跟隨輸出與輸出尺寸相同。");
#endif
            next.help += Tr(L" With an upscaler, a value above the output supersamples: the upscaler outputs at it and the image is scaled down.",
                            L" 開啟縮放器時，高於輸出的值會超取樣：縮放器以此解析度輸出，再縮小顯示。");
            break;
        case GraphicsRow::ShadowResolution:
            next.help = Tr(L"Shadow-map resolution multiplier. Higher values need more GPU memory and rendering time. Applies after saving.",
                           L"陰影貼圖解析度倍數。較高倍數需要更多 GPU 記憶體與渲染時間。儲存後套用。");
            break;
        case GraphicsRow::DynamicShadows:
            next.help = Tr(L"Real-time shadows cast by characters and objects. Off removes them and can raise the frame rate. Applies immediately after saving.",
                           L"角色與物件投射的即時陰影。關閉後陰影消失，可提升影格率。儲存後立即套用。");
            break;
        case GraphicsRow::AntiAliasing:
#if LO_PLATFORM_ANDROID
            if (graphics_menu::AndroidFsrAvailable && edit.upscaler == gpu::upscaling::Upscaler::Fsr)
                next.help = Tr(L"FSR 3.1 needs D3D12 or Vulkan and an FSR-enabled build. Unsupported scenes use normal rendering.",
                              L"FSR 3.1 需要 D3D12 或 Vulkan 與包含 FSR 的版本。不支援的場景使用常規渲染。");
            else if (graphics_menu::AaChoice(edit) == 3)
                next.help = Tr(L"Camera-based TAA; moving effects may trail. Unsupported scenes use SMAA.",
                              L"以相機重投影的 TAA；動態特效可能拖影。不支援的場景使用 SMAA。");
#else
            if (edit.upscaler == gpu::upscaling::Upscaler::MetalFx)
                next.help = Tr(L"Apple's temporal upscaler: renders the scene below the output size and reconstructs detail. Menus and transitions use normal rendering.",
                              L"Apple 的時間性縮放：以低於輸出的解析度渲染場景並重建細節。選單和過場使用常規渲染。");
            else if (edit.upscaler == gpu::upscaling::Upscaler::Fsr)
                next.help = Tr(L"FSR 3.1 needs D3D12 or Vulkan and an FSR-enabled build. Unsupported scenes use normal rendering.",
                              L"FSR 3.1 需要 D3D12 或 Vulkan 與包含 FSR 的版本。不支援的場景使用常規渲染。");
            else if (edit.upscaler == gpu::upscaling::Upscaler::Xess)
                next.help = Tr(L"Intel XeSS needs Direct3D 12 and an XeSS-enabled build. Unsupported scenes use normal rendering.",
                              L"Intel XeSS 需要 Direct3D 12 與包含 XeSS 的版本。不支援的場景使用常規渲染。");
            else if (edit.upscaler == gpu::upscaling::Upscaler::Dlss)
                next.help = Tr(L"Saves the DLSS preference. The status line shows the latest DLSS result.",
                              L"儲存 DLSS 偏好。狀態列顯示最新的 DLSS 結果。");
            else if (edit.antialiasing == 3)
                next.help = Tr(L"Camera-based TAA; moving effects may trail. Unsupported scenes use SMAA.",
                              L"以相機重投影的 TAA；動態特效可能拖影。不支援的場景使用 SMAA。");
#endif
            break;
        case GraphicsRow::AmbientOcclusion:
            next.help = Tr(L"Screen-space ambient occlusion adds contact shading. Applies after saving.",
                           L"螢幕空間環境光遮蔽可加強接觸處的陰影。儲存後套用。");
            break;
        case GraphicsRow::DlssQuality:
            next.help = gpu::upscaling::UsesFsrQuality(edit.upscaler) ?
                Tr(L"Performance, Balanced, Quality, or Native AA. Native AA keeps the output resolution.", L"效能、平衡、品質或 Native AA。Native AA 維持輸出解析度。") : Tr(L"Performance, Balanced, Quality, or DLAA. The status line shows the submitted mode.",
                           L"效能、平衡、品質或 DLAA。狀態列顯示已提交的模式。");
            break;
        case GraphicsRow::FsrSharpness:
            next.help = Tr(L"FSR sharpening: Off disables RCAS; 1-100% sets sharpening strength.",
                           L"FSR 銳化：關閉會停用 RCAS；1-100% 調整銳化強度。");
            break;
        case GraphicsRow::DlssNeuralRendering:
        {
            next.help = Tr(L"NVIDIA's DLSS 5 neural rendering on the DLSS image. 1×-4× runs it that many times: stronger, but each pass costs frame rate. Needs an RTX GPU and nvngx_dlssnr.dll next to the game (not included). Applies after saving.",
                           L"在 DLSS 畫面上執行 NVIDIA 的 DLSS 5 神經渲染。1×-4× 為執行次數：次數越多效果越強，但每次都會降低影格率。需要 RTX 顯示卡，並將 nvngx_dlssnr.dll 放在遊戲旁（不隨附）。儲存後套用。");
            next.help = next.help + L" " + Tr(L"Confirm opens the tuning page.", L"按確認開啟調整頁面。");
            using gpu::dlss::NeuralRenderingState;
            const auto state = gpu::dlss::g_neuralRenderingState.load(std::memory_order_relaxed);
            const wchar_t *status =
                state == NeuralRenderingState::Active ? Tr(L"Running.", L"執行中。") :
                state == NeuralRenderingState::MissingRuntime ? Tr(L"nvngx_dlssnr.dll is missing or unusable.", L"找不到 nvngx_dlssnr.dll 或無法使用。") :
                state == NeuralRenderingState::Unsupported ? Tr(L"This GPU or driver cannot run it.", L"此顯示卡或驅動程式無法執行。") :
                state == NeuralRenderingState::Failed ? Tr(L"It stopped after an error; see the log.", L"發生錯誤後已停止，請查看記錄檔。") : nullptr;
            if (status) next.help = next.help + L" " + status;
            break;
        }
        case GraphicsRow::AnisotropicFiltering:
            next.help = Tr(L"Improves texture clarity at oblique viewing angles. Changes apply immediately after saving.",
                           L"提升斜角觀看時的紋理清晰度。儲存後立即套用。");
            break;
        case GraphicsRow::DepthOfField:
            next.help = Tr(L"Strength of the game's depth-of-field blur. 100% is the original look; Off keeps distant scenery sharp. Applies immediately after saving.",
                           L"遊戲景深模糊的強度。100% 為原版效果；關閉後遠景保持清晰。儲存後立即套用。");
            break;
        case GraphicsRow::Bloom:
            next.help = Tr(L"The game's glow around bright areas. Off removes it and the image gets slightly darker. Applies immediately after saving.",
                           L"遊戲中亮部周圍的光暈。關閉後光暈消失，畫面會稍暗。儲存後立即套用。");
            break;
        case GraphicsRow::MotionBlur:
            next.help = Tr(L"The game's blur during fast camera and character movement. Off keeps moving scenes sharp. Applies immediately after saving.",
                           L"遊戲在鏡頭與角色快速移動時的模糊。關閉後移動畫面保持清晰。儲存後立即套用。");
            break;
        case GraphicsRow::Culling:
            next.help = Tr(L"How soon the game stops drawing characters and objects at the screen edges. 100% is the original; lower keeps them until they are fully off screen but costs frame rate, higher hides them sooner. Applies immediately after saving.",
                           L"遊戲在畫面邊緣停止繪製角色與物件的時機。100% 為原版；數值越低越晚隱藏，直到完全離開畫面，但會降低影格率；數值越高越早隱藏。儲存後立即套用。");
            break;
        case GraphicsRow::ScalingQuality:
#if LO_PLATFORM_MACOS
            next.help = Tr(L"Controls filtering when upscaling is active. MetalFX uses Apple's spatial upscaler; pair it with a lower render resolution.",
                           L"控制啟用縮放時的取樣濾鏡。MetalFX 使用 Apple 的空間放大器，建議搭配較低的渲染解析度。");
#else
            next.help = Tr(L"Controls filtering when upscaling is active.",
                           L"控制啟用縮放時的取樣濾鏡。");
#endif
            break;
        case GraphicsRow::RgbRange:
            next.help = Tr(L"Expands only the game image from RGB 16–235 to 0–255. Applies immediately after saving.",
                           L"僅將遊戲畫面從 RGB 16–235 擴展到 0–255。儲存後立即套用。");
            break;
        case GraphicsRow::FrameRate:
            next.help = edit.frameRate > 60
                ? Tr(L"90/120 FPS render real game frames. No extra flag is needed. Verify speed, audio and battle timing.",
                     L"90/120 FPS 渲染真實遊戲影格，無需額外開關。請確認遊戲速度、音訊與戰鬥時序。")
                : Tr(L"Native game-frame target, independent of frame generation. Applies after saving.",
                     L"原生遊戲影格率，獨立於影格生成。儲存後套用。");
            break;
        case GraphicsRow::VariableRefreshRate:
#if LO_PLATFORM_MACOS
            next.help = Tr(L"Shows each frame for one game frame so ProMotion and adaptive-sync displays follow the game's frame rate. Display sync stays on.",
                           L"每個影格至少顯示一個遊戲影格的時間，讓 ProMotion 與自適應同步螢幕跟隨遊戲影格率；顯示同步保持開啟。");
#else
            next.help = Tr(L"VRR-friendly pacing. Enable adaptive sync in your display/driver. Actual VRR is not detected.",
                           L"VRR 友善限幀；請在螢幕與驅動程式啟用自適應同步。無法偵測實際 VRR 狀態。");
#endif
            break;
        case GraphicsRow::FrameGeneration:
        case GraphicsRow::FrameGenerationMultiplier:
        {
            const auto compiled = [](GraphicsBackend backend, framegen::Provider provider) {
                return gpu::frame_generation::CompiledProvider(backend, provider);
            };
            const bool dlss = compiled(edit.graphicsBackend, framegen::Provider::Dlss);
            const bool fsr = compiled(edit.graphicsBackend, framegen::Provider::Fsr);
            const bool xess = compiled(edit.graphicsBackend, framegen::Provider::Xess);
            const bool d3d12 = compiled(GraphicsBackend::D3D12, framegen::Provider::Dlss) ||
                compiled(GraphicsBackend::D3D12, framegen::Provider::Fsr) ||
                compiled(GraphicsBackend::D3D12, framegen::Provider::Xess);
            if (!dlss && !fsr && !xess && !compiled(edit.graphicsBackend, framegen::Provider::MetalFx))
                next.help = edit.graphicsBackend != GraphicsBackend::D3D12 && d3d12
                    ? Tr(L"FG requires Direct3D 12. Change the graphics backend and restart first.",
                         L"影格生成需要 Direct3D 12。請先變更圖形後端並重新啟動。")
                    : Tr(L"Frame generation is unavailable for this backend in this build.", L"此版本的目前圖形後端未包含影格生成功能。");
            else if (GraphicsRow(row) == GraphicsRow::FrameGenerationMultiplier)
                next.help = Tr(L"Includes the rendered frame. Available multipliers depend on the GPU and driver.",
                               L"倍數包含原始渲染影格。可用倍數取決於顯示卡與驅動程式。");
            else if (edit.graphicsBackend == GraphicsBackend::Metal)
                next.help = Tr(L"MetalFX frame generation uses 2× on supported GPUs with macOS 26 or later.",
                               L"MetalFX 影格生成在 macOS 26 或更新版本及支援的 GPU 上使用 2×。");
            else if (edit.graphicsBackend == GraphicsBackend::Vulkan)
                next.help = dlss && fsr
                    ? Tr(L"Vulkan supports DLSS fixed multipliers and FSR 2×. Enabling or changing the FG provider requires a restart.",
                         L"Vulkan 支援 DLSS 固定倍數與 FSR 2×。啟用或切換影格生成提供者需重新啟動。")
                    : dlss
                    ? Tr(L"Vulkan supports DLSS fixed multipliers. Enabling or changing the FG provider requires a restart.",
                         L"Vulkan 支援 DLSS 固定倍數。啟用或切換影格生成提供者需重新啟動。")
                    : Tr(L"Vulkan supports FSR 2×. Enabling or changing the FG provider requires a restart.",
                         L"Vulkan 支援 FSR 2×。啟用或切換影格生成提供者需重新啟動。");
            else if (edit.frameGenerationProvider == framegen::Provider::Xess)
                next.help = Tr(L"FG works independently of upscaling. XeSS uses a fixed 2× multiplier.",
                               L"影格生成可獨立於超解析度使用。XeSS 固定為 2×。");
            else
                next.help = Tr(L"FG works independently of upscaling. FSR uses a fixed 2× multiplier.",
                               L"影格生成可獨立於超解析度使用。FSR 固定為 2×。");
            break;
        }
        case GraphicsRow::Hdr:
            next.help = graphics_menu::HdrAvailable(edit.graphicsBackend)
                ? Tr(L"Requires an HDR display. Applies after saving; with frame generation on, after a restart. Works with every AA mode, upscaler and scaling filter; frame generation stays SDR except DLSS on Vulkan.",
                     L"需要 HDR 螢幕。儲存後套用；開啟影格生成時需重新啟動。可搭配任一抗鋸齒、超解析度與縮放濾鏡；影格生成僅 Vulkan 的 DLSS 可與 HDR 同時開啟。")
                : Tr(L"HDR output is unavailable for this graphics backend.",
                     L"目前圖形後端無法使用 HDR 輸出。");
            break;
        case GraphicsRow::HdrPaperWhite:
#if LO_PLATFORM_MACOS
            next.help = Tr(L"Reference white uses system SDR white on Metal. Changes apply after saving.",
                           L"Metal 以系統 SDR 白位作為參考白位；儲存後套用。");
#else
            next.help = Tr(L"Reference white controls normal scene brightness. Changes apply after saving.",
                           L"參考白位控制一般場景亮度；儲存後套用。");
#endif
            break;
        case GraphicsRow::HdrPeak:
            next.help = Tr(L"Press A to calibrate peak brightness against an HDR comparison pattern. Auto follows the active display report.",
                           L"按 A 以 HDR 對比圖校準最高亮度；自動模式跟隨目前顯示器的回報值。");
            break;
        case GraphicsRow::Brightness:
            next.help = Tr(L"Adjusts the game image's brightness and gamma. Press A to compare against the current scene. Changes apply after saving.",
                           L"調整遊戲畫面的亮度與 Gamma。按 A 與目前畫面對照；儲存後套用。");
            break;
        case GraphicsRow::DisplayMode:
        case GraphicsRow::Save:
        case GraphicsRow::Count:
            break;
        }
    }
    if (!status.empty()) next.help = status;
    if (restartPrompt)
    {
        next.dialogTitle = Tr(L"Restart required", L"需要重新啟動");
#ifdef _WIN32
        next.dialogMessage = restartSaveFailed
            ? Tr(L"Settings could not be saved. Check settings.ini permissions, then retry or cancel.",
                 L"無法儲存設定。請檢查 settings.ini 權限後重試或取消。")
            : savedRestartPrompt && restartForFgProvider
                ? edit.graphicsBackend == GraphicsBackend::Vulkan
                    ? Tr(L"Enabling or changing Vulkan frame generation requires a restart. Settings saved. Restart now?",
                         L"啟用或切換 Vulkan 影格生成需要重新啟動。設定已儲存，現在重新啟動嗎？")
                    : Tr(L"Switching from DLSS FG to FSR FG requires a restart. Settings saved. Restart now?",
                     L"從 DLSS 影格生成切換到 FSR 影格生成需要重新啟動。設定已儲存，現在重新啟動嗎？")
            : savedRestartPrompt ? Tr(L"Settings saved. Restart now?", L"設定已儲存。立即重新啟動嗎？")
            : Tr(L"Save these settings and restart now?", L"儲存這些設定並立即重新啟動嗎？");
        next.dialogChoices = {Tr(L"Restart now", L"立即重新啟動"), Tr(L"Later", L"稍後"), Tr(L"Cancel", L"取消")};
        if (savedRestartPrompt) next.dialogChoices.resize(2);
#else
        next.dialogMessage = restartSaveFailed
            ? Tr(L"Settings could not be saved. Check settings.ini permissions, then retry or cancel.",
                 L"無法儲存設定。請檢查 settings.ini 權限後重試或取消。")
            : savedRestartPrompt ? Tr(L"Settings saved. Restart manually to apply changes.", L"設定已儲存。請手動重新啟動以套用變更。")
            : Tr(L"Save these settings? Restart manually to apply changes.", L"儲存這些設定嗎？請手動重新啟動以套用變更。");
        next.dialogChoices = savedRestartPrompt
            ? std::vector<std::wstring>{Tr(L"OK", L"確定")}
            : std::vector<std::wstring>{Tr(L"Save and restart later", L"儲存並稍後手動重啟"), Tr(L"Cancel", L"取消")};
#endif
        next.dialogSelection = restartChoice;
    }
    if (collectionPrompt) {
        next.dialogTitle = os::log_collection::Label(edit.uiLanguage);
        next.dialogMessage = os::log_collection::Message(edit.uiLanguage);
        next.dialogChoices = {Tr(L"Yes", L"是"), Tr(L"No", L"否")};
        next.dialogSelection = collectionChoice;
    }
    if (mainMenuPrompt)
    {
        next.dialogTitle = Tr(L"Quit to Main Menu", L"退出到主選單");
        next.dialogMessage = Tr(L"Return to the main menu? Unsaved progress will be lost.",
                                L"返回主選單嗎？尚未儲存的進度將會遺失。");
        next.dialogChoices = {Tr(L"Return", L"返回"), Tr(L"Cancel", L"取消")};
        next.dialogSelection = mainMenuChoice;
    }
    if (importPrompt)
    {
        next.dialogTitle = Tr(L"Import discs & DLC", L"匯入光碟與 DLC");
        next.dialogMessage = Tr(L"Close the game and open the importer? Unsaved progress will be lost.",
                                L"關閉遊戲並開啟匯入器嗎？尚未儲存的進度將會遺失。");
        next.dialogChoices = {Tr(L"Open importer", L"開啟匯入器"), Tr(L"Cancel", L"取消")};
        next.dialogSelection = importChoice;
    }
    if (displayConfirm)
    {
        // Shown over a restart prompt from the same save; that one follows.
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(displayConfirmDeadline - MenuNow()).count();
        const auto seconds = std::max<long long>(0, (left + 999) / 1000);
        next.dialogTitle = Tr(L"Keep this display?", L"保留這台顯示器嗎？");
        next.dialogMessage = Tr(L"The game moved to the chosen display. Without an answer it returns to the previous display.",
                                L"遊戲已移到所選的顯示器。若未回應，將回到之前的顯示器。");
        next.dialogChoices = {Tr(L"Keep", L"保留"),
                              std::wstring(Tr(L"Revert", L"還原")) + L" (" + std::to_wstring(seconds) + L")"};
        next.dialogSelection = displayConfirmChoice;
    }
#if LO_PLATFORM_ANDROID
    next.notice = tab == 2 && graphics_menu::AndroidFsrAvailable &&
        edit.upscaler == gpu::upscaling::Upscaler::Fsr ? DlssNotice() : std::wstring{};
#else
    // Vulkan DLSS-G presents through the HDR10 swap chain; other frame
    // generation paths keep an SDR swap chain (see video.cpp).
    const bool fgKeepsSdr = edit.frameGenerationProvider != framegen::Provider::Off &&
        !(edit.graphicsBackend == GraphicsBackend::Vulkan && edit.frameGenerationProvider == framegen::Provider::Dlss);
    const bool hdrConflict = edit.hdr && fgKeepsSdr;
    next.notice = tab == 2 && edit.hdr && !graphics_menu::HdrAvailable(edit.graphicsBackend)
        ? Tr(L"The saved HDR preference is inactive on this graphics backend.",
             L"已儲存的 HDR 偏好在目前圖形後端不會啟用。")
        : tab == 2 && hdrConflict
        ? Tr(L"HDR is paused while this frame generation provider is selected (only DLSS on Vulkan keeps HDR).",
             L"選取這個影格生成提供者時 HDR 會暫停（只有 Vulkan 的 DLSS 可保持 HDR）。")
        : tab == 2 ? (row == int(GraphicsRow::FrameGeneration) ||
        row == int(GraphicsRow::FrameGenerationMultiplier) ? FgNotice() : DlssNotice()) : std::wstring{};
#endif
    std::lock_guard lock(snapshotMutex);
    // Presentation may have published availability while this snapshot was built.
    next.calibration.sceneAvailable = calibrationSceneAvailable.load(std::memory_order_relaxed);
    next.brightness.sceneAvailable = next.calibration.sceneAvailable;
    next.neuralRendering.sceneAvailable = nrSceneAvailable.load(std::memory_order_relaxed);
    if (next.tab == snapshot.tab && next.row == snapshot.row && next.scroll == snapshot.scroll && next.language == snapshot.language &&
        next.calibration == snapshot.calibration && next.brightness == snapshot.brightness &&
        next.neuralRendering == snapshot.neuralRendering &&
        next.rows == snapshot.rows && next.help == snapshot.help && next.notice == snapshot.notice && next.dialogTitle == snapshot.dialogTitle &&
        next.dialogMessage == snapshot.dialogMessage && next.dialogChoices == snapshot.dialogChoices &&
        next.dialogSelection == snapshot.dialogSelection)
        return;
    next.revision = snapshot.revision + 1;
    snapshot = std::move(next);
}
} // namespace
void SetHdrDisplayInfo(HdrDisplayInfo info)
{
    hdrDisplayInfo.store(uint64_t(info.peakNits) | (uint64_t(info.active) << 32) |
        (uint64_t(info.relative) << 33), std::memory_order_relaxed);
}
void SetHdrCalibrationSceneAvailable(bool available)
{
    calibrationSceneAvailable.store(available, std::memory_order_relaxed);
    // The guest can be paused while the presentation thread captures the scene.
    // Publish the change immediately so DrawMenu invalidates its raster cache.
    std::lock_guard lock(snapshotMutex);
    if (snapshot.calibration.sceneAvailable != available)
    {
        snapshot.calibration.sceneAvailable = snapshot.brightness.sceneAvailable = available;
        ++snapshot.revision;
    }
}
void SetNeuralRenderingPreviewAvailable(bool available)
{
    nrSceneAvailable.store(available, std::memory_order_relaxed);
    std::lock_guard lock(snapshotMutex);
    if (snapshot.neuralRendering.sceneAvailable != available)
    {
        snapshot.neuralRendering.sceneAvailable = available;
        ++snapshot.revision;
    }
}
NeuralRenderingTuning GetNeuralRenderingTuning()
{
    NeuralRenderingTuning result;
    {
        std::lock_guard lock(snapshotMutex);
        result = snapshot.neuralRendering;
    }
    // The open menu previews its unsaved values; gameplay uses the saved ones.
    if (!active.load())
        return MakeNeuralRenderingTuning(GetConfig(), false);
    result.sceneAvailable = nrSceneAvailable.load(std::memory_order_relaxed);
    return result;
}
BrightnessCalibration GetBrightnessCalibration()
{
    BrightnessCalibration result;
    {
        std::lock_guard lock(snapshotMutex);
        result = snapshot.brightness;
    }
    // The open menu previews its unsaved values; gameplay uses the saved ones.
    if (!active.load())
        return MakeBrightnessCalibration(GetConfig(), false);
    result.sceneAvailable = calibrationSceneAvailable.load(std::memory_order_relaxed);
    return result;
}
HdrCalibration GetHdrCalibration()
{
    HdrCalibration result;
    {
        std::lock_guard lock(snapshotMutex);
        result = snapshot.calibration;
    }
    if (!active.load() || !result.open)
        return MakeHdrCalibration(GetConfig(), false);
    const auto display = CurrentHdrDisplayInfo();
    result.detectedValid = display.peakNits > 0;
    result.detectedNits = display.peakNits;
    result.relative = display.relative;
    result.hdrActive = display.active;
    result.sceneAvailable = calibrationSceneAvailable.load(std::memory_order_relaxed);
    result.effectiveNits = std::clamp(result.automatic && result.detectedValid
        ? result.detectedNits : result.automatic ? 1000u : result.manualNits,
        result.paperWhiteNits, 10000u);
    return result;
}
bool CalibrationKey(uint32_t key)
{
    // Escape has no controller mapping; it answers Revert on the display prompt.
    if (key == 27 && displayConfirmOpen.load())
    {
        displayConfirmEscape = true;
        return true;
    }
    const bool brightnessKey = (brightnessOpen.load() || nrOpen.load()) && (key == 13 || key == 27);
    if (!brightnessKey && (!calibrationOpen.load() || !(key == 8 || key == 13 || key == 27 || (key >= '0' && key <= '9'))))
        return false;
    std::lock_guard lock(calibrationKeyMutex);
    calibrationKeys.push_back(key);
    return true;
}
void PointerDrag(float x, float y, bool held)
{
    if (held && nrOpen.load())
    {
        int focus = 0, value = 0, side = 0;
        bool track = false;
        if (NrControlAt(x, y, focus, track, value, side) && NrIsSlider(focus))
            nrAction = NrAction(4, focus, value);
        return;
    }
    if (held && brightnessOpen.load())
    {
        if (y >= 516 && y < 552)
            brightnessDragBrightness = int(std::lround(BrightnessSliderFraction(x) * 40.0f)) - 20;
        else if (y >= 558 && y < 594)
            brightnessDragGamma = 50 + int(std::lround(BrightnessSliderFraction(x) * 20.0f)) * 5;
        return;
    }
    if (!held || !calibrationOpen.load() || y < 540 || y >= 595) return;
    uint32_t paperWhite = 203;
    {
        std::lock_guard lock(snapshotMutex);
        paperWhite = snapshot.calibration.paperWhiteNits;
    }
    calibrationDragNits.store(int(CalibrationSliderValue(x, paperWhite)));
}
bool FilterInput(uint16_t &buttons, int16_t x, int16_t y)
{
    // A held Back must not become a fresh press in the parent menu. Consume
    // the neutral poll as well so its edge detector sees the release first.
    if (releaseToParent.load())
    {
        if (!buttons && std::abs(int(x)) <= 16000 && std::abs(int(y)) <= 16000)
            releaseToParent = false;
        buttons = 0;
        return true;
    }
    if (unsigned polls = cancelPolls.load(); polls && cancelPolls.compare_exchange_strong(polls, polls - 1))
    {
        buttons = cancelButton.load();
        return false;
    }
    if (!active.load())
        return false;
    if (swapConfirm.load())
        buttons = (buttons & ~0x3000) | ((buttons & 0x1000) << 1) | ((buttons & 0x2000) >> 1);
    if (x < -16000)
        buttons |= 4;
    if (x > 16000)
        buttons |= 8;
    if (y < -16000)
        buttons |= 2;
    if (y > 16000)
        buttons |= 1;
    static uint16_t previous = 0;
    static auto repeat = std::chrono::steady_clock::time_point{};
    if (waitForRelease.load())
    {
        previous = buttons;
        if (!buttons)
            waitForRelease = false;
        buttons = 0;
        return true;
    }
    auto now = std::chrono::steady_clock::now();
    uint16_t edge = buttons & ~previous;
    if (buttons != previous)
        repeat = now + std::chrono::milliseconds(350);
    else if (now >= repeat)
    {
        edge |= buttons & 15;
        repeat = now + std::chrono::milliseconds(100);
    }
    previous = buttons;
    pending.fetch_or(edge);
    buttons = 0;
    return true;
}
void PointerClick(float x, float y, bool reverse)
{
    if (!active.load())
        return;
    std::lock_guard lock(snapshotMutex);
    if (snapshot.neuralRendering.open)
    {
        int focus = 0, value = 0, side = 0;
        bool track = false;
        if (y >= NrButtonTop && y < NrButtonTop + NrButtonHeight)
        {
            for (int i = 0; i < 3; ++i)
            {
                const int left = NrColumnX[0] + i * (NrButtonWidth + NrButtonGap);
                if (x >= left && x < left + NrButtonWidth) nrAction = NrAction(5, 9 + i);
            }
        }
        else if (NrControlAt(x, y, focus, track, value, side))
            nrAction = track ? NrAction(4, focus, value)
                     : (side == 0 || NrIsSlider(focus)) ? NrAction(1, focus)
                     : NrAction(side, focus);
        return;
    }
    if (snapshot.brightness.open)
    {
        // Same top toggle and button row as the HDR page; sliders take the pointer x.
        if (y >= 34 && y < 78 && x >= 800 && x < 1120)
            brightnessClick = x < 940 ? 7 : 8;
        else if (y >= 516 && y < 552 && x >= 370 && x < 1010)
            brightnessDragBrightness = int(std::lround(BrightnessSliderFraction(x) * 40.0f)) - 20;
        else if (y >= 558 && y < 594 && x >= 370 && x < 1010)
            brightnessDragGamma = 50 + int(std::lround(BrightnessSliderFraction(x) * 20.0f)) * 5;
        else if (y >= 605 && y < 655)
            brightnessClick = x >= 160 && x < 400 ? 2 : x >= 420 && x < 720 ? 3 :
                              x >= 740 && x < 930 ? 4 : x >= 950 && x < 1120 ? 5 : -1;
        return;
    }
    if (snapshot.calibration.open)
    {
        if (y >= 34 && y < 78 && x >= 800 && x < 1120)
            calibrationClick = x < 940 ? 5 : 6;
        else if (y >= 540 && y < 595 && x >= 245 && x < 1035)
            calibrationDragNits = int(CalibrationSliderValue(x, snapshot.calibration.paperWhiteNits));
        else if (y >= 605 && y < 655)
            calibrationClick = x >= 160 && x < 400 ? 1 : x >= 420 && x < 720 ? 2 :
                               x >= 740 && x < 930 ? 3 : x >= 950 && x < 1120 ? 4 : -1;
        return;
    }
    if (!snapshot.dialogChoices.empty())
    {
        const int selected = int(y - 360) / 43;
        if (x >= 390 && x < 890 && y >= 360 && selected >= 0 && selected < int(snapshot.dialogChoices.size()))
        {
            mouseDialog = selected;
            mouseAction = 0x1000;
        }
        return;
    }
    if (x >= 386 && x < 1026 && y >= 110 && y < 142)
    {
        mouseTab = int(x - 386) / MenuTabWidth;
        return;
    }
    constexpr int height = 43;
    constexpr int top = 150;
    const int slot = int(y - top) / height;
    if (x < 38 || x >= 1026 || y < top || slot < 0 || slot >= kMenuVisibleRows || (top + (slot + 1) * height) > 640)
        return;
    // Translate the clicked slot through hidden rows and the scroll window
    // back to a logical row index.
    const int target = snapshot.scroll + slot;
    int visible = 0;
    size_t hit = snapshot.rows.size();
    for (size_t i = 0; i < snapshot.rows.size(); ++i)
    {
        if (snapshot.rows[i].hidden)
            continue;
        if (visible++ == target)
        {
            hit = i;
            break;
        }
    }
    if (hit >= snapshot.rows.size())
        return;
    mouseRow = int(hit);
    if (x >= 386 && snapshot.rows[hit].enabled) {
        const bool hdrLevel = snapshot.tab == 2 &&
            (hit == size_t(GraphicsRow::HdrPaperWhite) || hit == size_t(GraphicsRow::HdrPeak));
        // Pointer adjustment is explicit left/right, not a synthetic A press.
        // Keep confirmation reserved for action rows and modal dialogs.
        mouseAction = snapshot.tab == 2 && hit == size_t(GraphicsRow::HdrPeak) &&
                      x >= 600 && x < 820 ? 0x1000 : graphics_menu::IsAction(snapshot.tab, int(hit))
            ? (reverse ? 0 : 0x1000) : (reverse || (hdrLevel && x < 600) ||
               ((snapshot.rows[hit].singleValue || snapshot.rows[hit].choices.size() > 5) && x < 458) ? 4 : 8);
    }
}
} // namespace settings

// Resolve the explicit host choice instead of the retail language allowlist's
// default alias. IDs 1-9 map to INT/JPN/DEU/FRA/SPA/ITA/KOR/CHI/SCH. Return the
// registry record for the host language when the edition registers it, so voice
// and FMV audio lookups match it (#220). Languages the registry lacks (Asian SCH
// is registered as ID 10) still get the executable's static table pointer.
PPC_FUNC(sub_82481BE8)
{
    const uint32_t language = settings::GameLanguage();
    const auto object = ctx.r3.u32, request = ctx.r4.u32, caller = uint32_t(ctx.lr);
    if (settings::language::ResourceOverride(language, object, request))
    {
        ctx.r4.u64 = language;
        __imp__sub_82481BE8(ctx, base);
        const uint32_t record = ctx.r3.u32;
        if (!settings::language::KeepRegistryRecord(record, record ? PPC_LOAD_U16(record - 2) : 0, language))
            ctx.r3.u64 = PPC_LOAD_U32(0x832455F0 + language * 4);
        static const bool logged = [] {
            LOG_INFO("settings: explicit game resource language {}", settings::GameLanguage());
            return true;
        }();
        (void)logged;
        settings::language::TraceLookup(base, language, object, request, caller, ctx.r3.u32, true);
        return;
    }
    __imp__sub_82481BE8(ctx, base);
    settings::language::TraceLookup(base, language, object, request, caller, ctx.r3.u32, false);
}

PPC_FUNC(sub_822F19B0)
{
#if !defined(LO_GPU_PLUME)
    __imp__sub_822F19B0(ctx, base);
    return;
#else
    if (!gpu::video::GetDevice())
    {
        __imp__sub_822F19B0(ctx, base);
        return;
    }
    using namespace settings;
    const uint32_t menu = ctx.r3.u32;
    const uint32_t state = PPC_LOAD_U32(menu + 4);
    const uint32_t modal = PPC_LOAD_U32(menu + 0x1804);
    if (menu != lastMenu)
    {
        lastMenu = menu;
        bypass = false;
        sawModal = false;
        closing = false;
        mainMenuRequested = false;
        returnToBrightness = false;
        active = false;
    }
    if (closing)
    {
        // Keep the retail task alive through its own exit animation and
        // completion notification. Never replace these with a state write.
        __imp__sub_822F19B0(ctx, base);
        if (PPC_LOAD_U32(menu + 4) <= 2)
        {
            closing = false;
            if (mainMenuRequested)
            {
                mainMenuRequested = false;
                PPCContext request = ctx;
                RequestMainMenuAfterSettingsClose(request, base, menu);
            }
        }
        return;
    }
    if (state != 4)
    {
        if (state <= 2)
            bypass = sawModal = returnToBrightness = false;
        else if (bypass)
            sawModal = true;
        active = false;
        __imp__sub_822F19B0(ctx, base);
        return;
    }
    if (bypass)
    {
        if (modal)
        {
            sawModal = true;
            __imp__sub_822F19B0(ctx, base);
            return;
        }
        if (!sawModal)
        {
            __imp__sub_822F19B0(ctx, base);
            return;
        }
        bypass = false;
        sawModal = false;
    }
    const uint32_t config = ConfigAddress(base);
    if (!config)
    {
        __imp__sub_822F19B0(ctx, base);
        return;
    }
    if (!active.exchange(true))
    {
        // Back from the original calibration screen: keep the unsaved edit and
        // reopen the brightness page.
        if (!std::exchange(returnToBrightness, false))
        {
            edit = GetConfig();
            brightnessOpen = false;
        }
        else brightnessOpen = true;
        brightnessClick = -1;
        brightnessDragBrightness = INT_MIN;
        brightnessDragGamma = -1;
        nrOpen = false;
        nrAction = 0;
        calibrationOpen = false;
        calibrationNumberEditing = false;
        calibrationNumber.clear();
        calibrationClick = -1;
        calibrationDragNits = -1;
        collectionPrompt = os::log_collection::Supported() && os::log_collection::Consent() < 0;
        collectionChoice = 1;
        pending = 0;
        waitForRelease = true;
        restartPrompt = false;
        savedRestartPrompt = false;
        restartSaveFailed = false;
        restartForFgProvider = false;
        mainMenuPrompt = false;
        importPrompt = false;
        importLaunchPending = false;
        displayConfirm = displayReverting = false;
        displayConfirmOpen = false;
        {
            const auto saved = GetConfig();
            syncedDisplayName = saved.displayName;
            syncedDisplayIndex = saved.displayIndex;
        }
        status.clear();
        Publish(base, config);
        LOG_INFO("settings: replacement opened at guest menu {:#x}", menu);
        language::TraceConfig(base, config, "menu-open");
        for (uint32_t i = 0; i < VoiceCount(base); ++i)
            LOG_INFO("settings: voice option {} -> language {}", i, VoiceLanguage(base, i));
    }
    uint16_t input = pending.exchange(0);
    {
        // Follow a display the player moved the window to while the menu is
        // open, unless the Display row was changed here.
        const auto saved = GetConfig();
        if (saved.displayName != syncedDisplayName || saved.displayIndex != syncedDisplayIndex)
        {
            if (edit.displayName == syncedDisplayName && edit.displayIndex == syncedDisplayIndex)
            {
                edit.displayName = saved.displayName;
                edit.displayIndex = saved.displayIndex;
            }
            syncedDisplayName = saved.displayName;
            syncedDisplayIndex = saved.displayIndex;
        }
    }
    if (int selected = mouseTab.exchange(-1); selected >= 0)
    {
        tab = selected;
        row = 0;
        input |= 0x400;
    }
    if (int selected = mouseRow.exchange(-1); selected >= 0)
    {
        row = selected;
        input |= 0x400;
    }
    input |= mouseAction.exchange(0);
    swapConfirm = (PPC_LOAD_U32(config + 4) & 0x02000000) != 0;
    auto closeSettings = [&] {
        // Retail 822F2904 applies the guest configuration before asking again.
        // Its accepted confirmation at 822F2098 calls 82889E50, which refreshes
        // language resources and starts state 3. Preserve those real operations
        // and the parent's persistence/completion path without a second dialog.
        closing = true;
        bypass = false;
        sawModal = false;
        releaseToParent = true;
        active = false;
        calibrationOpen = false;
        brightnessOpen = false;
        nrOpen = false;
        cancelPolls = 0;
        pending = 0;
        PPCContext apply = ctx;
        apply.r3.u64 = config;
        language::TraceConfig(base, config, "menu-before-close");
        __imp__sub_82870E38(apply, base);
        language::TraceConfig(base, config, "menu-after-close-apply");
        PPCContext close = ctx;
        close.r3.u64 = menu;
        __imp__sub_82889E50(close, base);
        LOG_INFO("settings: replacement closing menu={:08X} state={} (native completion)",
                 menu, PPC_LOAD_U32(menu + 4));
    };
    auto openOriginalCalibration = [&] {
        // Hand the original calibration screen its own brightness row.
        const uint32_t list = menu + 0x558, table = PPC_LOAD_U32(list + 0x84);
        // Retail row 12 opens brightness calibration; replacement menu row ids are independent.
        for (uint32_t i = 0; i < 13; i++)
            if (PPC_LOAD_U32(table + i * 0x30 + 4) == 12)
            {
                PPC_STORE_U32(list + 0x38, i);
                break;
            }
        bypass = true;
        sawModal = false;
        active = false;
        cancelButton = swapConfirm.load() ? 0x2000 : 0x1000;
        cancelPolls = 6;
    };
    if (restart::ConsumeLaunchFailure())
    {
        status = importLaunchPending
            ? Tr(L"Could not open importer. The game is still running; try again.",
                 L"無法開啟匯入器。遊戲仍在執行，請重試。")
            : Tr(L"Restart could not be started. This game is still running; your saved settings are safe.",
                 L"無法啟動重新啟動程序。本遊戲仍在執行，已儲存的設定安全保留。");
        importLaunchPending = false;
        Publish(base, config);
    }
    // A parked child will open the importer only after this process exits.
    // Further presses must not queue another child or alter the pending action.
    if (restart::InstallRequested())
    {
        Publish(base, config);
        return;
    }
    if (collectionPrompt) {
        if (int selected = mouseDialog.exchange(-1); selected >= 0) collectionChoice = std::min(selected, 1);
        if (input & 3) collectionChoice = 1 - collectionChoice;
        if (input & 0x2000) collectionChoice = 1;
        if (input & 0x3000) {
            if (os::log_collection::SetConsent(collectionChoice == 0)) { collectionPrompt = false; status.clear(); }
            else status = Tr(L"Settings could not be saved.", L"無法儲存設定。");
        }
        Publish(base, config);
        return;
    }
    if (displayConfirm)
    {
        if (int selected = mouseDialog.exchange(-1); selected >= 0)
            displayConfirmChoice = std::min(selected, 1);
        if (input & 3) displayConfirmChoice = 1 - displayConfirmChoice;
        // A / Start (keyboard Enter) answer the selected choice; B, Back and
        // Escape revert. An answer in the last tick still counts.
        const bool revert = displayConfirmEscape.exchange(false) || (input & 0x2020);
        const bool choose = (input & 0x1010) != 0;
        if (!revert && choose && displayConfirmChoice == 0)
            KeepDisplayChoice();
        else if (revert || choose)
            RevertDisplayChoice("player");
        else if (MenuNow() >= displayConfirmDeadline)
            RevertDisplayChoice("no answer in 5 s");
        Publish(base, config);
        return;
    }
    if (restartPrompt)
    {
#ifdef _WIN32
        const int choices = savedRestartPrompt ? 2 : 3;
#else
        const int choices = savedRestartPrompt ? 1 : 2;
#endif
        if (int selected = mouseDialog.exchange(-1); selected >= 0)
            restartChoice = std::min(selected, choices - 1);
        if (input & 1) restartChoice = (restartChoice + choices - 1) % choices;
        if (input & 2) restartChoice = (restartChoice + 1) % choices;
        if (input & 0x2000) restartChoice = choices - 1;
        if (input & 0x3000)
        {
            if (savedRestartPrompt)
            {
                // Graphics were already saved and applied. Back means Later;
                // neither choice writes or reverts that completed transaction.
                restartPrompt = false;
                savedRestartPrompt = false;
                restartSaveFailed = false;
                restartForFgProvider = false;
#ifdef _WIN32
                status = restartChoice == 0
                    ? Tr(L"Saved. Preparing a safe restart…", L"已儲存，正在準備安全重新啟動……")
                    : Tr(L"Saved. Changes take effect after restarting.", L"已儲存，重新啟動後套用變更。");
                if (restartChoice == 0) restart::Request();
#else
                status = Tr(L"Saved. Please restart manually to apply changes.", L"已儲存，請手動重新啟動以套用變更。");
#endif
            }
            else if (restartChoice == (choices - 1))
            {
                restartPrompt = false;
                restartSaveFailed = false;
                restartForFgProvider = false;
                status = Tr(L"Changes requiring restart were cancelled.", L"已取消需要重新啟動的變更。");
            }
            else if (SaveConfig(restartAfter))
            {
                edit = restartAfter;
                restartPrompt = false;
                restartSaveFailed = false;
                restartForFgProvider = false;
#ifdef _WIN32
                status = restartChoice == 0
                    ? Tr(L"Saved. Preparing a safe restart…", L"已儲存，正在準備安全重新啟動……")
                    : Tr(L"Saved. Changes take effect after restarting.", L"已儲存，重新啟動後套用變更。");
                if (restartChoice == 0) restart::Request();
#else
                status = Tr(L"Saved. Please restart manually to apply changes.", L"已儲存，請手動重新啟動以套用變更。");
#endif
            }
            else
                restartSaveFailed = true;
        }
        Publish(base, config);
        return;
    }
    if (mainMenuPrompt)
    {
        if (int selected = mouseDialog.exchange(-1); selected >= 0)
            mainMenuChoice = std::min(selected, 1);
        if (input & 3) mainMenuChoice = 1 - mainMenuChoice;
        if (input & 0x2000) mainMenuChoice = 1;
        if (input & 0x3000)
        {
            mainMenuPrompt = false;
            if (mainMenuChoice == 0)
            {
                // Close the retail Settings task first, then request the title
                // transition after the native completion notification.
                mainMenuRequested = true;
                closeSettings();
                return;
            }
        }
        Publish(base, config);
        return;
    }
    if (importPrompt)
    {
        if (int selected = mouseDialog.exchange(-1); selected >= 0)
            importChoice = std::min(selected, 1);
        if (input & 3) importChoice = 1 - importChoice;
        if (input & 0x2000) importChoice = 1;
        if (input & 0x3000)
        {
            importPrompt = false;
            if (importChoice == 0)
            {
                restart::RequestInstall();
                importLaunchPending = true;
                status = Tr(L"Closing game and opening importer…", L"正在關閉遊戲並開啟匯入器……");
            }
        }
        Publish(base, config);
        return;
    }
    if (calibrationOpen.load())
    {
        if (const int dragged = calibrationDragNits.exchange(-1); dragged >= 0)
        {
            edit.hdrPeakAutomatic = false;
            edit.hdrPeakNits = uint32_t(std::clamp(dragged, int(edit.hdrPaperWhiteNits), 10000));
            calibrationFocus = 0;
        }
        std::vector<uint32_t> keys;
        {
            std::lock_guard lock(calibrationKeyMutex);
            keys.swap(calibrationKeys);
        }
        auto commitNumber = [&] {
            if (!calibrationNumber.empty())
            {
                uint32_t number = 0;
                for (wchar_t digit : calibrationNumber) number = number * 10 + uint32_t(digit - L'0');
                edit.hdrPeakNits = std::clamp(number, edit.hdrPaperWhiteNits, 10000u);
                edit.hdrPeakAutomatic = false;
            }
            calibrationNumberEditing = false;
            calibrationNumber.clear();
        };
        bool cancelCalibration = false;
        for (const uint32_t key : keys)
        {
            if (key >= '0' && key <= '9')
            {
                if (!calibrationNumberEditing)
                {
                    calibrationNumberEditing = true;
                    calibrationNumber.clear();
                    calibrationFocus = 2;
                }
                if (calibrationNumber.size() < 5) calibrationNumber.push_back(wchar_t(key));
            }
            else if (key == 8 && calibrationNumberEditing && !calibrationNumber.empty())
                calibrationNumber.pop_back();
            else if (key == 13)
            {
                if (calibrationNumberEditing)
                    commitNumber();
                else input |= 0x1000;
            }
            else if (key == 27)
            {
                if (calibrationNumberEditing)
                {
                    calibrationNumberEditing = false;
                    calibrationNumber.clear();
                }
                else cancelCalibration = true;
            }
        }
        if (cancelCalibration)
        {
            edit.hdrPeakAutomatic = calibrationStartAutomatic;
            edit.hdrPeakNits = calibrationStartPeakNits;
            calibrationOpen = false;
        }
        else
        {
            if (const int clicked = calibrationClick.exchange(-1); clicked >= 1)
            {
                if (clicked >= 5)
                {
                    calibrationFocus = 5;
                    calibrationScenePreview = clicked == 5;
                }
                else
                {
                    calibrationFocus = clicked;
                    input |= 0x1000;
                }
            }
            if (calibrationNumberEditing && (input & 0x2000))
            {
                calibrationNumberEditing = false;
                calibrationNumber.clear();
                input &= ~0x2000;
            }
            if (input & 0x2000) calibrationOpen = false;
            if (calibrationOpen.load())
            {
                if (input & 0x100) calibrationScenePreview = true;
                if (input & 0x200) calibrationScenePreview = false;
                if (input & 1) calibrationFocus = (calibrationFocus + 5) % 6;
                if (input & 2) calibrationFocus = (calibrationFocus + 1) % 6;
                const int delta = (input & 4) ? -1 : (input & 8) ? 1 : 0;
                if (delta && calibrationFocus == 0 && !calibrationNumberEditing)
                {
                    const auto display = MakeHdrCalibration(edit, true);
                    const uint32_t start = edit.hdrPeakAutomatic ? display.effectiveNits : edit.hdrPeakNits;
                    const int step = start < 1000 ? 10 : 100;
                    edit.hdrPeakAutomatic = false;
                    edit.hdrPeakNits = uint32_t(std::clamp(int(start) + delta * step,
                        int(edit.hdrPaperWhiteNits), 10000));
                }
                if (input & 0x1000)
                {
                    if (calibrationNumberEditing) commitNumber();
                    else if (calibrationFocus == 1) edit.hdrPeakAutomatic = true;
                    else if (calibrationFocus == 2)
                    {
                        calibrationNumberEditing = true;
                        calibrationNumber = std::to_wstring(edit.hdrPeakAutomatic
                            ? MakeHdrCalibration(edit, true).effectiveNits : edit.hdrPeakNits);
                    }
                    else if (calibrationFocus == 3) calibrationOpen = false;
                    else if (calibrationFocus == 4)
                    {
                        edit.hdrPeakAutomatic = calibrationStartAutomatic;
                        edit.hdrPeakNits = calibrationStartPeakNits;
                        calibrationOpen = false;
                    }
                    else if (calibrationFocus == 5)
                        calibrationScenePreview = !calibrationScenePreview.load();
                }
            }
        }
        if (!calibrationOpen.load())
        {
            calibrationNumberEditing = false;
            calibrationNumber.clear();
            calibrationClick = -1;
            calibrationDragNits = -1;
        }
        Publish(base, config);
        return;
    }
    if (nrOpen.load())
    {
        std::vector<uint32_t> keys;
        {
            std::lock_guard lock(calibrationKeyMutex);
            keys.swap(calibrationKeys);
        }
        bool cancel = (input & 0x2000) != 0, close = false;
        for (const uint32_t key : keys)
        {
            if (key == 13) input |= 0x1000;
            else if (key == 27) cancel = true;
        }
        if (const uint32_t action = nrAction.exchange(0); action & 0x80000000u)
        {
            const int kind = int(action & 15), focus = int((action >> 4) & 15), value = int((action >> 8) & 0xFFFFF) - 100;
            if (focus < NrFocusCount)
            {
                nrFocus = focus;
                if (kind == 2 || kind == 3) StepNrControl(edit, focus, kind == 3 ? 1 : -1);
                else if (kind == 4) SetNrValue(edit, focus, value);
                else if (kind == 5) input |= 0x1000;
            }
        }
        if (input & 1) nrFocus = (nrFocus + NrFocusCount - 1) % NrFocusCount;
        if (input & 2) nrFocus = (nrFocus + 1) % NrFocusCount;
        if (const int delta = (input & 4) ? -1 : (input & 8) ? 1 : 0)
        {
            if (nrFocus < NrControlCount) StepNrControl(edit, nrFocus, delta);
            else nrFocus = 9 + (nrFocus - 9 + 3 + delta) % 3;
        }
        if (input & 0x1000)
        {
            if (nrFocus == 9) ResetNeuralRenderingTuning(edit);
            else if (nrFocus == 10) close = true;
            else if (nrFocus == 11) cancel = true;
        }
        if (cancel)
            RestoreNeuralRenderingTuning(edit, nrStart);
        if (cancel || close)
        {
            nrOpen = false;
            nrAction = 0;
        }
        Publish(base, config);
        return;
    }
    if (brightnessOpen.load())
    {
        if (const int dragged = brightnessDragBrightness.exchange(INT_MIN); dragged != INT_MIN)
        {
            edit.displayBrightness = std::clamp(dragged, -20, 20);
            brightnessFocus = 0;
        }
        if (const int dragged = brightnessDragGamma.exchange(-1); dragged >= 0)
        {
            edit.displayGamma = uint32_t(std::clamp(dragged, 50, 150));
            brightnessFocus = 1;
        }
        std::vector<uint32_t> keys;
        {
            std::lock_guard lock(calibrationKeyMutex);
            keys.swap(calibrationKeys);
        }
        bool cancel = false;
        for (const uint32_t key : keys)
        {
            if (key == 13) input |= 0x1000;
            else if (key == 27) cancel = true;
        }
        if (const int clicked = brightnessClick.exchange(-1); clicked >= 2)
        {
            if (clicked >= 7)
                calibrationScenePreview = clicked == 7;
            else
            {
                brightnessFocus = clicked;
                input |= 0x1000;
            }
        }
        bool close = (input & 0x2000) != 0;
        // LB / RB pick scene or pattern, like the tab bar.
        if (input & 0x100) calibrationScenePreview = true;
        if (input & 0x200) calibrationScenePreview = false;
        if (input & 1) brightnessFocus = (brightnessFocus + 5) % 6;
        if (input & 2) brightnessFocus = (brightnessFocus + 1) % 6;
        if (const int delta = (input & 4) ? -1 : (input & 8) ? 1 : 0)
        {
            if (brightnessFocus == 0)
                edit.displayBrightness = std::clamp(edit.displayBrightness + delta, -20, 20);
            else if (brightnessFocus == 1)
                edit.displayGamma = uint32_t(std::clamp(int(edit.displayGamma) + delta * 5, 50, 150));
            else
                brightnessFocus = 2 + (brightnessFocus - 2 + 4 + delta) % 4;
        }
        bool original = false;
        if (input & 0x1000)
        {
            if (brightnessFocus == 2)
            {
                edit.displayBrightness = 0;
                edit.displayGamma = 100;
            }
            else if (brightnessFocus == 3) original = true;
            else if (brightnessFocus == 4) close = true;
            else if (brightnessFocus == 5) cancel = true;
        }
        if (cancel)
        {
            edit.displayBrightness = brightnessStart;
            edit.displayGamma = gammaStart;
        }
        if (cancel || close || original)
        {
            brightnessOpen = false;
            brightnessClick = -1;
            brightnessDragBrightness = INT_MIN;
            brightnessDragGamma = -1;
        }
        Publish(base, config);
        if (original)
        {
            returnToBrightness = true;
            openOriginalCalibration();
        }
        return;
    }
    auto graphicsSaved = [&] {
        status = Tr(L"Display settings saved.", L"顯示設定已儲存。");
        const auto running = gpu::video::GetFrameGenerationStatus();
        // Ask only when this save changes the FG request. After "Later", the FG
        // notice keeps reporting the pending restart on unrelated saves.
        const bool fgChanged = edit.frameGenerationProvider != previousDisplay.frameGenerationProvider ||
            edit.frameGenerationMode != previousDisplay.frameGenerationMode ||
            edit.frameGenerationMultiplier != previousDisplay.frameGenerationMultiplier;
        restartForFgProvider = fgChanged && ((edit.graphicsBackend == GraphicsBackend::D3D12 &&
            (edit.frameGenerationProvider == framegen::Provider::Fsr || edit.frameGenerationProvider == framegen::Provider::Xess) &&
            (previousDisplay.frameGenerationProvider == framegen::Provider::Dlss ||
             running.sessionProvider == framegen::Provider::Dlss)) ||
            (edit.graphicsBackend == GraphicsBackend::Vulkan &&
             running.phase == gpu::video::FrameGenerationPhase::RestartRequired));
        // HDR switches live unless a frame generation session, which owns the
        // swap chain, already runs in this process (even with FG now Off).
        const bool hdrNeedsRestart = edit.hdr != previousDisplay.hdr &&
            running.sessionProvider != framegen::Provider::Off;
        if (restart::Required(previousDisplay, edit) || restartForFgProvider || hdrNeedsRestart)
        {
            restartPrompt = savedRestartPrompt = true;
            restartSaveFailed = false;
            restartChoice = 0;
        }
        // Only a display choice that actually moved the window asks to be kept.
        if ((edit.displayName != previousDisplay.displayName || edit.displayIndex != previousDisplay.displayIndex) &&
            gpu::video::DisplayMoveCount() != displayMovesBefore)
        {
            displayConfirm = true;
            displayConfirmOpen = true;
            displayConfirmEscape = false;
            displayConfirmChoice = 0;
            displayConfirmDeadline = MenuNow() + kDisplayConfirmTime;
            LOG_INFO("settings: display choice \"{}\"#{} moved the window; waiting 5 s for Keep",
                     edit.displayName, edit.displayIndex);
        }
    };
    if (displayTicket)
    {
        const auto result = gpu::video::QueryDisplayChange(displayTicket);
        if (result == gpu::video::DisplayChangeResult::Pending)
        {
            Publish(base, config);
            return;
        }
        displayTicket = 0;
        if (displayReverting)
        {
            displayReverting = false;
            status = result != gpu::video::DisplayChangeResult::Applied
                ? Tr(L"Could not return to the previous display.", L"無法回到之前的顯示器。")
                : rollbackSaveFailed
                    ? Tr(L"Previous display restored; settings file could not be updated.", L"已回到之前的顯示器，但無法更新設定檔。")
                    : Tr(L"Previous display restored.", L"已回到之前的顯示器。");
        }
        else if (displayRollback)
        {
            displayRollback = false;
            status = result == gpu::video::DisplayChangeResult::Applied
                ? rollbackSaveFailed
                    ? Tr(L"Display restored; settings file could not be restored.", L"顯示已還原，但無法還原設定檔。")
                    : Tr(L"Display mode unavailable; previous settings restored.", L"此顯示模式不可用，已還原之前的設定。")
                : Tr(L"Could not restore the display mode.", L"無法還原顯示模式。");
        }
        else if (result == gpu::video::DisplayChangeResult::Failed)
        {
            rollbackSaveFailed = !SaveConfig(previousDisplay);
            if (rollbackSaveFailed) PreviewConfig(previousDisplay);
            edit = previousDisplay;
            displayRollback = true;
            displayTicket = gpu::video::BeginDisplayChange(previousDisplay);
            status = Tr(L"Restoring display settings…", L"正在還原顯示設定……");
        }
        else graphicsSaved();
        Publish(base, config);
        return;
    }
    bool changed = false;
    if (input & 0x300)
    {
        tab = (tab + ((input & 0x200) ? 1 : MenuTabCount - 1)) % MenuTabCount;
        row = 0;
        status.clear();
    }
    const int count = tab == 0 ? GameRowCount : tab == 1 ? AudioRowCount : tab == 2 ? int(GraphicsRow::Count) : SystemRowCount;
    // Provider-specific rows keep their logical ids and navigation skips them
    // when unavailable. Keyboard Enter reaches the menu as GAMEPAD_START
    // (hid.cpp), so one branch covers gamepad Start and Enter.
    auto rowHidden = [&](int r) {
        return tab == 2 && GraphicsRowHidden(r);
    };
    if (input & 1)
        do { row = (row + count - 1) % count; } while (rowHidden(row));
    if (input & 2)
        do { row = (row + 1) % count; } while (rowHidden(row));
    if (input & 0x10)
    {
        const int saveRow = tab == 2 ? int(GraphicsRow::Save) : tab == 3 ? SystemSaveRow : -1;
        // Start / Enter shifts focus to Save; inhibit confirm on the same tick so
        // simultaneous input (or key bindings sending both) cannot save from another
        // row. Pressed again on Save it saves, so keyboard Enter confirms like A.
        if (saveRow >= 0 && row == saveRow)
            input |= 0x1000;
        else
        {
            if (saveRow >= 0) row = saveRow;
            input &= ~0x1000;
        }
    }
    if (tab == 3 && row == SystemCollectionRow && (input & 0x000c) && os::log_collection::Supported()) {
        if (os::log_collection::Enabled()) {
            if (!os::log_collection::SetConsent(false)) status = Tr(L"Settings could not be saved.", L"無法儲存設定。");
        } else { collectionPrompt = true; collectionChoice = 1; }
        Publish(base, config); return;
    }
    // A confirms actions/dialogs only. Option values change with Left/Right.
    const int delta = (input & 4) ? -1 : (input & 8) ? 1 : 0;
    auto cycle = [&](uint32_t value, uint32_t count) {
        return uint32_t((int(value) + int(count) + delta) % int(count));
    };
    if (delta)
    {
        if (tab == 0 && row == GamePromptRow)
        {
            // Host setting: applied and saved at once, like Vibration.
            const uint32_t style = cycle(std::min(edit.buttonPrompts, 2u), 3);
            edit.buttonPrompts = style;
            Config saved = GetConfig();
            saved.buttonPrompts = style;
            if (!SaveConfig(saved)) status = Tr(L"Could not save settings.", L"無法儲存設定。");
            hid::SetPromptStyle(style);
        }
        else if (tab == 0 && row == GameVibrationRow)
        {
            // Host setting: applied and saved at once, merged into the saved
            // settings so unsaved Graphics edits stay unsaved. Min turns it off.
            const auto strength = uint32_t(std::clamp(int(edit.vibrationPercent) + delta * 10, 0, 100));
            if (strength != edit.vibrationPercent)
            {
                edit.vibrationPercent = strength;
                Config saved = GetConfig();
                saved.vibrationPercent = strength;
                if (!SaveConfig(saved)) status = Tr(L"Could not save settings.", L"無法儲存設定。");
                hid::SetVibrationStrength(strength);
                hid::PreviewVibration();
            }
        }
        else if (tab == 0 && row < GameRetailRowCount)
        {
            if (row == 0)
                PPC_STORE_U32(config, cycle(PPC_LOAD_U32(config), 3));
            else
            {
                constexpr uint32_t masks[] = {0,          0x40000000, 0x10000000, 0x00800000,
                                              0x08000000, 0x04000000, 0x02000000};
                PPC_STORE_U32(config + 4, PPC_LOAD_U32(config + 4) ^ masks[row]);
            }
            changed = true;
        }
        else if (tab == 1 && row == AudioOutputRow)
        {
            // Host setting beside the retail rows: applied and saved at once.
            edit.audioOutput = cycle(edit.audioOutput, 2);
            apu::SetSurround(edit.audioOutput == AudioOutputSurround);
            if (!SaveAudioOutput(edit.audioOutput))
                status = Tr(L"Could not save settings.", L"無法儲存設定。");
        }
        else if (tab == 1)
        {
            if (row == AudioVoiceRow)
            {
                if (PPC_LOAD_U32(config + 24) >= VoiceCount(base))
                {
                    Publish(base, config);
                    return;
                }
                PPC_STORE_U32(config + 24, cycle(PPC_LOAD_U32(config + 24), VoiceCount(base)));
            }
            else
            {
                auto offset = row == AudioMusicRow ? 8 : 12;
                PPC_STORE_U32(config + offset, std::clamp(int(PPC_LOAD_U32(config + offset)) + delta * 4, 0, 100));
            }
            changed = true;
        }
        else if (tab == 2)
        {
            switch (GraphicsRow(row))
            {
            case GraphicsRow::Backend:
#ifdef _WIN32
                edit.graphicsBackend = GraphicsBackend(cycle(uint32_t(edit.graphicsBackend), 3));
#elif LO_PLATFORM_MACOS
                edit.graphicsBackend = GraphicsBackend::Metal;
#else
                edit.graphicsBackend = GraphicsBackend::Vulkan;
#endif
                break;
            case GraphicsRow::Gpu:
            {
                const auto gpus = GpuChoices();
                const auto choice = cycle(gpus.selected, uint32_t(gpus.names.size() + 1));
                edit.gpuDevice = choice ? gpus.names[choice - 1] : std::string{};
                break;
            }
            case GraphicsRow::DisplayMode:
                edit.windowMode = WindowMode(cycle(uint32_t(edit.windowMode), 2));
                break;
            case GraphicsRow::Display:
            {
                const auto displays = DisplayChoices();
                const auto choice = cycle(displays.selected, uint32_t(displays.labels.size()));
                if (!choice)
                {
                    edit.displayName.clear();
                    edit.displayIndex = 0;
                }
                else if (choice <= displays.displays.size())
                {
                    edit.displayName = displays.displays[choice - 1].name;
                    edit.displayIndex = choice - 1;
                }
                // The last choice keeps a saved display that is not connected.
                break;
            }
            case GraphicsRow::AspectRatio:
            {
                // A fixed shape also moves the output resolution to that shape's
                // list, so a window of that size shows no bars. Auto keeps it.
                edit.aspectRatio = AspectMode(cycle(uint32_t(edit.aspectRatio), gpu::aspect_ratio::ModeCount));
                if (edit.aspectRatio != AspectMode::Auto)
                {
                    const auto list = OutputResolutions(edit.aspectRatio, edit.width, edit.height);
                    const uint32_t index = FindNearestResolutionIndex(list.data(), list.size(), edit.width, edit.height);
                    edit.width = list[index][0];
                    edit.height = list[index][1];
                }
                break;
            }
            case GraphicsRow::OutputResolution:
            {
                const auto list = OutputResolutions(edit.aspectRatio, edit.width, edit.height);
                uint32_t index = 0;
                for (size_t i = 0; i < list.size(); ++i)
                    if (edit.width == list[i][0] && edit.height == list[i][1])
                        index = uint32_t(i);
                index = cycle(index, uint32_t(list.size()));
                edit.width = list[index][0];
                edit.height = list[index][1];
                break;
            }
            case GraphicsRow::RenderResolution:
                edit.internalResolution = graphics_menu::RenderResolutions[
                    cycle(graphics_menu::RenderResolutionChoice(edit), uint32_t(std::size(graphics_menu::RenderResolutions)))];
                break;
            case GraphicsRow::ShadowResolution:
                edit.shadowResolution = graphics_menu::ShadowResolutions[
                    cycle(graphics_menu::ShadowResolutionChoice(edit), uint32_t(std::size(graphics_menu::ShadowResolutions)))];
                break;
            case GraphicsRow::DynamicShadows:
                edit.dynamicShadows = !edit.dynamicShadows;
                break;
            case GraphicsRow::AntiAliasing:
                graphics_menu::SelectAa(edit, cycle(graphics_menu::AaChoice(edit), graphics_menu::AaChoiceCount));
                break;
            case GraphicsRow::AmbientOcclusion:
                edit.ambientOcclusion = cycle(std::min(edit.ambientOcclusion, 2u), 3);
                break;
            case GraphicsRow::DlssQuality:
                if (gpu::upscaling::UsesFsrQuality(edit.upscaler))
                    edit.fsrQuality = gpu::upscaling::FsrQuality(
                        qualityMenuIds[cycle(QualityMenuIndex(uint32_t(edit.fsrQuality)), std::size(qualityMenuIds))]);
                else edit.dlssQuality = gpu::upscaling::DlssQuality(
                    qualityMenuIds[cycle(QualityMenuIndex(uint32_t(edit.dlssQuality)), std::size(qualityMenuIds))]);
                break;
            case GraphicsRow::FsrSharpness:
                edit.fsrSharpnessPercent = uint32_t(std::clamp(int(edit.fsrSharpnessPercent) + delta, 0, 100));
                break;
            case GraphicsRow::DlssNeuralRendering:
                edit.dlssNeuralRendering = cycle(std::min(edit.dlssNeuralRendering, DlssNeuralRenderingMaxPasses),
                                                 DlssNeuralRenderingMaxPasses + 1);
                break;
            case GraphicsRow::AnisotropicFiltering:
            {
                constexpr uint32_t levels[] = {0, 2, 4, 8, 16};
                uint32_t index = edit.anisotropicFiltering == 16 ? 4 : edit.anisotropicFiltering == 8 ? 3 :
                                 edit.anisotropicFiltering == 4 ? 2 : edit.anisotropicFiltering == 2 ? 1 : 0;
                edit.anisotropicFiltering = levels[cycle(index, 5)];
                break;
            }
            case GraphicsRow::DepthOfField:
                edit.depthOfFieldPercent = cycle((std::min(edit.depthOfFieldPercent, 100u) + 5) / 10, 11) * 10;
                break;
            case GraphicsRow::Bloom:
                edit.bloom = !edit.bloom;
                break;
            case GraphicsRow::MotionBlur:
                edit.motionBlur = !edit.motionBlur;
                break;
            case GraphicsRow::Culling:
                edit.cullingPercent = uint32_t(std::clamp(int(std::min(edit.cullingPercent, 200u) + 5) / 10 * 10 + delta * 10, 0, 200));
                break;
            case GraphicsRow::ScalingQuality:
#if LO_PLATFORM_MACOS
                edit.scalingQuality = cycle(edit.scalingQuality, ScalingMetalFx + 1);
#else
                edit.scalingQuality = cycle(edit.scalingQuality, 2);
#endif
                break;
            case GraphicsRow::RgbRange:
                edit.expandRgbRange = !edit.expandRgbRange;
                break;
            case GraphicsRow::FrameRate:
            {
                const auto index = gpu::frame_rate::MenuIndex(edit.frameRate);
                edit.frameRate = gpu::frame_rate::FromMenuIndex(cycle(index, gpu::frame_rate::kCount));
                break;
            }
            case GraphicsRow::VariableRefreshRate:
                edit.variableRefreshRate = !edit.variableRefreshRate;
                break;
            case GraphicsRow::FrameGeneration:
            {
                const auto providers = FgProviders();
                auto found = std::find(providers.begin(), providers.end(), edit.frameGenerationProvider);
                edit.frameGenerationProvider = providers[cycle(uint32_t(found - providers.begin()), uint32_t(providers.size()))];
                edit.frameGenerationMode = framegen::Mode::Fixed;
                edit.frameGenerationTargetFps = 0;
                if (edit.frameGenerationProvider == framegen::Provider::Fsr || edit.frameGenerationProvider == framegen::Provider::MetalFx) edit.frameGenerationMultiplier = 2;
                break;
            }
            case GraphicsRow::FrameGenerationMultiplier:
                if (!GraphicsRowHidden(row)) {
                    edit.frameGenerationMultiplier = cycle(std::clamp(edit.frameGenerationMultiplier, 2u,
                        framegen::kMaxMultiplier) - 2, framegen::kMaxMultiplier - 1) + 2;
                    edit.frameGenerationMode = framegen::Mode::Fixed;
                    edit.frameGenerationTargetFps = 0;
                }
                break;
            case GraphicsRow::Hdr:
                if (graphics_menu::HdrAvailable(edit.graphicsBackend)) edit.hdr = !edit.hdr;
                break;
            case GraphicsRow::HdrPaperWhite:
                if (graphics_menu::HdrAvailable(edit.graphicsBackend))
                    edit.hdrPaperWhiteNits = uint32_t(std::clamp(int(edit.hdrPaperWhiteNits) + delta * 10, 80, 400));
                break;
            case GraphicsRow::HdrPeak:
                if (graphics_menu::HdrAvailable(edit.graphicsBackend))
                {
                    const uint32_t start = edit.hdrPeakAutomatic
                        ? MakeHdrCalibration(edit, false).effectiveNits : edit.hdrPeakNits;
                    edit.hdrPeakAutomatic = false;
                    edit.hdrPeakNits = uint32_t(std::clamp(int(start) + delta * 100,
                        int(edit.hdrPaperWhiteNits), 10000));
                }
                break;
            case GraphicsRow::Brightness:
            case GraphicsRow::Save:
            case GraphicsRow::Count:
                break;
            }
        }
        else
        {
            if (row == SystemUiLanguageRow)
                edit.uiLanguage = cycle(edit.uiLanguage, 5);
            if (row == SystemGameLanguageRow)
                edit.gameLanguage = GameLanguageIds[cycle(GameLanguageIndex(edit.gameLanguage), uint32_t(GameLanguageIds.size()))];
            if (row == SystemUpdatesRow)
                edit.automaticUpdates = !edit.automaticUpdates;
            if (row == SystemDebugLogRow)
                edit.debugLog = !edit.debugLog;
        }
    }
    if (changed)
    {
        language::TraceConfig(base, config, "menu-before-apply");
        PPCContext call = ctx;
        call.r3.u32 = config;
        __imp__sub_82870E38(call, base);
        language::TraceConfig(base, config, "menu-after-apply");
    }
    if ((input & 0x1000) && tab == 0 && row == GameRestoreRow)
    {
        PPCContext call = ctx;
        call.r3.u32 = config;
        language::TraceConfig(base, config, "menu-before-defaults");
        __imp__sub_828710A0(call, base);
        call = ctx;
        call.r3.u32 = config;
        __imp__sub_82870E38(call, base);
        language::TraceConfig(base, config, "menu-after-defaults");
        status = Tr(L"Game defaults restored.", L"遊戲預設設定已恢復。");
    }
    if ((input & 0x1000) && tab == 0 && row == GameMainMenuRow)
    {
        mainMenuPrompt = true;
        mainMenuChoice = 1; // Require an explicit selection of Return; Back always cancels.
        status.clear();
    }
    if ((input & 0x1000) && tab == 3 && row == SystemImportRow)
    {
        importPrompt = true;
        importChoice = 1;
        status.clear();
    }
    if ((input & 0x1000) && tab == 2 && row == int(GraphicsRow::HdrPeak) &&
        graphics_menu::HdrAvailable(edit.graphicsBackend))
    {
        calibrationStartAutomatic = edit.hdrPeakAutomatic;
        calibrationStartPeakNits = edit.hdrPeakNits;
        calibrationNumberEditing = false;
        calibrationNumber.clear();
        calibrationFocus = 0;
        calibrationScenePreview = true;
        calibrationOpen = true;
        Publish(base, config);
        return;
    }
    if ((input & 0x1000) && tab == 2 && row == int(GraphicsRow::Save))
    {
        previousDisplay = GetConfig();
        displayMovesBefore = gpu::video::DisplayMoveCount();
        Config graphics = edit;
        graphics.uiLanguage = previousDisplay.uiLanguage;
        graphics.gameLanguage = previousDisplay.gameLanguage;
        graphics.automaticUpdates = previousDisplay.automaticUpdates;
        graphics.debugLog = previousDisplay.debugLog;
        if (graphics.frameGenerationProvider == framegen::Provider::Fsr || graphics.frameGenerationProvider == framegen::Provider::MetalFx)
            graphics.frameGenerationMultiplier = 2;
        if (!SaveConfig(graphics))
            status = Tr(L"Could not save settings.", L"無法儲存設定。");
        else
        {
            graphics = GetConfig();
            graphics.uiLanguage = edit.uiLanguage;
            graphics.gameLanguage = edit.gameLanguage;
            graphics.automaticUpdates = edit.automaticUpdates;
            graphics.debugLog = edit.debugLog;
            edit = graphics;
            if (edit.width != previousDisplay.width || edit.height != previousDisplay.height ||
                edit.windowMode != previousDisplay.windowMode || edit.displayName != previousDisplay.displayName ||
                edit.displayIndex != previousDisplay.displayIndex || gpu::video::DisplayModeFailed() || gpu::video::WindowModeOverridden())
            {
                displayRollback = false;
                displayTicket = gpu::video::BeginDisplayChange(edit);
                status = Tr(L"Applying display settings…", L"正在套用顯示設定……");
            }
            else graphicsSaved();
        }
        Publish(base, config);
        return;
    }
    if ((input & 0x1000) && tab == 3 && row == SystemSaveRow)
    {
        Config languages = GetConfig();
        languages.uiLanguage = edit.uiLanguage;
        languages.gameLanguage = edit.gameLanguage;
        languages.automaticUpdates = edit.automaticUpdates;
        languages.debugLog = edit.debugLog;
        const Config before = GetConfig();
        if (restart::Required(before, languages))
        {
            restartPrompt = true;
            savedRestartPrompt = false;
            restartSaveFailed = false;
            restartForFgProvider = false;
            restartChoice = 0;
            restartAfter = languages;
        }
        else
            status = SaveConfig(languages) ? Tr(L"System settings saved.", L"系統設定已儲存。")
                                           : Tr(L"Could not save settings.", L"無法儲存設定。");
    }
    // Back in the same poll wins, as it does over every other action.
    if ((input & 0x1000) && !(input & 0x2000) && tab == 2 && row == int(GraphicsRow::Brightness))
    {
        brightnessStart = edit.displayBrightness;
        gammaStart = edit.displayGamma;
        brightnessFocus = 0;
        calibrationScenePreview = true;
        brightnessOpen = true;
        Publish(base, config);
        return;
    }
    if ((input & 0x1000) && !(input & 0x2000) && tab == 2 && row == int(GraphicsRow::DlssNeuralRendering) &&
        !GraphicsRowHidden(row))
    {
        nrStart = edit;
        nrFocus = 0;
        nrOpen = true;
        Publish(base, config);
        return;
    }
    if (input & 0x2000)
    {
        closeSettings();
        return;
    }
    // The status line follows the latest DLSS result, including while the menu sits idle.
    Publish(base, config);
    // Only explicit brightness calibration delegates input to the retail UI.
    // The parent task continues ticking throughout.
#endif
}

bool settings::IsOpen()
{
    return active.load();
}

bool settings::DrawMenu(std::vector<uint32_t> &pixels, uint64_t &revision, uint32_t width, uint32_t height)
{
    // This cache belongs to the sole presentation thread. Dimensions must be
    // checked independently: portrait and landscape buffers can have equal area.
    static uint32_t cachedWidth = 0, cachedHeight = 0;
    static bool cachedPlayStation = false, cachedPreviewPage = false, cachedDialog = false, shown = false, closing = false;
    static int cachedTab = -1;
    static std::shared_ptr<const menu_assets::Assets> cachedAssets;
    // Motion is presentation only: input and menu state change at once, and the
    // shown image moves toward the newest raster (#151), timed like the retail menus.
    static MenuTransition motion;
    static std::vector<uint32_t> raster;
    // The list arrow is a layer over the raster: it slides between rows and sways.
    static MenuArrow arrow;
    static MenuRect arrowDrawn;
    static double arrowY = 0;
    static bool arrowPlaced = false;
    static MenuTransition::Clock::time_point openTime{}, arrowTime{};
    const auto seconds = [](MenuTransition::Clock::duration value) { return std::chrono::duration<double>(value).count(); };
    // Removes the arrow from pixels; Advance must already have run this frame.
    const auto eraseArrow = [&] {
        if (arrowDrawn.x1 > arrowDrawn.x0)
            motion.Erase(pixels, arrowDrawn);
        arrowDrawn = {};
    };
    const auto drawArrow = [&](MenuTransition::Clock::time_point now) {
        if (!arrow.visible || closing)
            return;
        // It waits above the first row while an open starts, then follows its row.
        if (now < openTime + menu_motion::ArrowOpenDelay)
        {
            arrowTime = now;
            return;
        }
        const double dt = std::clamp(seconds(now - arrowTime), 0.0, 1.0);
        arrowTime = now;
        arrowY += (arrow.y - arrowY) * std::clamp(dt * menu_motion::ArrowSpeed, 0.0, 1.0);
        if (std::abs(arrow.y - arrowY) < 0.5)
            arrowY = arrow.y;
        const double phase = seconds(now - openTime) / seconds(menu_motion::ArrowSwayPeriod);
        const int x = arrow.x + int(std::lround(menu_motion::ArrowSway * (0.5 - 0.5 * std::cos(6.283185307179586 * phase))));
        const int y = int(std::lround(arrowY));
        DrawMenuArrow(pixels, width, height, x, y);
        arrowDrawn = MenuArrowBounds(width, height, x, y);
    };
    if (!active.load())
    {
        // Closing: the content fades off the panels before the game's own exit
        // animation shows. The arrow fades with it.
        if (shown && !closing && !cachedPreviewPage && cachedWidth == width && cachedHeight == height && !pixels.empty())
        {
            MenuSnapshot panels;
            panels.assets = cachedAssets;
            panels.backdropOnly = true;
            motion.Advance(pixels, MenuNow());
            arrowDrawn = {};
            closing = RasterizeMenu(panels, width, height, raster) &&
                      motion.Start(pixels, raster, width, MenuNow(), menu_motion::CloseFade) && motion.Running();
        }
        if (closing && motion.Advance(pixels, MenuNow()))
            return true;
        if (shown)
        {
            shown = closing = arrowPlaced = false;
            arrowDrawn = {};
            cachedAssets.reset();
            motion.Reset();
            std::vector<uint32_t>().swap(raster);
        }
        return false;
    }
    if (closing)
    {
        // Reopened while the close was fading: start over.
        shown = closing = arrowPlaced = false;
        motion.Reset();
    }
    MenuSnapshot current;
    {
        std::lock_guard lock(snapshotMutex);
        current = snapshot;
    }
    // Input style can change without a guest menu tick (hot-plug or keyboard).
    current.playStationPrompts = hid::UsesPlayStationPrompts();
    const bool opened = !shown;
    if (!opened && revision == current.revision && cachedWidth == width && cachedHeight == height &&
        cachedPlayStation == current.playStationPrompts && !pixels.empty())
    {
        const auto now = MenuNow();
        motion.Advance(pixels, now);
        eraseArrow();
        drawArrow(now);
        return true;
    }
    current.assets = menu_assets::Cached(FileSystem::GetGameRoot(), current.language);
    MenuArrow nextArrow;
    if (!RasterizeMenu(current, width, height, raster, &nextArrow))
        return false;
    // Ease what the player did: opening (content over the panels) and a new
    // revision at the same size and prompt style. A resize, a controller style
    // change and the calibration pages, whose preview presentation paints
    // into the frame, switch at once. The clock starts after rasterizing (tens
    // of milliseconds at 4K, more on the first open) so no part of it is lost.
    const auto now = MenuNow();
    const bool previewPage = current.calibration.open || current.brightness.open || current.neuralRendering.open;
    const bool dialog = !current.dialogChoices.empty();
    bool eased = false;
    if (opened && !previewPage)
    {
        MenuSnapshot panels;
        panels.assets = current.assets;
        panels.backdropOnly = true;
        eased = RasterizeMenu(panels, width, height, pixels) &&
                motion.StartCascade(pixels, raster, width, now, MenuOpenCascade(width, height));
    }
    else if (!opened && cachedWidth == width && cachedHeight == height &&
             cachedPlayStation == current.playStationPrompts && !previewPage && !cachedPreviewPage)
    {
        // Settle this frame's image first. The arrow goes into the outgoing
        // image only when the new raster draws it itself (under a prompt).
        motion.Advance(pixels, now);
        if (nextArrow.visible)
            eraseArrow();
        arrowDrawn = {};
        const auto fade = current.tab != cachedTab ? menu_motion::PageFade
                        : dialog != cachedDialog   ? menu_motion::DialogFade
                                                   : menu_motion::ChangeFade;
        eased = motion.Start(pixels, raster, width, now, fade);
    }
    if (eased)
        motion.Advance(pixels, now);
    else
        motion.Cut(pixels, raster, width);
    arrowDrawn = {};
    if (opened)
    {
        openTime = arrowTime = now;
        arrowPlaced = false;
    }
    arrow = nextArrow;
    if (arrow.visible && !arrowPlaced)
    {
        arrowY = arrow.y - (opened ? menu_motion::ArrowOpenDrop : 0);
        arrowPlaced = true;
    }
    drawArrow(now);
    shown = true;
    cachedWidth = width;
    cachedHeight = height;
    cachedPlayStation = current.playStationPrompts;
    cachedPreviewPage = previewPage;
    cachedDialog = dialog;
    cachedTab = current.tab;
    cachedAssets = current.assets;
    revision = current.revision;
    return true;
}
