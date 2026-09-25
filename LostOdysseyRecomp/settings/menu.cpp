#include <gpu/taa_collection.h>
#include "menu.h"
#include "menu_render.h"
#include "menu_assets.h"
#include "config.h"
#include "graphics_menu.h"
#include "restart.h"
#include "translations.h"
#include <gpu/video.h>
#include <gpu/frame_plan.h>
#include <kernel/io/file_system.h>
#include <os/logger.h>
#include <stdafx.h>
#include "language_trace.h"
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
int restartChoice = 0;
bool mainMenuPrompt = false;
int mainMenuChoice = 1;
bool mainMenuRequested = false;
Config restartAfter;
int tab = 0, row = 0;
bool bypass = false, sawModal = false;
bool closing = false;
uint32_t lastMenu = 0;
std::wstring status;
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
    if (GetConfig().upscaler == gpu::upscaling::Upscaler::Fsr ||
        (execution && execution->actualProvider == gpu::upscaling::Upscaler::Fsr)) {
        std::wstring fsrText;
        if (execution && execution->actualProvider == gpu::upscaling::Upscaler::Fsr &&
            execution->outcome == gpu::frame_plan::DlssExecutionOutcome::Submitted) {
            const wchar_t* modes[] = {Tr(L"Quality", L"品質"), Tr(L"Balanced", L"平衡"), Tr(L"Performance", L"效能"), L"Native AA"};
            fsrText = std::wstring(L"FSR ") + modes[uint32_t(gpu::upscaling::NormalizeFsrQuality(execution->plan.fsrQuality))] +
                Tr(L" output submitted.", L" 輸出已提交。") + ExecutionSizeSuffix(execution->plan);
            if (edit.upscaler == gpu::upscaling::Upscaler::Fsr && edit.fsrQuality != execution->plan.fsrQuality)
                fsrText += Tr(L" The selected FSR quality is not applied yet.", L" 選取的 FSR 品質尚未套用。");
        } else fsrText = Tr(L"FSR has no submitted output. Normal rendering is used until it is ready.", L"FSR 尚無已提交的輸出。準備完成前使用常規渲染。");
        if (edit.upscaler != gpu::upscaling::Upscaler::Fsr)
            fsrText += Tr(L" The selected upscaler is not applied yet.", L" 選取的縮放技術尚未套用。");
        return fsrText;
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
        text += std::wstring(L" ") + (edit.upscaler == gpu::upscaling::Upscaler::Fsr ? Tr(L"The FSR choice is not applied yet.", L"FSR 選項尚未套用。") : Tr(L"The Off choice is not applied yet.", L"關閉選項尚未套用。"));
    else if (!runningDlss && edit.upscaler == gpu::upscaling::Upscaler::Dlss && !backendPending &&
             running.device.backend != gpu::backend::Backend::Vulkan)
        text += std::wstring(L" ") + Tr(L"DLSS needs Vulkan and a restart.", L"DLSS 需要 Vulkan，並在重新啟動後才會使用。");
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
    return (r == int(GraphicsRow::DlssQuality) && edit.upscaler == gpu::upscaling::Upscaler::Off) ||
           (r == int(GraphicsRow::FsrSharpness) && edit.upscaler != gpu::upscaling::Upscaler::Fsr);
}
static_assert(int(GraphicsRow::Save) + 1 == int(GraphicsRow::Count));
void Publish(uint8_t *base, uint32_t config)
{
    Snapshot next;
    next.tab = tab;
    next.row = row;
    next.language = edit.uiLanguage;
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
    }
    else if (tab == 2)
    {
        next.rows.resize(int(GraphicsRow::Count));
        auto placeGraphics = [&](GraphicsRow id, Row value) {
            next.rows[int(id)] = std::move(value);
        };
#ifdef _WIN32
        placeGraphics(GraphicsRow::Backend, makeChoices(L"Graphics backend", L"圖形後端", {L"Direct3D 12", L"Vulkan", Tr(L"Direct3D 11 (unsupported)", L"Direct3D 11（尚未支援）")},
                   uint32_t(edit.graphicsBackend)));
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
        placeGraphics(GraphicsRow::AntiAliasing, makeChoices(L"Anti-aliasing / Upscaling", L"抗鋸齒 / 超解析度",
                   {Tr(L"Off", L"關"), L"FXAA", L"SMAA", Tr(L"TAA (Experimental)", L"TAA（實驗性）"), L"DLSS", L"FSR 3.1"},
                   graphics_menu::AaChoice(edit)));
        const bool savedFsr = edit.upscaler == gpu::upscaling::Upscaler::Fsr;
        auto dlssQuality = makeChoices(savedFsr ? L"FSR quality" : L"DLSS quality", savedFsr ? L"FSR 品質" : L"DLSS 品質",
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
        placeGraphics(GraphicsRow::ScalingQuality, makeChoices(L"Scaling filter", L"縮放濾鏡",
                   {Tr(L"Standard", L"標準"), Tr(L"High", L"高")},
                   std::min(edit.scalingQuality, 1u)));
        placeGraphics(GraphicsRow::FrameRate, makeChoices(L"Frame rate", L"影格率",
                   {L"30 FPS", std::wstring(L"60 FPS") + Tr(L" (experimental)", L"（實驗性）"),
                    std::wstring(L"120 FPS") + Tr(L" (experimental)", L"（實驗性）")},
                   edit.frameRate == 120 ? 2 : edit.frameRate == 60 ? 1 : 0));
        placeGraphics(GraphicsRow::Brightness, makeChoices(L"Brightness calibration", L"亮度校準", {Tr(L"Open", L"開啟")}, 0));
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
    if (tab >= 0 && tab < 4)
    {
        static int scrollByTab[4] = {0, 0, 0, 0};
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
    if (tab == 3 && row == 1)
        next.help = Tr(L"Game language takes effect after restarting. Requires matching language assets.",
                       L"遊戲語言重新啟動後生效，需要對應語言資源。中文遊戲文本需要亞洲版資源。");
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
                selected == gpu::backend::Backend::D3D12 ? L"Direct3D 12" : L"-";
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
        case GraphicsRow::AntiAliasing:
            if (edit.upscaler == gpu::upscaling::Upscaler::Fsr)
                next.help = Tr(L"FSR 3.1 needs Vulkan and an FSR-enabled build. Unsupported scenes use normal rendering.",
                              L"FSR 3.1 需要 Vulkan 與包含 FSR 的版本。不支援的場景使用常規渲染。");
            else if (edit.upscaler == gpu::upscaling::Upscaler::Dlss)
                next.help = Tr(L"Saves the DLSS preference. The status line shows the latest DLSS result.",
                              L"儲存 DLSS 偏好。狀態列顯示最新的 DLSS 結果。");
            else if (edit.antialiasing == 3)
                next.help = Tr(L"Camera-based TAA; moving effects may trail. Unsupported scenes use SMAA.",
                              L"以相機重投影的 TAA；動態特效可能拖影。不支援的場景使用 SMAA。");
            break;
        case GraphicsRow::DlssQuality:
            next.help = edit.upscaler == gpu::upscaling::Upscaler::Fsr ?
                Tr(L"Performance, Balanced, Quality, or Native AA. Native AA keeps the output resolution.", L"效能、平衡、品質或 Native AA。Native AA 維持輸出解析度。") : Tr(L"Performance, Balanced, Quality, or DLAA. The status line shows the submitted mode.",
                           L"效能、平衡、品質或 DLAA。狀態列顯示已提交的模式。");
            break;
        case GraphicsRow::FsrSharpness:
            next.help = Tr(L"FSR sharpening: Off disables RCAS; 1-100% sets sharpening strength.",
                           L"FSR 銳化：關閉會停用 RCAS；1-100% 調整銳化強度。");
            break;
        case GraphicsRow::ScalingQuality:
            next.help = Tr(L"Controls filtering when upscaling is active.",
                           L"控制啟用縮放時的取樣濾鏡。");
            break;
        case GraphicsRow::FrameRate:
            next.help = edit.frameRate == 120
                ? Tr(L"120 FPS is experimental and requires LO_EXPERIMENTAL_120; otherwise runs at 60 FPS.",
                     L"120 FPS 為實驗性功能，需啟用 LO_EXPERIMENTAL_120，否則以 60 FPS 執行。")
                : Tr(L"60/120 FPS are experimental. Verify game speed, audio and battle timing.",
                     L"60/120 FPS 為實驗性功能，請確認遊戲速度、音訊與戰鬥時序。");
            break;
        case GraphicsRow::DisplayMode:
        case GraphicsRow::Brightness:
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
    next.notice = tab == 2 ? DlssNotice() : std::wstring{};
    std::lock_guard lock(snapshotMutex);
    if (next.tab == snapshot.tab && next.row == snapshot.row && next.scroll == snapshot.scroll && next.language == snapshot.language &&
        next.rows == snapshot.rows && next.help == snapshot.help && next.notice == snapshot.notice && next.dialogTitle == snapshot.dialogTitle &&
        next.dialogMessage == snapshot.dialogMessage && next.dialogChoices == snapshot.dialogChoices &&
        next.dialogSelection == snapshot.dialogSelection)
        return;
    next.revision = snapshot.revision + 1;
    snapshot = std::move(next);
}
} // namespace
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
        mouseTab = int(x - 386) / 160;
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
        // Pointer adjustment is explicit left/right, not a synthetic A press.
        // Keep confirmation reserved for action rows and modal dialogs.
        mouseAction = graphics_menu::IsAction(snapshot.tab, int(hit))
            ? (reverse ? 0 : 0x1000) : (reverse || (snapshot.rows[hit].choices.size() > 5 && x < 458) ? 4 : 8);
    }
}
} // namespace settings

// Resolve the explicit host choice instead of the retail language allowlist's
// default alias. Resource suffixes come from the original executable's table.
// IDs 1-9 map to INT/JPN/DEU/FRA/SPA/ITA/KOR/CHI/SCH. The original function
// aliases languages missing from the runtime allowlist to the ID-0 record, so
// Europe text languages and Simplified Chinese must return the table pointer.
PPC_FUNC(sub_82481BE8)
{
    const uint32_t language = settings::GameLanguage();
    const auto object = ctx.r3.u32, request = ctx.r4.u32, caller = uint32_t(ctx.lr);
    if (settings::language::ResourceOverride(language, object, request))
    {
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
            bypass = sawModal = false;
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
        edit = GetConfig();
        collectionPrompt = gpu::taa_collection::Consent() < 0;
        collectionChoice = 1;
        pending = 0;
        waitForRelease = true;
        restartPrompt = false;
        savedRestartPrompt = false;
        restartSaveFailed = false;
        mainMenuPrompt = false;
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
    if (restart::ConsumeLaunchFailure())
    {
        status = Tr(L"Restart could not be started. This game is still running; your saved settings are safe.",
                    L"無法啟動重新啟動程序。本遊戲仍在執行，已儲存的設定安全保留。");
        Publish(base, config);
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
                status = Tr(L"Changes requiring restart were cancelled.", L"已取消需要重新啟動的變更。");
            }
            else if (SaveConfig(restartAfter))
            {
                edit = restartAfter;
                restartPrompt = false;
                restartSaveFailed = false;
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
    auto graphicsSaved = [&] {
        status = Tr(L"Display settings saved.", L"顯示設定已儲存。");
        if (restart::Required(previousDisplay, edit))
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
        tab = (tab + ((input & 0x200) ? 1 : 3)) % 4;
        row = 0;
        status.clear();
    }
    const int count = tab == 0 ? GameMainMenuRow + 1 : tab == 1 ? 3 : tab == 2 ? int(GraphicsRow::Count) : 5;
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
            case GraphicsRow::AntiAliasing:
                graphics_menu::SelectAa(edit, cycle(graphics_menu::AaChoice(edit), graphics_menu::AaChoiceCount));
                break;
            case GraphicsRow::DlssQuality:
                if (edit.upscaler == gpu::upscaling::Upscaler::Fsr)
                    edit.fsrQuality = gpu::upscaling::FsrQuality(
                        qualityMenuIds[cycle(QualityMenuIndex(uint32_t(edit.fsrQuality)), std::size(qualityMenuIds))]);
                else edit.dlssQuality = gpu::upscaling::DlssQuality(
                    qualityMenuIds[cycle(QualityMenuIndex(uint32_t(edit.dlssQuality)), std::size(qualityMenuIds))]);
                break;
            case GraphicsRow::FsrSharpness:
                edit.fsrSharpnessPercent = uint32_t(std::clamp(int(edit.fsrSharpnessPercent) + delta, 0, 100));
                break;
            case GraphicsRow::ScalingQuality:
                edit.scalingQuality = cycle(edit.scalingQuality, 2);
                break;
            case GraphicsRow::FrameRate:
            {
                constexpr uint32_t rates[] = {30, 60, 120};
                const uint32_t index = edit.frameRate == 120 ? 2u : edit.frameRate == 60 ? 1u : 0u;
                edit.frameRate = rates[cycle(index, 3)];
                break;
            }
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
    if ((input & 0x1000) && tab == 2 && row == int(GraphicsRow::Save))
    {
        previousDisplay = GetConfig();
        if (!SaveConfig(edit))
            status = Tr(L"Could not save settings.", L"無法儲存設定。");
        else
        {
            edit = GetConfig();
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
            restartChoice = 0;
            restartAfter = languages;
        }
        else
            status = SaveConfig(languages) ? Tr(L"Language settings saved.", L"語言設定已儲存。")
                                           : Tr(L"Could not save settings.", L"無法儲存設定。");
    }
    if ((input & 0x1000) && tab == 2 && row == int(GraphicsRow::Brightness))
    {
        // Hand the original calibration screen its own brightness row.
        const uint32_t list = menu + 0x558, table = PPC_LOAD_U32(list + 0x84);
        // Internal row 11 is screen position; the visible Brightness row is 12.
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
    // This cache belongs to the sole presentation thread. Dimensions must be
    // checked independently: portrait and landscape buffers can have equal area.
    static uint32_t cachedWidth = 0, cachedHeight = 0;
    if (revision == current.revision && cachedWidth == width && cachedHeight == height && !pixels.empty())
        return true;
    current.assets = menu_assets::Cached(FileSystem::GetGameRoot(), current.language);
    if (!RasterizeMenu(current, width, height, pixels))
        return false;
    cachedWidth = width;
    cachedHeight = height;
    revision = current.revision;
    return true;
}
