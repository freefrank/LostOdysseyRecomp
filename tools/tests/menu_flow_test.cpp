// Exercise the production hook, substituting only its guest-call boundaries.
// No game, renderer device, window, save or profile is started by this fixture.
#include <stdafx.h>
#include <gpu/video.h>
#include <settings/config.h>
#include <fstream>
#include <stdexcept>
namespace gpu::video {
plume::RenderDevice* MenuFlowTestDevice();
inline bool WindowModeOverridden() { return false; }
inline std::optional<gpu::backend::Backend> SelectedBackend() { return gpu::backend::Backend::D3D12; }
}
#include <kernel/io/file_system.h>
inline std::filesystem::path FileSystem::GetGameRoot() { return {}; }
namespace gpu::taa_collection {
inline const wchar_t* Label(uint32_t) { return L"Collection"; }
inline const wchar_t* Message(uint32_t) { return L"Message"; }
inline bool Enabled() { return false; }
inline int Consent() { return 0; }
inline bool SetConsent(bool) { return true; }
}
namespace settings {
Config MenuFlowGetConfig();
bool MenuFlowSaveConfig(const Config&);
void MenuFlowPreviewConfig(const Config&);
inline uint32_t GameLanguage() { return 1; }
}
namespace gpu::video {
uint64_t MenuFlowBeginDisplayChange(const settings::Config&);
DisplayChangeResult MenuFlowQueryDisplayChange(uint64_t);
bool MenuFlowDisplayModeFailed();
}
#define GetConfig MenuFlowGetConfig
#define SaveConfig MenuFlowSaveConfig
#define PreviewConfig MenuFlowPreviewConfig
#define BeginDisplayChange MenuFlowBeginDisplayChange
#define QueryDisplayChange MenuFlowQueryDisplayChange
#define DisplayModeFailed MenuFlowDisplayModeFailed
#define GetDevice MenuFlowTestDevice
#define __imp__sub_822F19B0 MenuFlowOriginalTick
#define __imp__sub_82481BE8 MenuFlowOriginalLanguage
#define __imp__sub_82870E38 MenuFlowApply
#define __imp__sub_828710A0 MenuFlowDefaults
#define __imp__sub_82889E50 MenuFlowClose
#define Translate MenuFlowTranslate
#include "../../LostOdysseyRecomp/settings/menu.cpp"
#undef Translate
#undef GetConfig
#undef SaveConfig
#undef PreviewConfig
#undef BeginDisplayChange
#undef QueryDisplayChange
#undef DisplayModeFailed
#undef GetDevice
#undef __imp__sub_822F19B0
#undef __imp__sub_82481BE8
#undef __imp__sub_82870E38
#undef __imp__sub_828710A0
#undef __imp__sub_82889E50

using settings::GraphicsRow;

namespace gpu::frame_plan {
DlssEffectSnapshot menuFlowDlssEffect{};
DlssEffectSnapshot CurrentDlssEffect() { return menuFlowDlssEffect; }
}

namespace {
constexpr uint32_t Menu = 0x10000, ConfigData = 0x21000;
bool deviceReady = true;
unsigned ticks = 0, applies = 0, closes = 0;
std::vector<char> calls;
settings::Config currentConfig{}, diskConfig{};
unsigned saves = 0, previews = 0, requests = 0;
bool saveFails = false, modeFailed = false;
gpu::video::DisplayChangeTracker displayChanges;
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
bool covers(const settings::menu_assets::Font& font, const std::wstring& text)
{
    return std::all_of(text.begin(), text.end(), [&](wchar_t c) {
        return c == L' ' || font.glyphs.contains(uint32_t(c));
    });
}
void Poll(uint16_t buttons, bool consumed, int16_t x = 0, int16_t y = 0)
{
    const auto original = buttons;
    Require(settings::FilterInput(buttons, x, y) == consumed, "input ownership");
    Require(buttons == (consumed ? 0 : original), "filtered buttons");
}
void Tick(uint8_t* base)
{
    PPCContext ctx{};
    ctx.r3.u64 = Menu;
    ctx.r1.u64 = 0x1ff00;
    ctx.r31.u64 = 0x1234567812345678;
    ctx.f1.f64 = 1.0 / 60;
    ctx.lr = 0x822DBFA4;
    const auto before = ctx;
    sub_822F19B0(ctx, base);
    Require(std::memcmp(&ctx, &before, sizeof(ctx)) == 0, "hook must isolate helper context mutations");
}
void WriteBmp(const std::filesystem::path& path, const std::vector<uint32_t>& rgba)
{
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + uint32_t(rgba.size() * 4);
    info.biSize = sizeof(info); info.biWidth = 1280; info.biHeight = -720;
    info.biPlanes = 1; info.biBitCount = 32;
    auto bgra = rgba;
    for (auto& p : bgra) p = (p & 0xff00ff00u) | ((p & 0xff) << 16) | ((p >> 16) & 0xff);
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(&file), sizeof(file));
    stream.write(reinterpret_cast<const char*>(&info), sizeof(info));
    stream.write(reinterpret_cast<const char*>(bgra.data()), bgra.size() * 4);
    Require(bool(stream), "write menu preview");
}
}
plume::RenderDevice* gpu::video::MenuFlowTestDevice()
{
    return deviceReady ? reinterpret_cast<plume::RenderDevice*>(1) : nullptr;
}
settings::Config settings::MenuFlowGetConfig() { return currentConfig; }
bool settings::MenuFlowSaveConfig(const Config& value)
{
    ++saves;
    if (saveFails) return false;
    currentConfig = diskConfig = value;
    return true;
}
void settings::MenuFlowPreviewConfig(const Config& value) { ++previews; currentConfig = value; }
uint64_t gpu::video::MenuFlowBeginDisplayChange(const settings::Config& value)
{
    ++requests;
    return displayChanges.Begin(value.width, value.height, uint32_t(value.windowMode));
}
gpu::video::DisplayChangeResult gpu::video::MenuFlowQueryDisplayChange(uint64_t ticket)
{
    return displayChanges.Query(ticket);
}
bool gpu::video::MenuFlowDisplayModeFailed() { return modeFailed; }
extern "C" PPC_FUNC(MenuFlowOriginalTick)
{
    Require(ctx.r3.u32 == Menu, "original tick menu pointer");
    ++ticks;
    // Model only the documented completion boundary; the real animation is
    // retained in production and checked by the separate live entry/exit run.
    if (PPC_LOAD_U32(Menu + 4) == 3) PPC_STORE_U32(Menu + 4, 1);
}
extern "C" PPC_FUNC(MenuFlowApply)
{
    Require(ctx.r3.u32 == ConfigData, "apply must receive config pointer");
    ++applies; calls.push_back('A');
    ctx.r3.u64 = 0xDEADBEEF; ctx.r1.u64 = 0; ctx.r31.u64 = 0;
}
extern "C" PPC_FUNC(MenuFlowClose)
{
    Require(ctx.r3.u32 == Menu && ctx.r1.u32 == 0x1ff00, "close must receive original menu/context");
    Require(!calls.empty() && calls.back() == 'A', "apply precedes close");
    Require(PPC_LOAD_U32(Menu + 0x1804) == 0, "no retail confirmation requested");
    ++closes; calls.push_back('C');
    PPC_STORE_U32(Menu + 4, 3);
    ctx.r3.u64 = 0; ctx.r31.u64 = 0;
}
extern "C" PPC_FUNC(MenuFlowOriginalLanguage) { (void)ctx; (void)base; }
extern "C" PPC_FUNC(MenuFlowDefaults) { (void)ctx; (void)base; }

std::string Utf8(const std::wstring& text)
{
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(size > 1 ? size - 1 : 0, '\0');
    if (size > 1) WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, out.data(), size, nullptr, nullptr);
    return out;
}
int BrightGlyphs(const std::vector<uint32_t>& pixels, int y0, int y1)
{
    int count = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = 131; x < 1190; ++x)
        {
            const auto p = pixels[size_t(y) * 1280 + x];
            if (int(p & 255) + int((p >> 8) & 255) + int((p >> 16) & 255) > 500) ++count;
        }
    return count;
}
bool OptionRowsMatch(const std::vector<uint32_t>& withNotice, const std::vector<uint32_t>& without)
{
    for (int y = 0; y < 640; ++y)
        for (int x = 0; x < 1280; ++x)
            if (withNotice[size_t(y) * 1280 + x] != without[size_t(y) * 1280 + x]) return false;
    return true;
}
// Stub observation only. This does not claim a production GPU submission.
// The menu trusts phase and the snapshot reason; it does not read outcome or ids.
gpu::frame_plan::DlssExecutionObservation SubmittedObservation(gpu::upscaling::DlssQuality quality,
    uint32_t inW = 0, uint32_t inH = 0, uint32_t outW = 0, uint32_t outH = 0)
{
    gpu::frame_plan::DlssExecutionObservation observed;
    observed.plan.requestedUpscaler = gpu::upscaling::Upscaler::Dlss;
    observed.plan.dlssQuality = quality;
    observed.plan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
    observed.plan.width = inW;
    observed.plan.height = inH;
    observed.plan.output.width = outW;
    observed.plan.output.height = outH;
    observed.renderFrame = 8675309;
    observed.submissionSerial = 424242;
    observed.outcome = gpu::frame_plan::DlssExecutionOutcome::Submitted;
    observed.reason = gpu::frame_plan::DlssEffectReason::None;
    return observed;
}
void CheckBr03DlssMenu(uint8_t* base)
{
    using gpu::backend::Backend;
    using gpu::frame_plan::DlssEffectPhase;
    using gpu::frame_plan::DlssEffectReason;
    using gpu::upscaling::DlssQuality;
    using gpu::upscaling::SizingState;
    using gpu::upscaling::Upscaler;
    const auto needsVulkan = std::wstring(L"DLSS needs Vulkan and a restart.");
    const auto evidence = std::filesystem::current_path() / "out" / "br03-dlss-menu";
    std::filesystem::create_directories(evidence);
    std::ofstream notes(evidence / "notices.txt", std::ios::binary);
    auto saveState = [&](const char* name) {
        auto preview = settings::snapshot;
        preview.assets.reset();
        std::vector<uint32_t> pixels, plain;
        Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "BR-03 raster");
        Require(BrightGlyphs(pixels, 672, 692) > 20, "status line is painted in the help bar");
        auto cleared = preview;
        cleared.notice.clear();
        Require(settings::RasterizeMenu(cleared, 1280, 720, plain), "BR-03 raster without notice");
        Require(OptionRowsMatch(pixels, plain), "status line does not move the option rows");
        int differ = 0;
        for (int y = 650; y < 694; ++y)
            for (int x = 116; x < 1210; ++x)
                differ += pixels[size_t(y) * 1280 + x] != plain[size_t(y) * 1280 + x];
        Require(differ > 20, "renderer paints snapshot.notice");
        WriteBmp(evidence / name, pixels);
        notes << name << "\t" << Utf8(preview.notice) << "\n";
    };

    deviceReady = true;
    settings::restartPrompt = false;
    settings::collectionPrompt = false;
    settings::displayTicket = 0;
    settings::closing = false;
    settings::bypass = false;
    currentConfig.uiLanguage = 0;
    currentConfig.graphicsBackend = settings::GraphicsBackend::D3D12;
    currentConfig.upscaler = Upscaler::Dlss;
    currentConfig.dlssQuality = DlssQuality::Quality;
    settings::active = false;
    PPC_STORE_U32(Menu + 4, 4);
    gpu::frame_plan::DlssEffectSnapshot running{};
    running.device.backend = Backend::D3D12;
    running.device.deviceReady = true;
    running.phase = DlssEffectPhase::NeedsVulkanRestart;
    gpu::frame_plan::menuFlowDlssEffect = running;
    Tick(base);
    Require(settings::active, "BR-03 menu is open");
    settings::tab = 2;
    settings::row = int(GraphicsRow::Upscaler);
    settings::status.clear();
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == needsVulkan, "D3D12 DLSS shows Vulkan restart");
    Require(settings::snapshot.rows.size() == size_t(GraphicsRow::Count), "BR-03 keeps every graphics row");
    Require(settings::snapshot.rows[int(GraphicsRow::Upscaler)].enabled && settings::snapshot.rows[int(GraphicsRow::Upscaler)].choices.size() == 2, "DLSS choice stays enabled on D3D12");
    Require(!settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden && settings::snapshot.rows[int(GraphicsRow::DlssQuality)].enabled, "quality row stays available");
    Require(settings::snapshot.rows[int(GraphicsRow::Backend)].enabled && settings::snapshot.rows[int(GraphicsRow::Backend)].choices.size() == 3, "backend choices stay available");
    Require(settings::snapshot.help == L"Saves the DLSS preference. The status line shows the latest DLSS result.",
            "upscaler help points at the status line");
    saveState("01-d3d12-needs-vulkan.bmp");
    settings::row = int(GraphicsRow::DlssQuality);
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.help == L"Quality, Balanced, Performance, or DLAA. The status line shows the submitted mode.",
            "quality help names the submitted mode");
    settings::row = int(GraphicsRow::Upscaler);
    settings::pending = 0;
    Tick(base);
    settings::pending = 8;
    Tick(base);
    Require(settings::edit.upscaler == Upscaler::Off && settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden, "DLSS can be turned off on D3D12");
    Require(settings::snapshot.notice == needsVulkan + L" The Off choice is not applied yet.",
            "turning DLSS off before it is applied does not claim the plan is off");
    settings::pending = 8;
    Tick(base);
    Require(settings::edit.upscaler == Upscaler::Dlss && settings::snapshot.notice == needsVulkan, "DLSS can be turned back on");

    running = {};
    running.device.backend = Backend::Vulkan;
    running.device.deviceReady = true;
    running.device.dlssAvailable = false;
    running.phase = DlssEffectPhase::DeviceUnavailable;
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::edit.graphicsBackend = settings::GraphicsBackend::Vulkan;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS is not available on this device.", "Vulkan device unavailable");
    Require(settings::snapshot.rows[int(GraphicsRow::Upscaler)].enabled && !settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden, "unavailable device does not lock DLSS");
    saveState("02-vulkan-device-unavailable.bmp");

    running.device.dlssAvailable = true;
    running.device.deviceReady = true;
    running.phase = DlssEffectPhase::TemporaryFallback;
    running.reason = DlssEffectReason::SizingPending;
    running.hasPlan = true;
    running.plannedRequest = Upscaler::Dlss;
    running.plannedQuality = DlssQuality::Quality;
    running.sizingKnown = true;
    running.sizingState = SizingState::Error;
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS is querying the render resolution. Normal rendering is used for now.",
            "size pending comes from the classified reason, not sizingState");
    saveState("03-size-pending.bmp");

    running.device.deviceReady = true;
    running.device.dlssAvailable = true;
    running.reason = DlssEffectReason::DeviceNotReady;
    running.sizingState = SizingState::Ready;
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS is waiting for the graphics device. Normal rendering is used for now.",
            "device wait comes from the classified reason, not deviceReady");

    running.device.deviceReady = true;
    running.device.dlssAvailable = true;
    running.phase = DlssEffectPhase::Active;
    running.reason = DlssEffectReason::None;
    running.plannedQuality = DlssQuality::Performance;
    running.inputWidth = 111;
    running.inputHeight = 222;
    running.outputWidth = 333;
    running.outputHeight = 444;
    running.execution = SubmittedObservation(DlssQuality::Quality);
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::edit.dlssQuality = DlssQuality::Balanced;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"Submitted DLSS Quality output. The selected DLSS quality is not applied yet.",
            "editing quality does not replace the submitted Quality output");
    Require(settings::snapshot.notice.find(L"Performance") == std::wstring::npos,
            "CPU plan quality is not described as the submitted mode");
    Require(settings::snapshot.notice.find(L"111") == std::wstring::npos, "CPU plan size is not shown for Active");

    const auto savesBefore = saves;
    running.execution = SubmittedObservation(DlssQuality::Quality, 1707, 960, 2560, 1440);
    running.plannedQuality = DlssQuality::Balanced;
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::edit.dlssQuality = DlssQuality::Quality;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"Submitted DLSS Quality output. 1707×960 - 2560×1440",
            "active text names the submitted output and its sizes");
    Require(settings::snapshot.notice.find(L"8675309") == std::wstring::npos, "status hides the render frame id");
    Require(settings::snapshot.notice.find(L"424242") == std::wstring::npos, "status hides the submission serial");
    Require(settings::snapshot.notice.find(L"Balanced") == std::wstring::npos, "CPU plan quality is not the submitted mode");
    Require(saves == savesBefore, "status refresh does not save");
    saveState("04-active-frame-plan.bmp");

    running.execution = SubmittedObservation(DlssQuality::Dlaa, 2560, 1440, 2560, 1440);
    running.plannedQuality = DlssQuality::Quality;
    running.inputWidth = 11;
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::edit.dlssQuality = DlssQuality::Dlaa;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"Submitted DLAA output. 2560×1440 - 2560×1440", "DLAA names the submitted output");
    Require(settings::snapshot.notice.find(L"Quality") == std::wstring::npos, "DLAA does not keep the CPU plan mode");

    running = {};
    running.device.backend = Backend::D3D12;
    running.device.deviceReady = true;
    running.phase = DlssEffectPhase::NeedsVulkanRestart;
    running.plannedRequest = Upscaler::Dlss;
    running.plannedQuality = DlssQuality::Balanced;
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::edit.graphicsBackend = settings::GraphicsBackend::D3D12;
    settings::edit.upscaler = Upscaler::Dlss;
    settings::edit.dlssQuality = DlssQuality::Balanced;
    settings::row = int(GraphicsRow::Backend);
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == needsVulkan, "matching D3D12 edit still needs Vulkan");
    const auto savesAtCycle = saves;
    settings::pending = 8;
    Tick(base);
    Require(settings::edit.graphicsBackend == settings::GraphicsBackend::Vulkan, "Vulkan can be selected while running D3D12");
    Require(settings::edit.upscaler == Upscaler::Dlss && settings::edit.dlssQuality == DlssQuality::Balanced,
            "backend edit keeps the DLSS preference");
    Require(settings::snapshot.rows[int(GraphicsRow::Backend)].enabled && settings::snapshot.rows[int(GraphicsRow::Backend)].selectedChoice == 1, "Vulkan cell stays selectable");
    Require(settings::snapshot.rows[int(GraphicsRow::Upscaler)].enabled && !settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden, "pending backend does not hide DLSS");
    Require(settings::snapshot.notice == needsVulkan + L" Still using Direct3D 12 until restart. DLSS is checked after restart.",
            "pending backend is added after the current plan and does not replace it");
    Require(saves == savesAtCycle, "selecting Vulkan does not save by itself");
    saveState("05-backend-pending.bmp");
    settings::pending = 8;
    Tick(base);
    Require(settings::edit.graphicsBackend == settings::GraphicsBackend::D3D11, "Direct3D 11 remains selectable");
    Require(settings::snapshot.notice == needsVulkan + L" Still using Direct3D 12 until restart. DLSS is checked after restart.",
            "an unapplied backend still names the running device");

    settings::edit.graphicsBackend = settings::GraphicsBackend::D3D12;
    settings::status = L"Display settings saved.";
    settings::row = int(GraphicsRow::Save);
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.help == L"Display settings saved.", "save message stays on the help line");
    Require(settings::snapshot.notice == needsVulkan, "save message does not hide the DLSS status");

    settings::status.clear();
    settings::edit.uiLanguage = 4;
    settings::edit.graphicsBackend = settings::GraphicsBackend::Vulkan;
    settings::edit.upscaler = Upscaler::Dlss;
    settings::edit.dlssQuality = DlssQuality::Quality;
    running = {};
    running.device.backend = Backend::Vulkan;
    running.device.deviceReady = true;
    running.device.dlssAvailable = true;
    running.phase = DlssEffectPhase::Active;
    running.reason = DlssEffectReason::None;
    running.hasPlan = true;
    running.plannedRequest = Upscaler::Dlss;
    running.plannedQuality = DlssQuality::Balanced;
    running.sizingKnown = true;
    running.sizingState = SizingState::Ready;
    running.inputWidth = 9;
    running.inputHeight = 9;
    running.outputWidth = 9;
    running.outputHeight = 9;
    running.execution = SubmittedObservation(DlssQuality::Quality, 1280, 720, 1920, 1080);
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"已提交 DLSS 质量输出。 1280×720 - 1920×1080", "simplified status");
    saveState("06-active-simplified.bmp");
    settings::edit.uiLanguage = 1;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"已提交 DLSS 品質輸出。 1280×720 - 1920×1080", "traditional status");
    settings::edit.uiLanguage = 2;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS 品質出力を提出しました。 1280×720 - 1920×1080", "japanese status");
    settings::edit.uiLanguage = 3;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS 품질 출력을 제출했습니다. 1280×720 - 1920×1080", "korean status");

    settings::edit.uiLanguage = 0;
    settings::tab = 0;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice.empty(), "other tabs keep a single help line");
    settings::tab = 2;
    running.phase = DlssEffectPhase::TemporaryFallback;
    running.reason = DlssEffectReason::SizingPending;
    running.plannedQuality = DlssQuality::Quality;
    running.sizingState = SizingState::Error;
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS is querying the render resolution. Normal rendering is used for now.",
            "an open graphics page reads a new result without reopening");
    Require(settings::snapshot.notice.find(L"1280") == std::wstring::npos, "a querying result does not keep the submitted size");
    settings::edit.upscaler = Upscaler::Off;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS is querying the render resolution. Normal rendering is used for now. The Off choice is not applied yet.",
            "an unsaved Off choice does not replace a plan that is still querying resolution");
    Require(settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden && settings::snapshot.rows.size() == size_t(GraphicsRow::Count),
            "turning DLSS off hides quality and keeps the row count");
    const auto activePlan = std::wstring(L"Submitted DLSS Quality output. 1707×960 - 2560×1440");
    running = {};
    running.device.backend = Backend::Vulkan;
    running.device.deviceReady = true;
    running.device.dlssAvailable = true;
    running.phase = DlssEffectPhase::Active;
    running.reason = DlssEffectReason::None;
    running.hasPlan = true;
    running.plannedRequest = Upscaler::Dlss;
    running.plannedQuality = DlssQuality::Performance;
    running.sizingKnown = true;
    running.sizingState = SizingState::Ready;
    running.inputWidth = 111;
    running.inputHeight = 222;
    running.outputWidth = 333;
    running.outputHeight = 444;
    running.execution = SubmittedObservation(DlssQuality::Quality, 1707, 960, 2560, 1440);
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::edit.uiLanguage = 0;
    settings::edit.graphicsBackend = settings::GraphicsBackend::Vulkan;
    settings::edit.upscaler = Upscaler::Off;
    settings::edit.dlssQuality = DlssQuality::Quality;
    settings::tab = 2;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == activePlan + L" The Off choice is not applied yet.",
            "Active plan stays visible when Off is not saved");
    Require(settings::snapshot.notice.find(L"DLSS is not in use") == std::wstring::npos, "unsaved Off does not claim DLSS stopped");
    saveState("07-active-edit-off-unsaved.bmp");
    settings::edit.upscaler = Upscaler::Dlss;
    settings::edit.dlssQuality = DlssQuality::Balanced;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == activePlan + L" The selected DLSS quality is not applied yet.",
            "Active Quality stays visible when Balanced is not saved");
    Require(settings::snapshot.notice.find(L"DLSS Balanced") == std::wstring::npos, "unsaved Balanced is not described as submitted");
    Require(settings::snapshot.notice.find(L"Performance") == std::wstring::npos, "CPU plan quality is not described as submitted");
    Require(settings::snapshot.notice.find(L"DLSS is not running") == std::wstring::npos,
            "an unsaved quality edit is not a fallback");
    saveState("08-active-quality-unapplied.bmp");
    running.phase = DlssEffectPhase::Inactive;
    running.plannedRequest = Upscaler::Off;
    running.plannedQuality = DlssQuality::Quality;
    running.device.backend = Backend::D3D12;
    running.inputWidth = running.inputHeight = running.outputWidth = running.outputHeight = 0;
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::edit.graphicsBackend = settings::GraphicsBackend::D3D12;
    settings::edit.upscaler = Upscaler::Dlss;
    settings::edit.dlssQuality = DlssQuality::Quality;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS is not in use. DLSS needs Vulkan and a restart.",
            "unsaved DLSS on D3D12 says it is not in use and needs Vulkan");
    Require(settings::snapshot.notice.find(L"Submitted") == std::wstring::npos,
            "unsaved DLSS is not described as submitted output");
    Require(settings::snapshot.notice.find(L"1707") == std::wstring::npos, "an inactive result does not keep a submitted size");
    Require(settings::snapshot.rows[int(GraphicsRow::Upscaler)].enabled && !settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden, "unsaved DLSS choice stays available");
    saveState("09-d3d12-edit-dlss-unsaved.bmp");
    running.device.backend = Backend::Vulkan;
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::edit.graphicsBackend = settings::GraphicsBackend::Vulkan;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS is not in use. The DLSS choice is not applied yet.",
            "unsaved DLSS on Vulkan stays not in use without a Vulkan warning");

    settings::edit.uiLanguage = 0;
    settings::edit.graphicsBackend = settings::GraphicsBackend::Vulkan;
    settings::edit.upscaler = Upscaler::Dlss;
    settings::edit.dlssQuality = DlssQuality::Quality;
    settings::status.clear();
    settings::tab = 2;
    settings::row = int(GraphicsRow::Upscaler);
    auto show = [&](DlssEffectPhase phase, DlssEffectReason reason) {
        running.phase = phase;
        running.reason = reason;
        running.device.backend = Backend::Vulkan;
        running.device.deviceReady = true;
        running.device.dlssAvailable = true;
        gpu::frame_plan::menuFlowDlssEffect = running;
        settings::pending = 0;
        Tick(base);
    };
    const auto savesAtExecution = saves;
    running = {};
    running.execution = SubmittedObservation(DlssQuality::Quality, 1707, 960, 2560, 1440);
    running.plannedRequest = Upscaler::Dlss;
    running.plannedQuality = DlssQuality::Quality;
    running.inputWidth = 1707;
    running.inputHeight = 960;
    running.outputWidth = 2560;
    running.outputHeight = 1440;
    show(DlssEffectPhase::AwaitingExecution, DlssEffectReason::AwaitingGpuFrame);
    Require(settings::snapshot.notice == L"Waiting for the first DLSS result.",
            "waiting for the first result is not submitted output");
    Require(settings::snapshot.notice.find(L"Submitted") == std::wstring::npos, "waiting cannot claim a submission");
    Require(settings::snapshot.notice.find(L"1707") == std::wstring::npos, "waiting does not show a planned size");
    Require(settings::active && settings::tab == 2, "refresh keeps the graphics page open");
    saveState("10-awaiting-first-result.bmp");
    show(DlssEffectPhase::AwaitingExecution, DlssEffectReason::None);
    Require(settings::snapshot.notice == L"Waiting for the first DLSS result.",
            "awaiting phase without a reason still waits");

    running.execution = SubmittedObservation(DlssQuality::Quality, 1280, 720, 1920, 1080);
    show(DlssEffectPhase::InputProbeOnly, DlssEffectReason::InputProbeOnly);
    Require(settings::snapshot.notice == L"Input capture only. DLSS is not run.",
            "input capture does not execute DLSS");
    Require(settings::snapshot.notice.find(L"Submitted") == std::wstring::npos, "input capture is not active");
    saveState("11-input-probe.bmp");

    running.execution->reason = DlssEffectReason::RequestFailure;
    show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::MotionPipelinePending);
    Require(settings::snapshot.notice == L"Motion data is still being prepared.",
            "motion preparation uses the snapshot reason, not the observation reason");
    saveState("12-motion-preparing.bmp");
    show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::UnknownColorEncoding);
    Require(settings::snapshot.notice == L"Color conditions do not support DLSS.", "unsupported color");
    saveState("13-color-unsupported.bmp");
    show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::NoEligibleScene);
    Require(settings::snapshot.notice == L"No eligible scene this frame.", "no eligible scene");
    saveState("14-no-eligible-scene.bmp");
    show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::FeatureReconfigurePending);
    Require(settings::snapshot.notice == L"Waiting to rebuild DLSS.", "rebuild wait");
    saveState("15-rebuild-waiting.bmp");
    show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::PromotionUnavailable);
    Require(settings::snapshot.notice == L"DLSS output was not adopted.", "output not adopted");

    running.sizingState = SizingState::Pending;
    running.sizingKnown = false;
    show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::SizingError);
    Require(settings::snapshot.notice == L"Render resolution query failed.", "resolution query failed");
    saveState("16-resolution-query-failed.bmp");
    running.sizingState = SizingState::Error;
    show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::SizingUnavailable);
    Require(settings::snapshot.notice == L"Render resolution is unavailable.", "resolution unavailable");
    running.failure = gpu::frame_plan::FailureReason::InvalidInput;
    show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::RequestFailure);
    Require(settings::snapshot.notice == L"DLSS request failed and fell back. No automatic retry.",
            "request failure names the fallback and does not promise a retry");
    Require(settings::snapshot.notice.find(L"InvalidInput") == std::wstring::npos, "failure enum is not shown");
    Require(settings::snapshot.notice.find(L"will retry") == std::wstring::npos, "request failure does not promise a retry");
    saveState("17-request-failed.bmp");
    show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::CapabilityUnavailable);
    Require(settings::snapshot.notice == L"DLSS is not available on this device.",
            "capability reason is shown without a device-flag decision");

    running.failure = gpu::frame_plan::FailureReason::DlssOutOfMemory;
    running.execution.reset();
    show(DlssEffectPhase::GpuStopped, DlssEffectReason::GpuWorkStopped);
    Require(settings::snapshot.notice == L"GPU work has stopped.", "GPU work stopped");
    Require(settings::snapshot.notice.find(L"OutOfMemory") == std::wstring::npos, "GPU stop hides the failure code");
    Require(settings::snapshot.notice.find(L"Submitted") == std::wstring::npos, "GPU stop is not active");
    saveState("18-gpu-stopped.bmp");
    running.failure.reset();
    show(DlssEffectPhase::GpuStopped, DlssEffectReason::None);
    Require(settings::snapshot.notice == L"GPU work has stopped.", "GPU stop phase is enough when no reason is set");

    running.execution = SubmittedObservation(DlssQuality::Performance, 1280, 720, 1920, 1080);
    running.plannedQuality = DlssQuality::Quality;
    running.inputWidth = 111;
    settings::edit.dlssQuality = DlssQuality::Performance;
    show(DlssEffectPhase::Active, DlssEffectReason::RequestFailure);
    Require(settings::snapshot.notice == L"Submitted DLSS Performance output. 1280×720 - 1920×1080",
            "Active uses the submitted mode even if a stale reason is set");
    Require(settings::snapshot.notice.find(L"Quality output") == std::wstring::npos, "Active does not use the CPU plan mode");
    Require(settings::snapshot.notice.find(L"111×") == std::wstring::npos, "Active does not use the CPU plan size");
    Require(settings::snapshot.notice.find(L"failed") == std::wstring::npos, "Active does not show a stale failure reason");

    running.execution = SubmittedObservation(DlssQuality::Balanced, 1500, 844, 2560, 1440);
    running.plannedQuality = DlssQuality::Balanced;
    settings::edit.dlssQuality = DlssQuality::Quality;
    show(DlssEffectPhase::Active, DlssEffectReason::None);
    Require(settings::snapshot.notice ==
                L"Submitted DLSS Balanced output. 1500×844 - 2560×1440 The selected DLSS quality is not applied yet.",
            "submitted Balanced stays visible when the edit is still Quality");
    Require(settings::snapshot.notice.find(L"Quality output") == std::wstring::npos, "the edit is not described as submitted");

    running.execution.reset();
    running.plannedQuality = DlssQuality::Quality;
    settings::edit.dlssQuality = DlssQuality::Performance;
    show(DlssEffectPhase::Active, DlssEffectReason::None);
    Require(settings::snapshot.notice == L"Waiting for the first DLSS result.",
            "Active without an execution record waits and does not claim a submission");
    Require(settings::snapshot.notice.find(L"Submitted") == std::wstring::npos, "missing execution is not described as submitted");
    Require(settings::snapshot.notice.find(L"Performance") == std::wstring::npos, "missing execution does not name the edit");
    Require(settings::snapshot.notice.find(L"not applied") == std::wstring::npos,
            "missing execution does not invent a quality comparison");
    Require(saves == savesAtExecution, "execution status refresh does not save");
    Require(settings::snapshot.rows.size() == size_t(GraphicsRow::Count), "execution status keeps every graphics row");
    Require(settings::active, "execution status refresh leaves the menu open");
    notes.close();
    Require(bool(notes), "write BR-03 notice list");
    std::puts("PASS BR-03 menu status from stubbed snapshots: submitted output, waiting, probe, fallback reasons, GPU stopped, unsaved edits");
}

int main(int argc, char** argv)
{
    uint8_t* base = nullptr;
    try
    {
        base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x100000000ull, MEM_RESERVE, PAGE_NOACCESS));
        Require(base != nullptr, "reserve guest address space");
        for (uint32_t page : {0x10000u, 0x20000u, 0x83260000u, 0x83360000u})
            Require(VirtualAlloc(base + page, 0x10000, MEM_COMMIT, PAGE_READWRITE) != nullptr, "commit fixture pages");
        PPC_STORE_U32(0x8326A068, 0x20000);
        PPC_STORE_U32(0x20004, 0x20100);
        PPC_STORE_U32(0x20118, ConfigData);
        PPC_STORE_U32(Menu + 4, 4);
        Tick(base);
        Require(settings::active && !settings::closing, "replacement opens");
        Poll(0, true);
        // A real gameplay edit reaches the existing native apply boundary.
        Poll(8, true); Tick(base); Poll(0, true);
        Require(PPC_LOAD_U32(ConfigData) == 1 && applies == 1, "guest config edit remains effective");
        for (bool swapped : {false, true})
        {
            PPC_STORE_U32(ConfigData + 4, swapped ? 0x02000000 : 0);
            Tick(base); Poll(0, true);
            const unsigned oldCloses = closes, oldApplies = applies, oldTicks = ticks;
            Poll(swapped ? 0x1000 : 0x2000, true); Tick(base);
            Require(closes == oldCloses + 1 && applies == oldApplies + 1, "one apply/close pair");
            Require(!settings::active && settings::closing && settings::cancelPolls == 0, "close without injected buttons");
            Poll(swapped ? 0x1000 : 0x2000, true);
            Tick(base);
            Require(ticks == oldTicks + 1 && !settings::closing && PPC_LOAD_U32(Menu + 4) == 1, "native tick completes close");
            Poll(0, true, 20000); // Sticks must return to neutral as well.
            Poll(0, true); Poll(0, false); Poll(0x2000, false);
            PPC_STORE_U32(Menu + 4, 4); Tick(base); Poll(0, true);
            Require(settings::active && closes == oldCloses + 1, "same address reopens without duplicate close");
        }
        // Existing brightness handoff must return to this same replacement.
        settings::tab = 2; settings::row = int(GraphicsRow::Brightness);
        PPC_STORE_U32(Menu + 0x558 + 0x84, 0x22000);
        PPC_STORE_U32(0x22000 + 4, 12);
        settings::pending = 0x3000; Tick(base);
        Require(settings::closing && settings::cancelPolls == 0, "Back discards same-frame calibration injection");
        Tick(base); Poll(0, true); Poll(0x1000, false);
        PPC_STORE_U32(Menu + 4, 4); Tick(base); Poll(0, true);
        settings::pending = 0x1000; Tick(base);
        Require(settings::bypass && !settings::active && settings::cancelPolls == 6, "explicit calibration handoff");
        settings::cancelPolls = 0; // The native input/calibration boundary is simulated below.
        PPC_STORE_U32(Menu + 4, 6); Tick(base);
        PPC_STORE_U32(Menu + 4, 4); Tick(base);
        Require(settings::active && !settings::bypass, "calibration returns to replacement");
        settings::bypass = true; settings::sawModal = false;
        PPC_STORE_U32(Menu + 4, 1); Tick(base);
        PPC_STORE_U32(Menu + 4, 4); Tick(base);
        Require(settings::active && !settings::bypass, "idle address reuse clears stale handoff");
        deviceReady = false;
        const auto oldTicks = ticks; Tick(base);
        Require(ticks == oldTicks + 1, "no-device retail fallback retained");
        std::puts("PASS actual menu hook: edit/apply, close order/context, native completion, held-key gate, swapped buttons, reopen, calibration return, no-device fallback");
        deviceReady = true;
        settings::tab = 2; settings::row = int(GraphicsRow::Save);
        settings::edit = currentConfig;
        const auto original = currentConfig;
        settings::edit.width = original.width + 160;
        saveFails = true;
        settings::pending = 0x1000; Tick(base);
        Require(saves == 1 && requests == 0 && previews == 0 && currentConfig.width == original.width,
                "failed save cannot change current display or start application");
        saveFails = false;
        settings::pending = 0x1000; Tick(base);
        const auto firstTicket = settings::displayTicket;
        Require(saves == 2 && requests == 1 && firstTicket && diskConfig.width == settings::edit.width,
                "single save persists before asynchronous application");
        settings::pending = 0x2000; Tick(base);
        Require(settings::displayTicket == firstTicket && saves == 2 && !settings::closing,
                "Back while applying cannot cancel or pretend success");
        auto windowTicket = displayChanges.WindowTicket(currentConfig.width, currentConfig.height, uint32_t(currentConfig.windowMode));
        Require(windowTicket == firstTicket, "window stage matches request");
        displayChanges.WindowComplete(windowTicket, true);
        Require(displayChanges.Query(firstTicket) == gpu::video::DisplayChangeResult::Pending,
                "window application alone is not a presentation acknowledgment");
        displayChanges.Complete(firstTicket, true); Tick(base);
        Require(!settings::displayTicket && saves == 2 && !settings::restartPrompt, "one click completes without Keep changes");

        const auto stable = currentConfig;
        settings::edit.width += 160;
        settings::pending = 0x1000; Tick(base);
        const auto failedTicket = settings::displayTicket;
        displayChanges.WindowComplete(failedTicket, false); Tick(base);
        const auto restoreTicket = settings::displayTicket;
        Require(settings::displayRollback && restoreTicket > failedTicket && diskConfig.width == stable.width,
                "display failure restores saved configuration with a new request");
        displayChanges.Complete(failedTicket, true);
        Require(displayChanges.Query(restoreTicket) == gpu::video::DisplayChangeResult::Pending,
                "late failed-request completion cannot confirm rollback");
        displayChanges.WindowComplete(restoreTicket, true);
        displayChanges.Complete(restoreTicket, true); Tick(base);
        Require(!settings::displayTicket && !settings::displayRollback && currentConfig.width == stable.width,
                "rollback is reported after its actual presentation result");

        settings::edit.width += 160;
        settings::pending = 0x1000; Tick(base);
        const auto retryTicket = settings::displayTicket;
        Require(retryTicket > restoreTicket, "same mode retry owns a fresh ticket");
        displayChanges.Complete(failedTicket, true);
        Require(displayChanges.Query(retryTicket) == gpu::video::DisplayChangeResult::Pending, "same-key retry ignores old success");
        saveFails = true;
        displayChanges.WindowComplete(retryTicket, false); Tick(base);
        const auto diskFailureRestore = settings::displayTicket;
        Require(previews == 1 && currentConfig.width == stable.width && diskConfig.width != stable.width,
                "failed rollback save still restores runtime and retains honest disk state");
        displayChanges.WindowComplete(diskFailureRestore, true);
        displayChanges.Complete(diskFailureRestore, true); Tick(base);
        Require(settings::status == L"Display restored; settings file could not be restored.", "rollback disk error remains visible");
        saveFails = false;

        settings::edit = currentConfig;
        settings::edit.graphicsBackend = currentConfig.graphicsBackend == settings::GraphicsBackend::Vulkan
            ? settings::GraphicsBackend::D3D12 : settings::GraphicsBackend::Vulkan;
        const auto beforeRestartSave = saves;
        settings::pending = 0x1000; Tick(base);
        Require(settings::restartPrompt && settings::savedRestartPrompt && saves == beforeRestartSave + 1 &&
                settings::snapshot.dialogChoices.size() == 2, "saved restart setting asks only Now or Later");
        settings::pending = 0x2000; Tick(base);
        Require(!settings::restartPrompt && saves == beforeRestartSave + 1 && diskConfig.graphicsBackend == settings::edit.graphicsBackend,
                "Back means Later without resaving or rolling back saved graphics");
        settings::edit.graphicsBackend = original.graphicsBackend;
        settings::pending = 0x1000; Tick(base);
        const auto beforeNow = saves;
        settings::pending = 0x1000; Tick(base);
        Require(settings::restart::Requested() && saves == beforeNow, "Now requests restart without a second save");
        // No window event loop runs here, so Request cannot launch a process.
        auto invalidated = displayChanges.Begin(1280, 720, 0);
        displayChanges.Reset(); displayChanges.WindowComplete(invalidated, true); displayChanges.Complete(invalidated, true);
        Require(displayChanges.Query(invalidated) == gpu::video::DisplayChangeResult::Failed && !displayChanges.PresentationTicket(),
                "reset invalidates pending and late presentation results");
        std::puts("PASS actual Graphics Save: write failure, one click/apply acknowledgment, rollback and disk error, same-mode retry, stale completion, Now/Later, reset invalidation");

        // Widescreen workflow verification:
        // 1. Initial 3440x1440 configuration: widescreen switch derives ON, selects 3440x1440 in 21:9 list
        {
            currentConfig.width = 3440;
            currentConfig.height = 1440;
            diskConfig = currentConfig;
            settings::edit = currentConfig;
            settings::tab = 2;
            settings::row = int(GraphicsRow::Widescreen);
            settings::Publish(base, ConfigData);
            Require(settings::snapshot.rows.size() == size_t(GraphicsRow::Count), "graphics tab has one row per id");
            const auto& wsRow = settings::snapshot.rows[int(GraphicsRow::Widescreen)];
            Require(wsRow.name == L"Widescreen" && wsRow.value == L"On" && wsRow.selectedChoice == 0,
                    "initial 3440x1440 automatically enables Widescreen switch");
            const auto& resRow = settings::snapshot.rows[int(GraphicsRow::OutputResolution)];
            Require(resRow.name == L"Output resolution", "output resolution keeps its graphics id");
            Require(resRow.choices.size() == 5, "21:9 resolution choices count is 5");
            Require(resRow.choices[0] == L"1720 × 720" && resRow.choices[1] == L"2560 × 1080" &&
                    resRow.choices[2] == L"3440 × 1440" && resRow.choices[3] == L"3840 × 1600" &&
                    resRow.choices[4] == L"5120 × 2160", "21:9 resolution choices match specification");
            Require(resRow.selectedChoice == 2 && resRow.value == L"3440 × 1440",
                    "3440x1440 selected in 21:9 output choices");
        }

        // 2. Start from 16:9 1280x720, toggle switch ON -> 1720x720, cycle all 5 ultrawide tiers including 5120x2160
        {
            currentConfig.width = 1280;
            currentConfig.height = 720;
            diskConfig = currentConfig;
            settings::edit = currentConfig;
            settings::tab = 2;
            settings::row = int(GraphicsRow::Widescreen);
            settings::Publish(base, ConfigData);
            Require(settings::snapshot.rows[int(GraphicsRow::Widescreen)].value == L"Off", "1280x720 starts with Widescreen Off");
            Require(settings::snapshot.rows[int(GraphicsRow::OutputResolution)].choices.size() == 5 &&
                    settings::snapshot.rows[int(GraphicsRow::OutputResolution)].choices[0] == L"1280 × 720" &&
                    settings::snapshot.rows[int(GraphicsRow::OutputResolution)].choices[4] == L"3840 × 2160",
                    "16:9 resolution choices present");

            // Toggle switch ON (delta +1)
            settings::pending = 0x1008; Tick(base);
            Require(settings::edit.width == 1720 && settings::edit.height == 720,
                    "toggle ON from 1280x720 maps to 1720x720");
            Require(settings::snapshot.rows[int(GraphicsRow::Widescreen)].value == L"On", "Widescreen switch is now On");
            Require(settings::snapshot.rows[int(GraphicsRow::OutputResolution)].selectedChoice == 0 &&
                    settings::snapshot.rows[int(GraphicsRow::OutputResolution)].value == L"1720 × 720", "1720x720 selected");

            // Move to output resolution and cycle forward through all 5 ultrawide tiers
            settings::row = int(GraphicsRow::OutputResolution);
            constexpr uint32_t expected21_9[][2] = {
                {2560, 1080}, {3440, 1440}, {3840, 1600}, {5120, 2160}, {1720, 720}};
            for (size_t i = 0; i < 5; ++i)
            {
                settings::pending = 0x1008; Tick(base); // Right arrow / Confirm
                Require(settings::edit.width == expected21_9[i][0] && settings::edit.height == expected21_9[i][1],
                        "cycle 21:9 resolution matches expected tier");
            }
            Require(settings::edit.width == 1720 && settings::edit.height == 720, "cycled back to 1720x720");

            // Direct step to 5120x2160
            settings::edit.width = 5120;
            settings::edit.height = 2160;
            settings::Publish(base, ConfigData);
            Require(settings::snapshot.rows[int(GraphicsRow::OutputResolution)].selectedChoice == 4 &&
                    settings::snapshot.rows[int(GraphicsRow::OutputResolution)].value == L"5120 × 2160", "5120x2160 tier verified");

            // Switch back to 16:9: height 2160 preserves height and maps to 3840x2160
            settings::row = int(GraphicsRow::Widescreen);
            settings::pending = 0x1008; Tick(base);
            Require(settings::edit.width == 3840 && settings::edit.height == 2160,
                    "switch to 16:9 preserves 2160 height mapping to 3840x2160");
            Require(settings::snapshot.rows[int(GraphicsRow::Widescreen)].value == L"Off", "switch is now Off");
            Require(settings::snapshot.rows[int(GraphicsRow::OutputResolution)].choices[4] == L"3840 × 2160", "16:9 4K selected");

            // Cancel / exit without saving: disk remains untouched at initial 1280x720
            const auto oldSaves = saves;
            const unsigned oldCloses = closes;
            settings::pending = 0x2000; Tick(base); // Back button to close menu
            Require(closes == oldCloses + 1, "close called on Back");
            Require(saves == oldSaves, "cancel does not write to disk");
            Require(diskConfig.width == 1280 && diskConfig.height == 720, "disk config unchanged on cancel");

            // Native tick completes the close
            Tick(base);
            Require(!settings::closing && PPC_LOAD_U32(Menu + 4) == 1, "native tick completes close");
            settings::releaseToParent = false;
            settings::waitForRelease = false;

            // Reopen menu: edit restores cleanly from GetConfig()
            PPC_STORE_U32(Menu + 4, 4); Tick(base); Poll(0, true);
            Require(settings::edit.width == 1280 && settings::edit.height == 720,
                    "reopening restores saved config without unapplied preview changes");

            // Switch to 3440x1440 and Save: goes through display change state machine
            settings::tab = 2;
            settings::row = int(GraphicsRow::Widescreen);
            settings::pending = 0x1008; Tick(base); // Switch ON -> 1720x720
            settings::row = int(GraphicsRow::OutputResolution);
            settings::pending = 0x1008; Tick(base); // 2560x1080
            settings::pending = 0x1008; Tick(base); // 3440x1440
            Require(settings::edit.width == 3440 && settings::edit.height == 1440, "selected 3440x1440");
            settings::row = int(GraphicsRow::Save);
            settings::pending = 0x1000; Tick(base);
            Require(saves == oldSaves + 1 && diskConfig.width == 3440 && diskConfig.height == 1440,
                    "Save persists 3440x1440 to disk config");
            auto saveTicket = settings::displayTicket;
            Require(saveTicket != 0, "Save triggers BeginDisplayChange ticket");
            displayChanges.WindowComplete(saveTicket, true);
            displayChanges.Complete(saveTicket, true); Tick(base);
            Require(!settings::displayTicket, "display state machine completed successfully for 3440x1440");
            Require(settings::status == L"Display settings saved.", "status shows display saved");
        }
        std::puts("PASS Widescreen workflow: 3440x1440 auto-derive, 5 ultrawide tiers cycle incl 5120x2160, height-preserved 16:9 switch, cancel discard, Save state machine");

        // Start / Enter focus-jump, simultaneous confirm suppression, PointerClick viewport clipping and DLSS quality visibility:
        {
            const auto savesBefore = saves;
            settings::tab = 2; // Graphics tab
            settings::row = int(GraphicsRow::Backend);
            settings::edit.upscaler = gpu::upscaling::Upscaler::Off;
            settings::Publish(base, ConfigData);

            Require(settings::snapshot.rows.size() == size_t(GraphicsRow::Count), "graphics tab has one row per id");
            Require(settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden, "DLSS quality is hidden when upscaler is Off");

            settings::row = int(GraphicsRow::Upscaler);
            settings::pending = 2; Tick(base); // D-pad down
            Require(settings::row == int(GraphicsRow::ScalingQuality), "down from Upscaler skips hidden DLSS quality");
            settings::pending = 1; Tick(base); // D-pad up
            Require(settings::row == int(GraphicsRow::Upscaler), "up from Scaling quality skips hidden DLSS quality");

            // Start (0x10) jumps focus to Save graphics settings without saving
            settings::pending = 0x10; Tick(base);
            Require(settings::row == int(GraphicsRow::Save), "Start jumps focus to Save");
            Require(saves == savesBefore, "Start jump does not save immediately");

            // Simultaneous Start (0x10) + Confirm (0x1000) does NOT save on the same tick
            settings::row = int(GraphicsRow::Backend);
            settings::pending = 0x1010; Tick(base);
            Require(settings::row == int(GraphicsRow::Save), "simultaneous Start+A still focuses Save row");
            Require(saves == savesBefore, "simultaneous Start+A suppresses same-tick save");

            // Subsequent A (0x1000) on focused Save row executes the save
            settings::pending = 0x1000; Tick(base);
            Require(saves == savesBefore + 1, "subsequent A on Save row triggers save");

            // Language tab (tab 3): Start (0x10) jumps focus to Save settings (row 3)
            settings::tab = 3;
            settings::row = 0;
            settings::pending = 0x10; Tick(base);
            Require(settings::row == 3, "Start on Language tab jumps to Save settings (row 3)");

            // PointerClick boundary tests:
            // Valid slot 0 (y = 150) hits row 0
            settings::PointerClick(100.0f, 150.0f, false);
            Require(settings::mouseRow.load() == 0, "click at y=150 selects visible slot 0 (row 0)");

            // Click at y < 150 (above rows) is ignored
            settings::mouseRow = -1;
            settings::PointerClick(100.0f, 140.0f, false);
            Require(settings::mouseRow.load() == -1, "click above y=150 ignored");

            // Click below visible viewport (y >= 640 or slot >= 11) is ignored
            settings::mouseRow = -1;
            settings::PointerClick(100.0f, 640.0f, false);
            Require(settings::mouseRow.load() == -1, "click at y=640 (below viewport) ignored");
            settings::PointerClick(100.0f, 700.0f, false);
            Require(settings::mouseRow.load() == -1, "click at y=700 (below viewport) ignored");

            std::puts("PASS Start/Enter focus jump, simultaneous confirm suppression, and PointerClick viewport clipping");
        }
        if (argc == 3)
        {
            settings::status.clear();
            settings::edit.uiLanguage = 4; settings::tab = 2;
            auto assets = settings::menu_assets::Cached(argv[1], 4);
            Require(bool(assets), "installed SCH assets");
            std::filesystem::create_directories(argv[2]);
            for (GraphicsRow selected : {GraphicsRow::AntiAliasing, GraphicsRow::FrameRate})
            {
                settings::row = int(selected); settings::Publish(base, ConfigData);
                auto preview = settings::snapshot; preview.assets = assets;
                for (GraphicsRow label : {GraphicsRow::AntiAliasing, GraphicsRow::FrameRate, GraphicsRow::Save})
                    Require(covers(assets->body, preview.rows[int(label)].name), "changed label must use original body face");
                std::vector<uint32_t> pixels;
                Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "translated Graphics preview");
                WriteBmp(std::filesystem::path(argv[2]) / (selected == GraphicsRow::AntiAliasing ? "graphics-aa.bmp" : "graphics-rate.bmp"), pixels);
            }
            std::puts("PASS two Graphics previews from actual Publish/Translate, normal and selected changed labels");
        }
        CheckBr03DlssMenu(base);
        VirtualFree(base, 0, MEM_RELEASE);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        if (base) VirtualFree(base, 0, MEM_RELEASE);
        return 1;
    }
}
