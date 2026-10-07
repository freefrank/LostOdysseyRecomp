#include <gpu/taa_collection.h>
#include "menu.h"
#include "menu_render.h"
#include "menu_assets.h"
#include "config.h"
#include "graphics_menu.h"
#include <gpu/frame_rate.h>
#include "restart.h"
#include "translations.h"
#include <gpu/video.h>
#include <gpu/frame_plan.h>
#include <gpu/frame_generation_settings.h>
#include <kernel/io/file_system.h>
#include <os/logger.h>
#include <stdafx.h>
#include "language_trace.h"
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
std::mutex snapshotMutex;
using Row = MenuRow;
using Snapshot = MenuSnapshot;
Snapshot snapshot;
Config edit;
Config previousDisplay;
uint64_t displayTicket = 0;
bool displayRollback = false, rollbackSaveFailed = false;
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
#if LO_PLATFORM_ANDROID
    // Android owns the native surface; the renderer derives aspect from its drawable.
    // NGX and frame generation have no Android providers in this build.
    if (r == int(GraphicsRow::Backend) || r == int(GraphicsRow::DisplayMode) ||
        r == int(GraphicsRow::Widescreen) || r == int(GraphicsRow::OutputResolution) ||
        r == int(GraphicsRow::VariableRefreshRate) || r == int(GraphicsRow::FrameGeneration) ||
        r == int(GraphicsRow::FrameGenerationMultiplier))
        return true;
    if (r == int(GraphicsRow::DlssQuality) || r == int(GraphicsRow::FsrSharpness))
        return !graphics_menu::AndroidFsrAvailable || edit.upscaler != gpu::upscaling::Upscaler::Fsr;
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
void Publish(uint8_t *base, uint32_t config)
{
#if LO_PLATFORM_ANDROID
    // The first four graphics ids are hidden; enter the tab on a visible row.
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
        addAction(L"Restore game defaults", L"恢復遊戲預設設定", Tr(L"Restore", L"恢復"));
        addAction(L"Quit to Main Menu", L"退出到主選單", Tr(L"Return", L"返回"));
        addAction(L"Import discs & DLC", L"匯入光碟與 DLC", Tr(L"Open", L"開啟"));
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
        addSlider(L"Vibration", L"震動", edit.vibrationPercent);
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
        placeGraphics(GraphicsRow::DisplayMode, makeChoices(L"Display mode", L"顯示模式",
                   {Tr(L"Windowed", L"視窗"), Tr(L"Borderless fullscreen", L"無邊框全螢幕"),
                    Tr(L"Exclusive fullscreen", L"獨占全螢幕")},
                   uint32_t(edit.windowMode)));
        const bool ultrawide = IsUltrawideAspect(edit.width, edit.height);
        placeGraphics(GraphicsRow::Widescreen, makeChoices(L"Widescreen", L"寬螢幕", onOff(), ultrawide ? 0 : 1));
        std::vector<std::wstring> outputChoices;
        uint32_t outputChoice = 0;
        if (ultrawide)
        {
            for (uint32_t i = 0; i < std::size(resolutions21_9); ++i)
            {
                outputChoices.push_back(std::to_wstring(resolutions21_9[i][0]) + L" × " +
                                        std::to_wstring(resolutions21_9[i][1]));
                if (edit.width == resolutions21_9[i][0] && edit.height == resolutions21_9[i][1]) outputChoice = i;
            }
        }
        else
        {
            for (uint32_t i = 0; i < std::size(resolutions16_9); ++i)
            {
                outputChoices.push_back(std::to_wstring(resolutions16_9[i][0]) + L" × " +
                                        std::to_wstring(resolutions16_9[i][1]));
                if (edit.width == resolutions16_9[i][0] && edit.height == resolutions16_9[i][1]) outputChoice = i;
            }
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
#if LO_PLATFORM_MACOS
        std::vector<std::wstring> aaChoices{Tr(L"Off", L"關"), L"FXAA", L"SMAA", Tr(L"TAA (Experimental)", L"TAA（實驗性）"), L"MetalFX Temporal"};
#elif LO_PLATFORM_ANDROID
        std::vector<std::wstring> aaChoices{Tr(L"Off", L"關"), L"FXAA", L"SMAA", Tr(L"TAA (Experimental)", L"TAA（實驗性）")};
        if (graphics_menu::AndroidFsrAvailable) aaChoices.emplace_back(L"FSR 3.1");
#else
        std::vector<std::wstring> aaChoices{Tr(L"Off", L"關"), L"FXAA", L"SMAA", Tr(L"TAA (Experimental)", L"TAA（實驗性）"), L"DLSS", L"FSR 3.1", L"XeSS"};
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
        for (const auto fps : gpu::frame_rate::kNativeRates) {
            auto label = std::to_wstring(fps) + L" FPS";
            if (fps > 30) label += Tr(L" (experimental)", L"（實驗性）");
            frameRates.push_back(std::move(label));
        }
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
        addAction(L"Save settings", L"儲存設定", Tr(L"Save", L"儲存"));
        next.rows.push_back({gpu::taa_collection::Label(edit.uiLanguage), gpu::taa_collection::Enabled() ? Tr(L"On", L"開") : Tr(L"Off", L"關"), true, {}, 0});
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
    if (tab == 1 && row == 3)
        next.help = Tr(L"Controller vibration strength. Min turns it off. Applies immediately.",
                       L"控制器震動強度。調到最小即關閉。立即套用。");
    if (tab == 3 && row == 1)
        next.help = Tr(L"Game language takes effect after restarting. Requires matching language assets.",
                       L"遊戲語言重新啟動後生效，需要對應語言資源。中文遊戲文本需要亞洲版資源。");
    if (tab == 0 && row == GameImportRow)
        next.help = Tr(L"Close the game to import selected discs or DLC again. Other content and saves stay intact.",
                       L"關閉遊戲並重新匯入所選光碟或 DLC；其他內容與存檔保留。");
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
        case GraphicsRow::Widescreen:
            next.help = Tr(L"Switches resolution choices between 16:9 and 21:9 ultrawide.",
                           L"在 16:9 與 21:9 寬螢幕規格之間切換解析度選項。");
            break;
        case GraphicsRow::OutputResolution:
            next.help = Tr(L"Sets the output size. Borderless fullscreen uses the desktop size.",
                           L"設定輸出尺寸；無邊框全螢幕使用桌面尺寸。");
            break;
        case GraphicsRow::RenderResolution:
#if LO_PLATFORM_MACOS
            next.help = Tr(L"Scene resolution before scaling. Follow output uses the window size in points; Native uses every Retina pixel (4x the work).",
                           L"縮放前的場景解析度。跟隨輸出使用視窗的點尺寸；原生使用全部 Retina 像素（4 倍工作量）。");
#else
            next.help = Tr(L"Scene resolution before scaling to the output. Follow output matches the output size.",
                           L"縮放至輸出前的場景解析度。跟隨輸出與輸出尺寸相同。");
#endif
            break;
        case GraphicsRow::ShadowResolution:
            next.help = Tr(L"Shadow-map resolution multiplier. Higher values need more GPU memory and rendering time. Applies after saving.",
                           L"陰影貼圖解析度倍數。較高倍數需要更多 GPU 記憶體與渲染時間。儲存後套用。");
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
                next.help = Tr(L"Experimental MetalFX frame generation uses 2× on supported GPUs with macOS 26 or later.",
                               L"實驗性 MetalFX 影格生成在 macOS 26 或更新版本及支援的 GPU 上使用 2×。");
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
        next.dialogTitle = gpu::taa_collection::Label(edit.uiLanguage);
        next.dialogMessage = gpu::taa_collection::Message(edit.uiLanguage);
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
    if (next.tab == snapshot.tab && next.row == snapshot.row && next.scroll == snapshot.scroll && next.language == snapshot.language &&
        next.calibration == snapshot.calibration && next.brightness == snapshot.brightness &&
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
    const bool brightnessKey = brightnessOpen.load() && (key == 13 || key == 27);
    if (!brightnessKey && (!calibrationOpen.load() || !(key == 8 || key == 13 || key == 27 || (key >= '0' && key <= '9'))))
        return false;
    std::lock_guard lock(calibrationKeyMutex);
    calibrationKeys.push_back(key);
    return true;
}
void PointerDrag(float x, float y, bool held)
{
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
               (snapshot.rows[hit].choices.size() > 5 && x < 458) ? 4 : 8);
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
        calibrationOpen = false;
        calibrationNumberEditing = false;
        calibrationNumber.clear();
        calibrationClick = -1;
        calibrationDragNits = -1;
        collectionPrompt = gpu::taa_collection::Consent() < 0;
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
        status.clear();
        Publish(base, config);
        LOG_INFO("settings: replacement opened at guest menu {:#x}", menu);
        language::TraceConfig(base, config, "menu-open");
        for (uint32_t i = 0; i < VoiceCount(base); ++i)
            LOG_INFO("settings: voice option {} -> language {}", i, VoiceLanguage(base, i));
    }
    uint16_t input = pending.exchange(0);
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
            if (gpu::taa_collection::SetConsent(collectionChoice == 0)) { collectionPrompt = false; status.clear(); }
            else status = Tr(L"Settings could not be saved.", L"無法儲存設定。");
        }
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
        if (displayRollback)
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
    const int count = tab == 0 ? GameImportRow + 1 : tab == 1 ? 4 : tab == 2 ? int(GraphicsRow::Count) : 5;
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
        if (tab == 2)
            row = int(GraphicsRow::Save);
        else if (tab == 3)
            row = 3;
        // Start / Enter only shifts focus to Save; inhibit confirm on the same tick
        // so simultaneous input (or key bindings sending both) cannot trigger saving.
        input &= ~0x1000;
    }
    if (tab == 3 && row == 4 && (input & 0x000c)) {
        if (gpu::taa_collection::Enabled()) {
            if (!gpu::taa_collection::SetConsent(false)) status = Tr(L"Settings could not be saved.", L"無法儲存設定。");
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
        if (tab == 0 && row < GameRestoreRow)
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
        else if (tab == 1 && row == 3)
        {
            // Host setting beside the retail sliders: applied and saved at once,
            // merged into the saved settings so unsaved Graphics edits stay unsaved.
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
        else if (tab == 1)
        {
            if (row == 0)
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
                auto offset = row == 1 ? 8 : 12;
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
            case GraphicsRow::DisplayMode:
                edit.windowMode = WindowMode(cycle(uint32_t(edit.windowMode), 3));
                break;
            case GraphicsRow::Widescreen:
            {
                const bool currentUltrawide = IsUltrawideAspect(edit.width, edit.height);
                const bool newUltrawide = !currentUltrawide;
                const auto &targetList = newUltrawide ? resolutions21_9 : resolutions16_9;
                const size_t targetCount = newUltrawide ? std::size(resolutions21_9) : std::size(resolutions16_9);
                uint32_t targetIndex = FindNearestResolutionIndex(targetList, targetCount, edit.width, edit.height);
                edit.width = targetList[targetIndex][0];
                edit.height = targetList[targetIndex][1];
                break;
            }
            case GraphicsRow::OutputResolution:
            {
                const bool ultrawide = IsUltrawideAspect(edit.width, edit.height);
                const auto &list = ultrawide ? resolutions21_9 : resolutions16_9;
                const size_t listCount = ultrawide ? std::size(resolutions21_9) : std::size(resolutions16_9);
                uint32_t index = 0;
                for (size_t i = 0; i < listCount; ++i)
                    if (edit.width == list[i][0] && edit.height == list[i][1])
                        index = uint32_t(i);
                index = cycle(index, uint32_t(listCount));
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
            if (row == 0)
                edit.uiLanguage = cycle(edit.uiLanguage, 5);
            if (row == 1)
                edit.gameLanguage = GameLanguageIds[cycle(GameLanguageIndex(edit.gameLanguage), uint32_t(GameLanguageIds.size()))];
            if (row == 2)
                edit.automaticUpdates = !edit.automaticUpdates;
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
    if ((input & 0x1000) && tab == 0 && row == GameImportRow)
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
        Config graphics = edit;
        graphics.uiLanguage = previousDisplay.uiLanguage;
        graphics.gameLanguage = previousDisplay.gameLanguage;
        graphics.automaticUpdates = previousDisplay.automaticUpdates;
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
            edit = graphics;
            if (edit.width != previousDisplay.width || edit.height != previousDisplay.height ||
                edit.windowMode != previousDisplay.windowMode || gpu::video::DisplayModeFailed() || gpu::video::WindowModeOverridden())
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
    if ((input & 0x1000) && tab == 3 && row == 3)
    {
        Config languages = GetConfig();
        languages.uiLanguage = edit.uiLanguage;
        languages.gameLanguage = edit.gameLanguage;
        languages.automaticUpdates = edit.automaticUpdates;
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
            status = SaveConfig(languages) ? Tr(L"Language settings saved.", L"語言設定已儲存。")
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
    if (!active.load())
        return false;
    MenuSnapshot current;
    {
        std::lock_guard lock(snapshotMutex);
        current = snapshot;
    }
    // Input style can change without a guest menu tick (hot-plug or keyboard).
    current.playStationPrompts = hid::UsesPlayStationPrompts();
    // This cache belongs to the sole presentation thread. Dimensions must be
    // checked independently: portrait and landscape buffers can have equal area.
    static uint32_t cachedWidth = 0, cachedHeight = 0;
    static bool cachedPlayStation = false;
    if (revision == current.revision && cachedWidth == width && cachedHeight == height &&
        cachedPlayStation == current.playStationPrompts && !pixels.empty())
        return true;
    current.assets = menu_assets::Cached(FileSystem::GetGameRoot(), current.language);
    if (!RasterizeMenu(current, width, height, pixels))
        return false;
    cachedWidth = width;
    cachedHeight = height;
    cachedPlayStation = current.playStationPrompts;
    revision = current.revision;
    return true;
}
