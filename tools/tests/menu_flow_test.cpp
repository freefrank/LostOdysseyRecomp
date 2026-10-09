// Exercise the production hook, substituting only its guest-call boundaries.
// No game, renderer device, window, save or profile is started by this fixture.
#include <stdafx.h>
#include <gpu/video.h>
#include <settings/config.h>
#define SDL_MAIN_HANDLED // This host fixture supplies its own main and does not initialize SDL.
#include <SDL3/SDL.h>
#include <fstream>
#include <stdexcept>
#include <tuple>
extern "C" int SDLCALL MenuFlowPushEvent(SDL_Event* event);
namespace settings { void RequestMainMenuAfterSettingsClose(PPCContext& ctx, uint8_t* base, uint32_t settingsMenu); }
namespace gpu::video {
plume::RenderDevice* MenuFlowTestDevice();
inline bool WindowModeOverridden() { return false; }
inline std::optional<gpu::backend::Backend> SelectedBackend() { return gpu::backend::Backend::D3D12; }
}
#include <kernel/io/file_system.h>
inline std::filesystem::path FileSystem::GetGameRoot() { return {}; }
namespace os::log_collection {
inline bool Supported() { return true; }
inline const wchar_t* Label(uint32_t) { return L"Collection"; }
inline const wchar_t* Message(uint32_t) { return L"Message"; }
inline bool Enabled() { return false; }
inline int Consent() { return 0; }
inline bool SetConsent(bool) { return true; }
}
namespace settings {
Config MenuFlowGetConfig();
bool MenuFlowSaveConfig(const Config&);
bool MenuFlowSaveAudioOutput(uint32_t);
void MenuFlowPreviewConfig(const Config&);
inline uint32_t GameLanguage() { return 1; }
}
namespace gpu::video {
uint64_t MenuFlowBeginDisplayChange(const settings::Config&);
DisplayChangeResult MenuFlowQueryDisplayChange(uint64_t);
bool MenuFlowDisplayModeFailed();
// No display list here, so a display choice never moves the window.
inline uint64_t DisplayMoveCount() { return 0; }
inline uint64_t BeginDisplayRevert(const settings::Config& c) { return MenuFlowBeginDisplayChange(c); }
}
#define GetConfig MenuFlowGetConfig
#define SaveConfig MenuFlowSaveConfig
#define SaveAudioOutput MenuFlowSaveAudioOutput
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
#define SDL_PushEvent MenuFlowPushEvent
#include "../../LostOdysseyRecomp/settings/menu.cpp"
#undef SDL_PushEvent
#undef Translate
#undef GetConfig
#undef SaveConfig
#undef SaveAudioOutput
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

namespace gpu::video {
FrameGenerationStatus menuFlowFgStatus{};
FrameGenerationStatus GetFrameGenerationStatus() { return menuFlowFgStatus; }
std::vector<std::string> GpuDeviceNames() { return {}; }
std::string ActiveGpuDeviceName() { return {}; }
std::vector<display_choice::Display> Displays() { return {}; }
}

// Compile the real settings reader/writer into this menu fixture as well.
// Its public entry points stay distinct from the menu hook's mock persistence.
#define GameLanguage MenuFlowRealGameLanguage
#undef LOG_INFO
#define LOG_INFO(...) ((void)0)
#include "../../LostOdysseyRecomp/settings/config.cpp"
#undef LOG_INFO
#undef GameLanguage

using settings::GraphicsRow;

namespace gpu::frame_plan {
DlssEffectSnapshot menuFlowDlssEffect{};
DlssEffectSnapshot CurrentDlssEffect() { return menuFlowDlssEffect; }
std::optional<UpscalerExecutionObservation> CurrentUpscalerExecution() { return menuFlowDlssEffect.execution; }
}
namespace {
uint32_t vibrationStrength = 100;
unsigned vibrationPreviews = 0;
uint32_t promptStyle = 0;
}
namespace hid {
bool UsesPlayStationPrompts() { return false; }
void SetVibrationStrength(uint32_t percent) { vibrationStrength = percent; }
void SetPromptStyle(uint32_t style) { promptStyle = style; }
void PreviewVibration() { ++vibrationPreviews; }
}
namespace apu {
Output menuFlowOutput = Output::Stereo;
uint32_t menuFlowMatrixPhase = 90;
bool menuFlowTestSignal = false;
void SetOutput(Output output) { menuFlowOutput = output; }
void SetMatrixPhase(uint32_t degrees) { menuFlowMatrixPhase = degrees; }
void SetTestSignal(bool on) { menuFlowTestSignal = on; }
float TestSignalPosition() { return menuFlowTestSignal ? 0.0f : -1.0f; }
uint32_t OutputChannels() { return menuFlowOutput == Output::Surround ? 6 : 2; }
}

namespace {
constexpr uint32_t Menu = 0x10000, ConfigData = 0x21000;
bool deviceReady = true;
unsigned ticks = 0, applies = 0, closes = 0;
std::vector<char> calls;
settings::Config currentConfig{}, diskConfig{};
unsigned saves = 0, previews = 0, requests = 0;
bool saveFails = false, modeFailed = false;
unsigned quitEventAttempts = 0, mainMenuRequests = 0;
gpu::video::DisplayChangeTracker displayChanges;
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
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
bool settings::MenuFlowSaveAudioOutput(uint32_t output)
{
    ++saves;
    if (saveFails) return false;
    currentConfig.audioOutput = diskConfig.audioOutput = output;
    return true;
}
extern "C" int SDLCALL MenuFlowPushEvent(SDL_Event* event)
{
    (void)event;
    ++quitEventAttempts;
    throw std::runtime_error("return to main menu must never push an SDL event");
}
void settings::RequestMainMenuAfterSettingsClose(PPCContext& ctx, uint8_t* base, uint32_t settingsMenu)
{
    Require(settingsMenu == Menu && PPC_LOAD_U32(settingsMenu + 4) <= 2 &&
            !settings::active && ctx.r3.u32 == Menu, "main-menu request follows native Settings completion");
    (void)base;
    ++mainMenuRequests;
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
    settings::row = int(GraphicsRow::AntiAliasing);
    settings::status.clear();
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == needsVulkan, "D3D12 DLSS shows Vulkan restart");
    Require(settings::snapshot.rows.size() == size_t(GraphicsRow::Count), "BR-03 keeps every graphics row");
    Require(settings::snapshot.rows[int(GraphicsRow::AntiAliasing)].enabled && settings::snapshot.rows[int(GraphicsRow::AntiAliasing)].choices.size() == 6, "DLSS and FSR choices stay enabled on D3D12");
    Require(settings::snapshot.rows[int(GraphicsRow::AntiAliasing)].choices[3] == L"TAA" &&
            settings::snapshot.rows[int(GraphicsRow::FrameRate)].choices.back().find(L"(") == std::wstring::npos,
            "TAA and frame-rate choices carry no experimental label");
    Require(!settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden && settings::snapshot.rows[int(GraphicsRow::DlssQuality)].enabled, "quality row stays available");
    Require(settings::snapshot.rows[int(GraphicsRow::Backend)].enabled && settings::snapshot.rows[int(GraphicsRow::Backend)].choices.size() == 3, "backend choices stay available");
    const auto& shadow = settings::snapshot.rows[int(GraphicsRow::ShadowResolution)];
    Require(shadow.choices == std::vector<std::wstring>{L"1×", L"2×", L"4×"} &&
            shadow.selectedChoice == 0, "shadow resolution defaults to 1x");
    const auto& ao = settings::snapshot.rows[int(GraphicsRow::AmbientOcclusion)];
    Require(ao.choices == std::vector<std::wstring>{L"Off", L"SSAO", L"GTAO"} &&
            ao.selectedChoice == 0, "ambient occlusion defaults to Off");
    settings::row = int(GraphicsRow::ShadowResolution);
    settings::pending = 8; Tick(base);
    Require(settings::edit.shadowResolution == 2 &&
            settings::snapshot.rows[int(GraphicsRow::ShadowResolution)].value == L"2×",
            "shadow resolution selects 2x");
    settings::pending = 8; Tick(base);
    Require(settings::edit.shadowResolution == 4, "shadow resolution selects 4x");
    settings::pending = 8; Tick(base);
    Require(settings::edit.shadowResolution == 1, "shadow resolution wraps to 1x");
    settings::row = int(GraphicsRow::AmbientOcclusion);
    settings::pending = 8; Tick(base);
    Require(settings::edit.ambientOcclusion == 1 &&
            settings::snapshot.rows[int(GraphicsRow::AmbientOcclusion)].value == L"SSAO",
            "ambient occlusion selects SSAO");
    settings::pending = 8; Tick(base);
    Require(settings::edit.ambientOcclusion == 2, "ambient occlusion selects GTAO");
    settings::pending = 8; Tick(base);
    Require(settings::edit.ambientOcclusion == 0, "ambient occlusion wraps to Off");
    settings::row = int(GraphicsRow::AntiAliasing);
    settings::pending = 0; Tick(base);
    Require(settings::snapshot.help == L"Saves the DLSS preference. The status line shows the latest DLSS result.",
            "upscaler help points at the status line");
    saveState("01-d3d12-needs-vulkan.bmp");
    settings::row = int(GraphicsRow::DlssQuality);
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.help == L"Performance, Balanced, Quality, or DLAA. The status line shows the submitted mode.",
            "quality help names the submitted mode");
    auto checkQuality = [&](const std::vector<std::wstring>& choices, int selected, const char* message) {
        const auto& quality = settings::snapshot.rows[int(GraphicsRow::DlssQuality)];
        Require(quality.choices == choices && quality.selectedChoice == selected && quality.value == choices[size_t(selected)],
                message);
    };
    const std::vector<std::wstring> dlssChoices{L"Performance", L"Balanced", L"Quality", L"DLAA"};
    checkQuality(dlssChoices, 2, "saved DLSS Quality displays in third position");
    settings::pending = 8; Tick(base);
    Require(settings::edit.dlssQuality == DlssQuality::Dlaa, "DLSS right from Quality selects DLAA");
    checkQuality(dlssChoices, 3, "DLAA displays in fourth position");
    settings::pending = 8; Tick(base);
    Require(settings::edit.dlssQuality == DlssQuality::Performance, "DLSS right wraps to Performance");
    checkQuality(dlssChoices, 0, "DLSS Performance displays first");
    settings::pending = 8; Tick(base);
    Require(settings::edit.dlssQuality == DlssQuality::Balanced, "DLSS right selects Balanced");
    checkQuality(dlssChoices, 1, "DLSS Balanced displays second");
    settings::pending = 8; Tick(base);
    Require(settings::edit.dlssQuality == DlssQuality::Quality, "DLSS right selects Quality");
    checkQuality(dlssChoices, 2, "DLSS Quality displays third after cycling");
    settings::pending = 4; Tick(base);
    Require(settings::edit.dlssQuality == DlssQuality::Balanced, "DLSS left from Quality selects Balanced");
    settings::edit.dlssQuality = DlssQuality::Quality;
    settings::row = int(GraphicsRow::AntiAliasing);
    settings::pending = 0;
    Tick(base);
    settings::pending = 8;
    Tick(base);
    Require(settings::edit.upscaler == Upscaler::Fsr && !settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden,
            "FSR can be selected independently on D3D12");
    settings::row = int(GraphicsRow::DlssQuality);
    settings::edit.fsrQuality = gpu::upscaling::FsrQuality::Quality;
    settings::pending = 0; Tick(base);
    const std::vector<std::wstring> fsrChoices{L"Performance", L"Balanced", L"Quality", L"Native AA"};
    checkQuality(fsrChoices, 2, "saved FSR Quality displays in third position with Native AA last");
    Require(settings::snapshot.help == L"Performance, Balanced, Quality, or Native AA. Native AA keeps the output resolution.",
            "FSR help matches the choice order");
    settings::pending = 4; Tick(base);
    Require(settings::edit.fsrQuality == gpu::upscaling::FsrQuality::Balanced, "FSR left from Quality selects Balanced");
    checkQuality(fsrChoices, 1, "FSR Balanced displays second");
    settings::pending = 4; Tick(base);
    Require(settings::edit.fsrQuality == gpu::upscaling::FsrQuality::Performance, "FSR left selects Performance");
    checkQuality(fsrChoices, 0, "FSR Performance displays first");
    settings::pending = 4; Tick(base);
    Require(settings::edit.fsrQuality == gpu::upscaling::FsrQuality::NativeAA, "FSR left wraps to Native AA");
    checkQuality(fsrChoices, 3, "FSR Native AA displays last");
    settings::pending = 8; Tick(base);
    Require(settings::edit.fsrQuality == gpu::upscaling::FsrQuality::Performance, "FSR right wraps to Performance");
    settings::row = int(GraphicsRow::AntiAliasing);
    settings::pending = 8;
    Tick(base);
    Require(settings::edit.upscaler == Upscaler::Off && settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden, "DLSS can be turned off on D3D12");
    Require(settings::snapshot.notice == needsVulkan + L" The Off choice is not applied yet.",
            "turning DLSS off before it is applied does not claim the plan is off");
    for (int i = 0; i < 4; ++i) { settings::pending = 8; Tick(base); } // Off -> FXAA -> SMAA -> TAA -> DLSS
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
    Require(settings::snapshot.rows[int(GraphicsRow::AntiAliasing)].enabled && !settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden, "unavailable device does not lock DLSS");
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
    Require(settings::snapshot.rows[int(GraphicsRow::AntiAliasing)].enabled && !settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden, "pending backend does not hide DLSS");
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
    Require(settings::snapshot.notice == L"DLSS is not in use. The DLSS choice is not applied yet.",
            "inactive D3D12 DLSS reports pending choice without claiming submitted output");
    Require(settings::snapshot.notice.find(L"Submitted") == std::wstring::npos,
            "unsaved DLSS is not described as submitted output");
    Require(settings::snapshot.notice.find(L"1707") == std::wstring::npos, "an inactive result does not keep a submitted size");
    Require(settings::snapshot.rows[int(GraphicsRow::AntiAliasing)].enabled && !settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden, "unsaved DLSS choice stays available");
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
    settings::row = int(GraphicsRow::AntiAliasing);
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
        Require(settings::brightnessOpen && settings::active && !settings::bypass, "A opens the brightness page");
        const int savedBrightness = settings::edit.displayBrightness;
        settings::pending = 8; Tick(base);
        Require(settings::edit.displayBrightness == savedBrightness + 1, "page edits brightness");
        settings::brightnessFocus = 3; // Original pattern.
        settings::pending = 0x1000; Tick(base);
        Require(settings::bypass && !settings::active && settings::cancelPolls == 6, "explicit calibration handoff");
        settings::cancelPolls = 0; // The native input/calibration boundary is simulated below.
        PPC_STORE_U32(Menu + 4, 6); Tick(base);
        PPC_STORE_U32(Menu + 4, 4); Tick(base);
        Require(settings::active && !settings::bypass && settings::brightnessOpen &&
                settings::edit.displayBrightness == savedBrightness + 1, "calibration returns to the page with its edit");
        settings::bypass = true; settings::sawModal = false;
        PPC_STORE_U32(Menu + 4, 1); Tick(base);
        PPC_STORE_U32(Menu + 4, 4); Tick(base);
        Require(settings::active && !settings::bypass && !settings::brightnessOpen &&
                settings::edit.displayBrightness == savedBrightness, "idle address reuse clears stale handoff");
        // DLSS 5 neural rendering tuning page.
        {
            const settings::Config savedEdit = settings::edit;
            auto press = [&](uint16_t bits) { settings::pending = bits; Tick(base); };
            auto openPage = [&] {
                settings::nrFocus = 0;
                press(0x1000);
                Require(settings::nrOpen && settings::active && settings::nrFocus == 0, "A opens the neural rendering page");
            };
            settings::tab = 2; settings::row = int(GraphicsRow::DlssNeuralRendering);
            settings::edit.upscaler = gpu::upscaling::Upscaler::Off;
            press(0x1000);
            Require(!settings::nrOpen, "the page needs the DLSS upscaler");
            settings::edit.upscaler = gpu::upscaling::Upscaler::Dlss;
            settings::edit.dlssNeuralRendering = 2;
            settings::edit.dlssNrStyle = 0;
            settings::edit.dlssNrIntensity = 100;
            settings::edit.dlssNrSkin = -100;
            press(8);
            Require(settings::edit.dlssNeuralRendering == 3 && !settings::nrOpen, "right on the row still cycles passes");
            press(4);
            Require(settings::edit.dlssNeuralRendering == 2, "left on the row still cycles passes");
            Require(settings::snapshot.help.find(L"tuning page") != std::wstring::npos ||
                    settings::snapshot.help.find(L"調整頁面") != std::wstring::npos, "the row help mentions the tuning page");

            // Cancel restores every field, passes included.
            openPage();
            Require(settings::snapshot.neuralRendering.open && settings::snapshot.neuralRendering.passes == 2, "snapshot carries the open page");
            press(8);
            Require(settings::edit.dlssNeuralRendering == 3, "focus 0 edits passes");
            press(2); press(2); press(2);
            Require(settings::nrFocus == 3, "down moves focus");
            press(8); press(8);
            Require(settings::edit.dlssNrIntensity == 110, "intensity steps by 5");
            const auto live = settings::GetNeuralRenderingTuning();
            Require(live.open && live.intensity == 110 && live.passes == 3 && live.focus == 3,
                    "the live tuning returns unsaved values while the page is open");
            settings::nrFocus = 11;
            press(0x1000);
            Require(!settings::nrOpen && settings::edit.dlssNrIntensity == 100 && settings::edit.dlssNeuralRendering == 2,
                    "Cancel restores the start values and closes");
            Require(!settings::GetNeuralRenderingTuning().open && settings::GetNeuralRenderingTuning().intensity == 100,
                    "closed page reports saved values");

            // Done keeps the edit.
            openPage();
            press(2); press(8);
            Require(settings::edit.dlssNrStyle == 1, "focus 1 cycles the model");
            settings::nrFocus = 10;
            press(0x1000);
            Require(!settings::nrOpen && settings::edit.dlssNrStyle == 1, "Done keeps the edit and closes");

            // Default resets the tuning but keeps passes; Back then behaves like Cancel.
            openPage();
            press(8);
            settings::nrFocus = 3; press(8);
            settings::nrFocus = 7; press(8);
            Require(settings::edit.dlssNrIntensity == 105 && settings::edit.dlssNrSkin == -95 && settings::edit.dlssNeuralRendering == 3,
                    "edits before Default");
            settings::nrFocus = 9;
            press(0x1000);
            Require(settings::nrOpen && settings::edit.dlssNrStyle == 0 && settings::edit.dlssNrIntensity == 100 &&
                    settings::edit.dlssNrSkin == -100 && settings::edit.dlssNeuralRendering == 3,
                    "Default resets the tuning and keeps passes");
            press(0x2000);
            Require(!settings::nrOpen && settings::active && settings::edit.dlssNrStyle == 1 && settings::edit.dlssNeuralRendering == 2,
                    "Back closes like Cancel");

            // Percent bounds, the toggle, button navigation and wrap.
            openPage();
            settings::nrFocus = 3;
            for (int i = 0; i < 30; ++i) press(8);
            Require(settings::edit.dlssNrIntensity == 200, "percent clamps at 200");
            settings::nrFocus = 8; press(4);
            Require(!settings::edit.dlssNrAutoMask, "left flips the character mask");
            settings::nrFocus = 0; press(1);
            Require(settings::nrFocus == 11, "up wraps from the first control to Cancel");
            press(8);
            Require(settings::nrFocus == 9, "right wraps between the buttons");
            settings::nrFocus = 11; press(2);
            Require(settings::nrFocus == 0, "down wraps from Cancel to the first control");

            // Pointer: a slider click sets the value and focus, a button click activates.
            settings::nrFocus = 0;
            settings::PointerClick(476, 515, false); // intensity track at 75% of its width
            press(0);
            Require(settings::nrFocus == 3 && settings::edit.dlssNrIntensity == 150, "slider click sets the value");
            settings::PointerDrag(settings::NrColumnX[0] + 155 + 215 + 40, 515, true);
            press(0);
            Require(settings::edit.dlssNrIntensity == 200, "dragging clamps at the track end");
            settings::PointerClick(300, 620, false); // Default
            press(0);
            Require(settings::nrOpen && settings::edit.dlssNrIntensity == 100, "button click runs Default");
            settings::PointerClick(settings::NrColumnX[0] + 30 + 2 * 330 + 10, 620, false); // Cancel
            press(0);
            Require(!settings::nrOpen, "Cancel button click closes");

            // Escape cancels like Back.
            openPage();
            press(8);
            Require(settings::CalibrationKey(27), "the open page takes Escape");
            press(0);
            Require(!settings::nrOpen && settings::edit.dlssNeuralRendering == 2, "Escape cancels");

            // Preview availability reaches the snapshot at once.
            settings::SetNeuralRenderingPreviewAvailable(false);
            const uint64_t revision = settings::snapshot.revision;
            settings::SetNeuralRenderingPreviewAvailable(true);
            Require(settings::snapshot.revision == revision + 1 && settings::snapshot.neuralRendering.sceneAvailable &&
                    settings::GetNeuralRenderingTuning().sceneAvailable, "preview availability bumps the revision");
            settings::SetNeuralRenderingPreviewAvailable(true);
            Require(settings::snapshot.revision == revision + 1, "unchanged availability does not bump it");
            settings::SetNeuralRenderingPreviewAvailable(false);
            settings::edit = savedEdit;
            std::puts("PASS neural rendering page: open gate, steps, Cancel/Done/Default/Back/Escape, pointer, preview availability");
        }
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

        // Aspect ratio workflow:
        // 1. A 3440x1440 Auto configuration lists the 21:9 output resolutions.
        using AspectMode = gpu::aspect_ratio::Mode;
        {
            currentConfig.width = 3440;
            currentConfig.height = 1440;
            currentConfig.aspectRatio = AspectMode::Auto;
            diskConfig = currentConfig;
            settings::edit = currentConfig;
            settings::tab = 2;
            settings::row = int(GraphicsRow::AspectRatio);
            settings::status.clear();
            settings::Publish(base, ConfigData);
            Require(settings::snapshot.rows.size() == size_t(GraphicsRow::Count), "graphics tab has one row per id");
            const auto& aspectRow = settings::snapshot.rows[int(GraphicsRow::AspectRatio)];
            Require(aspectRow.name == L"Aspect ratio" && aspectRow.value == L"Auto" && aspectRow.selectedChoice == 0 &&
                    aspectRow.choices.size() == 4 && aspectRow.choices[1] == L"16:9" && aspectRow.choices[2] == L"21:9" &&
                    aspectRow.choices[3] == L"4:3", "Aspect ratio offers Auto, 16:9, 21:9 and 4:3");
            Require(settings::snapshot.help.find(L"black bars") != std::wstring::npos, "Aspect ratio help names the bars");
            const auto& resRow = settings::snapshot.rows[int(GraphicsRow::OutputResolution)];
            Require(resRow.name == L"Output resolution", "output resolution keeps its graphics id");
            Require(resRow.choices.size() == 5 && resRow.choices[0] == L"1720 × 720" && resRow.choices[1] == L"2560 × 1080" &&
                    resRow.choices[2] == L"3440 × 1440" && resRow.choices[3] == L"3840 × 1600" &&
                    resRow.choices[4] == L"5120 × 2160", "Auto on a 21:9 resolution lists the 21:9 choices");
            Require(resRow.selectedChoice == 2 && resRow.value == L"3440 × 1440",
                    "3440x1440 selected in 21:9 output choices");
        }

        // 2. From 1280x720 Auto: 16:9, then 21:9 (1720x720) and its five tiers,
        // 4:3 by nearest height, Auto keeping the resolution, cancel, save.
        {
            currentConfig.width = 1280;
            currentConfig.height = 720;
            currentConfig.aspectRatio = AspectMode::Auto;
            diskConfig = currentConfig;
            settings::edit = currentConfig;
            settings::tab = 2;
            settings::row = int(GraphicsRow::AspectRatio);
            settings::Publish(base, ConfigData);
            Require(settings::snapshot.rows[int(GraphicsRow::OutputResolution)].choices.size() == 5 &&
                    settings::snapshot.rows[int(GraphicsRow::OutputResolution)].choices[0] == L"1280 × 720" &&
                    settings::snapshot.rows[int(GraphicsRow::OutputResolution)].choices[4] == L"3840 × 2160",
                    "Auto on 1280x720 lists the 16:9 choices");

            settings::pending = 0x1008; Tick(base); // Right arrow / Confirm
            Require(settings::edit.aspectRatio == AspectMode::Wide && settings::edit.width == 1280 && settings::edit.height == 720,
                    "16:9 keeps a 16:9 resolution");
            settings::pending = 0x1008; Tick(base);
            Require(settings::edit.aspectRatio == AspectMode::Ultrawide && settings::edit.width == 1720 && settings::edit.height == 720,
                    "21:9 from 1280x720 maps to 1720x720");
            Require(settings::snapshot.rows[int(GraphicsRow::AspectRatio)].value == L"21:9", "Aspect ratio shows 21:9");
            Require(settings::snapshot.rows[int(GraphicsRow::OutputResolution)].selectedChoice == 0 &&
                    settings::snapshot.rows[int(GraphicsRow::OutputResolution)].value == L"1720 × 720", "1720x720 selected");

            // Move to output resolution and cycle forward through all 5 ultrawide tiers
            settings::row = int(GraphicsRow::OutputResolution);
            constexpr uint32_t expected21_9[][2] = {
                {2560, 1080}, {3440, 1440}, {3840, 1600}, {5120, 2160}, {1720, 720}};
            for (size_t i = 0; i < 5; ++i)
            {
                settings::pending = 0x1008; Tick(base);
                Require(settings::edit.width == expected21_9[i][0] && settings::edit.height == expected21_9[i][1],
                        "cycle 21:9 resolution matches expected tier");
            }

            // 4:3 from 5120x2160 keeps the 2160 height; its list has four choices.
            settings::edit.width = 5120;
            settings::edit.height = 2160;
            settings::row = int(GraphicsRow::AspectRatio);
            settings::pending = 0x1008; Tick(base);
            Require(settings::edit.aspectRatio == AspectMode::Standard && settings::edit.width == 2880 && settings::edit.height == 2160,
                    "4:3 from 5120x2160 maps to 2880x2160");
            const auto& standardRow = settings::snapshot.rows[int(GraphicsRow::OutputResolution)];
            Require(standardRow.choices.size() == 4 && standardRow.choices[0] == L"960 × 720" &&
                    standardRow.choices[1] == L"1440 × 1080" && standardRow.choices[2] == L"1920 × 1440" &&
                    standardRow.choices[3] == L"2880 × 2160" && standardRow.selectedChoice == 3, "4:3 output choices");
            settings::row = int(GraphicsRow::OutputResolution);
            settings::pending = 0x1008; Tick(base);
            Require(settings::edit.width == 960 && settings::edit.height == 720, "4:3 resolutions wrap to 960x720");

            // Auto keeps the window size and its list; Left goes back to 4:3.
            settings::row = int(GraphicsRow::AspectRatio);
            settings::pending = 0x1008; Tick(base);
            Require(settings::edit.aspectRatio == AspectMode::Auto && settings::edit.width == 960 && settings::edit.height == 720,
                    "Auto keeps the resolution");
            Require(settings::snapshot.rows[int(GraphicsRow::OutputResolution)].choices.size() == 4,
                    "Auto on a 4:3 resolution lists the 4:3 choices");
            settings::pending = 0x1004; Tick(base);
            Require(settings::edit.aspectRatio == AspectMode::Standard && settings::edit.width == 960,
                    "Left cycles back to 4:3");

            // Cancel / exit without saving: disk remains untouched at initial 1280x720
            const auto oldSaves = saves;
            const unsigned oldCloses = closes;
            settings::pending = 0x2000; Tick(base); // Back button to close menu
            Require(closes == oldCloses + 1, "close called on Back");
            Require(saves == oldSaves, "cancel does not write to disk");
            Require(diskConfig.width == 1280 && diskConfig.height == 720 && diskConfig.aspectRatio == AspectMode::Auto,
                    "disk config unchanged on cancel");

            // Native tick completes the close
            Tick(base);
            Require(!settings::closing && PPC_LOAD_U32(Menu + 4) == 1, "native tick completes close");
            settings::releaseToParent = false;
            settings::waitForRelease = false;

            // Reopen menu: edit restores cleanly from GetConfig()
            PPC_STORE_U32(Menu + 4, 4); Tick(base); Poll(0, true);
            Require(settings::edit.width == 1280 && settings::edit.height == 720 && settings::edit.aspectRatio == AspectMode::Auto,
                    "reopening restores saved config without unapplied preview changes");

            // 21:9 at 3440x1440 and Save: goes through display change state machine
            settings::tab = 2;
            settings::row = int(GraphicsRow::AspectRatio);
            settings::pending = 0x1008; Tick(base); // 16:9
            settings::pending = 0x1008; Tick(base); // 21:9 -> 1720x720
            settings::row = int(GraphicsRow::OutputResolution);
            settings::pending = 0x1008; Tick(base); // 2560x1080
            settings::pending = 0x1008; Tick(base); // 3440x1440
            Require(settings::edit.width == 3440 && settings::edit.height == 1440, "selected 3440x1440");
            settings::row = int(GraphicsRow::Save);
            settings::pending = 0x1000; Tick(base);
            Require(saves == oldSaves + 1 && diskConfig.width == 3440 && diskConfig.height == 1440 &&
                    diskConfig.aspectRatio == AspectMode::Ultrawide, "Save persists 21:9 and 3440x1440 to disk config");
            auto saveTicket = settings::displayTicket;
            Require(saveTicket != 0, "Save triggers BeginDisplayChange ticket");
            displayChanges.WindowComplete(saveTicket, true);
            displayChanges.Complete(saveTicket, true); Tick(base);
            Require(!settings::displayTicket, "display state machine completed successfully for 3440x1440");
            Require(settings::status == L"Display settings saved.", "status shows display saved");
            Require(!settings::restartPrompt, "an aspect ratio change needs no restart");
        }
        std::puts("PASS Aspect ratio workflow: Auto lists by shape, 16:9/21:9/4:3 snap the resolution, 4:3 choices, Auto keeps the size, cancel discard, Save state machine");

        // Start / Enter focus-jump, simultaneous confirm suppression, PointerClick viewport clipping and DLSS quality visibility:
        {
            const auto savesBefore = saves;
            settings::tab = 2; // Graphics tab
            settings::row = int(GraphicsRow::Backend);
            settings::edit.upscaler = gpu::upscaling::Upscaler::Off;
            settings::Publish(base, ConfigData);

            Require(settings::snapshot.rows.size() == size_t(GraphicsRow::Count), "graphics tab has one row per id");
            Require(settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden, "DLSS quality is hidden when upscaler is Off");

            settings::row = int(GraphicsRow::AntiAliasing);
            settings::pending = 2; Tick(base); // D-pad down
            Require(settings::row == int(GraphicsRow::FrameGeneration),
                    "down from Upscaler skips hidden quality, sharpness and neural rendering rows to FG");
            settings::pending = 1; Tick(base); // D-pad up
            Require(settings::row == int(GraphicsRow::AntiAliasing),
                    "up from FG skips the hidden upscaler rows back to Upscaler");

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

            // System tab (tab 3): Start (0x10) jumps focus to its last row, Save settings
            settings::tab = 3;
            settings::row = 0;
            settings::pending = 0x10; Tick(base);
            Require(settings::row == settings::SystemSaveRow && settings::snapshot.rows.size() == size_t(settings::SystemRowCount) &&
                    settings::snapshot.rows[settings::SystemSaveRow].name == L"Save settings",
                    "Start on System tab jumps to Save settings, the last row");

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
        // FG is a dedicated section within Graphics. Its rows share the
        // Graphics Save action while System retains its separate Save.
        {
            using framegen::Provider;
            const auto priorCurrent = currentConfig;
            const auto priorDisk = diskConfig;
            settings::Config saved = currentConfig;
            saved.graphicsBackend = settings::GraphicsBackend::D3D12;
            saved.uiLanguage = 0;
            saved.frameGenerationProvider = Provider::Off;
            saved.frameGenerationMultiplier = 2;
            currentConfig = diskConfig = saved;
            settings::edit = saved;
            settings::tab = 2;
            settings::row = int(GraphicsRow::FrameGeneration);
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.rows.size() == size_t(GraphicsRow::Count), "Graphics page includes FG section rows");
            Require(settings::snapshot.rows[int(GraphicsRow::FrameGeneration)].choices ==
                    std::vector<std::wstring>{L"Off", L"DLSS", L"FSR"},
                    "D3D12 FG section offers Off, DLSS and FSR");
            Require(settings::snapshot.rows[int(GraphicsRow::FrameGenerationMultiplier)].hidden,
                    "Off hides the multiplier");
            Require(settings::snapshot.notice == L"Frame generation is off.", "FG status shows Off");
            gpu::video::menuFlowFgStatus.phase = gpu::video::FrameGenerationPhase::Pending;
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.notice == L"Applying FG settings…", "FG status shows pending transition");
            gpu::video::menuFlowFgStatus.phase = gpu::video::FrameGenerationPhase::Ready;
            gpu::video::menuFlowFgStatus.applied = Provider::Dlss;
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.notice.find(L"DLSS FG ready.") != std::wstring::npos,
                    "FG status names the applied provider");
            gpu::video::menuFlowFgStatus.phase = gpu::video::FrameGenerationPhase::Unavailable;
            gpu::video::menuFlowFgStatus.environmentOverride = true;
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.notice.find(L"FG is unavailable") != std::wstring::npos &&
                    settings::snapshot.notice.find(L"diagnostic override") != std::wstring::npos,
                    "FG status exposes fallback and diagnostic override");
            gpu::video::menuFlowFgStatus = {};
            settings::pending = 2; Tick(base);
            Require(settings::row == int(GraphicsRow::AmbientOcclusion), "Off navigation skips hidden multiplier");

            settings::row = int(GraphicsRow::FrameGeneration);
            settings::pending = 8; Tick(base);
            Require(settings::edit.frameGenerationProvider == Provider::Dlss &&
                    !settings::snapshot.rows[int(GraphicsRow::FrameGenerationMultiplier)].hidden,
                    "DLSS reveals multiplier row");
            settings::row = int(GraphicsRow::FrameGenerationMultiplier);
            settings::pending = 4; Tick(base);
            Require(settings::edit.frameGenerationMultiplier == 6 &&
                    settings::snapshot.rows[int(GraphicsRow::FrameGenerationMultiplier)].value == L"6×" &&
                    settings::snapshot.rows[int(GraphicsRow::FrameGenerationMultiplier)].choices.size() == 5,
                    "DLSS multiplier wraps from 2 to 6");
            settings::pending = 8; Tick(base);
            Require(settings::edit.frameGenerationMultiplier == 2, "DLSS multiplier wraps from 6 to 2");
            settings::pending = 8; Tick(base);
            Require(settings::edit.frameGenerationMultiplier == 3, "DLSS multiplier accepts 3x");

            settings::row = int(GraphicsRow::FrameGeneration);
            settings::pending = 8; Tick(base);
            Require(settings::edit.frameGenerationProvider == Provider::Fsr &&
                    settings::edit.frameGenerationMultiplier == 2 &&
                    settings::snapshot.rows[int(GraphicsRow::FrameGenerationMultiplier)].hidden,
                    "FSR uses fixed 2x and hides multiplier");
            settings::pending = 2; Tick(base);
            Require(settings::row == int(GraphicsRow::AmbientOcclusion), "FSR navigation skips hidden multiplier");
            settings::row = int(GraphicsRow::FrameGeneration);
            settings::pending = 4; Tick(base);
            Require(settings::edit.frameGenerationProvider == Provider::Dlss, "FG provider cycles back to DLSS");
            settings::row = int(GraphicsRow::FrameGenerationMultiplier);
            settings::pending = 8; Tick(base);
            Require(settings::edit.frameGenerationMultiplier == 3, "DLSS multiplier can be reselected after FSR");

            settings::edit.scalingQuality = saved.scalingQuality == 0 ? 1 : 0;
            settings::edit.uiLanguage = saved.uiLanguage == 4 ? 0 : 4;
            settings::edit.frameGenerationMode = framegen::Mode::Dynamic;
            settings::edit.frameGenerationTargetFps = 144;
            settings::row = int(GraphicsRow::FrameGeneration);
            const auto beforeSave = saves;
            settings::pending = 0x1010; Tick(base);
            Require(settings::row == int(GraphicsRow::Save) && saves == beforeSave,
                    "Start plus Confirm in Graphics focuses Save without writing");
            settings::pending = 0x1000; Tick(base);
            Require(saves == beforeSave + 1 && diskConfig.frameGenerationProvider == Provider::Dlss &&
                    diskConfig.frameGenerationMultiplier == 3 &&
                    diskConfig.frameGenerationMode == framegen::Mode::Dynamic &&
                    diskConfig.frameGenerationTargetFps == 144 &&
                    diskConfig.scalingQuality == settings::edit.scalingQuality &&
                    diskConfig.uiLanguage == saved.uiLanguage,
                    "Graphics Save commits FG and graphics together without Language edits");
            Require(settings::edit.uiLanguage != saved.uiLanguage &&
                    settings::snapshot.help == L"显示设置已保存。",
                    "Graphics Save preserves pending Language edit and acknowledges save");

            // The System page still saves only language and update fields, leaving
            // a pending FG change for the shared Graphics Save action.
            const auto graphicsSaved = diskConfig;
            settings::edit.frameGenerationProvider = Provider::Fsr;
            settings::edit.frameGenerationMultiplier = 2;
            settings::tab = 3;
            settings::row = settings::SystemSaveRow;
            const auto beforeLanguageSave = saves;
            settings::pending = 0x1000; Tick(base);
            Require(saves == beforeLanguageSave + 1 && diskConfig.uiLanguage == settings::edit.uiLanguage &&
                    diskConfig.frameGenerationProvider == graphicsSaved.frameGenerationProvider &&
                    diskConfig.frameGenerationMultiplier == graphicsSaved.frameGenerationMultiplier,
                    "System Save does not apply a pending Graphics FG change");
            Require(settings::edit.frameGenerationProvider == Provider::Fsr,
                    "System Save keeps the unsaved FG selection");
            Require(settings::status == (settings::edit.uiLanguage == 4 ? L"系统设置已保存。" : L"System settings saved."),
                    "System Save acknowledges with the System page name");

            settings::tab = 0;
            settings::row = 0;
            settings::pending = 0; Tick(base);
            settings::PointerClick(float(386 + 2 * settings::MenuTabWidth + 8), 126.0f, false);
            settings::pending = 0; Tick(base);
            Require(settings::tab == 2 && settings::row == 0, "mouse selects the Graphics tab");
            settings::row = int(GraphicsRow::FrameGeneration);
            settings::pending = 0; Tick(base);
            int visibleFg = 0;
            for (int i = 0; i < int(GraphicsRow::FrameGeneration); ++i)
                visibleFg += !settings::snapshot.rows[size_t(i)].hidden;
            const int fgSlot = visibleFg - settings::snapshot.scroll;
            Require(fgSlot >= 0 && fgSlot < settings::kMenuVisibleRows,
                    "focused FG section stays inside scrolled Graphics viewport");
            settings::PointerClick(100.0f, float(150 + fgSlot * 43 + 18), false);
            settings::pending = 0; Tick(base);
            Require(settings::row == int(GraphicsRow::FrameGeneration),
                    "mouse hit-testing maps scrolled FG section to its logical row");
            settings::pending = 0x200; Tick(base);
            Require(settings::tab == 3, "right shoulder moves Graphics to System in four tabs");
            settings::pending = 0x200; Tick(base);
            Require(settings::tab == 0, "right shoulder wraps System to Gameplay");
            // MenuFlowTranslate is the production Translate, renamed by the include above.
            Require(std::wstring(settings::MenuFlowTranslate(1, L"System", L"系統")) == L"系統" &&
                    std::wstring(settings::MenuFlowTranslate(2, L"System", L"系統")) == L"システム" &&
                    std::wstring(settings::MenuFlowTranslate(3, L"System", L"系統")) == L"시스템" &&
                    std::wstring(settings::MenuFlowTranslate(4, L"System", L"系統")) == L"系统",
                    "System tab name is translated in every interface language");
            gpu::video::menuFlowFgStatus.sessionProvider = Provider::Dlss;
            gpu::video::menuFlowFgStatus.requested = Provider::Fsr;
            settings::tab = 2;
            settings::row = int(GraphicsRow::Save);
            settings::pending = 0x1000; Tick(base);
            Require(settings::restartPrompt && settings::savedRestartPrompt &&
                    settings::snapshot.dialogMessage.find(L"DLSS FG") != std::wstring::npos &&
                    settings::snapshot.dialogMessage.find(L"FSR FG") != std::wstring::npos,
                    "DLSS FG to FSR FG save explains that restart is required");
            settings::pending = 0x2000; Tick(base);
            Require(!settings::restartPrompt && diskConfig.frameGenerationProvider == Provider::Fsr,
                    "Later retains the FSR FG choice for the next launch");
            gpu::video::menuFlowFgStatus = {};
            currentConfig = priorCurrent;
            diskConfig = priorDisk;
            std::puts("PASS Graphics FG section: provider/input navigation, DLSS multiplier bounds, FSR fixed 2x, shared Save, System isolation, mouse/scroll and four-tab navigation");
        }
        // FSR sharpness follows quality; Save is always last. Off disables
        // RCAS, and percent changes are bounded.
        {
            static_assert(int(GraphicsRow::Save) + 1 == int(GraphicsRow::Count) &&
                          int(GraphicsRow::FsrSharpness) == int(GraphicsRow::DlssQuality) + 1);
            settings::tab = 2;
            settings::status.clear();
            settings::edit = currentConfig;
            settings::edit.upscaler = gpu::upscaling::Upscaler::Off;
            settings::edit.fsrSharpnessPercent = 0;
            settings::row = int(GraphicsRow::AntiAliasing);
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].hidden, "Off hides FSR sharpness");
            settings::row = int(GraphicsRow::Save);
            settings::pending = 2; Tick(base);
            Require(settings::row == int(GraphicsRow::Backend), "navigation skips hidden FSR sharpness");
            settings::edit.upscaler = gpu::upscaling::Upscaler::Dlss;
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].hidden, "DLSS hides FSR sharpness");
            settings::edit.upscaler = gpu::upscaling::Upscaler::Fsr;
            settings::row = int(GraphicsRow::FsrSharpness);
            settings::pending = 0; Tick(base);
            Require(!settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].hidden &&
                    settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].value == L"Off" &&
                    settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].choices.size() == 101,
                    "FSR displays Off and 1-100 percent options");
            Require(settings::snapshot.help.find(L"Off disables RCAS") != std::wstring::npos,
                    "FSR help describes RCAS disabled at zero");
            settings::row = int(GraphicsRow::DlssQuality);
            settings::pending = 2; Tick(base);
            Require(settings::row == int(GraphicsRow::FsrSharpness) && settings::snapshot.scroll == 0,
                    "D-pad reaches FSR sharpness directly after quality without scrolling");
            settings::pending = 4; Tick(base);
            Require(settings::edit.fsrSharpnessPercent == 0, "left at zero does not wrap to 100");
            settings::pending = 8; Tick(base);
            Require(settings::edit.fsrSharpnessPercent == 1 &&
                    settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].value == L"1%", "right turns on 1 percent");
            settings::edit.fsrSharpnessPercent = 100;
            settings::pending = 8; Tick(base);
            Require(settings::edit.fsrSharpnessPercent == 100 &&
                    settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].value == L"100%", "right at 100 clamps");
            settings::edit.fsrSharpnessPercent = 64;
            const unsigned beforeSave = saves;
            settings::row = int(GraphicsRow::Save);
            settings::pending = 0x1000; Tick(base);
            Require(saves == beforeSave + 1 && diskConfig.fsrSharpnessPercent == 64 &&
                    currentConfig.fsrSharpnessPercent == 64, "existing Save action persists FSR sharpness");
            std::puts("PASS FSR sharpness menu visibility, 0/100 bounds, description, stable ids and Save");
        }
        // DLSS 5 neural rendering shows with DLSS only and cycles Off, 1x-4x.
        {
            const auto nr = int(GraphicsRow::DlssNeuralRendering);
            settings::tab = 2;
            settings::edit = currentConfig;
            settings::edit.upscaler = gpu::upscaling::Upscaler::Fsr;
            settings::edit.dlssNeuralRendering = 0;
            settings::row = int(GraphicsRow::AntiAliasing);
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.rows[nr].hidden, "FSR hides DLSS neural rendering");
            settings::edit.upscaler = gpu::upscaling::Upscaler::Dlss;
            settings::row = nr;
            settings::pending = 0; Tick(base);
            Require(!settings::snapshot.rows[nr].hidden && settings::snapshot.rows[nr].value == L"Off" &&
                    settings::snapshot.rows[nr].choices.size() == 5, "DLSS shows neural rendering Off and 1x-4x");
            for (uint32_t passes = 1; passes <= 4; ++passes) {
                settings::pending = 8; Tick(base);
                Require(settings::edit.dlssNeuralRendering == passes, "right adds one pass");
            }
            settings::pending = 8; Tick(base);
            Require(settings::edit.dlssNeuralRendering == 0, "right after 4x wraps to Off");
            settings::pending = 4; Tick(base);
            Require(settings::edit.dlssNeuralRendering == 4 && settings::snapshot.rows[nr].value == L"4×",
                    "left from Off wraps to 4x");
            std::puts("PASS DLSS neural rendering visibility and pass cycling");
        }
        // Button prompts follow the seven retail Game settings. Right cycles
        // Auto, Xbox, PlayStation, applies live and saves at once.
        {
            settings::tab = 0;
            settings::row = settings::GamePromptRow;
            settings::status.clear();
            currentConfig.buttonPrompts = diskConfig.buttonPrompts = 0;
            settings::edit = currentConfig;
            settings::edit.uiLanguage = 0;
            const unsigned beforeSaves = saves;
            promptStyle = 0;
            settings::pending = 8; Tick(base);
            Require(settings::snapshot.rows[settings::GamePromptRow].name == L"Button prompts" &&
                    settings::snapshot.rows[settings::GamePromptRow].choices.size() == 3 &&
                    settings::snapshot.rows[settings::GamePromptRow].selectedChoice == 1 &&
                    promptStyle == 1 && diskConfig.buttonPrompts == 1 && saves == beforeSaves + 1,
                    "Button prompts cycles to Xbox, applies live and saves");
            settings::pending = 8; Tick(base);
            Require(promptStyle == 2 && diskConfig.buttonPrompts == 2, "Button prompts cycles to PlayStation");
            settings::pending = 8; Tick(base);
            Require(promptStyle == 0 && diskConfig.buttonPrompts == 0, "Button prompts wraps back to Auto");
            Require(settings::snapshot.help.find(L"Auto follows the controller") != std::wstring::npos, "Button prompts help");
            std::puts("PASS Button prompts row");
        }
        // Vibration follows Button prompts on the Gameplay tab. It applies and saves
        // at once, stays within 0-100 and never commits unsaved Graphics edits.
        {
            settings::tab = 0;
            settings::row = settings::GamePromptRow;
            settings::status.clear();
            currentConfig.vibrationPercent = diskConfig.vibrationPercent = 100;
            settings::edit = currentConfig;
            settings::edit.uiLanguage = 0;
            settings::edit.width = currentConfig.width == 2560 ? 1920 : 2560;
            const unsigned beforeSaves = saves, beforeApplies = applies, beforePreviews = vibrationPreviews;
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::GameVibrationRow && settings::snapshot.rows.size() == size_t(settings::GameRowCount) &&
                    settings::snapshot.rows[settings::GameVibrationRow].name == L"Vibration" &&
                    settings::snapshot.rows[settings::GameVibrationRow].sliderPercent == 100,
                    "Vibration slider follows Button prompts on the Gameplay tab");
            Require(settings::snapshot.help.find(L"Min turns it off") != std::wstring::npos, "Vibration help");
            settings::pending = 8; Tick(base);
            Require(settings::edit.vibrationPercent == 100 && saves == beforeSaves, "right at 100 clamps without saving");
            settings::pending = 4; Tick(base);
            Require(settings::edit.vibrationPercent == 90 && saves == beforeSaves + 1 && diskConfig.vibrationPercent == 90 &&
                    vibrationStrength == 90 && vibrationPreviews == beforePreviews + 1 && applies == beforeApplies,
                    "left lowers, saves, applies live and previews without a guest apply");
            Require(diskConfig.width != settings::edit.width, "vibration save leaves unsaved Graphics edits unsaved");
            Require(settings::snapshot.rows[settings::GameVibrationRow].sliderPercent == 90, "slider follows the strength");
            settings::edit.vibrationPercent = currentConfig.vibrationPercent = 0;
            settings::pending = 4; Tick(base);
            Require(settings::edit.vibrationPercent == 0 && saves == beforeSaves + 1, "left at 0 does not wrap");
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::GameRestoreRow, "the game actions follow Vibration");
            // Audio output follows Sound effects: Right switches to 5.1 live and saves at once.
            settings::tab = 1;
            settings::row = settings::AudioEffectsRow;
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::AudioOutputRow && settings::snapshot.rows.size() == size_t(settings::AudioRowCount) &&
                    settings::snapshot.rows[settings::AudioOutputRow].name == L"Audio output" &&
                    settings::snapshot.rows[settings::AudioOutputRow].selectedChoice == 0,
                    "Audio output follows Sound effects, Stereo by default");
            settings::pending = 8; Tick(base);
            Require(settings::edit.audioOutput == settings::AudioOutputSurround && diskConfig.audioOutput == settings::AudioOutputSurround &&
                    apu::menuFlowOutput == apu::Output::Surround && saves == beforeSaves + 2 && applies == beforeApplies &&
                    diskConfig.width != settings::edit.width, "audio output switches live and saves alone");
            // Matrix phase closes the Audio tab and only moves with Matrix surround.
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::AudioMatrixPhaseRow && !settings::snapshot.rows[settings::AudioMatrixPhaseRow].enabled &&
                    !apu::menuFlowTestSignal, "Matrix phase follows Audio output, disabled and silent without Matrix surround");
            settings::pending = 8; Tick(base);
            Require(settings::edit.audioMatrixPhase == 90 && saves == beforeSaves + 2, "disabled Matrix phase ignores Right");
            settings::pending = 1; Tick(base);
            settings::pending = 8; Tick(base);
            Require(settings::edit.audioOutput == settings::AudioOutputMatrix && apu::menuFlowOutput == apu::Output::Matrix &&
                    diskConfig.audioOutput == settings::AudioOutputMatrix && saves == beforeSaves + 3 && !apu::menuFlowTestSignal,
                    "Right again selects Matrix surround; no test noise on Audio output");
            settings::pending = 2; Tick(base);
            Require(apu::menuFlowTestSignal, "test noise plays while Matrix phase is focused");
            Require(settings::snapshot.speakerLayout, "the speaker layout shows under the rows");
            settings::pending = 8; Tick(base);
            Require(settings::snapshot.rows[settings::AudioMatrixPhaseRow].enabled &&
                    settings::snapshot.rows[settings::AudioMatrixPhaseRow].value == L"105°" &&
                    settings::edit.audioMatrixPhase == 105 && diskConfig.audioMatrixPhase == 105 && apu::menuFlowMatrixPhase == 105 &&
                    settings::snapshot.speakerPhase == 105 &&
                    saves == beforeSaves + 4 && applies == beforeApplies && diskConfig.width != settings::edit.width,
                    "Matrix phase steps 15 degrees, applies live and saves alone");
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::AudioVoiceRow && !apu::menuFlowTestSignal && !settings::snapshot.speakerLayout,
                    "down from Matrix phase wraps to Voice language and stops the test noise and layout");
            settings::edit = currentConfig;
            std::puts("PASS Gameplay Vibration slider and Audio output: bounds, immediate save, live apply, Graphics edits untouched");
        }
        // The host Settings game tab is reached from the retail System menu.
        // Only explicit dialog confirmation may request the guest title transition.
        {
            settings::tab = 0;
            settings::row = settings::GameMainMenuRow;
            settings::edit.uiLanguage = 0;
            settings::status.clear();
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.rows.size() == size_t(settings::GameRowCount) &&
                    settings::snapshot.rows[settings::GameRestoreRow].name == L"Restore game defaults" &&
                    settings::snapshot.rows[settings::GameMainMenuRow].name == L"Quit to Main Menu" &&
                    settings::snapshot.rows[settings::GameMainMenuRow].value == L"Return",
                    "Quit to Main Menu follows the existing game actions");
            Require(settings::graphics_menu::IsAction(0, settings::GameMainMenuRow), "mouse treats Return as an action");
            const unsigned beforeSaves = saves, beforeApplies = applies, beforeCloses = closes;
            settings::pending = 8; Tick(base);
            Require(!settings::mainMenuPrompt && !mainMenuRequests, "right arrow cannot return to title");
            settings::PointerClick(500, float(150 + (settings::GameMainMenuRow - settings::snapshot.scroll) * 43 + 20), false);
            Tick(base);
            Require(settings::mainMenuPrompt && settings::snapshot.dialogTitle == L"Quit to Main Menu" &&
                    settings::snapshot.dialogMessage == L"Return to the main menu? Unsaved progress will be lost." &&
                    settings::snapshot.dialogChoices == std::vector<std::wstring>{L"Return", L"Cancel"} &&
                    settings::snapshot.dialogSelection == 1 && !mainMenuRequests,
                    "mouse opens confirmation with Cancel preselected and no transition");
            settings::pending = 0x1000; Tick(base);
            Require(!settings::mainMenuPrompt && !mainMenuRequests && settings::snapshot.dialogChoices.empty(),
                    "confirm on default Cancel dismisses without returning");
            settings::pending = 0x1000; Tick(base);
            Require(settings::mainMenuPrompt && settings::snapshot.dialogSelection == 1, "gamepad reopens safe main-menu dialog");
            settings::pending = 1; Tick(base);
            Require(settings::snapshot.dialogSelection == 0 && !mainMenuRequests, "up selects Return without dispatching");
            settings::pending = 0x2000; Tick(base);
            Require(!settings::mainMenuPrompt && !mainMenuRequests && !settings::closing,
                    "Back cancels even when Return is selected");
            settings::edit.uiLanguage = 1;
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.rows[settings::GameMainMenuRow].name == L"退出到主選單", "traditional Chinese main-menu label");
            settings::pending = 0x1000; Tick(base);
            Require(settings::snapshot.dialogTitle == L"退出到主選單" &&
                    settings::snapshot.dialogMessage == L"返回主選單嗎？尚未儲存的進度將會遺失。" &&
                    settings::snapshot.dialogChoices == std::vector<std::wstring>{L"返回", L"取消"} &&
                    settings::snapshot.dialogSelection == 1,
                    "traditional Chinese confirmation and default cancellation");
            for (const auto& [language, title, message, choice, cancel] : {
                     std::tuple{2u, L"メインメニューに戻る", L"メインメニューに戻りますか？保存していない進行状況は失われます。", L"戻る", L"キャンセル"},
                     std::tuple{3u, L"메인 메뉴로 돌아가기", L"메인 메뉴로 돌아갈까요? 저장하지 않은 진행 상황은 사라집니다.", L"돌아가기", L"취소"},
                     std::tuple{4u, L"退出到主菜单", L"返回主菜单吗？未保存的进度将会丢失。", L"返回", L"取消"}})
            {
                settings::edit.uiLanguage = language;
                settings::pending = 0; Tick(base);
                Require(settings::snapshot.rows[settings::GameMainMenuRow].name == title &&
                        settings::snapshot.dialogTitle == title && settings::snapshot.dialogMessage == message &&
                        settings::snapshot.dialogChoices == std::vector<std::wstring>{choice, cancel} &&
                        settings::snapshot.dialogSelection == 1,
                        "Japanese, Korean and Simplified Chinese preserve translated confirmation and default Cancel");
            }
            settings::edit.uiLanguage = 1;
            settings::pending = 0; Tick(base);
            settings::pending = 1; Tick(base);
            settings::pending = 0x1000; Tick(base);
            Require(!settings::mainMenuPrompt && settings::closing && !settings::active &&
                    mainMenuRequests == 0 && applies == beforeApplies + 1 && closes == beforeCloses + 1,
                    "confirmed Return closes the retail Settings task before requesting title");
            settings::pending = 0; Tick(base);
            Require(mainMenuRequests == 1 && quitEventAttempts == 0 && saves == beforeSaves &&
                    applies == beforeApplies + 1 && closes == beforeCloses + 1,
                    "native completion requests title exactly once without SDL_EVENT_QUIT or settings file writes");
            settings::pending = 0; Tick(base);
            Require(mainMenuRequests == 1 && quitEventAttempts == 0,
                    "repeated native ticks do not request title or SDL_EVENT_QUIT again");
            std::puts("PASS System Settings Quit to Main Menu: cancellation, Chinese labels, native close then one title request, no SDL_EVENT_QUIT");
        }
        // The importer is launched only after the safe restart child waits for
        // this process to stop reading installed content. The fixture exercises
        // actual menu input/dispatch without launching any process or importer.
        {
            PPC_STORE_U32(Menu + 4, 4);
            settings::restart::Cancel(); // Prior graphics-restart fixture did not run a video-thread exit.
            Tick(base);
            const unsigned importSaves = saves, importApplies = applies, importCloses = closes;
            settings::tab = 0;
            settings::row = settings::GameMainMenuRow;
            settings::edit.uiLanguage = 0;
            settings::pending = 2; Tick(base);
            Require(settings::row == 0 && settings::snapshot.scroll == 0 &&
                    settings::snapshot.rows.size() == size_t(settings::GameRowCount),
                    "Gameplay ends with Quit to Main Menu and fits without scrolling");
            settings::tab = 3;
            settings::row = settings::SystemCollectionRow;
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::SystemImportRow && settings::snapshot.scroll == 0 &&
                    settings::snapshot.rows.size() == size_t(settings::SystemRowCount) &&
                    settings::snapshot.rows[settings::SystemImportRow].name == L"Import discs & DLC" &&
                    settings::snapshot.rows[settings::SystemImportRow].value == L"Open" &&
                    settings::snapshot.rows[settings::SystemSaveRow].name == L"Save settings" &&
                    settings::graphics_menu::IsAction(3, settings::SystemImportRow),
                    "gamepad reaches the importer action on System, just before Save settings");
            settings::pending = 8; Tick(base);
            Require(!settings::importPrompt && !settings::restart::Requested(), "right arrow cannot launch importer");
            settings::PointerClick(500, float(150 + (settings::SystemImportRow - settings::snapshot.scroll) * 43 + 20), false);
            Tick(base);
            Require(settings::importPrompt && settings::snapshot.dialogSelection == 1 &&
                    settings::snapshot.dialogChoices == std::vector<std::wstring>{L"Open importer", L"Cancel"} &&
                    !settings::restart::Requested(), "mouse opens cancel-first dialog without starting importer");
            settings::pending = 0x1000; Tick(base);
            Require(!settings::importPrompt && !settings::restart::Requested(), "confirm on default Cancel dismisses");
            settings::pending = 0x1000; Tick(base);
            settings::pending = 1; Tick(base);
            settings::pending = 0x2000; Tick(base);
            Require(!settings::importPrompt && !settings::restart::Requested(), "Back cancels selected import action");
            for (const auto& [language, name] : {
                     std::pair{1u, L"匯入光碟與 DLC"}, std::pair{2u, L"ディスクとDLCをインポート"},
                     std::pair{3u, L"디스크 및 DLC 가져오기"}, std::pair{4u, L"导入光盘与 DLC"}})
            {
                settings::edit.uiLanguage = language;
                settings::pending = 0; Tick(base);
                Require(settings::snapshot.rows[settings::SystemImportRow].name == name, "translated importer action");
            }
            settings::edit.uiLanguage = 0;
            settings::pending = 0; Tick(base);
            auto preview = settings::snapshot;
            preview.assets.reset();
            std::vector<uint32_t> pixels;
            Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "importer menu preview");
            const auto evidence = std::filesystem::current_path() / "out" / "import-menu-preview";
            std::filesystem::create_directories(evidence);
            WriteBmp(evidence / "import-action.bmp", pixels);
            settings::pending = 0x1000; Tick(base);
            Require(settings::importPrompt && settings::snapshot.dialogSelection == 1, "gamepad opens importer dialog");
            preview = settings::snapshot;
            preview.assets.reset();
            Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "importer dialog preview");
            WriteBmp(evidence / "import-confirmation.bmp", pixels);
            settings::pending = 1; Tick(base);
            settings::pending = 0x1000; Tick(base);
#ifdef _WIN32
            Require(settings::restart::InstallRequested() && !settings::importPrompt &&
                    settings::snapshot.help == L"Closing game and opening importer…" &&
                    saves == importSaves && applies == importApplies && closes == importCloses,
                    "confirmation requests one install restart without saving or closing settings as a guest action");
            settings::pending = 0x1000; Tick(base);
            Require(settings::restart::InstallRequested() && !settings::importPrompt,
                    "repeated confirmation cannot spawn a second child");
            // Process exit is exercised by restart_test's isolated child; reset
            // only this fixture's request before testing duplicate presses.
            settings::restart::Cancel();
#else
            Require(!settings::restart::Requested(), "unsupported restart never overlaps live guest");
#endif
            settings::pending = 0; Tick(base);
            Require(!settings::restart::Requested() && !settings::importPrompt,
                    "idle ticks do not create a second import request");
            std::puts("PASS import action: gamepad/mouse focus, cancel-first dialog, translations, guarded restart request, previews");
        }
        if (argc == 3)
        {
            settings::status.clear();
            settings::edit.uiLanguage = 4; settings::tab = 2;
            auto assets = settings::menu_assets::Cached(argv[1], 4);
            Require(bool(assets), "installed SCH assets");
            std::filesystem::create_directories(argv[2]);
            auto labelPainted = [&](settings::MenuSnapshot preview, int label,
                                    const std::vector<uint32_t>& rendered) {
                preview.rows[size_t(label)].name.clear();
                std::vector<uint32_t> withoutLabel;
                Require(settings::RasterizeMenu(preview, 1280, 720, withoutLabel), "blank-label comparison raster");
                size_t changedPixels = 0;
                for (int y = 150; y < 640; ++y)
                    for (int x = 65; x < 365; ++x)
                        changedPixels += rendered[size_t(y) * 1280 + x] != withoutLabel[size_t(y) * 1280 + x];
                return changedPixels > 20;
            };
            for (GraphicsRow selected : {GraphicsRow::AntiAliasing, GraphicsRow::FrameRate})
            {
                settings::row = int(selected); settings::Publish(base, ConfigData);
                auto preview = settings::snapshot; preview.assets = assets;
                std::vector<uint32_t> pixels;
                Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "translated Graphics preview");
                Require(labelPainted(preview, int(selected), pixels), "selected Graphics label must paint visible pixels");
                WriteBmp(std::filesystem::path(argv[2]) / (selected == GraphicsRow::AntiAliasing ? "graphics-aa.bmp" : "graphics-rate.bmp"), pixels);
            }
            settings::tab = 2;
            settings::row = int(GraphicsRow::FrameGeneration);
            settings::edit.graphicsBackend = settings::GraphicsBackend::D3D12;
            settings::edit.frameGenerationProvider = framegen::Provider::Dlss;
            settings::edit.frameGenerationMultiplier = 3;
            gpu::video::menuFlowFgStatus.phase = gpu::video::FrameGenerationPhase::Ready;
            gpu::video::menuFlowFgStatus.applied = framegen::Provider::Dlss;
            for (uint32_t language : {4u, 0u})
            {
                settings::edit.uiLanguage = language;
                settings::Publish(base, ConfigData);
                auto preview = settings::snapshot;
                preview.assets = settings::menu_assets::Cached(argv[1], language);
                Require(bool(preview.assets), "installed FG preview assets");
                std::vector<uint32_t> pixels;
                Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "translated FG preview");
                Require(labelPainted(preview, int(GraphicsRow::FrameGeneration), pixels),
                        "FG provider label must paint visible pixels");
                WriteBmp(std::filesystem::path(argv[2]) /
                    (language == 4 ? "fg-zh-dlss.bmp" : "fg-en-dlss.bmp"), pixels);
            }
            settings::edit.frameGenerationProvider = framegen::Provider::Fsr;
            settings::edit.frameGenerationMultiplier = 2;
            gpu::video::menuFlowFgStatus.applied = framegen::Provider::Fsr;
            settings::Publish(base, ConfigData);
            auto fsrPreview = settings::snapshot;
            fsrPreview.assets = settings::menu_assets::Cached(argv[1], 0);
            Require(fsrPreview.rows[int(GraphicsRow::FrameGenerationMultiplier)].hidden,
                    "actual English FSR preview hides the multiplier");
            std::vector<uint32_t> fsrPixels;
            Require(settings::RasterizeMenu(fsrPreview, 1280, 720, fsrPixels),
                    "actual English Graphics FSR preview");
            WriteBmp(std::filesystem::path(argv[2]) / "fg-en-fsr.bmp", fsrPixels);
            gpu::video::menuFlowFgStatus = {};
            std::puts("PASS Graphics and bilingual DLSS/English FSR section previews from actual Publish/Translate with installed assets");
        }
        CheckBr03DlssMenu(base);
        {
            const auto previousEdit = settings::edit;
            settings::tab = 2;
            settings::status.clear();
            settings::edit = currentConfig;
            settings::edit.graphicsBackend = settings::GraphicsBackend::D3D12;
            settings::edit.antialiasing = 0;
            settings::edit.upscaler = gpu::upscaling::Upscaler::Off;
            settings::edit.frameGenerationProvider = framegen::Provider::Off;
            settings::row = int(GraphicsRow::Hdr);
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.rows[int(GraphicsRow::Hdr)].enabled &&
                    settings::snapshot.rows[int(GraphicsRow::HdrPaperWhite)].enabled &&
                    settings::snapshot.rows[int(GraphicsRow::HdrPeak)].enabled,
                    "D3D12 exposes HDR controls");
            settings::pending = 8; Tick(base);
            Require(settings::edit.hdr && settings::snapshot.rows[int(GraphicsRow::Hdr)].value == L"On",
                    "HDR toggle updates the pending preference");
            settings::row = int(GraphicsRow::HdrPaperWhite);
            settings::edit.hdrPaperWhiteNits = 400;
            settings::pending = 8; Tick(base);
            Require(settings::edit.hdrPaperWhiteNits == 400, "paper white respects 400-nit bound");
            settings::pending = 4; Tick(base);
            Require(settings::edit.hdrPaperWhiteNits == 390, "paper white steps down by ten");
            settings::row = int(GraphicsRow::HdrPeak);
            settings::edit.hdrPeakAutomatic = false;
            settings::edit.hdrPeakNits = 10000;
            settings::pending = 8; Tick(base);
            Require(settings::edit.hdrPeakNits == 10000, "peak brightness respects 10000-nit bound");
            settings::SetHdrDisplayInfo({true, 1031, false});
            settings::edit.hdrPeakAutomatic = true;
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.calibration.effectiveNits == 1031 &&
                    settings::snapshot.rows[int(GraphicsRow::HdrPeak)].value.find(L"1031") != std::wstring::npos,
                    "automatic peak follows the active display report");
            settings::pending = 0x1000; Tick(base);
            Require(settings::snapshot.calibration.open && settings::GetHdrCalibration().effectiveNits == 1031,
                    "peak row opens the calibration screen with a live display value");
            Require(settings::GetHdrCalibration().scenePreview && !settings::GetHdrCalibration().sceneAvailable,
                    "calibration requests a scene by default but never claims an uncaptured frame");
            const auto beforeScene = settings::snapshot.revision;
            settings::SetHdrCalibrationSceneAvailable(true);
            Require(settings::GetHdrCalibration().sceneAvailable && settings::snapshot.revision > beforeScene,
                    "a captured scene refreshes the paused menu without a guest tick");
            settings::pending = 0x200; Tick(base);
            Require(!settings::GetHdrCalibration().scenePreview, "RB switches to the precise test pattern");
            settings::PointerClick(850, 55, false);
            settings::pending = 0; Tick(base);
            Require(settings::GetHdrCalibration().scenePreview, "Scene button selects the captured scene");
            settings::PointerClick(1000, 55, false);
            settings::pending = 0; Tick(base);
            Require(!settings::GetHdrCalibration().scenePreview, "Test pattern button selects the pattern");
            settings::PointerDrag(900, 560, true);
            settings::pending = 0; Tick(base);
            Require(!settings::edit.hdrPeakAutomatic && settings::edit.hdrPeakNits >= 2000,
                    "pointer slider selects a manual peak");
            for (char digit : std::string("2500")) Require(settings::CalibrationKey(digit), "calibration digit accepted");
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.calibration.numericEditing, "numeric entry is visible during editing");
            Require(settings::CalibrationKey(13), "numeric enter accepted");
            settings::pending = 0; Tick(base);
            Require(settings::edit.hdrPeakNits == 2500 && !settings::snapshot.calibration.numericEditing,
                    "typed peak is committed to the pending Graphics settings");
            settings::PointerClick(270, 620, false);
            settings::pending = 0; Tick(base);
            Require(settings::edit.hdrPeakAutomatic && settings::snapshot.calibration.effectiveNits == 1031,
                    "Auto button restores the detected peak");
            Require(settings::CalibrationKey(27), "calibration Escape accepted");
            settings::pending = 0; Tick(base);
            Require(!settings::snapshot.calibration.open && settings::edit.hdrPeakAutomatic &&
                    settings::edit.hdrPeakNits == 10000, "Cancel restores the pre-calibration pending values");
            settings::pending = 0x1000; Tick(base);
            Require(settings::snapshot.calibration.open && settings::GetHdrCalibration().scenePreview,
                    "reopening calibration defaults to the game scene again");
            settings::SetHdrCalibrationSceneAvailable(false);
            Require(!settings::GetHdrCalibration().sceneAvailable,
                    "retiring the scene immediately removes HDR preview availability");
            Require(settings::CalibrationKey(27), "close reopened calibration");
            settings::pending = 0; Tick(base);
            settings::edit.antialiasing = 1;
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.notice.find(L"HDR is paused") == std::wstring::npos &&
                    settings::edit.antialiasing == 1,
                    "AA runs on the HDR scene and produces no HDR notice");
            settings::edit.upscaler = gpu::upscaling::Upscaler::Fsr;
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.notice.find(L"HDR is paused") == std::wstring::npos &&
                    settings::edit.upscaler == gpu::upscaling::Upscaler::Fsr,
                    "upscaling keeps HDR through the highlight gain and produces no HDR notice");
            settings::edit.upscaler = gpu::upscaling::Upscaler::Off;
            settings::edit.frameGenerationProvider = framegen::Provider::Fsr;
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.notice.find(L"HDR is paused") != std::wstring::npos &&
                    settings::edit.frameGenerationProvider == framegen::Provider::Fsr,
                    "SDR-only frame generation produces HDR notice without changing preference");
            settings::edit.frameGenerationProvider = framegen::Provider::Off;
            settings::edit.graphicsBackend = settings::GraphicsBackend::Vulkan;
            settings::row = int(GraphicsRow::Hdr);
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.rows[int(GraphicsRow::Hdr)].enabled &&
                    settings::snapshot.rows[int(GraphicsRow::HdrPaperWhite)].enabled &&
                    settings::snapshot.rows[int(GraphicsRow::HdrPeak)].enabled,
                    "Vulkan HDR controls are available for runtime capability detection");
            settings::pending = 8; Tick(base);
            Require(!settings::edit.hdr, "Vulkan HDR preference can be changed");
            settings::SetHdrDisplayInfo({});
            settings::edit = previousEdit;
            settings::pending = 0; Tick(base);
        }
        {
            const auto originalDir = std::filesystem::current_path();
            const auto sandbox = std::filesystem::temp_directory_path() /
                ("lo-fsr-sharpness-settings-" + std::to_string(GetCurrentProcessId()));
            std::filesystem::create_directories(sandbox);
            std::filesystem::current_path(sandbox);
            auto writeIni = [](const char* contents) {
                std::ofstream output("settings.ini", std::ios::trunc);
                output << contents;
                Require(bool(output), "write isolated settings.ini");
            };
            writeIni("upscaler=2\nfsr_sharpness=75\n");
            Require(settings::Read().fsrSharpnessPercent == 75, "FSR sharpness reads from INI");
            writeIni("fsr_sharpness=101\n");
            Require(settings::Read().fsrSharpnessPercent == 100, "out-of-range INI sharpness clamps to 100");
            writeIni("fsr_sharpness=4294967295\n");
            Require(settings::Read().fsrSharpnessPercent == 100, "largest uint32 clamps to 100");
            writeIni("fsr_sharpness=-1\n");
            Require(settings::Read().fsrSharpnessPercent == 0, "negative sharpness keeps Off default");
            writeIni("fsr_sharpness=garbage\n");
            Require(settings::Read().fsrSharpnessPercent == 0, "invalid sharpness keeps Off default");
            settings::Config sharpness{};
            sharpness.upscaler = gpu::upscaling::Upscaler::Fsr;
            sharpness.fsrSharpnessPercent = 64;
            Require(settings::SaveConfig(sharpness), "real config save succeeds in sandbox");
            Require(settings::GetConfig().fsrSharpnessPercent == 64 && settings::Read().fsrSharpnessPercent == 64,
                    "real config retains sharpness after save and disk reload");
            std::ifstream persisted("settings.ini");
            const std::string text((std::istreambuf_iterator<char>(persisted)), std::istreambuf_iterator<char>());
            Require(text.find("fsr_sharpness=64\n") != std::string::npos, "INI writes fsr_sharpness key");
            persisted.close(); // Windows cannot replace settings.ini while this reader holds it.
            sharpness.fsrSharpnessPercent = 102;
            Require(settings::SaveConfig(sharpness) && settings::Read().fsrSharpnessPercent == 100,
                    "real save validates out-of-range sharpness");
            sharpness.fsrSharpnessPercent = 0;
            Require(settings::SaveConfig(sharpness) && settings::Read().fsrSharpnessPercent == 0,
                    "Off roundtrips as zero");
            writeIni("frame_generation_provider=1\nframe_generation_mode=0\nframe_generation_multiplier=6\n");
            const auto maxFg = settings::Read();
            Require(maxFg.frameGenerationProvider == framegen::Provider::Dlss &&
                    maxFg.frameGenerationMultiplier == 6, "DLSS maximum multiplier reads from INI");
            writeIni("frame_generation_provider=1\nframe_generation_multiplier=7\n");
            Require(settings::Read().frameGenerationMultiplier == 2, "out-of-range FG multiplier resets to 2");
            writeIni("frame_generation_provider=1\nframe_generation_multiplier=16\n");
            Require(settings::Read().frameGenerationMultiplier == 2, "former 16x maximum resets to 2");
            writeIni("frame_generation_provider=99\nframe_generation_multiplier=2\n");
            Require(settings::Read().frameGenerationProvider == framegen::Provider::Off,
                    "unknown FG provider resets to Off");
            writeIni("frame_generation_provider=2\nframe_generation_mode=1\nframe_generation_multiplier=6\nframe_generation_target_fps=144\n");
            const auto fsrFg = settings::Read();
            Require(fsrFg.frameGenerationProvider == framegen::Provider::Fsr &&
                    fsrFg.frameGenerationMode == framegen::Mode::Fixed &&
                    fsrFg.frameGenerationMultiplier == 2 && fsrFg.frameGenerationTargetFps == 0,
                    "FSR INI normalizes to fixed 2x");
            settings::Config fgConfig{};
            fgConfig.frameGenerationProvider = framegen::Provider::Dlss;
            fgConfig.frameGenerationMultiplier = 2;
            Require(settings::SaveConfig(fgConfig) && settings::Read().frameGenerationMultiplier == 2,
                    "minimum DLSS multiplier survives real save/reload");
            fgConfig.frameGenerationMultiplier = 6;
            Require(settings::SaveConfig(fgConfig), "maximum DLSS multiplier saves");
            const auto savedFg = settings::Read();
            Require(savedFg.frameGenerationProvider == framegen::Provider::Dlss &&
                    savedFg.frameGenerationMultiplier == 6, "maximum DLSS multiplier survives real save/reload");
            {
                std::ifstream fgIni("settings.ini");
                const std::string fgText((std::istreambuf_iterator<char>(fgIni)), std::istreambuf_iterator<char>());
                Require(fgText.find("frame_generation_provider=1\n") != std::string::npos &&
                        fgText.find("frame_generation_multiplier=6\n") != std::string::npos,
                        "real INI writes FG provider and multiplier");
            }
            writeIni("width=2560\nheight=1080\n");
            Require(settings::Read().aspectRatio == gpu::aspect_ratio::Mode::Ultrawide,
                    "a 21:9 resolution saved before the Aspect ratio setting reads as 21:9");
            writeIni("width=1920\nheight=1080\n");
            Require(settings::Read().aspectRatio == gpu::aspect_ratio::Mode::Auto, "other saved resolutions read as Auto");
            writeIni("width=2560\nheight=1080\naspect_ratio=0\n");
            Require(settings::Read().aspectRatio == gpu::aspect_ratio::Mode::Auto, "a saved Auto stays Auto at 21:9");
            writeIni("aspect_ratio=9\n");
            Require(settings::Read().aspectRatio == gpu::aspect_ratio::Mode::Auto, "an unknown aspect ratio reads as Auto");
            {
                settings::Config aspect{};
                aspect.aspectRatio = gpu::aspect_ratio::Mode::Standard;
                Require(settings::SaveConfig(aspect) && settings::Read().aspectRatio == gpu::aspect_ratio::Mode::Standard,
                        "4:3 survives a real save/reload");
                std::ifstream aspectIni("settings.ini");
                const std::string aspectText((std::istreambuf_iterator<char>(aspectIni)), std::istreambuf_iterator<char>());
                Require(aspectText.find("aspect_ratio=3\n") != std::string::npos, "INI writes aspect_ratio");
            }
            writeIni("width=1600\ndisplay_name=M27P20\ndisplay_index=2\ngpu_device=GPU B\nwindow_mode=2\n");
            {
                const auto names = settings::Read();
                Require(names.displayName == "M27P20" && names.displayIndex == 2 && names.gpuDevice == "GPU B" &&
                        names.windowMode == settings::WindowMode::Borderless,
                        "display/GPU names read from INI; saved exclusive fullscreen loads as Fullscreen");
            }
            Require(settings::SaveDisplayChoice("M27P20", 1), "display choice of a player move saves");
            Require(settings::Read().displayIndex == 1 && settings::Read().displayName == "M27P20" &&
                    settings::Read().width == 1600 && settings::Read().gpuDevice == "GPU B" &&
                    settings::GetConfig().displayIndex == 1, "a player move saves only the display choice");
            std::puts("PASS FG settings.ini provider and multiplier validation, FSR normalization, and min/max save/reload");
            std::filesystem::current_path(originalDir);
            std::filesystem::remove_all(sandbox);
            std::puts("PASS FSR sharpness settings.ini read, validation and save/reload roundtrip");
        }
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
