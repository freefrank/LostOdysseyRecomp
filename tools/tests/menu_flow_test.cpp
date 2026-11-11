// Exercise the production hook, substituting only its guest-call boundaries.
// No game, renderer device, window, save or profile is started by this fixture.
#include <stdafx.h>
#include <gpu/video.h>
#include <settings/config.h>
#include <os/steam_shortcut.h>
#define SDL_MAIN_HANDLED // This host fixture supplies its own main and does not initialize SDL.
#include <SDL3/SDL.h>
#include <fstream>
#include <map>
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
namespace discord_rpc {
unsigned menuFlowChanges = 0;
bool menuFlowEnabled = true;
void SetEnabled(bool enabled) { ++menuFlowChanges; menuFlowEnabled = enabled; }
}
// The Steam action records a result; menu fixtures never access a Steam install.
namespace steam_shortcut {
unsigned menuFlowAdds = 0;
Status menuFlowStatus = Status::Added;
Result AddToSteam(const std::filesystem::path &) { ++menuFlowAdds; return {menuFlowStatus, {}}; }
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
// The Mods page's Open mods folder records the request instead of starting Explorer.
#include <settings/mods_page.h>
namespace settings::mods_page { bool MenuFlowOpenFolder(const std::filesystem::path& folder); }
#define OpenFolder MenuFlowOpenFolder
// The Achievements page reads a fake list instead of the title data and profile.
#include <settings/achievements_page.h>
namespace settings::achievements_page { std::vector<Item> MenuFlowAchievementItems(bool withIcons); }
#define LoadAchievementItems MenuFlowAchievementItems
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
#undef OpenFolder
#undef LoadAchievementItems
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

// System → Shader bundle records the request instead of writing the marker file.
namespace updater::shader_pack {
int menuFlowRedownloads = 0;
bool RequestRedownload() { ++menuFlowRedownloads; return true; }
}

namespace gpu::video {
FrameGenerationStatus menuFlowFgStatus{};
FrameGenerationStatus GetFrameGenerationStatus() { return menuFlowFgStatus; }
std::vector<std::string> GpuDeviceNames() { return {}; }
std::string menuFlowActiveGpu;
std::string ActiveGpuDeviceName() { return menuFlowActiveGpu; }
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
// PlannerObservation::eligible: the last execution of the request that had a scene.
std::optional<UpscalerExecutionObservation> menuFlowEligible;
DlssEffectSnapshot CurrentDlssEffect() { return menuFlowDlssEffect; }
std::optional<UpscalerExecutionObservation> CurrentUpscalerExecution()
{
    PlannerObservation observed;
    observed.execution = menuFlowDlssEffect.execution;
    observed.eligible = menuFlowEligible;
    return StatusExecution(observed);
}
}
namespace {
uint32_t vibrationStrength = 100;
unsigned vibrationPreviews = 0;
uint32_t promptStyle = 0;
unsigned modsFolderOpens = 0;
std::filesystem::path modsFolderOpened;
}
std::vector<settings::achievements_page::Item> settings::achievements_page::MenuFlowAchievementItems(bool withIcons)
{
    const auto make = [&](uint16_t id, uint16_t score, const char* name, const char* unlocked, const char* locked, bool secret,
                          int64_t time) {
        Item item;
        item.info.id = id;
        item.info.score = score;
        item.info.name = name;
        item.info.unlockedText = unlocked;
        item.info.lockedText = locked;
        item.info.secret = secret;
        item.earnedTime = time;
        // RGBA pixel 0xff332211: R 0x11, G 0x22, B 0x33.
        if (withIcons) item.icon = {std::vector<uint32_t>(4, 0xff332211u), 2, 2};
        return item;
    };
    return {make(1, 10, "Oldest", "Did it", "Do it", false, 1700000000),
            make(2, 30, "Locked one", "Done", "Do it", false, 0),
            make(3, 20, "Newest", "Done it", "Do it", false, 2000000000),
            make(4, 20, "Secret name", "Secret done", "Secret hint", true, 0),
            make(5, 10, "Hidden too", "Hidden done", "Hidden hint", true, 0)};
}
bool settings::mods_page::MenuFlowOpenFolder(const std::filesystem::path& folder)
{
    ++modsFolderOpens;
    modsFolderOpened = folder;
    return true;
}
// The Mods page's live reload (patches/mod_hot_reload.h): a request re-reads the
// mod list as the runtime does; the page shows whatever status the test sets.
namespace {
unsigned modsReloadRequests = 0;
bool modsReloadAvailable = true;
mod_hot_reload::Status modsReloadStatus;
}
bool mod_hot_reload::Request()
{
    ++modsReloadRequests;
    if (!modsReloadAvailable) return false;
    modding::Reload();
    modsReloadStatus = {Phase::Preparing};
    return true;
}
mod_hot_reload::Status mod_hot_reload::GetStatus() { return modsReloadStatus; }
namespace {
// Controls page: the sources held on the controllers, the layouts handed to hid.
uint32_t heldSources = 0;
hid::input_map::ControllerMap appliedController = hid::input_map::DefaultController;
hid::input_map::KeyboardMap appliedKeyboard = hid::input_map::DefaultKeyboard;
unsigned inputMapApplies = 0;
}
namespace hid {
bool UsesPlayStationPrompts() { return false; }
uint8_t PromptFamily() { return 0; }
namespace glyphs { std::shared_ptr<const Atlas> FamilyAtlas(prompts::Family) { return {}; } }
void SetVibrationStrength(uint32_t percent) { vibrationStrength = percent; }
void SetPromptStyle(uint32_t style) { promptStyle = style; }
void PreviewVibration() { ++vibrationPreviews; }
void SetInputMaps(const input_map::ControllerMap& controller, const input_map::KeyboardMap& keyboard)
{
    appliedController = controller;
    appliedKeyboard = keyboard;
    ++inputMapApplies;
}
uint32_t PressedSources() { return heldSources; }
// Without SDL: SDL's names for the default keys and Space, numbers otherwise.
std::string KeyName(uint32_t scancode)
{
    if (scancode >= 4 && scancode <= 29) return std::string(1, char('A' + scancode - 4));
    switch (scancode)
    {
    case 40: return "Return";
    case 42: return "Backspace";
    case 44: return "Space";
    case 79: return "Right";
    case 80: return "Left";
    case 81: return "Down";
    case 82: return "Up";
    }
    return "#" + std::to_string(scancode);
}
}
namespace apu {
Output menuFlowOutput = Output::Stereo;
uint32_t menuFlowMatrixRear = 110;
uint32_t menuFlowVoiceVolume = 100;
bool menuFlowTestSignal = false;
void SetOutput(Output output) { menuFlowOutput = output; }
void SetMatrixRearAngle(uint32_t degrees) { menuFlowMatrixRear = degrees; }
void SetVoiceVolume(uint32_t percent) { menuFlowVoiceVolume = percent; }
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
unsigned quitEventAttempts = 0, mainMenuRequests = 0, defaultsCalls = 0;
bool titleQuitAccepted = true;
gpu::video::DisplayChangeTracker displayChanges;
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void Poll(uint16_t buttons, bool consumed, int16_t x = 0, int16_t y = 0)
{
    const auto original = buttons;
    Require(settings::FilterInput(buttons, x, y, buttons) == consumed, "input ownership");
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

bool settings::quit_action::PushQuit()
{
    ++quitEventAttempts;
    return titleQuitAccepted;
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
extern "C" PPC_FUNC(MenuFlowDefaults) { (void)ctx; (void)base; ++defaultsCalls; }

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
    {
        const auto& shadows = settings::snapshot.rows[int(GraphicsRow::DynamicShadows)];
        Require(shadows.choices == std::vector<std::wstring>{L"On", L"Low", L"Off"} && shadows.selectedChoice == 0,
                "dynamic shadows default to On");
        settings::row = int(GraphicsRow::DynamicShadows);
        settings::pending = 8; Tick(base);
        Require(settings::edit.dynamicShadows == 2 &&
                settings::snapshot.rows[int(GraphicsRow::DynamicShadows)].value == L"Low" &&
                settings::snapshot.rows[int(GraphicsRow::Preset)].value == L"Custom", "dynamic shadows select Low");
        settings::pending = 8; Tick(base);
        Require(settings::edit.dynamicShadows == 0, "dynamic shadows select Off");
        settings::pending = 8; Tick(base);
        Require(settings::edit.dynamicShadows == 1, "dynamic shadows wrap to On");
    }
    settings::row = int(GraphicsRow::AmbientOcclusion);
    settings::pending = 8; Tick(base);
    Require(settings::edit.ambientOcclusion == 1 &&
            settings::snapshot.rows[int(GraphicsRow::AmbientOcclusion)].value == L"SSAO",
            "ambient occlusion selects SSAO");
    settings::pending = 8; Tick(base);
    Require(settings::edit.ambientOcclusion == 2, "ambient occlusion selects GTAO");
    settings::pending = 8; Tick(base);
    Require(settings::edit.ambientOcclusion == 0, "ambient occlusion wraps to Off");
    // Graphics preset: derived from the rows it sets, Custom when they match neither.
    {
        const settings::Config before = settings::edit;
        const auto& preset = settings::snapshot.rows[int(GraphicsRow::Preset)];
        Require(preset.choices == std::vector<std::wstring>{L"Original", L"Low-end", L"Custom"} &&
                preset.selectedChoice == 0, "the factory effects show the Original preset");
        settings::row = int(GraphicsRow::Preset);
        settings::pending = 8; Tick(base);
        Require(!settings::edit.dynamicShadows && !settings::edit.motionBlur && settings::edit.depthOfFieldPercent == 0 &&
                !settings::edit.bloom && settings::edit.scalingQuality == 0 &&
                settings::edit.internalResolution == 720 && settings::edit.frameRate == 30 &&
                settings::snapshot.rows[int(GraphicsRow::Preset)].value == L"Low-end", "Right selects the Low-end preset");
        settings::Config others = settings::edit;
        others.dynamicShadows = before.dynamicShadows; others.motionBlur = before.motionBlur;
        others.depthOfFieldPercent = before.depthOfFieldPercent; others.bloom = before.bloom;
        others.scalingQuality = before.scalingQuality;
        others.internalResolution = before.internalResolution; others.frameRate = before.frameRate;
        Require(others == before, "the preset changes only its own rows");
        // Render resolution and frame rate are preset rows too.
        for (const auto [presetRow, label] : {std::pair{GraphicsRow::RenderResolution, "render resolution"},
                                              std::pair{GraphicsRow::FrameRate, "frame rate"}})
        {
            settings::row = int(presetRow);
            settings::pending = 8; Tick(base);
            Require(settings::snapshot.rows[int(GraphicsRow::Preset)].value == L"Custom",
                    (std::string("a changed ") + label + " shows Custom").c_str());
            settings::pending = 4; Tick(base);
            Require(settings::snapshot.rows[int(GraphicsRow::Preset)].value == L"Low-end",
                    (std::string("the ") + label + " back at the preset value shows Low-end").c_str());
        }
        settings::row = int(GraphicsRow::Bloom);
        settings::pending = 8; Tick(base);
        Require(settings::edit.bloom && settings::snapshot.rows[int(GraphicsRow::Preset)].value == L"Custom" &&
                settings::snapshot.rows[int(GraphicsRow::Preset)].selectedChoice == 2, "one changed row shows Custom");
        settings::row = int(GraphicsRow::Preset);
        settings::pending = 4; Tick(base);
        Require(settings::graphics_menu::PresetOf(settings::edit) == settings::graphics_menu::Preset::LowEnd,
                "Left from Custom selects Low-end");
        settings::pending = 8; Tick(base);
        Require(settings::edit.dynamicShadows && settings::edit.motionBlur && settings::edit.depthOfFieldPercent == 100 &&
                settings::edit.bloom && settings::edit.scalingQuality == 1 &&
                settings::edit.internalResolution == 720 && settings::edit.frameRate == 30 &&
                settings::snapshot.rows[int(GraphicsRow::Preset)].value == L"Original", "Right back to Original");
        settings::edit = before;
    }
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
    settings::pending = 2; Tick(base);
    Require(settings::row == int(GraphicsRow::DlssModel), "DLSS model directly follows DLSS quality");
    {
        const auto& model = settings::snapshot.rows[int(GraphicsRow::DlssModel)];
        Require(!model.hidden && model.enabled && model.choices == std::vector<std::wstring>{L"M", L"L"} &&
                model.selectedChoice == 0 && model.value == L"M", "DLSS model offers M (default) and L with DLSS");
    }
    Require(settings::snapshot.help == L"M: steadier and sharper than older models, moderate cost. L: steadiest, least ghosting, but slower, mainly on RTX 20 and 30.",
            "DLSS model help explains the cost of each model");
    settings::pending = 8; Tick(base);
    Require(settings::edit.dlssModel == 1 && settings::snapshot.rows[int(GraphicsRow::DlssModel)].value == L"L",
            "DLSS model right selects L");
    settings::pending = 8; Tick(base);
    Require(settings::edit.dlssModel == 0, "DLSS model right wraps to M");
    settings::pending = 4; Tick(base);
    Require(settings::edit.dlssModel == 1, "DLSS model left wraps to L");
    settings::edit.dlssModel = 0;
    settings::pending = 2; Tick(base);
    Require(settings::row == int(GraphicsRow::DlssNeuralRendering),
            "down from the DLSS model skips the hidden FSR sharpness to DLSS 5 neural rendering");
    settings::row = int(GraphicsRow::AntiAliasing);
    settings::pending = 0;
    Tick(base);
    settings::pending = 8;
    Tick(base);
    Require(settings::edit.upscaler == Upscaler::Fsr && !settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden,
            "FSR can be selected independently on D3D12");
    Require(settings::snapshot.rows[int(GraphicsRow::DlssModel)].hidden, "FSR hides the DLSS model");
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
    Require(settings::edit.upscaler == Upscaler::Off && settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden &&
            settings::snapshot.rows[int(GraphicsRow::DlssModel)].hidden, "DLSS can be turned off on D3D12");
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
    Require(settings::snapshot.notice == L"DLSS is querying the render resolution.",
            "size pending comes from the classified reason, not sizingState");
    saveState("03-size-pending.bmp");

    running.device.deviceReady = true;
    running.device.dlssAvailable = true;
    running.reason = DlssEffectReason::DeviceNotReady;
    running.sizingState = SizingState::Ready;
    gpu::frame_plan::menuFlowDlssEffect = running;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS is waiting for the graphics device.",
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
    Require(settings::snapshot.notice == L"Submitted DLSS Quality output. The new quality is not applied yet.",
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
    Require(settings::snapshot.notice == needsVulkan + L" Still on Direct3D 12 until restart.",
            "pending backend is added after the current plan and does not replace it");
    Require(saves == savesAtCycle, "selecting Vulkan does not save by itself");
    saveState("05-backend-pending.bmp");
    settings::pending = 8;
    Tick(base);
    Require(settings::edit.graphicsBackend == settings::GraphicsBackend::D3D11, "Direct3D 11 remains selectable");
    Require(settings::snapshot.notice == needsVulkan + L" Still on Direct3D 12 until restart.",
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
    Require(settings::snapshot.notice == L"已提交 DLSS 质量输出。1280×720 - 1920×1080", "simplified status");
    saveState("06-active-simplified.bmp");
    settings::edit.uiLanguage = 1;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"已提交 DLSS 品質輸出。1280×720 - 1920×1080", "traditional status");
    settings::edit.uiLanguage = 2;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS 品質出力を提出。1280×720 - 1920×1080", "japanese status");
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
    Require(settings::snapshot.notice == L"DLSS is querying the render resolution.",
            "an open graphics page reads a new result without reopening");
    Require(settings::snapshot.notice.find(L"1280") == std::wstring::npos, "a querying result does not keep the submitted size");
    settings::edit.upscaler = Upscaler::Off;
    settings::pending = 0;
    Tick(base);
    Require(settings::snapshot.notice == L"DLSS is querying the render resolution. The Off choice is not applied yet.",
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
    Require(settings::snapshot.notice == activePlan + L" The new quality is not applied yet.",
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
    {
        // Menus, pause and the title screen draw no scene, so the newest frame
        // reports NoEligibleScene. The row describes the request's submitted
        // frame, or its last scene fallback when none (PlannerObservation::eligible).
        const auto previousExecution = running.execution;
        auto menuFrame = SubmittedObservation(DlssQuality::Quality, 1707, 960, 2560, 1440);
        menuFrame.outcome = gpu::frame_plan::DlssExecutionOutcome::Fallback;
        menuFrame.submissionSerial = 0;
        menuFrame.reason = DlssEffectReason::NoEligibleScene;
        running.execution = menuFrame;
        gpu::frame_plan::menuFlowEligible.reset();
        show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::NoEligibleScene);
        Require(settings::snapshot.notice == L"Upscaling starts once the game scene is on screen.",
                "title screen or fresh boot: no frame with a scene yet");
        Require(settings::snapshot.notice.find(L"eligible") == std::wstring::npos, "the menu frame itself is not described");
        saveState("14-title-not-started.bmp");
        gpu::frame_plan::menuFlowEligible = SubmittedObservation(DlssQuality::Quality, 1707, 960, 2560, 1440);
        show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::NoEligibleScene);
        Require(settings::snapshot.notice == L"Submitted DLSS Quality output. 1707×960 - 2560×1440",
                "menu opened after gameplay shows the last game frame");
        saveState("14-menu-after-gameplay.bmp");
        settings::edit.dlssQuality = DlssQuality::Performance;
        show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::NoEligibleScene);
        Require(settings::snapshot.notice ==
                    L"Submitted DLSS Quality output. 1707×960 - 2560×1440 The new quality is not applied yet.",
                "an unsaved quality edit keeps its note after the last game frame");
        settings::edit.dlssQuality = DlssQuality::Quality;
        auto stalled = menuFrame;
        stalled.reason = DlssEffectReason::UnknownColorEncoding;
        gpu::frame_plan::menuFlowEligible = stalled;
        show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::NoEligibleScene);
        Require(settings::snapshot.notice == L"Color conditions do not support DLSS.",
                "a request that never submitted shows its last scene fallback");
        saveState("14-menu-after-fallback.bmp");
        stalled.reason = DlssEffectReason::UnsupportedProjection;
        gpu::frame_plan::menuFlowEligible = stalled;
        show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::NoEligibleScene);
        Require(settings::snapshot.notice == L"DLSS is not running for now.",
                "an unnamed scene fallback still says DLSS is not running");
        auto otherRequest = SubmittedObservation(DlssQuality::Quality, 1707, 960, 2560, 1440);
        otherRequest.plan.requestSignature = 77;
        gpu::frame_plan::menuFlowEligible = otherRequest;
        show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::NoEligibleScene);
        Require(settings::snapshot.notice == L"Upscaling starts once the game scene is on screen.",
                "a frame of another request is not described");
        gpu::frame_plan::menuFlowEligible = SubmittedObservation(DlssQuality::Quality, 1707, 960, 2560, 1440);
        running.failure = gpu::frame_plan::FailureReason::InvalidInput;
        show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::RequestFailure);
        Require(settings::snapshot.notice == L"DLSS request failed; no automatic retry.",
                "a request failure wins over the last game frame");
        running.failure.reset();
        show(DlssEffectPhase::TemporaryFallback, DlssEffectReason::CapabilityUnavailable);
        Require(settings::snapshot.notice == L"DLSS is not available on this device.",
                "missing DLSS wins over the last game frame");
        gpu::frame_plan::menuFlowEligible.reset();
        running.execution = previousExecution;
    }
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
    Require(settings::snapshot.notice == L"DLSS request failed; no automatic retry.",
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
                L"Submitted DLSS Balanced output. 1500×844 - 2560×1440 The new quality is not applied yet.",
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

    // FSR, XeSS and MetalFX read the same last frame with a scene.
    for (const auto provider : {Upscaler::Fsr, Upscaler::Xess, Upscaler::MetalFx})
    {
        const std::wstring name = provider == Upscaler::Fsr ? L"FSR" : provider == Upscaler::Xess ? L"XeSS" : L"MetalFX";
        currentConfig.upscaler = provider;
        settings::edit.upscaler = provider;
        settings::edit.fsrQuality = gpu::upscaling::FsrQuality::Quality;
        running = {};
        running.hasPlan = true;
        running.plannedRequest = provider;
        running.device.backend = Backend::Vulkan;
        running.device.deviceReady = true;
        running.device.fsrAvailable = running.device.xessAvailable = running.device.metalFxAvailable = true;
        running.phase = DlssEffectPhase::Inactive;
        auto scene = SubmittedObservation(DlssQuality::Quality, 1280, 720, 1920, 1080);
        scene.plan.requestedUpscaler = provider;
        scene.plan.consumer = gpu::upscaling::FsrQualityConsumer(provider);
        scene.plan.fsrQuality = gpu::upscaling::FsrQuality::Quality;
        scene.actualProvider = provider;
        auto menuFrame = scene;
        menuFrame.outcome = gpu::frame_plan::DlssExecutionOutcome::Fallback;
        menuFrame.submissionSerial = 0;
        menuFrame.reason = DlssEffectReason::NoEligibleScene;
        running.execution = menuFrame;
        gpu::frame_plan::menuFlowDlssEffect = running;
        gpu::frame_plan::menuFlowEligible.reset();
        settings::pending = 0;
        Tick(base);
        Require(settings::snapshot.notice == L"Upscaling starts once the game scene is on screen.",
                "FSR, XeSS and MetalFX before a frame with a scene");
        gpu::frame_plan::menuFlowEligible = scene;
        settings::pending = 0;
        Tick(base);
        Require(settings::snapshot.notice == name + L" Quality output submitted. 1280×720 - 1920×1080",
                "FSR, XeSS and MetalFX after gameplay show the last game frame");
        auto pending = menuFrame;
        pending.reason = DlssEffectReason::MotionPipelinePending;
        gpu::frame_plan::menuFlowEligible = pending;
        settings::pending = 0;
        Tick(base);
        Require(settings::snapshot.notice == name + L": motion data is still being prepared.",
                "FSR, XeSS and MetalFX show a fallback in the last game frame");
    }
    gpu::frame_plan::menuFlowEligible.reset();
    gpu::frame_plan::menuFlowDlssEffect = {};
    currentConfig.upscaler = Upscaler::Dlss;
    settings::edit.upscaler = Upscaler::Dlss;
    notes.close();
    Require(bool(notes), "write BR-03 notice list");
    std::puts("PASS BR-03 menu status from stubbed snapshots: submitted output, waiting, probe, fallback reasons, GPU stopped, unsaved edits");
}

// Help lines are single lines: one that is too long for its box is drawn
// smaller until it is unreadable. Every help line a settings page can show, in
// every menu language and with the longest status it can carry, must keep at
// least this share of its normal size. With an installation the game's own
// faces are measured, otherwise the built-in font.
constexpr double kMinHelpFit = 0.85;
struct HelpLine
{
    double scale = 1;
    std::string where;
    const char* face = "";
};
struct HelpAudit
{
    std::array<std::shared_ptr<const settings::menu_assets::Assets>, 5> assets{};
    // The smallest scale of each distinct text, per language.
    std::map<std::pair<uint32_t, std::wstring>, HelpLine> lines;
    // A Japanese, Korean or Simplified Chinese line identical to the English
    // one of the same state: its translation key no longer matches.
    std::vector<std::string> untranslated;
    // English lines whose article "A" PlayStation prompts would draw as a glyph.
    std::vector<std::string> articleGlyphs;
    void Record(const settings::TextFit& fit, uint32_t language, const std::string& where)
    {
        if (fit.text.empty()) return;
        // With PlayStation prompts, a glyph followed by a lowercase word other than
        // "to" is an article "A" (A key, A test sound) turned into the cross
        // button. Real prompts read "A:", "A /", "Press A to".
        if (language == 0)
        {
            const std::wstring glyphs = settings::PromptGlyphs(fit.text, true);
            for (size_t i = 0; i + 2 < glyphs.size(); ++i)
                if (glyphs[i] >= 0xe100 && glyphs[i] <= 0xe103 && glyphs[i + 1] == L' ' && glyphs[i + 2] >= L'a' &&
                    glyphs[i + 2] <= L'z' && glyphs.compare(i + 2, 3, L"to ") != 0)
                {
                    articleGlyphs.push_back(where + ": " + Utf8(fit.text));
                    break;
                }
        }
        auto [it, inserted] = lines.try_emplace({language, fit.text}, HelpLine{fit.scale, where, fit.face});
        if (!inserted && fit.scale < it->second.scale) it->second = {fit.scale, where, fit.face};
    }
    // Both prompt styles: PlayStation faces move a line with A/B/X/Y to the built-in font.
    void Measure(settings::MenuSnapshot shown, const std::string& where)
    {
        shown.assets = assets[shown.language];
        for (const bool playStation : {false, true})
        {
            shown.playStationPrompts = playStation;
            const auto fit = settings::MeasureHelp(shown);
            const std::string at = where + (playStation ? " (PlayStation prompts)" : "");
            Record(fit.help, shown.language, at);
            Record(fit.notice, shown.language, at);
        }
    }
    // A help box under the list with this help and notice.
    void MeasureBox(uint32_t language, std::wstring help, std::wstring notice, const std::string& where)
    {
        settings::MenuSnapshot shown;
        shown.language = language;
        shown.help = std::move(help);
        shown.notice = std::move(notice);
        Measure(std::move(shown), where);
    }
    // Also a line with an English sentence in it: four or more plain lowercase
    // words, which no Japanese, Korean or Chinese line has.
    void CompareWithEnglish(const std::wstring& english, const std::wstring& text, uint32_t language, const std::string& where)
    {
        if (language < 2 || text.empty()) return;
        int words = 0;
        for (size_t start = 0; start < text.size();)
        {
            size_t end = text.find_first_of(L" \n", start);
            if (end == std::wstring::npos) end = text.size();
            std::wstring word = text.substr(start, end - start);
            while (!word.empty() && std::wcschr(L",.;:()", word.back())) word.pop_back();
            words += word.size() >= 2 && std::all_of(word.begin(), word.end(), [](wchar_t c) { return c >= L'a' && c <= L'z'; });
            start = end + 1;
        }
        if (text == english || words >= 4)
            untranslated.push_back(std::to_string(language) + " " + where + ": " + Utf8(text));
    }
};

// LO_HELP_CAPTURE_DIR: every settings row with the installed faces in English
// and Simplified Chinese, for before / after pictures of help text changes.
// Graphics rows show DLSS with a submitted frame; the NR row also with each
// runtime state.
void CaptureHelpRows(uint8_t* base, const char* installRoot)
{
    const char* folder = std::getenv("LO_HELP_CAPTURE_DIR");
    if (!folder || !installRoot) return;
    using gpu::backend::Backend;
    using NrState = gpu::dlss::NeuralRenderingState;
    std::filesystem::create_directories(folder);
    const auto savedEdit = settings::edit;
    const auto savedConfig = currentConfig;
    const int savedTab = settings::tab, savedRow = settings::row;
    settings::status.clear();
    settings::restartPrompt = false;
    gpu::video::menuFlowActiveGpu = "NVIDIA GeForce RTX 5080";
    currentConfig.graphicsBackend = Backend::D3D12;
    currentConfig.upscaler = gpu::upscaling::Upscaler::Dlss;
    currentConfig.dlssQuality = gpu::upscaling::DlssQuality::Quality;
    currentConfig.frameGenerationProvider = framegen::Provider::Off;
    currentConfig.hdr = false;
    gpu::frame_plan::DlssEffectSnapshot running{};
    running.hasPlan = true;
    running.plannedRequest = gpu::upscaling::Upscaler::Dlss;
    running.device.backend = Backend::D3D12;
    running.device.deviceReady = running.device.dlssAvailable = true;
    running.phase = gpu::frame_plan::DlssEffectPhase::Active;
    running.execution = SubmittedObservation(gpu::upscaling::DlssQuality::Quality, 1707, 960, 2560, 1440);
    gpu::frame_plan::menuFlowDlssEffect = running;
    const auto capture = [&](uint32_t language, const std::string& name) {
        settings::edit.uiLanguage = language;
        settings::Publish(base, ConfigData);
        auto preview = settings::snapshot;
        preview.assets = settings::menu_assets::Cached(installRoot, language);
        Require(bool(preview.assets), "installed capture assets");
        std::vector<uint32_t> pixels;
        Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "help capture raster");
        WriteBmp(std::filesystem::path(folder) / (name + (language ? "-zh-hans.bmp" : "-en.bmp")), pixels);
    };
    for (const uint32_t language : {0u, 4u})
    {
        for (const int tab : {0, 1, 2, 3})
        {
            const int count = tab == 0 ? settings::GameRowCount : tab == 1 ? settings::AudioRowCount :
                              tab == 2 ? int(GraphicsRow::Count) : settings::SystemRowCount;
            for (int row = 0; row < count; ++row)
            {
                settings::edit = currentConfig;
                settings::tab = tab;
                settings::row = row;
                if (tab == 1) settings::edit.audioOutput = settings::AudioOutputMatrix;
                capture(language, "tab" + std::to_string(tab) + "-row" + std::to_string(row));
                if (tab == 1 && row == settings::AudioOutputRow)
                {
                    settings::edit.audioOutput = settings::AudioOutputSurround;
                    capture(language, "tab1-row" + std::to_string(row) + "-surround-on-stereo");
                }
            }
        }
        settings::edit = currentConfig;
        settings::tab = 2;
        settings::row = int(GraphicsRow::DlssNeuralRendering);
        const std::pair<NrState, const char*> states[] = {{NrState::Active, "running"}, {NrState::MissingRuntime, "dll-missing"},
            {NrState::Unsupported, "unsupported"}, {NrState::Failed, "failed"}};
        for (const auto& [state, name] : states)
        {
            gpu::dlss::g_neuralRenderingState = state;
            capture(language, std::string("nr-help-") + name);
        }
        gpu::dlss::g_neuralRenderingState = NrState::Off;
    }
    gpu::frame_plan::menuFlowDlssEffect = {};
    gpu::video::menuFlowActiveGpu.clear();
    currentConfig = savedConfig;
    settings::edit = savedEdit;
    settings::tab = savedTab;
    settings::row = savedRow;
    std::printf("help captures written to %s\n", folder);
}

// LO_SUBPAGE_CAPTURE_DIR: the page entry rows (Graphics with HDR and DLSS on,
// Gameplay, System) and the page each one opens, with the installed faces in
// English and Simplified Chinese.
void CaptureSubpageEntries(uint8_t* base, const char* installRoot)
{
    const char* folder = std::getenv("LO_SUBPAGE_CAPTURE_DIR");
    if (!folder || !installRoot) return;
    using gpu::backend::Backend;
    using NrState = gpu::dlss::NeuralRenderingState;
    std::filesystem::create_directories(folder);
    const auto savedEdit = settings::edit;
    const auto savedConfig = currentConfig;
    const int savedTab = settings::tab, savedRow = settings::row;
    settings::status.clear();
    settings::restartPrompt = false;
    currentConfig.graphicsBackend = Backend::D3D12;
    currentConfig.upscaler = gpu::upscaling::Upscaler::Dlss;
    currentConfig.dlssQuality = gpu::upscaling::DlssQuality::Quality;
    currentConfig.frameGenerationProvider = framegen::Provider::Off;
    currentConfig.hdr = true;
    gpu::frame_plan::DlssEffectSnapshot running{};
    running.hasPlan = true;
    running.plannedRequest = gpu::upscaling::Upscaler::Dlss;
    running.device.backend = Backend::D3D12;
    running.device.deviceReady = running.device.dlssAvailable = true;
    running.phase = gpu::frame_plan::DlssEffectPhase::Active;
    running.execution = SubmittedObservation(gpu::upscaling::DlssQuality::Quality, 1707, 960, 2560, 1440);
    gpu::frame_plan::menuFlowDlssEffect = running;
    gpu::dlss::g_neuralRenderingState = NrState::MissingRuntime;
    // A confirms (the game's default button setting).
    const uint32_t savedFlags = PPC_LOAD_U32(ConfigData + 4);
    PPC_STORE_U32(ConfigData + 4, savedFlags & ~0x02000000u);
    const auto capture = [&](uint32_t language, const std::string& name) {
        settings::edit.uiLanguage = language;
        settings::Publish(base, ConfigData);
        auto preview = settings::snapshot;
        preview.assets = settings::menu_assets::Cached(installRoot, language);
        Require(bool(preview.assets), "installed capture assets");
        std::vector<uint32_t> pixels;
        Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "page entry capture raster");
        WriteBmp(std::filesystem::path(folder) / (name + (language ? "-zh-hans.bmp" : "-en.bmp")), pixels);
    };
    for (const uint32_t language : {0u, 4u})
    {
        settings::edit = currentConfig;
        settings::tab = 2;
        for (const auto [row, name] : {std::pair{GraphicsRow::NeuralRenderingTuning, "graphics-nr-tuning-row"},
                                        std::pair{GraphicsRow::HdrCalibration, "graphics-hdr-calibration-row"},
                                        std::pair{GraphicsRow::Brightness, "graphics-brightness-row"}})
        {
            settings::row = int(row);
            capture(language, name);
        }
        // The pages, as Confirm on those rows opens them (menu.cpp input).
        settings::row = int(GraphicsRow::NeuralRenderingTuning);
        settings::nrStart = settings::edit;
        settings::nrFocus = 0;
        settings::nrOpen = true;
        capture(language, "page-nr-tuning-dll-missing");
        settings::nrOpen = false;
        settings::row = int(GraphicsRow::HdrCalibration);
        settings::calibrationStartAutomatic = settings::edit.hdrPeakAutomatic;
        settings::calibrationStartPeakNits = settings::edit.hdrPeakNits;
        settings::calibrationFocus = 0;
        settings::calibrationScenePreview = true;
        settings::calibrationOpen = true;
        capture(language, "page-hdr-calibration");
        settings::calibrationOpen = false;
        settings::row = int(GraphicsRow::Brightness);
        settings::brightnessStart = settings::edit.displayBrightness;
        settings::gammaStart = settings::edit.displayGamma;
        settings::brightnessFocus = 0;
        settings::calibrationScenePreview = true;
        settings::brightnessOpen = true;
        capture(language, "page-brightness");
        settings::brightnessOpen = false;
        settings::tab = 0;
        settings::row = settings::GameControlsRow;
        capture(language, "gameplay-controls-row");
        settings::tab = 3;
        settings::row = settings::SystemModsRow;
        capture(language, "system-mods-row");
    }
    PPC_STORE_U32(ConfigData + 4, savedFlags);
    gpu::dlss::g_neuralRenderingState = NrState::Off;
    gpu::frame_plan::menuFlowDlssEffect = {};
    currentConfig = savedConfig;
    settings::edit = savedEdit;
    settings::tab = savedTab;
    settings::row = savedRow;
    std::printf("page entry captures written to %s\n", folder);
}

void CheckHelpFit(uint8_t* base, const char* installRoot)
{
    using gpu::backend::Backend;
    using gpu::frame_plan::DlssEffectPhase;
    using gpu::frame_plan::DlssEffectReason;
    using gpu::upscaling::Upscaler;
    using NrState = gpu::dlss::NeuralRenderingState;
    HelpAudit audit;
    if (installRoot)
        for (uint32_t language = 0; language < 5; ++language)
        {
            audit.assets[language] = settings::menu_assets::Cached(installRoot, language);
            Require(bool(audit.assets[language]), "installed menu assets for every menu language");
        }
    const auto savedEdit = settings::edit;
    const auto savedConfig = currentConfig;
    const int savedTab = settings::tab, savedRow = settings::row;
    const auto savedFg = gpu::video::menuFlowFgStatus;
    const auto savedOutput = apu::menuFlowOutput;
    settings::status.clear();
    settings::restartPrompt = false;
    gpu::video::menuFlowActiveGpu = "NVIDIA GeForce RTX 5080 Laptop GPU";
    gpu::frame_plan::menuFlowDlssEffect = {};
    gpu::frame_plan::menuFlowEligible.reset();
    gpu::video::menuFlowFgStatus = {};

    // Each tab's rows through the real Publish, in states that change their help.
    struct Graphics
    {
        const char* name;
        Backend backend;
        Upscaler upscaler;
        uint32_t antialiasing;
        framegen::Provider fg;
        bool hdr;
        uint32_t frameRate;
    };
    const Graphics graphics[] = {
        {"D3D12 TAA", Backend::D3D12, Upscaler::Off, 3, framegen::Provider::Off, false, 60},
        {"D3D12 DLSS", Backend::D3D12, Upscaler::Dlss, 0, framegen::Provider::Dlss, true, 120},
        {"D3D12 FSR", Backend::D3D12, Upscaler::Fsr, 0, framegen::Provider::Fsr, true, 60},
        {"D3D12 XeSS", Backend::D3D12, Upscaler::Xess, 0, framegen::Provider::Xess, false, 60},
        {"Vulkan DLSS", Backend::Vulkan, Upscaler::Dlss, 0, framegen::Provider::Dlss, true, 60},
        {"Vulkan FSR", Backend::Vulkan, Upscaler::Fsr, 0, framegen::Provider::Fsr, true, 60},
        {"Metal MetalFX", Backend::Metal, Upscaler::MetalFx, 0, framegen::Provider::MetalFx, true, 60},
    };
    const NrState nrStates[] = {NrState::Off, NrState::Active, NrState::MissingRuntime, NrState::Unsupported, NrState::Failed};
    const char* nrNames[] = {"off", "running", "DLL missing", "unsupported", "failed"};
    std::map<std::string, std::wstring> english;
    const auto publish = [&](uint32_t language, const std::string& where) {
        settings::edit.uiLanguage = language;
        settings::Publish(base, ConfigData);
        const auto& shown = settings::snapshot;
        audit.Measure(shown, where);
        if (language == 0) english[where] = shown.help + L"\n" + shown.notice;
        else audit.CompareWithEnglish(english[where], shown.help + L"\n" + shown.notice, language, where);
    };
    for (uint32_t language = 0; language < 5; ++language)
    {
        for (const int tab : {0, 1, 3})
        {
            const int count = tab == 0 ? settings::GameRowCount : tab == 1 ? settings::AudioRowCount : settings::SystemRowCount;
            settings::edit = savedConfig;
            settings::tab = tab;
            for (int row = 0; row < count; ++row)
                for (const uint32_t output : {settings::AudioOutputStereo, settings::AudioOutputSurround, settings::AudioOutputMatrix})
                    for (const bool pack : {false, true})
                    {
                        settings::edit.audioOutput = output;
                        apu::menuFlowOutput = apu::Output::Stereo;
                        settings::edit.gameLanguagePack = pack ? "pt-br" : "";
                        settings::row = row;
                        publish(language, "tab " + std::to_string(tab) + " row " + std::to_string(row) +
                            " audio " + std::to_string(output) + (pack ? " language pack" : ""));
                    }
        }
        for (const auto& [key, traditional, where] : {
                 std::tuple{L"Full shader preparation will run at the next start.",
                            L"下次啟動時會完整準備著色器。", "shader preparation scheduled"},
                 std::tuple{L"Could not schedule preparation. Check the settings folder.",
                            L"無法安排著色器準備。請確認設定目錄可寫入。", "shader preparation failed"}})
        {
            const std::wstring line = settings::MenuFlowTranslate(language, key, traditional);
            audit.MeasureBox(language, line, L"", where);
            if (language == 0) english[where] = line;
            else audit.CompareWithEnglish(english[where], line, language, where);
        }
        for (const auto result : {steam_shortcut::Status::Added, steam_shortcut::Status::AddedFromFlatpak,
                                  steam_shortcut::Status::SteamRunning, steam_shortcut::Status::SteamNotFound,
                                  steam_shortcut::Status::UserNotFound, steam_shortcut::Status::Error})
        {
            const auto where = "Steam result " + std::to_string(int(result));
            const std::wstring line = settings::SteamShortcutMessage(result);
            audit.MeasureBox(language, line, L"", where);
            if (language == 0) english[where] = line;
            else audit.CompareWithEnglish(english[where], line, language, where);
        }
        settings::tab = 2;
        for (const auto& state : graphics)
            for (int row = 0; row < int(GraphicsRow::Count); ++row)
            {
                const bool nr = row == int(GraphicsRow::DlssNeuralRendering);
                for (size_t n = 0; n < (nr ? std::size(nrStates) : 1); ++n)
                {
                    settings::edit = savedConfig;
                    settings::edit.graphicsBackend = state.backend;
                    settings::edit.upscaler = state.upscaler;
                    settings::edit.antialiasing = state.antialiasing;
                    settings::edit.frameGenerationProvider = state.fg;
                    settings::edit.hdr = state.hdr;
                    settings::edit.frameRate = state.frameRate;
                    gpu::dlss::g_neuralRenderingState = nrStates[n];
                    settings::row = row;
                    publish(language, std::string("Graphics row ") + std::to_string(row) + " " + state.name +
                        (nr ? std::string(" NR ") + nrNames[n] : std::string()));
                }
            }
        gpu::dlss::g_neuralRenderingState = NrState::Off;
    }
    settings::edit = savedConfig;

    // Graphics notices: every upscaler status against every unsaved edit.
    // Each provider's sentence first, then what is not applied yet.
    struct Running
    {
        std::string name;
        gpu::frame_plan::DlssEffectSnapshot effect;
        std::optional<gpu::frame_plan::DlssExecutionObservation> eligible;
    };
    const auto runningStates = [&](Upscaler provider) {
        std::vector<Running> states;
        gpu::frame_plan::DlssEffectSnapshot base{};
        base.device.backend = Backend::D3D12;
        base.device.deviceReady = true;
        base.device.dlssAvailable = base.device.fsrAvailable = base.device.xessAvailable = base.device.metalFxAvailable = true;
        base.hasPlan = true;
        base.plannedRequest = provider;
        auto submitted = SubmittedObservation(gpu::upscaling::DlssQuality::Performance, 1920, 1080, 3840, 2160);
        submitted.actualProvider = provider;
        submitted.plan.requestedUpscaler = provider;
        submitted.plan.consumer = provider == Upscaler::Dlss ? gpu::upscaling::TemporalConsumer::DlssSr
                                                             : gpu::upscaling::FsrQualityConsumer(provider);
        submitted.plan.fsrQuality = gpu::upscaling::FsrQuality::Performance;
        for (int phase = 0; phase <= int(DlssEffectPhase::GpuStopped); ++phase)
            for (int reason = 0; reason <= int(DlssEffectReason::UnsupportedProjection); ++reason)
            {
                auto effect = base;
                effect.phase = DlssEffectPhase(phase);
                effect.reason = DlssEffectReason(reason);
                const std::string at = "phase " + std::to_string(phase) + " reason " + std::to_string(reason);
                states.push_back({at, effect, {}});
                auto withExecution = effect;
                withExecution.execution = submitted;
                states.push_back({at + " submitted", withExecution, {}});
                auto fallback = submitted;
                fallback.outcome = gpu::frame_plan::DlssExecutionOutcome::Fallback;
                fallback.reason = DlssEffectReason(reason);
                withExecution.execution = fallback;
                states.push_back({at + " fallback", withExecution, {}});
                // The menu frame has no scene; the last game frame is described.
                auto menuFrame = fallback;
                menuFrame.outcome = gpu::frame_plan::DlssExecutionOutcome::Fallback;
                menuFrame.reason = DlssEffectReason::NoEligibleScene;
                menuFrame.submissionSerial = 0;
                auto sceneless = effect;
                sceneless.phase = DlssEffectPhase::TemporaryFallback;
                sceneless.reason = DlssEffectReason::NoEligibleScene;
                sceneless.execution = menuFrame;
                states.push_back({at + " last game frame submitted", sceneless, submitted});
                states.push_back({at + " last game frame fallback", sceneless, fallback});
                auto failed = effect;
                failed.failure = gpu::frame_plan::FailureReason{};
                states.push_back({at + " failed", failed, {}});
            }
        for (const auto device : {0, 1, 2})
        {
            auto effect = base;
            if (device == 0) effect.device.gpuWorkStopped = true;
            if (device == 1) effect.device.deviceReady = false;
            if (device == 2) effect.device.fsrAvailable = effect.device.xessAvailable = effect.device.metalFxAvailable = false;
            states.push_back({"device " + std::to_string(device), effect, {}});
        }
        return states;
    };
    for (uint32_t language = 0; language < 5; ++language)
    {
        settings::edit.uiLanguage = language;
        for (const auto provider : {Upscaler::Dlss, Upscaler::Fsr, Upscaler::Xess, Upscaler::MetalFx})
            for (const auto& running : runningStates(provider))
                for (const auto edited : {Upscaler::Off, Upscaler::Dlss, Upscaler::Fsr, Upscaler::Xess, Upscaler::MetalFx})
                    for (const auto backends : {std::pair{Backend::D3D12, Backend::D3D12}, std::pair{Backend::Vulkan, Backend::D3D12},
                                                std::pair{Backend::D3D12, Backend::Vulkan}, std::pair{Backend::D3D12, Backend::D3D11},
                                                std::pair{Backend::D3D12, Backend::Metal}})
                        for (const bool otherQuality : {false, true})
                        {
                            currentConfig = savedConfig;
                            currentConfig.upscaler = provider;
                            settings::edit = currentConfig;
                            settings::edit.uiLanguage = language;
                            settings::edit.upscaler = edited;
                            settings::edit.graphicsBackend = backends.first;
                            settings::edit.dlssQuality = otherQuality ? gpu::upscaling::DlssQuality::Dlaa
                                                                      : gpu::upscaling::DlssQuality::Performance;
                            settings::edit.fsrQuality = otherQuality ? gpu::upscaling::FsrQuality::NativeAA
                                                                     : gpu::upscaling::FsrQuality::Performance;
                            auto effect = running.effect;
                            effect.device.backend = backends.second;
                            gpu::frame_plan::menuFlowDlssEffect = effect;
                            gpu::frame_plan::menuFlowEligible = running.eligible;
                            audit.MeasureBox(language, L"-", settings::DlssNotice(),
                                "upscaler notice " + std::to_string(int(provider)) + " " + running.name + " edit " +
                                std::to_string(int(edited)) + (otherQuality ? " other quality" : ""));
                        }
        // Frame generation status, and the restart after DLSS FG on D3D12.
        for (int phase = 0; phase <= int(gpu::video::FrameGenerationPhase::RestartRequired); ++phase)
            for (const auto provider : {framegen::Provider::Dlss, framegen::Provider::Fsr, framegen::Provider::MetalFx, framegen::Provider::Xess})
                for (const bool overridden : {false, true})
                    for (const bool afterDlss : {false, true})
                    {
                        gpu::video::menuFlowFgStatus = {};
                        gpu::video::menuFlowFgStatus.phase = gpu::video::FrameGenerationPhase(phase);
                        gpu::video::menuFlowFgStatus.applied = provider;
                        gpu::video::menuFlowFgStatus.requested = provider;
                        gpu::video::menuFlowFgStatus.environmentOverride = overridden;
                        if (afterDlss) gpu::video::menuFlowFgStatus.sessionProvider = framegen::Provider::Dlss;
                        audit.MeasureBox(language, L"-", settings::FgNotice(), "FG notice phase " + std::to_string(phase));
                    }
    }
    gpu::frame_plan::menuFlowDlssEffect = {};
    gpu::frame_plan::menuFlowEligible.reset();
    gpu::video::menuFlowFgStatus = savedFg;
    currentConfig = savedConfig;

    // The list pages: Controls (help over the page note), Mods, Achievements.
    const auto savedControlsFocus = settings::controlsFocus;
    const auto savedCaptureRow = settings::captureRow;
    const auto savedModsFocus = settings::modsFocus;
    const auto savedAchievementsFocus = settings::achievementsFocus;
    const auto savedAchievements = settings::achievementItems;
    settings::achievementItems = settings::achievements_page::MenuFlowAchievementItems(false);
    for (uint32_t language = 0; language < 5; ++language)
    {
        settings::edit.uiLanguage = language;
        for (int focus = 0; focus < settings::ControlsRowCount; ++focus)
            for (const int capture : {0, 1, 2})
            {
                settings::controlsFocus = focus;
                settings::captureRow = capture ? focus : -1;
                settings::captureMode = capture;
                const auto [help, note] = settings::ControlsHelp();
                audit.MeasureBox(language, help, note, "Controls row " + std::to_string(focus));
            }
        const auto rows = settings::mods_page::Layout({}, settings::mods_page::CanOpenFolder);
        for (const int focus : {rows.empty, rows.open, rows.save})
        {
            if (focus < 0) continue;
            settings::modsFocus = focus;
            const auto [help, note] = settings::ModsHelp(rows);
            // The mods folder path is the player's own.
            audit.MeasureBox(language, help, note == settings::PathText(modding::Root()) ? L"" : note,
                             "Mods row " + std::to_string(focus));
        }
        modsReloadStatus = {mod_hot_reload::Phase::Preparing, 1234, 5678};
        {
            const auto [help, note] = settings::ModsHelp(rows);
            audit.MeasureBox(language, help, note, "Mods page while the new mods are prepared");
        }
        modsReloadStatus = {};
        for (int focus = 0; focus < settings::AchievementsFirstRow + int(settings::achievementItems.size()); ++focus)
        {
            settings::achievementsFocus = focus;
            audit.MeasureBox(language, settings::AchievementsHelp(), L"", "Achievements row " + std::to_string(focus));
        }
    }
    settings::controlsFocus = savedControlsFocus;
    settings::captureRow = savedCaptureRow;
    settings::captureMode = 0;
    settings::modsFocus = savedModsFocus;
    settings::achievementsFocus = savedAchievementsFocus;
    settings::achievementItems = savedAchievements;

    // Full pages: neural rendering (help line and preview message), brightness, HDR peak.
    for (uint32_t language = 0; language < 5; ++language)
    {
        settings::MenuSnapshot page;
        page.language = language;
        page.neuralRendering.open = true;
        for (int focus = 0; focus < settings::NrFocusCount; ++focus)
            for (const auto blocker : {settings::NeuralRenderingTuning::Blocker::None,
                                       settings::NeuralRenderingTuning::Blocker::MissingRuntime,
                                       settings::NeuralRenderingTuning::Blocker::Unsupported})
            {
                page.neuralRendering.focus = focus;
                page.neuralRendering.blocker = blocker;
                audit.Measure(page, "NR page focus " + std::to_string(focus));
            }
        page.neuralRendering.open = false;
        page.brightness.open = true;
        audit.Measure(page, "Brightness page");
        page.brightness.open = false;
        page.calibration.open = true;
        audit.Measure(page, "HDR peak page");
    }

    settings::edit = savedEdit;
    settings::tab = savedTab;
    settings::row = savedRow;
    apu::menuFlowOutput = savedOutput;
    gpu::video::menuFlowActiveGpu.clear();

    std::vector<std::pair<std::pair<uint32_t, std::wstring>, HelpLine>> sorted(audit.lines.begin(), audit.lines.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second.scale < b.second.scale; });
    size_t failed = 0;
    for (const auto& [key, line] : sorted)
    {
        if (line.scale >= 0.97) break;
        const bool fails = line.scale < kMinHelpFit;
        failed += fails;
        std::printf("%s help fit %3d%% lang %u %s [%s]: %s\n", fails ? "FAIL" : "    ", int(line.scale * 100 + 0.5),
                    key.first, line.face, line.where.c_str(), Utf8(key.second).c_str());
    }
    for (const auto& line : audit.untranslated) std::printf("FAIL untranslated: %s\n", line.c_str());
    for (const auto& line : audit.articleGlyphs) std::printf("FAIL article drawn as a button: %s\n", line.c_str());
    std::printf("help fit: %zu lines measured (%s), %zu below %d%%, %zu untranslated\n", audit.lines.size(),
                installRoot ? "game faces" : "built-in font", failed, int(kMinHelpFit * 100), audit.untranslated.size());
    Require(failed == 0, "a settings help line shrinks below the minimum");
    Require(audit.untranslated.empty(), "a settings help line has no translation");
    Require(audit.articleGlyphs.empty(), "an English help line has a bare article A");
    {
        const std::wstring cross(1, wchar_t(0xe100)), circle(1, wchar_t(0xe101));
        Require(settings::PromptGlyphs(L"A key already in use", false) == L"A key already in use", "no glyphs without PlayStation prompts");
        Require(settings::PromptGlyphs(L"Press A to compare", true) == L"Press " + cross + L" to compare", "Press A becomes the cross");
        Require(settings::PromptGlyphs(L"A: choose     B: back", true) == cross + L": choose     " + circle + L": back", "A: and B: become faces");
        Require(settings::PromptGlyphs(L"Pro Logic II or Neural:X receivers", true) == L"Pro Logic II or Neural:X receivers", "Neural:X is a name");
        Require(settings::PromptGlyphs(L"Scales 4x and 2x", true) == L"Scales 4x and 2x", "4x is not a button");
    }
    std::puts("PASS settings help lines keep their size in every language, state and page");
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

            // Its own entry row, right under it, opens the same page: by
            // Confirm and by a click. Cancel returns to the entry row.
            {
                const int tuning = int(GraphicsRow::NeuralRenderingTuning);
                press(2);
                Require(settings::row == tuning && settings::snapshot.rows[size_t(tuning)].pageEntry &&
                        settings::snapshot.rows[size_t(tuning)].value == L"Open", "down from NR reaches NR tuning & preview");
                press(0x1000);
                Require(settings::nrOpen && settings::nrFocus == 0 && settings::snapshot.neuralRendering.open,
                        "NR tuning & preview opens the NR page");
                settings::nrFocus = 11; press(0x1000);
                Require(!settings::nrOpen && settings::row == tuning, "Cancel returns to NR tuning & preview");
                int visible = 0;
                for (int i = 0; i < tuning; ++i) visible += !settings::snapshot.rows[size_t(i)].hidden;
                settings::PointerClick(700, float(150 + (visible - settings::snapshot.scroll) * 43 + 20), false);
                press(0);
                Require(settings::nrOpen, "a click on NR tuning & preview opens the NR page");
                settings::nrFocus = 11; press(0x1000);
                Require(!settings::nrOpen, "Cancel closes the page opened by a click");
                settings::edit.upscaler = gpu::upscaling::Upscaler::Fsr;
                press(0);
                Require(settings::snapshot.rows[size_t(tuning)].hidden &&
                        settings::snapshot.rows[int(GraphicsRow::DlssNeuralRendering)].hidden,
                        "NR tuning & preview hides with the NR row");
                settings::edit.upscaler = gpu::upscaling::Upscaler::Dlss;
                settings::row = int(GraphicsRow::DlssNeuralRendering);
                press(0);
            }

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

        // Loading pace belongs to Graphics and follows its edit / discard / Save workflow.
        {
            const auto priorCurrent = currentConfig;
            const auto priorDisk = diskConfig;
            auto saved = currentConfig;
            saved.uiLanguage = 0;
            saved.loadingTransitions = 2;
            currentConfig = diskConfig = saved;
            settings::edit = saved;
            settings::status.clear();
            settings::tab = 2;
            settings::row = int(GraphicsRow::LoadingTransitions);
            settings::pending = 0; Tick(base);
            Require(int(GraphicsRow::LoadingTransitions) == int(GraphicsRow::FrameRate) + 1 &&
                    int(GraphicsRow::VariableRefreshRate) == int(GraphicsRow::LoadingTransitions) + 1,
                    "loading pace follows Frame rate before VSync");
            const auto& pace = settings::snapshot.rows[int(GraphicsRow::LoadingTransitions)];
            Require(pace.name == L"Loading speed" && pace.choices == std::vector<std::wstring>{L"Original", L"Fast"} &&
                    pace.selectedChoice == 1 && !pace.hidden && pace.enabled,
                    "loading pace offers Original and Fast, with Fast selected");
            Require(settings::snapshot.help == L"Original keeps the Xbox 360 loading pace. Fast loads at a higher frame rate. Effects play at normal speed either way.",
                    "loading pace help describes loading and normal-speed effects");
            const auto beforeEdit = saves;
            settings::pending = 0x1000; Tick(base);
            Require(settings::edit.loadingTransitions == 2 && saves == beforeEdit,
                    "Confirm on the loading option does not change or save it");
            settings::pending = 8; Tick(base);
            Require(settings::edit.loadingTransitions == 0 && currentConfig.loadingTransitions == 2 &&
                    diskConfig.loadingTransitions == 2 && saves == beforeEdit,
                    "Right selects Original without applying or saving the edit");
            settings::pending = 4; Tick(base);
            Require(settings::edit.loadingTransitions == 2, "Left returns to Fast");
            settings::pending = 8; Tick(base);
            settings::pending = 0x2000; Tick(base);
            Require(!settings::active && settings::closing && saves == beforeEdit && diskConfig.loadingTransitions == 2,
                    "Back discards the loading edit without saving");
            Tick(base);
            settings::releaseToParent = false;
            settings::waitForRelease = false;
            PPC_STORE_U32(Menu + 4, 4); Tick(base); Poll(0, true);
            Require(settings::active && settings::edit.loadingTransitions == 2,
                    "reopening restores the saved Fast loading choice");
            settings::tab = 2;
            settings::row = int(GraphicsRow::LoadingTransitions);
            settings::pending = 8; Tick(base);
            settings::row = int(GraphicsRow::Save);
            settings::pending = 0x1000; Tick(base);
            Require(currentConfig.loadingTransitions == 0 && diskConfig.loadingTransitions == 0 && !settings::restartPrompt,
                    "Graphics Save applies Original loading without a restart");
            settings::row = int(GraphicsRow::LoadingTransitions);
            settings::pending = 8; Tick(base);
            saveFails = true;
            settings::row = int(GraphicsRow::Save);
            settings::pending = 0x1000; Tick(base);
            Require(currentConfig.loadingTransitions == 0 && diskConfig.loadingTransitions == 0 &&
                    settings::edit.loadingTransitions == 2 && settings::status == L"Could not save settings.",
                    "failed Graphics Save retains the applied loading pace and pending edit");
            saveFails = false;
            settings::pending = 0x1000; Tick(base);
            Require(currentConfig.loadingTransitions == 2 && diskConfig.loadingTransitions == 2,
                    "Graphics Save applies Fast loading");
            settings::row = int(GraphicsRow::LoadingTransitions);
            settings::pending = 8; Tick(base);
            settings::tab = 3;
            settings::row = settings::SystemSaveRow;
            settings::pending = 0x1000; Tick(base);
            Require(settings::edit.loadingTransitions == 0 && diskConfig.loadingTransitions == 2,
                    "System Save leaves a pending loading pace edit unsaved");

            const wchar_t* labels[] = {L"Loading speed", L"載入速度", L"ロード速度", L"로딩 속도", L"加载速度"};
            const wchar_t* original[] = {L"Original", L"原版", L"オリジナル", L"원본", L"原版"};
            const wchar_t* fast[] = {L"Fast", L"快速", L"速い", L"빠르게", L"快"};
            const wchar_t* help[] = {
                L"Original keeps the Xbox 360 loading pace. Fast loads at a higher frame rate. Effects play at normal speed either way.",
                L"原版維持 Xbox 360 的載入節奏；快速以更高幀率載入。兩者的過場效果都維持正常速度。",
                L"オリジナルは Xbox 360 のロード速度、高速はより高いフレームレートで読み込みます。演出はどちらも通常の速度です。",
                L"원본은 Xbox 360의 로딩 속도를 유지하고, 빠름은 더 높은 프레임 레이트로 불러옵니다. 효과는 어느 쪽이든 원래 속도로 재생됩니다.",
                L"原版保持 Xbox 360 的加载节奏；快速以更高帧率加载。两种设置下过场效果都保持正常速度。"};
            settings::tab = 2;
            settings::row = int(GraphicsRow::LoadingTransitions);
            for (uint32_t language = 0; language < 5; ++language)
            {
                settings::edit.uiLanguage = language;
                settings::status.clear();
                settings::pending = 0; Tick(base);
                const auto& translated = settings::snapshot.rows[int(GraphicsRow::LoadingTransitions)];
                Require(translated.name == labels[language] && translated.choices ==
                        std::vector<std::wstring>{original[language], fast[language]} && settings::snapshot.help == help[language],
                        "loading option label, choices and help are localized in every menu language");
            }
            currentConfig = priorCurrent;
            diskConfig = priorDisk;
            settings::edit = currentConfig;
            settings::pending = 0; Tick(base);
            std::puts("PASS loading pace Graphics placement, two choices, edit/discard/Save, failed save and five languages");
        }

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
                    "down from Upscaler skips hidden quality, model, sharpness and neural rendering rows to FG");
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
                    settings::snapshot.rows[settings::SystemSaveRow].name == L"Save settings" && settings::snapshot.scroll == 1,
                    "Start on System tab jumps to Save settings, the last row");

            // PointerClick boundary tests:
            // Valid slot 0 (y = 150) hits the first scrolled row.
            settings::PointerClick(100.0f, 150.0f, false);
            Require(settings::mouseRow.load() == settings::snapshot.scroll, "click at y=150 selects the first visible row");
            settings::PointerClick(100.0f, 617.0f, false);
            Require(settings::mouseRow.load() == settings::SystemSaveRow,
                    "System's last row is visible and clickable after scrolling");
            settings::mouseRow = -1;
            settings::PointerClick(100.0f, 618.0f, false);
            Require(settings::mouseRow.load() == -1, "System ignores the gap below its compact rows");

            // Click at y < 150 (above rows) is ignored
            settings::mouseRow = -1;
            settings::PointerClick(100.0f, 140.0f, false);
            Require(settings::mouseRow.load() == -1, "click above y=150 ignored");

            // Click below the visible viewport is ignored.
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
            // A running DLSS FG session changes its multiplier in place, HDR on or not.
            settings::Config hdrFg = diskConfig;
            hdrFg.graphicsBackend = settings::GraphicsBackend::Vulkan;
            hdrFg.hdr = true;
            hdrFg.frameGenerationProvider = Provider::Dlss;
            hdrFg.frameGenerationMode = framegen::Mode::Fixed;
            hdrFg.frameGenerationMultiplier = 2;
            currentConfig = diskConfig = hdrFg;
            settings::edit = hdrFg;
            gpu::video::menuFlowFgStatus = {};
            gpu::video::menuFlowFgStatus.sessionProvider = gpu::video::menuFlowFgStatus.requested =
                gpu::video::menuFlowFgStatus.applied = Provider::Dlss;
            gpu::video::menuFlowFgStatus.phase = gpu::video::FrameGenerationPhase::Ready;
            settings::edit.frameGenerationMultiplier = 3;
            settings::row = int(GraphicsRow::Save);
            settings::pending = 0x1000; Tick(base);
            Require(!settings::restartPrompt && diskConfig.frameGenerationMultiplier == 3 && diskConfig.hdr,
                    "an FG multiplier change with HDR on saves without a restart prompt");
            gpu::video::menuFlowFgStatus = {};
            currentConfig = priorCurrent;
            diskConfig = priorDisk;
            std::puts("PASS Graphics FG section: provider/input navigation, DLSS multiplier bounds, FSR fixed 2x, shared Save, System isolation, mouse/scroll and four-tab navigation");
        }
        // FSR sharpness follows quality and the DLSS-only model row; Save is
        // always last. Off disables RCAS, and percent changes are bounded.
        {
            static_assert(int(GraphicsRow::Save) + 1 == int(GraphicsRow::Count) &&
                          int(GraphicsRow::DlssModel) == int(GraphicsRow::DlssQuality) + 1 &&
                          int(GraphicsRow::FsrSharpness) == int(GraphicsRow::DlssModel) + 1);
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
            Require(settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].hidden &&
                    !settings::snapshot.rows[int(GraphicsRow::DlssModel)].hidden, "DLSS hides FSR sharpness and shows the model");
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
        // Reopening drops an unsaved DLSS choice; focus leaves the now hidden model row.
        {
            const auto priorCurrent = currentConfig;
            currentConfig.upscaler = gpu::upscaling::Upscaler::Off;
            settings::tab = 2;
            settings::row = int(GraphicsRow::DlssModel);
            settings::active = false;
            PPC_STORE_U32(Menu + 4, 4);
            settings::pending = 0; Tick(base);
            Require(settings::active && settings::row == int(GraphicsRow::FrameGeneration),
                    "reopening on the hidden DLSS model row moves focus to the next visible row");
            currentConfig = priorCurrent;
            settings::edit = currentConfig;
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
        // Gameplay's host row is Controls, after the seven retail settings: it
        // opens the Controls page with Button prompts and Vibration, the
        // controller and keyboard layouts, Restore and Save. Button prompts
        // cycles Auto, Xbox, PlayStation, applies live and saves at once.
        {
            namespace map = hid::input_map;
            // The retail Confirmation button stays A / B through the Controls tests.
            const uint32_t retailFlags = PPC_LOAD_U32(ConfigData + 4);
            PPC_STORE_U32(ConfigData + 4, retailFlags & ~0x02000000u);
            settings::tab = 0;
            settings::row = settings::GameControlsRow;
            settings::status.clear();
            currentConfig.buttonPrompts = diskConfig.buttonPrompts = 0;
            settings::edit = currentConfig;
            settings::edit.uiLanguage = 0;
            settings::pending = 0; Tick(base);
            const auto& rows = settings::snapshot.rows;
            Require(settings::GameRowCount == 10 && rows.size() == size_t(settings::GameRowCount) &&
                    rows[settings::GameControlsRow].name == L"Controls" && rows[settings::GameControlsRow].value == L"Open" &&
                    rows[settings::GameControlsRow].pageEntry &&
                    rows[settings::GameRestoreRow].name == L"Restore game defaults" &&
                    settings::graphics_menu::IsAction(0, settings::GameControlsRow) &&
                    settings::snapshot.help == L"Button prompts, vibration, and the controller and keyboard buttons.",
                    "Gameplay has ten rows: the retail settings, Controls, Restore and Quit");
            settings::pending = 8; Tick(base);
            Require(!settings::controlsOpen, "right does not open the Controls page");
            settings::pending = 0x1000; Tick(base);
            Require(settings::controlsOpen && settings::snapshot.pageTitle == L"Controls" &&
                    settings::snapshot.row == settings::ControlsPromptRow && rows.size() == size_t(settings::ControlsRowCount) &&
                    settings::snapshot.keyLegend == L"A: change     Start: save     B: back" &&
                    settings::snapshot.notice == L"This page always uses the default buttons and keys.",
                    "A on Controls opens the Controls page");
            const int padA = settings::ControlsControllerFirst + map::A, keyA = settings::ControlsKeyboardFirst + map::A;
            Require(rows[settings::ControlsPromptRow].name == L"Button prompts" && rows[settings::ControlsVibrationRow].name == L"Vibration" &&
                    rows[settings::ControlsControllerTitle].header && rows[settings::ControlsControllerTitle].name == L"Controller" &&
                    rows[settings::ControlsKeyboardTitle].header && rows[settings::ControlsKeyboardTitle].name == L"Keyboard" &&
                    rows[padA].name == L"A" && rows[padA].value == L"A" && !rows[padA].literal &&
                    rows[padA + map::LeftTrigger].name == L"LT" && rows[padA + map::LeftTrigger].value == L"LT" &&
                    rows[padA + map::LeftThumb].name == L"Left stick click" &&
                    rows[padA + map::DpadRight].name == L"D-pad right" && rows[padA + map::DpadRight].value == L"D-pad right" &&
                    rows[keyA].name == L"A" && rows[keyA].value == L"Z" && rows[keyA].literal &&
                    rows[keyA + map::X].value == L"A" && rows[keyA + map::Start].value == L"Return" &&
                    rows[keyA + map::LeftThumb].value == L"None" && !rows[keyA + map::LeftThumb].literal &&
                    rows[keyA + map::StickUp].name == L"Left stick up" && rows[keyA + map::StickUp].value == L"I" &&
                    rows[settings::ControlsRestoreRow].name == L"Restore default buttons" &&
                    rows[settings::ControlsSaveRow].name == L"Save controls" && rows[settings::ControlsSaveRow].value == L"Save",
                    "the page lists each Xbox 360 input with its controller button, then with its key; a stick click has no key");
            const unsigned beforeSaves = saves;
            promptStyle = 0;
            settings::pending = 8; Tick(base);
            Require(rows[settings::ControlsPromptRow].choices.size() == 6 && rows[settings::ControlsPromptRow].selectedChoice == 1 &&
                    promptStyle == 1 && diskConfig.buttonPrompts == 1 && saves == beforeSaves + 1,
                    "Button prompts cycles to Xbox, applies live and saves");
            settings::pending = 8; Tick(base);
            Require(promptStyle == 2 && diskConfig.buttonPrompts == 2, "Button prompts cycles to PlayStation");
            for (uint32_t style : {3u, 4u, 5u})
            {
                settings::pending = 8; Tick(base);
                Require(promptStyle == style && diskConfig.buttonPrompts == style,
                        "Button prompts cycles to Switch, Steam Deck and Keyboard");
            }
            settings::pending = 8; Tick(base);
            Require(promptStyle == 0 && diskConfig.buttonPrompts == 0, "Button prompts wraps back to Auto");
            Require(settings::snapshot.help.find(L"Auto follows the controller") != std::wstring::npos, "Button prompts help");
            PPC_STORE_U32(ConfigData + 4, retailFlags);
            std::puts("PASS Gameplay Controls row opens the Controls page; its rows; Button prompts");
        }
        // Vibration follows Button prompts on the Controls page. It applies and
        // saves at once, stays within 0-100 and never commits unsaved Graphics edits.
        {
            settings::status.clear();
            currentConfig.vibrationPercent = diskConfig.vibrationPercent = 100;
            settings::edit = currentConfig;
            settings::edit.uiLanguage = 0;
            settings::edit.width = currentConfig.width == 2560 ? 1920 : 2560;
            settings::controlsFocus = settings::ControlsPromptRow;
            const unsigned beforeSaves = saves, beforeApplies = applies, beforePreviews = vibrationPreviews;
            settings::pending = 2; Tick(base);
            Require(settings::controlsOpen && settings::controlsFocus == settings::ControlsVibrationRow &&
                    settings::snapshot.rows[settings::ControlsVibrationRow].name == L"Vibration" &&
                    settings::snapshot.rows[settings::ControlsVibrationRow].sliderPercent == 100,
                    "Vibration slider follows Button prompts on the Controls page");
            Require(settings::snapshot.help.find(L"Min turns it off") != std::wstring::npos, "Vibration help");
            settings::pending = 8; Tick(base);
            Require(settings::edit.vibrationPercent == 100 && saves == beforeSaves, "right at 100 clamps without saving");
            settings::pending = 4; Tick(base);
            Require(settings::edit.vibrationPercent == 90 && saves == beforeSaves + 1 && diskConfig.vibrationPercent == 90 &&
                    vibrationStrength == 90 && vibrationPreviews == beforePreviews + 1 && applies == beforeApplies,
                    "left lowers, saves, applies live and previews without a guest apply");
            Require(diskConfig.width != settings::edit.width, "vibration save leaves unsaved Graphics edits unsaved");
            Require(settings::snapshot.rows[settings::ControlsVibrationRow].sliderPercent == 90, "slider follows the strength");
            settings::edit.vibrationPercent = currentConfig.vibrationPercent = 0;
            settings::pending = 4; Tick(base);
            Require(settings::edit.vibrationPercent == 0 && saves == beforeSaves + 1, "left at 0 does not wrap");
            settings::pending = 2; Tick(base);
            Require(settings::controlsFocus == settings::ControlsControllerFirst, "down skips the Controller title");
            settings::pending = 0x2000; Tick(base);
            Require(!settings::controlsOpen && settings::tab == 0 && settings::row == settings::GameControlsRow,
                    "B without changes returns to Gameplay's Controls row");
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::GameRestoreRow, "the game actions follow Controls");
            // Voice volume follows Sound effects: a host slider in the retail volume
            // steps, saved at once, with a guest apply so the game refreshes its gains.
            settings::tab = 1;
            settings::row = settings::AudioEffectsRow;
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::AudioVoiceVolumeRow && settings::snapshot.rows.size() == size_t(settings::AudioRowCount) &&
                    settings::snapshot.rows[settings::AudioVoiceVolumeRow].name == L"Voice volume" &&
                    settings::snapshot.rows[settings::AudioVoiceVolumeRow].sliderPercent == 100,
                    "Voice volume follows Sound effects at 100%");
            Require(settings::snapshot.help.find(L"Cutscene dialogue volume") != std::wstring::npos, "Voice volume help");
            settings::pending = 8; Tick(base);
            Require(settings::edit.voiceVolume == 100 && saves == beforeSaves + 1 && applies == beforeApplies,
                    "right at 100 clamps without saving");
            settings::pending = 4; Tick(base);
            Require(settings::edit.voiceVolume == 96 && diskConfig.voiceVolume == 96 && apu::menuFlowVoiceVolume == 96 &&
                    saves == beforeSaves + 2 && applies == beforeApplies + 1 && diskConfig.width != settings::edit.width &&
                    settings::snapshot.rows[settings::AudioVoiceVolumeRow].sliderPercent == 96,
                    "left lowers by 4, saves alone, applies live and re-applies the guest volumes");
            // Audio output follows Voice volume: Right switches to 5.1 live and saves at once.
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::AudioOutputRow &&
                    settings::snapshot.rows[settings::AudioOutputRow].name == L"Audio output" &&
                    settings::snapshot.rows[settings::AudioOutputRow].selectedChoice == 0,
                    "Audio output follows Voice volume, Stereo by default");
            settings::pending = 8; Tick(base);
            Require(settings::edit.audioOutput == settings::AudioOutputSurround && diskConfig.audioOutput == settings::AudioOutputSurround &&
                    apu::menuFlowOutput == apu::Output::Surround && saves == beforeSaves + 3 && applies == beforeApplies + 1 &&
                    diskConfig.width != settings::edit.width, "audio output switches live and saves alone");
            // Rear angle closes the Audio tab and only moves with Matrix surround.
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::AudioRearAngleRow && !settings::snapshot.rows[settings::AudioRearAngleRow].enabled &&
                    !apu::menuFlowTestSignal, "Rear angle follows Audio output, disabled and silent without Matrix surround");
            settings::pending = 8; Tick(base);
            Require(settings::edit.audioMatrixRear == 110 && saves == beforeSaves + 3, "disabled Rear angle ignores Right");
            settings::pending = 1; Tick(base);
            settings::pending = 8; Tick(base);
            Require(settings::edit.audioOutput == settings::AudioOutputMatrix && apu::menuFlowOutput == apu::Output::Matrix &&
                    diskConfig.audioOutput == settings::AudioOutputMatrix && saves == beforeSaves + 4 && !apu::menuFlowTestSignal,
                    "Right again selects Matrix surround; no test noise on Audio output");
            settings::pending = 2; Tick(base);
            Require(apu::menuFlowTestSignal, "test noise plays while Rear angle is focused");
            Require(settings::snapshot.speakerLayout, "the speaker layout shows under the rows");
            settings::pending = 8; Tick(base);
            Require(settings::snapshot.rows[settings::AudioRearAngleRow].enabled &&
                    settings::snapshot.rows[settings::AudioRearAngleRow].value == L"120°" &&
                    settings::edit.audioMatrixRear == 120 && diskConfig.audioMatrixRear == 120 && apu::menuFlowMatrixRear == 120 &&
                    settings::snapshot.speakerRear == 120 &&
                    saves == beforeSaves + 5 && applies == beforeApplies + 1 && diskConfig.width != settings::edit.width,
                    "Rear angle steps 10 degrees, applies live and saves alone");
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::AudioVoiceRow && !apu::menuFlowTestSignal && !settings::snapshot.speakerLayout,
                    "down from Rear angle wraps to Voice language and stops the test noise and layout");
            settings::edit = currentConfig;
            std::puts("PASS Gameplay Vibration, Voice volume and Audio output: bounds, immediate save, live apply, Graphics edits untouched");
            // Discord is a live host setting: save just that field and leave
            // pending Graphics/System edits alone. A failed save keeps it active.
            settings::tab = 3; settings::row = settings::SystemDiscordStatusRow;
            currentConfig.discordStatus = diskConfig.discordStatus = settings::edit.discordStatus = true;
            settings::edit.width = 1234;
            settings::edit.automaticUpdates = !currentConfig.automaticUpdates;
            const auto discordSaves = saves;
            const auto discordChanges = discord_rpc::menuFlowChanges;
            Tick(base);
            Require(settings::snapshot.rows[settings::SystemDiscordStatusRow].name == L"Discord status" &&
                    !settings::snapshot.rows[settings::SystemDiscordStatusRow].hidden &&
                    settings::SystemDiscordStatusRow == settings::SystemUpdatesRow + 1 &&
                    settings::SystemDebugLogRow == settings::SystemDiscordStatusRow + 1 &&
                    settings::snapshot.help == L"Shows your location and activity on Discord. Applies and saves immediately.",
                    "Discord status is grouped with host preferences and explains its live save");
            settings::pending = 0x1000; Tick(base);
            Require(settings::edit.discordStatus && saves == discordSaves &&
                    discord_rpc::menuFlowChanges == discordChanges, "confirm does not toggle Discord status");
            settings::pending = 8; Tick(base);
            Require(!settings::edit.discordStatus && !currentConfig.discordStatus && !diskConfig.discordStatus &&
                    !discord_rpc::menuFlowEnabled && saves == discordSaves + 1 &&
                    discord_rpc::menuFlowChanges == discordChanges + 1 &&
                    diskConfig.width != settings::edit.width &&
                    diskConfig.automaticUpdates != settings::edit.automaticUpdates,
                    "Discord disables live and persists without saving pending settings");
            saveFails = true;
            settings::pending = 4; Tick(base);
            Require(!settings::edit.discordStatus && !discord_rpc::menuFlowEnabled &&
                    discord_rpc::menuFlowChanges == discordChanges + 1 &&
                    settings::snapshot.help == L"Could not save settings.",
                    "failed Discord save retains the active preference");
            saveFails = false;
            settings::pending = 4; Tick(base);
            Require(settings::edit.discordStatus && diskConfig.discordStatus && discord_rpc::menuFlowEnabled &&
                    discord_rpc::menuFlowChanges == discordChanges + 2, "Discord enables live and persists");
            settings::edit = currentConfig;
            std::puts("PASS Discord setting: row placement, immediate save/live apply, failed save and pending settings");
            // System > Achievements opens the page; Notifications cycles On -> Silent -> Off and saves alone.
            settings::tab = 3; settings::row = settings::SystemAchievementsRow;
            settings::edit.achievementNotifications = true; settings::edit.achievementSound = true;
            const int beforeAchSaves = saves;
            Tick(base);
            Require(settings::snapshot.rows[settings::SystemAchievementsRow].name == L"Achievements" &&
                    settings::snapshot.rows[settings::SystemAchievementsRow].value == L"2/5 · 30G" &&
                    settings::graphics_menu::IsAction(3, settings::SystemAchievementsRow) &&
                    settings::snapshot.rows.size() == size_t(settings::SystemRowCount),
                    "System row shows earned/total and gamerscore");
            settings::pending = 0x1000; Tick(base);
            {
                const auto &rows = settings::snapshot.rows;
                Require(settings::achievementsOpen && settings::snapshot.pageTitle == L"Achievements" &&
                        settings::snapshot.row == settings::AchievementsNotificationsRow &&
                        rows.size() == size_t(settings::AchievementsFirstRow + 5) && !settings::snapshot.keyLegend.empty(),
                        "A on System Achievements opens the page");
                Require(rows[settings::AchievementsNotificationsRow].name == L"Notifications" &&
                        rows[settings::AchievementsNotificationsRow].choices.size() == 3 &&
                        rows[settings::AchievementsNotificationsRow].selectedChoice == 0 &&
                        rows[settings::AchievementsSummaryRow].header && rows[settings::AchievementsSummaryRow].name == L"Earned 2 of 5" &&
                        rows[settings::AchievementsSummaryRow].detail == L"30 / 90G",
                        "Notifications row and the summary title");
                const auto entry = [&](int i) -> const settings::MenuRow & { return rows[size_t(settings::AchievementsFirstRow + i)]; };
                Require(entry(0).name == L"Newest" && entry(0).detail == L"Done it" &&
                        entry(0).value == L"20G   " + settings::achievements_page::FormatDate(2000000000) && entry(0).icon &&
                        entry(1).name == L"Oldest" && entry(1).value.rfind(L"10G   ", 0) == 0 &&
                        entry(2).name == L"Locked one" && entry(2).detail == L"Do it" && entry(2).value == L"30G" && entry(2).icon &&
                        entry(3).name == L"Secret achievement" && entry(3).value == L"20G" && !entry(3).icon && entry(3).iconGlyph == L"?" &&
                        entry(3).detail == L"Keep playing to unlock this secret achievement." &&
                        entry(4).name == L"Secret achievement" && entry(4).detail == entry(3).detail,
                        "earned newest first, then locked by id; a locked secret hides name, text and icon");
                // Earned icons keep colour (BGRA from RGBA); locked ones are gray and darker.
                const uint32_t locked = entry(2).icon->pixels[0];
                Require(entry(0).icon->pixels[0] == 0xff112233u && ((locked >> 16) & 255) == ((locked >> 8) & 255) &&
                        (locked & 255) == ((locked >> 8) & 255) && (locked & 255) < 0x22,
                        "icon converted to BGRA; locked icon desaturated and dimmed");
            }
            settings::pending = 2; Tick(base);
            Require(settings::achievementsFocus == settings::AchievementsFirstRow, "Down skips the summary title");
            settings::pending = 1; Tick(base);
            Require(settings::achievementsFocus == settings::AchievementsNotificationsRow, "Up skips the summary title");
            settings::pending = 8; Tick(base);
            Require(settings::edit.achievementNotifications && !settings::edit.achievementSound &&
                    diskConfig.achievementNotifications && !diskConfig.achievementSound && saves == beforeAchSaves + 1 &&
                    settings::snapshot.rows[settings::AchievementsNotificationsRow].selectedChoice == 1, "Right selects Silent and saves");
            settings::pending = 8; Tick(base);
            Require(!settings::edit.achievementNotifications && !diskConfig.achievementNotifications &&
                    saves == beforeAchSaves + 2 && settings::snapshot.rows[settings::AchievementsNotificationsRow].selectedChoice == 2,
                    "Right again selects Off and saves");
            settings::pending = 8; Tick(base);
            Require(settings::edit.achievementNotifications && settings::edit.achievementSound && diskConfig.achievementSound &&
                    settings::snapshot.rows[settings::AchievementsNotificationsRow].selectedChoice == 0, "Right wraps to On with sound");
            settings::pending = 4; Tick(base);
            Require(!settings::edit.achievementNotifications && saves == beforeAchSaves + 4, "Left wraps to Off");
            settings::pending = 2; Tick(base);
            Require(settings::snapshot.help.find(L"Newest: Done it") == 0, "help for an entry");
            settings::pending = 0x2000; Tick(base);
            Require(!settings::achievementsOpen && settings::tab == 3 && settings::row == settings::SystemAchievementsRow &&
                    settings::snapshot.pageTitle.empty(), "B leaves the page for the System row");
            settings::pending = 0x1000; Tick(base);
            Require(settings::achievementsOpen, "the page opens again");
            settings::pending = 0x2000; Tick(base);
            Require(!settings::achievementsOpen, "and B leaves it");
            std::puts("PASS System Achievements page: summary, order, secret hiding, icons, notifications On / Silent / Off, back");
        }
        // Controls page layouts. A, Enter or a click on a layout row starts a
        // capture; the next press binds it, a button or key already in use
        // swaps, Escape cancels, and 5 s without a press change nothing.
        // Restore, Save (live, merged into the saved settings), the discard
        // prompt, and the default layout the page itself runs on.
        {
            namespace map = hid::input_map;
            const uint32_t retailFlags = PPC_LOAD_U32(ConfigData + 4);
            PPC_STORE_U32(ConfigData + 4, retailFlags & ~0x02000000u);
            settings::tab = 0;
            settings::row = settings::GameControlsRow;
            settings::status.clear();
            settings::menuClockOffset = {};
            currentConfig.controllerMap = diskConfig.controllerMap = map::DefaultController;
            currentConfig.keyboardMap = diskConfig.keyboardMap = map::DefaultKeyboard;
            settings::edit = currentConfig;
            settings::edit.uiLanguage = 0;
            settings::edit.width = currentConfig.width == 2560 ? 1920 : 2560; // an unsaved Graphics edit
            heldSources = 0;
            const unsigned beforeApplies = inputMapApplies;
            settings::pending = 0x1000; Tick(base);
            Require(settings::controlsOpen, "Controls page opens");
            const auto& rows = settings::snapshot.rows;
            const auto rowY = [&](int r) { return float(150 + (r - settings::snapshot.scroll) * 43 + 20); };
            const int padA = settings::ControlsControllerFirst + map::A, padB = padA + map::B;
            const int keyA = settings::ControlsKeyboardFirst + map::A, keyB = keyA + map::B;
            const auto preview = [&](const char* name, bool playStation = false) {
                auto shown = settings::snapshot;
                shown.assets.reset();
                shown.playStationPrompts = playStation;
                std::vector<uint32_t> pixels;
                Require(settings::RasterizeMenu(shown, 1280, 720, pixels), "Controls page preview");
                const auto evidence = std::filesystem::current_path() / "out" / "controls-page-preview";
                std::filesystem::create_directories(evidence);
                WriteBmp(evidence / name, pixels);
            };
            preview("controls-page.bmp");

            // A on a controller row: the A that started it is ignored until released.
            settings::controlsFocus = padA;
            heldSources = 1u << 0;
            settings::pending = 0x1000; Tick(base);
            Require(settings::captureRow == padA && settings::captureMode == 1 && rows[padA].value == L"Press a button… (5)" &&
                    settings::snapshot.help == L"Press a button on any controller. Esc on the keyboard cancels.",
                    "A starts a controller capture with a 5 s countdown");
            preview("controls-capture.bmp");
            Tick(base);
            Require(settings::captureRow == padA && !settings::captureArmed, "the press that started the capture is not taken");
            heldSources = 0; Tick(base);
            Require(settings::captureArmed, "releasing it arms the capture");
            settings::pending = 0x2000 | 1; Tick(base);
            Require(settings::captureRow == padA && settings::controlsOpen && settings::controlsFocus == padA,
                    "menu input does nothing during a capture");
            heldSources = 1u << 1; Tick(base);
            Require(settings::captureRow < 0 && settings::captureMode == 0 && settings::waitForRelease &&
                    settings::controlsController[map::A] == 1 && settings::controlsController[map::B] == 0 &&
                    rows[padA].value == L"B" && rows[padB].value == L"A" && settings::snapshot.help == L"Swapped with: B",
                    "a button in use swaps: A takes B's button and B takes A's");
            Require(diskConfig.controllerMap == map::DefaultController && inputMapApplies == beforeApplies,
                    "a capture stays unsaved");
            heldSources = 0;
            settings::waitForRelease = false;
            // A trigger past half way binds as well; Guide never does.
            const int padLb = settings::ControlsControllerFirst + map::LeftShoulder;
            settings::controlsFocus = padLb;
            settings::pending = 0x1000; Tick(base); Tick(base);
            heldSources = 1u << map::GuideSource; Tick(base);
            Require(settings::captureRow == padLb, "Guide is not bound");
            heldSources |= 1u << map::LeftTriggerSource; Tick(base);
            Require(settings::controlsController[map::LeftShoulder] == map::LeftTriggerSource &&
                    settings::controlsController[map::LeftTrigger] == 9 && rows[padLb].value == L"LT" &&
                    rows[settings::ControlsControllerFirst + map::LeftTrigger].value == L"LB",
                    "a pulled trigger binds and LT takes LB's button");
            heldSources = 0;
            settings::waitForRelease = false;
            // Escape cancels a controller capture; other keys stay with the game.
            const int padY = settings::ControlsControllerFirst + map::Y;
            settings::controlsFocus = padY;
            settings::pending = 0x1000; Tick(base);
            Require(!settings::CaptureKey(29, false), "a controller capture leaves other keys alone");
            Require(settings::CaptureKey(map::EscapeKey, false), "Escape belongs to the capture");
            Tick(base);
            Require(settings::captureRow < 0 && settings::controlsController[map::Y] == 3 && settings::snapshot.help == L"Cancelled.",
                    "Escape cancels without a change");
            Require(!settings::CaptureKey(map::EscapeKey, false), "without a capture no key is taken");
            // Five seconds without a press change nothing.
            const int padX = settings::ControlsControllerFirst + map::X;
            settings::controlsFocus = padX;
            settings::pending = 0x1000; Tick(base);
            settings::menuClockOffset += std::chrono::milliseconds(4100); Tick(base);
            Require(settings::captureRow == padX && rows[padX].value == L"Press a button… (1)", "the countdown runs");
            settings::menuClockOffset += std::chrono::seconds(1); Tick(base);
            Require(settings::captureRow < 0 && settings::controlsController[map::X] == 2 &&
                    settings::snapshot.help == L"No button pressed. Nothing changed.", "the capture times out after 5 s");
            settings::menuClockOffset = {};

            // Keyboard: Enter starts a capture; repeats, F1 and the controller do not bind.
            settings::controlsFocus = keyA;
            Require(settings::CalibrationKey(13), "Enter belongs to the Controls page");
            settings::pending = 0; Tick(base);
            Require(settings::captureRow == keyA && settings::captureMode == 2 && rows[keyA].value == L"Press a key… (5)" &&
                    settings::snapshot.help == L"Press a key. Esc cancels.", "Enter starts a keyboard capture");
            preview("controls-capture-key.bmp");
            Require(settings::CaptureKey(40, true), "a keyboard capture takes a repeating Enter"); Tick(base);
            Require(settings::captureRow == keyA, "a repeat is not a press");
            Require(settings::CaptureKey(map::F1Key, false), "a keyboard capture takes F1"); Tick(base);
            Require(settings::captureRow == keyA, "F1 is not bound");
            heldSources = 1u << 0; Tick(base); Tick(base);
            Require(settings::captureRow == keyA, "a controller button does not bind a key");
            heldSources = 0;
            Require(settings::CaptureKey(27, false), "a keyboard capture takes X"); Tick(base);
            Require(settings::captureRow < 0 && settings::controlsKeyboard[map::A] == 27 && settings::controlsKeyboard[map::B] == 29 &&
                    rows[keyA].value == L"X" && rows[keyA].literal && rows[keyB].value == L"Z" &&
                    settings::snapshot.help == L"Swapped with: B", "a key in use swaps");
            // With PlayStation prompts the inputs show their glyphs; key names stay letters.
            settings::controlsFocus = keyA + map::Y;
            settings::pending = 0; Tick(base);
            preview("controls-keys-playstation.bmp", true);
            // A stick click has no key: a free key binds and no other key moves.
            const int keyThumb = settings::ControlsKeyboardFirst + map::LeftThumb;
            settings::controlsFocus = keyThumb;
            settings::pending = 0x1000; Tick(base);
            Require(settings::CaptureKey(44, false), "a keyboard capture takes Space"); Tick(base);
            Require(settings::controlsKeyboard[map::LeftThumb] == 44 && rows[keyThumb].value == L"Space" &&
                    settings::controlsKeyboard[map::A] == 27 && settings::snapshot.help.empty() == false &&
                    settings::snapshot.help.find(L"Swapped") == std::wstring::npos, "a free key binds a stick click");
            settings::pending = 0x1000; Tick(base);
            Require(settings::CaptureKey(map::EscapeKey, false), "Escape belongs to a keyboard capture"); Tick(base);
            Require(settings::captureRow < 0 && settings::controlsKeyboard[map::LeftThumb] == 44 &&
                    settings::snapshot.help == L"Cancelled.", "Escape cancels a keyboard capture");
            settings::pending = 0x1000; Tick(base);
            settings::menuClockOffset += std::chrono::seconds(6); Tick(base);
            Require(settings::captureRow < 0 && settings::controlsKeyboard[map::LeftThumb] == 44 &&
                    settings::snapshot.help == L"No key pressed. Nothing changed.", "a keyboard capture times out too");
            settings::menuClockOffset = {};

            // Navigation steps over the section titles; Start goes to Save.
            settings::controlsFocus = settings::ControlsKeyboardFirst;
            settings::pending = 1; Tick(base);
            Require(settings::controlsFocus == settings::ControlsControllerFirst + map::DpadRight, "up skips the Keyboard title");
            settings::pending = 2; Tick(base);
            Require(settings::controlsFocus == settings::ControlsKeyboardFirst, "down skips it too");
            settings::controlsFocus = settings::ControlsSaveRow;
            settings::pending = 2; Tick(base);
            Require(settings::controlsFocus == settings::ControlsPromptRow && settings::snapshot.scroll == 0, "down from Save wraps to the top");
            settings::pending = 1; Tick(base);
            Require(settings::controlsFocus == settings::ControlsSaveRow &&
                    settings::snapshot.scroll == settings::ControlsRowCount - settings::kMenuVisibleRows, "the page scrolls to Save");
            preview("controls-page-end.bmp");
            // Pointer: a title is not a row; a click on a layout row starts its capture.
            settings::controlsFocus = settings::ControlsControllerFirst + map::DpadRight;
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.scroll == settings::controlsFocus, "the focused row tops the list");
            settings::PointerClick(500, rowY(settings::ControlsKeyboardTitle), false); Tick(base);
            Require(settings::controlsFocus == settings::ControlsControllerFirst + map::DpadRight && settings::captureRow < 0,
                    "a click on a section title does nothing");
            settings::PointerClick(700, rowY(keyB), false); Tick(base);
            Require(settings::controlsFocus == keyB && settings::captureRow == keyB && settings::captureMode == 2,
                    "a click on a key row starts its capture");
            settings::PointerClick(700, rowY(keyA), false); Tick(base);
            Require(settings::controlsFocus == keyB && settings::captureRow == keyB, "a click during a capture changes nothing");
            settings::CaptureKey(map::EscapeKey, false); Tick(base);

            // Leaving with changes asks first; Cancel is the default.
            settings::pending = 0x2000; Tick(base);
            Require(settings::controlsDiscardPrompt && settings::snapshot.dialogTitle == L"Unsaved changes" &&
                    settings::snapshot.dialogMessage == L"Leave without saving the controls?" &&
                    settings::snapshot.dialogChoices == std::vector<std::wstring>{L"Discard changes", L"Cancel"} &&
                    settings::snapshot.dialogSelection == 1, "B with unsaved changes asks before leaving");
            settings::pending = 0x1000; Tick(base);
            Require(!settings::controlsDiscardPrompt && settings::controlsOpen && settings::controlsController[map::A] == 1,
                    "Cancel keeps the page and the changes");
            Require(settings::CalibrationKey(27), "Escape belongs to the Controls page"); Tick(base);
            Require(settings::controlsDiscardPrompt, "Escape asks as well");
            settings::pending = 1; Tick(base);
            settings::pending = 0x1000; Tick(base);
            Require(!settings::controlsOpen && settings::tab == 0 && settings::row == settings::GameControlsRow &&
                    diskConfig.controllerMap == map::DefaultController && inputMapApplies == beforeApplies,
                    "Discard leaves without saving");
            settings::pending = 0x1000; Tick(base);
            Require(settings::controlsOpen && settings::controlsController == map::DefaultController &&
                    settings::snapshot.rows[padA].value == L"A", "reopened, the page shows the saved layout");

            // Save: Start goes to Save, a second Start saves and applies at once.
            settings::controlsFocus = padA;
            settings::pending = 0x1000; Tick(base); Tick(base);
            heldSources = 1u << 1; Tick(base);
            heldSources = 0;
            settings::controlsFocus = keyA;
            settings::pending = 0x1000; Tick(base);
            settings::CaptureKey(44, false); Tick(base);
            const unsigned beforeSaves = saves;
            settings::pending = 0x10; Tick(base);
            Require(settings::controlsFocus == settings::ControlsSaveRow && saves == beforeSaves, "Start moves to Save without saving");
            settings::pending = 0x10; Tick(base);
            Require(saves == beforeSaves + 1 && diskConfig.controllerMap[map::A] == 1 && diskConfig.controllerMap[map::B] == 0 &&
                    diskConfig.keyboardMap[map::A] == 44 && appliedController == diskConfig.controllerMap &&
                    appliedKeyboard == diskConfig.keyboardMap && inputMapApplies == beforeApplies + 1 &&
                    settings::edit.controllerMap == diskConfig.controllerMap && settings::snapshot.help == L"Controls saved.",
                    "Save writes both layouts and applies them at once");
            Require(diskConfig.width != settings::edit.width, "Save leaves unsaved Graphics edits unsaved");
            settings::pending = 0x2000; Tick(base);
            Require(!settings::controlsOpen && !settings::controlsDiscardPrompt, "after Save, B leaves without asking");
            // Restore puts the defaults back, unsaved until Save.
            settings::pending = 0x1000; Tick(base);
            settings::controlsFocus = settings::ControlsRestoreRow;
            settings::pending = 0x1000; Tick(base);
            Require(settings::controlsController == map::DefaultController && settings::controlsKeyboard == map::DefaultKeyboard &&
                    diskConfig.controllerMap[map::A] == 1 && settings::snapshot.help == L"Default buttons restored. Save to keep them.",
                    "Restore puts the default layouts back without saving");
            settings::pending = 0x1000 | 2; Tick(base);
            Require(settings::controlsFocus == settings::ControlsSaveRow && diskConfig.controllerMap == map::DefaultController &&
                    diskConfig.keyboardMap == map::DefaultKeyboard && appliedController == map::DefaultController,
                    "A on Save saves the defaults");

            // The page runs on the default layout: with A and B swapped, the
            // button labelled A still confirms here, and the swap applies again
            // after the page closes and every button is up.
            {
                settings::pending = 0;
                uint16_t buttons = 0;
                settings::FilterInput(buttons, 0, 0, 0);
                buttons = 0;
                settings::FilterInput(buttons, 0, 0, 0);
                settings::pending = 0;
                buttons = 0x2000; // the guest's B from the button labelled A
                Require(settings::FilterInput(buttons, 0, 0, 0x1000) && buttons == 0, "the menu takes the input");
                Require(settings::pending == 0x1000, "the Controls page sees the default A");
                settings::pending = 0;
                buttons = 0;
                settings::FilterInput(buttons, 0, 0, 0);
                settings::controlsOpen = false;
                buttons = 0x2000;
                settings::FilterInput(buttons, 0, 0, 0x1000);
                Require(settings::pending == 0, "leaving the page waits for a release");
                buttons = 0;
                settings::FilterInput(buttons, 0, 0, 0);
                buttons = 0x2000;
                settings::FilterInput(buttons, 0, 0, 0x1000);
                Require(settings::pending == 0x2000, "the rest of the menu uses the layout");
                settings::pending = 0;
                buttons = 0;
                settings::FilterInput(buttons, 0, 0, 0);
                settings::controlsOpen = true;
                settings::FilterInput(buttons, 0, 0, 0);
                settings::pending = 0;
            }
            // Translations.
            for (const auto& [language, title, controller] : {std::tuple{1u, L"按鍵設定", L"控制器"}, std::tuple{2u, L"操作設定", L"コントローラー"},
                                                              std::tuple{3u, L"조작 설정", L"컨트롤러"}, std::tuple{4u, L"按键设置", L"手柄"}})
            {
                settings::edit.uiLanguage = language;
                settings::pending = 0; Tick(base);
                Require(settings::snapshot.pageTitle == title && settings::snapshot.rows[settings::ControlsControllerTitle].name == controller,
                        "translated Controls page");
            }
            settings::edit.uiLanguage = 0;
            settings::pending = 0x2000; Tick(base);
            Require(!settings::controlsOpen, "B leaves the Controls page");
            settings::edit = currentConfig;
            PPC_STORE_U32(ConfigData + 4, retailFlags);
            std::puts("PASS Controls page: capture, swap, trigger, Guide, Escape, timeout, keyboard capture, titles, pointer, discard prompt, Save, Restore, default-layout navigation, translations");
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
            Require(settings::row == settings::SystemModsRow && settings::snapshot.rows[settings::SystemModsRow].name == L"Mods" &&
                    settings::snapshot.rows[settings::SystemModsRow].value == L"Open" &&
                    settings::graphics_menu::IsAction(3, settings::SystemModsRow),
                    "the Mods action follows the TAA collection row on System");
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::SystemImportRow &&
                    settings::snapshot.rows.size() == size_t(settings::SystemRowCount) &&
                    settings::snapshot.rows[settings::SystemImportRow].name == L"Import discs & DLC" &&
                    settings::snapshot.rows[settings::SystemImportRow].value == L"Open" &&
                    settings::snapshot.rows[settings::SystemSaveRow].name == L"Save settings" &&
                    settings::graphics_menu::IsAction(3, settings::SystemImportRow),
                    "gamepad reaches the importer action on System, before Shader bundle and Save settings");
            settings::pending = 8; Tick(base);
            Require(!settings::importPrompt && !settings::restart::Requested(), "right arrow cannot launch importer");
            settings::PointerClick(500, float(150 + (settings::SystemImportRow - settings::snapshot.scroll) *
                                               settings::MenuRowHeight(3) + 20), false);
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
        // System → Add to Steam uses the shared service and keeps its result
        // visible until another action; this fixture never touches Steam files.
        {
            settings::tab = 3;
            settings::row = settings::SystemImportRow;
            settings::edit.uiLanguage = 0;
            settings::status.clear();
            const auto beforeAdds = steam_shortcut::menuFlowAdds;
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::SystemAddToSteamRow &&
                    settings::snapshot.rows[settings::SystemAddToSteamRow].name == L"Add to Steam" &&
                    settings::snapshot.rows[settings::SystemAddToSteamRow].value == L"Add" &&
                    !settings::snapshot.rows[settings::SystemAddToSteamRow].hidden &&
                    settings::graphics_menu::IsAction(3, settings::SystemAddToSteamRow) &&
                    settings::snapshot.help.find(L"Close Steam first.") != std::wstring::npos,
                    "Steam action follows Import discs & DLC and explains closing Steam");
            settings::pending = 8; Tick(base);
            Require(steam_shortcut::menuFlowAdds == beforeAdds, "right arrow cannot add to Steam");
            for (const auto& [result, message] : {
                     std::pair{steam_shortcut::Status::Added, L"Added to Steam. Restart Steam to see it."},
                     std::pair{steam_shortcut::Status::AddedFromFlatpak, L"Restart Steam to see it; if Steam was open, close it and add again."},
                     std::pair{steam_shortcut::Status::SteamRunning, L"Close Steam, then try again."},
                     std::pair{steam_shortcut::Status::SteamNotFound, L"Steam was not found. Install and open Steam first."},
                     std::pair{steam_shortcut::Status::UserNotFound, L"No Steam user found. Sign in to Steam, close it, then try again."},
                     std::pair{steam_shortcut::Status::Error, L"Could not add to Steam. Check the log for details."}})
            {
                steam_shortcut::menuFlowStatus = result;
                const auto calls = steam_shortcut::menuFlowAdds;
                settings::pending = 0x1000; Tick(base);
                Require(steam_shortcut::menuFlowAdds == calls + 1 && settings::snapshot.help == message,
                        "confirm runs one Steam action and shows its result");
                settings::pending = 0; Tick(base);
                Require(settings::snapshot.help == message, "Steam result stays visible on an idle tick");
                for (uint32_t language = 1; language < 5; ++language)
                {
                    settings::edit.uiLanguage = language;
                    Require(std::wstring(settings::SteamShortcutMessage(result)) != message,
                            "Steam result is translated in every menu language");
                }
                settings::edit.uiLanguage = 0;
            }
            for (const auto& [language, name] : {
                     std::pair{1u, L"加入 Steam"}, std::pair{2u, L"Steam に追加"},
                     std::pair{3u, L"Steam에 추가"}, std::pair{4u, L"添加到 Steam"}})
            {
                settings::edit.uiLanguage = language;
                settings::status.clear();
                settings::pending = 0; Tick(base);
                Require(settings::snapshot.rows[settings::SystemAddToSteamRow].name == name &&
                        settings::snapshot.help != L"Adds the game to your Steam library with artwork. Close Steam first. Add it again if you move the game.",
                        "Steam action and help are translated");
            }
            settings::edit.uiLanguage = 0;
            settings::status.clear();
            std::puts("PASS Add to Steam action: shared service dispatch, messages, idle persistence and all menu languages");
        }
        // System → Shader bundle: cancel-first dialog, then one redownload request.
        {
            settings::tab = 3;
            settings::row = settings::SystemAddToSteamRow;
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::SystemShaderBundleRow &&
                    settings::snapshot.rows[settings::SystemShaderBundleRow].name == L"Shader bundle" &&
                    settings::snapshot.rows[settings::SystemShaderBundleRow].value == L"Download again" &&
                    settings::graphics_menu::IsAction(3, settings::SystemShaderBundleRow),
                    "the Shader bundle action follows Add to Steam on System");
            settings::pending = 0x1000; Tick(base);
            Require(settings::shaderBundlePrompt && settings::snapshot.dialogSelection == 1 &&
                    settings::snapshot.dialogChoices == std::vector<std::wstring>{L"Download again", L"Cancel"},
                    "confirm opens a cancel-first dialog");
            settings::pending = 0x1000; Tick(base);
            Require(!settings::shaderBundlePrompt && updater::shader_pack::menuFlowRedownloads == 0 &&
                    !settings::restart::Requested(), "confirm on the default Cancel requests nothing");
            settings::pending = 0x1000; Tick(base);
            settings::pending = 1; Tick(base);
            settings::pending = 0x1000; Tick(base);
            Require(!settings::shaderBundlePrompt && updater::shader_pack::menuFlowRedownloads == 1,
                    "Download again records one redownload request");
#ifdef _WIN32
            Require(settings::restart::Requested() && !settings::restart::InstallRequested() &&
                    settings::snapshot.help == L"Restarting to download the shader bundle…",
                    "Windows restarts the game for the download");
            settings::restart::Cancel();
#else
            Require(!settings::restart::Requested() &&
                    settings::snapshot.help == L"Restart the game to download the shader bundle again.",
                    "other platforms ask for a manual restart");
#endif
            settings::pending = 0; Tick(base);
            std::puts("PASS shader bundle action: cancel-first dialog, one redownload request, restart on Windows");
        }
        // System → Prepare shaders again writes a one-shot request in an isolated directory.
        {
            const auto previousDirectory = std::filesystem::current_path();
            const auto sandbox = std::filesystem::temp_directory_path() /
                ("lo-menu-shader-preparation-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(sandbox);
            std::filesystem::current_path(sandbox);
            const auto marker = std::filesystem::absolute(gpu::shader_preparation::RequestPath());
            settings::tab = 3;
            settings::row = settings::SystemShaderBundleRow;
            settings::edit.uiLanguage = 0;
            settings::status.clear();
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::SystemShaderPreparationRow &&
                    settings::snapshot.rows[settings::SystemShaderPreparationRow].name == L"Prepare shaders again" &&
                    settings::snapshot.rows[settings::SystemSaveRow].name == L"Save settings" &&
                    settings::snapshot.rows.size() == size_t(settings::SystemRowCount) &&
                    settings::graphics_menu::IsAction(3, settings::SystemShaderPreparationRow),
                    "Prepare shaders again follows Shader bundle and precedes Save");
            settings::pending = 0x1000; Tick(base);
            Require(settings::shaderPreparationPrompt && settings::snapshot.dialogSelection == 1 &&
                    settings::snapshot.dialogChoices == std::vector<std::wstring>{L"Prepare again", L"Cancel"},
                    "shader preparation confirmation defaults to Cancel");
            settings::pending = 0x1000; Tick(base);
            Require(!settings::shaderPreparationPrompt && !gpu::shader_preparation::PendingNextStart(marker) &&
                    !settings::restart::Requested(), "default Cancel does not request shader preparation or restart");
            settings::pending = 0x1000; Tick(base);
            settings::pending = 0x2000; Tick(base);
            Require(!settings::shaderPreparationPrompt && !gpu::shader_preparation::PendingNextStart(marker),
                    "Back cancels shader preparation on the virtual pad");
            for (const auto& [language, name] : {
                     std::pair{1u, L"重新準備著色器"}, std::pair{2u, L"シェーダーを再準備"},
                     std::pair{3u, L"셰이더 다시 준비"}, std::pair{4u, L"重新准备着色器"}})
            {
                settings::edit.uiLanguage = language;
                settings::pending = 0; Tick(base);
                Require(settings::snapshot.rows[settings::SystemShaderPreparationRow].name == name,
                        "translated shader preparation action");
            }
            settings::edit.uiLanguage = 0;
            settings::pending = 0x1000; Tick(base);
            settings::pending = 1; Tick(base);
            settings::pending = 0x1000; Tick(base);
            Require(!settings::shaderPreparationPrompt && gpu::shader_preparation::PendingNextStart(marker) &&
                    settings::snapshot.help == L"Full shader preparation will run at the next start." &&
                    !settings::restart::Requested(),
                    "confirmation persists one next-start request without restarting");
            std::filesystem::current_path(previousDirectory);
            std::filesystem::remove(marker);
            std::filesystem::remove(sandbox);
            settings::pending = 0; Tick(base);
            std::puts("PASS shader preparation action: System placement, translations, cancel-first dialog, persistent request, no restart");
        }
        // System → Mods over a real mods folder and mod-list.ini through the mod API.
        {
            const auto root = std::filesystem::temp_directory_path() / "lo-menu-flow-mods";
            std::filesystem::remove_all(root);
            const auto write = [&](const std::filesystem::path& file, const std::string& text) {
                std::filesystem::create_directories(file.parent_path());
                std::ofstream(file, std::ios::binary) << text;
            };
            write(root / "mods/alpha/mod.ini",
                  "api_version=2\nid=alpha\nname=Alpha Pack\nversion=1.0\nauthor=Ann\ndescription=First test mod.\npriority=10\n");
            write(root / "mods/alpha/overlay/textures/fp-0123456789abcdef.lotex2", "x");
            write(root / "mods/beta/mod.ini", "api_version=2\nid=beta\nname=Beta\npriority=5\n");
            write(root / "mods/gamma/mod.ini", "api_version=2\nid=gamma\nname=Gamma\nenabled=false\npriority=1\n");
            write(root / "mods/broken/mod.ini", "api_version=9\n");
            write(root / "mods/pt-br/language.ini", "id=pt-br\nname=Brasil\nbase=int\n");
            const auto list = root / "mod-list.ini";
            modding::Initialize(root / "mods", list);
            const auto readList = [&] {
                std::ifstream file(list, std::ios::binary);
                std::string text, line, lines;
                while (std::getline(file, line))
                    if (!line.empty() && line[0] != '#') lines += line + "\n";
                return lines;
            };
            const auto rowY = [](int row) { return float(150 + (row - settings::snapshot.scroll) * 43 + 20); };
            PPC_STORE_U32(Menu + 4, 4);
            settings::edit.uiLanguage = 0;
            settings::tab = 3;
            settings::row = settings::SystemModsRow;
            settings::pending = 0x1000; Tick(base);
            const int open = settings::mods_page::CanOpenFolder ? 5 : -1, save = open >= 0 ? 6 : 5;
            {
                const auto& rows = settings::snapshot.rows;
                Require(settings::modsOpen && settings::snapshot.pageTitle == L"Mods" && settings::snapshot.row == 0 &&
                        rows.size() == size_t(save + 1) && !settings::snapshot.keyLegend.empty(),
                        "A on System Mods opens the Mods page");
                Require(rows[0].name == L"Alpha Pack" && rows[0].detail == L"1.0 - Ann" &&
                        rows[0].choices == std::vector<std::wstring>{L"On", L"Off"} && rows[0].selectedChoice == 0 &&
                        rows[1].name == L"Beta" && rows[1].detail == L"beta" &&
                        rows[2].name == L"Gamma" && !rows[2].enabled && rows[2].value == L"mod.ini: Off" && rows[2].detail == L"! gamma" &&
                        rows[3].name == L"broken" && !rows[3].enabled && rows[3].value == L"Error" && rows[3].detail == L"! —" &&
                        rows[4].name == L"Brasil" && rows[4].detail == L"Language pack: English" && rows[4].choices.empty() &&
                        (open < 0 || rows[size_t(open)].name == L"Open mods folder") && rows[size_t(save)].name == L"Save mod list",
                        "mods in effective order with version and author, a mod off in mod.ini, a rejected mod, the language pack, the actions");
                Require(settings::snapshot.help == L"First test mod." &&
                        settings::snapshot.notice == L"Folder: alpha     Overlay files: 1" && settings::snapshot.reorder,
                        "help shows the description, the folder and the file count");
            }
            settings::pending = 2; Tick(base);
            Require(settings::snapshot.help == L"No description." && settings::snapshot.notice == L"Folder: beta",
                    "a mod without description");
            settings::pending = 0x1000; Tick(base);
            Require(settings::snapshot.rows[1].selectedChoice == 1, "A turns a mod off");
            settings::pending = 0x100; Tick(base);
            Require(settings::modsFocus == 0 && settings::snapshot.rows[0].name == L"Beta" &&
                    settings::snapshot.rows[1].name == L"Alpha Pack", "LB moves the focused mod up and focus follows it");
            settings::pending = 0x100; Tick(base);
            Require(settings::snapshot.rows[0].name == L"Beta", "the first mod stays first");
            settings::pending = 2; Tick(base);
            settings::pending = 2; Tick(base);
            Require(settings::snapshot.help == L"Its mod.ini turns it off (enabled=false)." && settings::snapshot.reorder,
                    "a mod off in mod.ini says so and can still move");
            settings::pending = 0x1000; Tick(base);
            Require(settings::snapshot.rows[2].value == L"mod.ini: Off", "its switch does nothing");
            settings::pending = 2; Tick(base);
            Require(settings::snapshot.help == L"Not loaded: api_version must be 1 or 2" && !settings::snapshot.reorder,
                    "a rejected mod shows its first problem and cannot move");
            settings::pending = 0x1000; Tick(base);
            settings::pending = 0x200; Tick(base);
            Require(settings::snapshot.rows[3].name == L"broken" && settings::snapshot.rows[3].value == L"Error",
                    "A and RB leave a rejected mod alone");
            settings::pending = 2; Tick(base);
            Require(settings::snapshot.help == L"This is a language pack. Choose it in System → Game language." &&
                    settings::snapshot.notice == L"Folder: pt-br" && !settings::snapshot.reorder, "a language pack is read-only");
            // Pointer: select Alpha, then its move up cell.
            settings::PointerClick(200, rowY(1), false); Tick(base);
            Require(settings::modsFocus == 1 && settings::snapshot.reorder, "pointer selects a mod");
            settings::PointerClick(float(settings::ModsMoveLeft + 10), rowY(1), false); Tick(base);
            Require(settings::modsFocus == 0 && settings::snapshot.rows[0].name == L"Alpha Pack", "the move up cell moves the mod");
            settings::pending = 0x200; Tick(base);
            Require(settings::modsFocus == 1 && settings::snapshot.rows[1].name == L"Alpha Pack", "RB moves it back down");
            settings::PointerClick(700, rowY(0), false); Tick(base);
            Require(settings::modsFocus == 0 && settings::snapshot.rows[0].selectedChoice == 0, "a click on the switch turns Beta on");
            settings::pending = 8; Tick(base);
            Require(settings::snapshot.rows[0].selectedChoice == 1, "right turns it off again");
            // Leaving with changes asks first; Cancel is the default.
            settings::pending = 0x2000; Tick(base);
            Require(settings::modsDiscardPrompt && settings::snapshot.dialogSelection == 1 &&
                    settings::snapshot.dialogChoices == std::vector<std::wstring>{L"Discard changes", L"Cancel"},
                    "B with unsaved changes asks before leaving");
            settings::pending = 0x1000; Tick(base);
            Require(!settings::modsDiscardPrompt && settings::modsOpen, "Cancel keeps the page");
            if (open >= 0)
            {
                settings::modsFocus = open;
                settings::pending = 0x1000; Tick(base);
                Require(modsFolderOpens == 1 && modsFolderOpened == modding::Root() &&
                        settings::snapshot.notice == settings::PathText(modding::Root()), "Open mods folder opens the mods root");
            }
            // Start goes to Save, and saves when pressed there.
            settings::modsFocus = 0;
            settings::pending = 0x10; Tick(base);
            Require(settings::modsFocus == save && !std::filesystem::exists(list), "Start moves to Save without saving");
            settings::pending = 0x10; Tick(base);
            Require(readList() == "beta=off\nalpha=on\ngamma=on\n", "Save writes the order and switches to mod-list.ini");
            // A changed list applies while the game runs: no restart, one line says when models follow.
            Require(modsReloadRequests == 1 && !settings::restartPrompt && !settings::restartForMods && settings::modsOpen &&
                    settings::snapshot.help == L"Models update in the next area; party members after a restart." && settings::snapshot.notice.empty(),
                    "a changed mod list reloads in game and says when models follow");
            modsReloadStatus = {mod_hot_reload::Phase::Preparing, 12, 393};
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.help == L"Models update in the next area; party members after a restart." &&
                    settings::snapshot.notice == L"Preparing mods  12 / 393", "the count of packages prepared so far");
            modsReloadStatus = {mod_hot_reload::Phase::Done};
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.help == L"Saves the order and switches. Textures change at once, models in the next area.",
                    "the row help returns once the new models are in use");
            settings::pending = 0x2000; Tick(base);
            Require(!settings::modsOpen && settings::tab == 3 && settings::row == settings::SystemModsRow,
                    "B without changes returns to the System tab");
            // Reopened, the page lists the mods as the reload read them: the saved order.
            settings::pending = 0x1000; Tick(base);
            Require(settings::snapshot.rows[0].name == L"Beta" && settings::snapshot.rows[0].selectedChoice == 1,
                    "the reopened page shows the saved order");
            // Saving the list in use changes nothing.
            settings::pending = 0x10; Tick(base);
            settings::pending = 0x10; Tick(base);
            Require(readList() == "beta=off\nalpha=on\ngamma=on\n" && modsReloadRequests == 1 && !settings::restartPrompt &&
                    settings::snapshot.help == L"Mod list saved.", "saving the running order needs no reload");
            // Without a live reload (mods off for this run) a change asks for a restart.
            modsReloadAvailable = false;
            settings::modsFocus = 0;
            settings::pending = 0x200; Tick(base);
            settings::pending = 0x10; Tick(base);
            settings::pending = 0x10; Tick(base);
            Require(readList() == "alpha=on\nbeta=off\ngamma=on\n" && modsReloadRequests == 2, "Save writes the moved order");
#ifdef _WIN32
            Require(settings::restartPrompt && settings::savedRestartPrompt && settings::restartForMods &&
                    settings::snapshot.dialogMessage == L"Mod list saved. Restart now?" &&
                    settings::snapshot.dialogChoices == std::vector<std::wstring>{L"Restart now", L"Later"},
                    "a change without a live reload offers the restart");
            settings::pending = 2; Tick(base);
            settings::pending = 0x1000; Tick(base);
            Require(!settings::restartPrompt && !settings::restartForMods && !settings::restart::Requested() &&
                    settings::modsOpen && settings::snapshot.help == L"Saved. Changes take effect after restarting.",
                    "Later stays on the page");
#else
            Require(settings::restartPrompt && settings::snapshot.dialogMessage == L"Mod list saved. Restart the game to apply the changes.",
                    "a change without a live reload asks for a manual restart");
            settings::pending = 0x1000; Tick(base);
#endif
            modsReloadAvailable = true;
            modsReloadStatus = {};
            // Escape leaves too.
            Require(settings::CalibrationKey(27), "Escape belongs to the Mods page");
            settings::pending = 0; Tick(base);
            Require(!settings::modsOpen, "Escape leaves the Mods page");
            for (const auto& [language, title] : {std::pair{1u, L"Mod"}, std::pair{2u, L"MOD"}, std::pair{3u, L"모드"}, std::pair{4u, L"Mod"}})
            {
                settings::edit.uiLanguage = language;
                settings::pending = 0; Tick(base);
                Require(settings::snapshot.rows[settings::SystemModsRow].name == title, "translated Mods row");
            }
            settings::edit.uiLanguage = 0;
            settings::pending = 0x1000; Tick(base);
            auto preview = settings::snapshot;
            preview.assets.reset();
            std::vector<uint32_t> pixels;
            Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "Mods page preview");
            const auto evidence = std::filesystem::current_path() / "out" / "mods-page-preview";
            std::filesystem::create_directories(evidence);
            WriteBmp(evidence / "mods-page.bmp", pixels);
            settings::pending = 0x2000; Tick(base);
            // No mods: one line says where they go.
            modding::Initialize(root / "empty", root / "empty-list.ini");
            settings::pending = 0x1000; Tick(base);
            Require(settings::snapshot.rows.size() == size_t(open >= 0 ? 2 : 1) && settings::snapshot.rows[0].name == L"No mods installed" &&
                    settings::snapshot.help == L"Put each mod in its own folder inside the mods folder, then restart the game." &&
                    !settings::snapshot.reorder, "the empty page says where mods go");
            settings::pending = 0x10; Tick(base);
            settings::pending = 0x2000; Tick(base);
            Require(!settings::modsOpen, "the empty page has no Save and B leaves");
            modding::Shutdown();
            std::filesystem::remove_all(root);
            std::puts("PASS Mods page: listing, switches, LB/RB and pointer moves, read-only rows, discard prompt, Open folder, Save + live reload, restart without it, saved order, empty page");
        }
        // Opened from the title menu (title_entry.cpp) no game is loaded: the
        // retail Gameplay and Audio options, Restore, Quit and the retail
        // calibration are out of reach, focus lands on visible rows and nothing
        // reaches the guest config.
        {
            settings::edit.uiLanguage = 0;
            settings::status.clear();
            settings::tab = 0;
            settings::row = 0;
            settings::MarkTitleEntry();
            const uint32_t textSpeed = PPC_LOAD_U32(ConfigData), flags = PPC_LOAD_U32(ConfigData + 4);
            const unsigned beforeApplies = applies, beforeDefaults = defaultsCalls, beforeRequests = mainMenuRequests;
            settings::pending = 0; Tick(base);
            Require(settings::row == settings::GameControlsRow, "title mode focuses the first visible Gameplay row");
            for (int r = 0; r < settings::GameRowCount; ++r)
                Require(settings::snapshot.rows[size_t(r)].hidden == (r != settings::GameControlsRow),
                        "title mode hides the retail options, Restore and Quit");
            settings::pending = 1; Tick(base);
            Require(settings::row == settings::GameControlsRow, "Up wraps past the hidden rows");
            settings::pending = 2; Tick(base);
            Require(settings::row == settings::GameControlsRow, "Down wraps past the hidden rows");
            settings::row = settings::GameRestoreRow;
            settings::PointerClick(100, 150 + 20, false); Tick(base);
            Require(settings::row == settings::GameControlsRow, "the first list slot is Controls");
            // A stale focus on a hidden row edits nothing and moves to a visible row.
            for (int r : {0, settings::GameRetailRowCount - 1, settings::GameRestoreRow, settings::GameMainMenuRow})
            {
                settings::row = r;
                settings::pending = 8; Tick(base);
                Require(settings::row == settings::GameControlsRow, "focus leaves a hidden row");
                settings::row = r;
                settings::pending = 0x1000; Tick(base);
            }
            Require(PPC_LOAD_U32(ConfigData) == textSpeed && PPC_LOAD_U32(ConfigData + 4) == flags &&
                    applies == beforeApplies && defaultsCalls == beforeDefaults &&
                    !settings::mainMenuPrompt && settings::snapshot.dialogChoices.empty(),
                    "hidden rows write nothing to the guest config");
            // Audio keeps Voice volume, Audio output and Rear angle, which are
            // in settings.ini: Voice language, Music and Sound effects live in
            // the save as well.
            const uint32_t voice = PPC_LOAD_U32(ConfigData + 24), music = PPC_LOAD_U32(ConfigData + 8),
                           effects = PPC_LOAD_U32(ConfigData + 12), brightness = PPC_LOAD_U32(ConfigData + 16),
                           contrast = PPC_LOAD_U32(ConfigData + 20);
            const uint32_t voiceVolume = settings::edit.voiceVolume;
            settings::pending = 0x200; Tick(base);
            Require(settings::tab == 1 && settings::row == settings::AudioVoiceVolumeRow, "Audio opens on Voice volume");
            for (int r = 0; r < settings::AudioRowCount; ++r)
                Require(settings::snapshot.rows[size_t(r)].hidden == (r < settings::AudioVoiceVolumeRow),
                        "title mode hides Voice language, Music and Sound effects");
            settings::pending = 1; Tick(base);
            Require(settings::row == settings::AudioRearAngleRow, "Up from Voice volume wraps past the hidden rows");
            for (int r : {settings::AudioVoiceRow, settings::AudioMusicRow, settings::AudioEffectsRow})
                for (uint16_t press : {uint16_t(4), uint16_t(8)})
                {
                    settings::row = r;
                    settings::pending = press; Tick(base);
                    Require(settings::row == settings::AudioVoiceVolumeRow, "focus leaves a hidden Audio row");
                }
            Require(PPC_LOAD_U32(ConfigData + 24) == voice && PPC_LOAD_U32(ConfigData + 8) == music &&
                    PPC_LOAD_U32(ConfigData + 12) == effects && applies == beforeApplies &&
                    settings::edit.voiceVolume == voiceVolume,
                    "hidden Audio rows write nothing to the guest config");
            // The brightness page stays, but Original pattern would hand input
            // to the retail list, which writes the save's brightness.
            settings::pending = 0x200; Tick(base);
            settings::row = int(GraphicsRow::Brightness);
            settings::pending = 0x1000; Tick(base);
            Require(settings::brightnessOpen && !settings::snapshot.brightness.originalPattern,
                    "title mode brightness page marks Original pattern unavailable");
            settings::brightnessFocus = 2;
            settings::pending = 8; Tick(base);
            Require(settings::brightnessFocus == 4, "Right steps over Original pattern");
            settings::pending = 4; Tick(base);
            Require(settings::brightnessFocus == 2, "Left steps over Original pattern");
            settings::pending = 2; Tick(base);
            Require(settings::brightnessFocus == 4, "Down steps over Original pattern");
            settings::brightnessClick = 3; Tick(base);
            Require(settings::brightnessOpen && settings::brightnessFocus == 4, "a click on Original pattern does nothing");
            settings::brightnessFocus = 3;
            settings::pending = 0x1000; Tick(base);
            Require(settings::brightnessOpen && settings::active && !settings::bypass && settings::cancelPolls == 0 &&
                    !settings::returnToBrightness && PPC_LOAD_U32(ConfigData + 16) == brightness &&
                    PPC_LOAD_U32(ConfigData + 20) == contrast,
                    "the retail calibration list is never reached");
            settings::pending = 0x2000; Tick(base);
            Require(!settings::brightnessOpen && settings::active, "B leaves the brightness page");
            settings::pending = 0x100; Tick(base);
            settings::pending = 0x100; Tick(base);
            Require(settings::tab == 0 && settings::row == settings::GameControlsRow, "back on Gameplay's first visible row");
            settings::pending = 0x200; Tick(base);
            settings::pending = 0x100; Tick(base);
            Require(settings::tab == 0 && settings::row == settings::GameControlsRow, "a tab switch lands on a visible row");
            settings::pending = 0x2000; Tick(base);
            Require(settings::closing && !settings::active && applies == beforeApplies + 1,
                    "Back closes with the usual apply");
            Tick(base); Tick(base);
            Poll(0, true); Poll(0, false);
            Require(PPC_LOAD_U32(Menu + 4) == 1 && !settings::titleEntry, "the idle task ends title mode");
            PPC_STORE_U32(Menu + 4, 4); Tick(base); Poll(0, true);
            Require(settings::active && settings::row == settings::GameControlsRow &&
                    std::none_of(settings::snapshot.rows.begin(), settings::snapshot.rows.end(),
                                 [](const settings::MenuRow& row) { return row.hidden; }) &&
                    mainMenuRequests == beforeRequests,
                    "a later open from a game shows every Gameplay row");
            std::puts("PASS title-menu Settings: hidden retail Gameplay and Audio rows, Restore, Quit and Original pattern, visible focus, guest config untouched");
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
        if (argc == 3)
        {
            // The DLSS row status over a menu frame, with installed fonts in
            // English and Simplified Chinese: after gameplay and before any scene.
            using gpu::frame_plan::DlssEffectPhase;
            using gpu::frame_plan::DlssEffectReason;
            using gpu::upscaling::DlssQuality;
            const auto previousEdit = settings::edit;
            const auto previousConfig = currentConfig;
            currentConfig.upscaler = gpu::upscaling::Upscaler::Dlss;
            currentConfig.dlssQuality = DlssQuality::Quality;
            currentConfig.graphicsBackend = settings::GraphicsBackend::Vulkan;
            settings::edit = currentConfig;
            settings::edit.frameGenerationProvider = framegen::Provider::Off;
            settings::edit.hdr = false;
            settings::status.clear();
            settings::tab = 2;
            settings::row = int(GraphicsRow::DlssQuality);
            gpu::frame_plan::DlssEffectSnapshot running{};
            running.hasPlan = true;
            running.plannedRequest = gpu::upscaling::Upscaler::Dlss;
            running.plannedQuality = DlssQuality::Quality;
            running.device.backend = gpu::backend::Backend::Vulkan;
            running.device.deviceReady = true;
            running.device.dlssAvailable = true;
            running.phase = DlssEffectPhase::TemporaryFallback;
            running.reason = DlssEffectReason::NoEligibleScene;
            auto menuFrame = SubmittedObservation(DlssQuality::Quality, 1707, 960, 2560, 1440);
            menuFrame.outcome = gpu::frame_plan::DlssExecutionOutcome::Fallback;
            menuFrame.submissionSerial = 0;
            menuFrame.reason = DlssEffectReason::NoEligibleScene;
            running.execution = menuFrame;
            gpu::frame_plan::menuFlowDlssEffect = running;
            for (const bool played : {true, false})
            {
                gpu::frame_plan::menuFlowEligible.reset();
                if (played) gpu::frame_plan::menuFlowEligible = SubmittedObservation(DlssQuality::Quality, 1707, 960, 2560, 1440);
                for (const uint32_t language : {0u, 4u})
                {
                    settings::edit.uiLanguage = language;
                    settings::Publish(base, ConfigData);
                    auto preview = settings::snapshot;
                    preview.assets = settings::menu_assets::Cached(argv[1], language);
                    Require(bool(preview.assets), "installed DLSS status assets");
                    const std::wstring expected = played
                        ? (language ? L"已提交 DLSS 质量输出。1707×960 - 2560×1440" : L"Submitted DLSS Quality output. 1707×960 - 2560×1440")
                        : (language ? L"进入游戏场景后才会开始缩放。" : L"Upscaling starts once the game scene is on screen.");
                    Require(preview.notice == expected, "translated DLSS status over a menu frame");
                    std::vector<uint32_t> pixels;
                    Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "translated DLSS status preview");
                    WriteBmp(std::filesystem::path(argv[2]) / (std::string("dlss-status-") + (language ? "zh-" : "en-") +
                        (played ? "last-game-frame.bmp" : "not-started.bmp")), pixels);
                }
            }
            gpu::frame_plan::menuFlowEligible.reset();
            gpu::frame_plan::menuFlowDlssEffect = {};
            currentConfig = previousConfig;
            settings::edit = previousEdit;
            std::puts("PASS DLSS status after gameplay and before any scene, English and Chinese, installed assets");
        }
        {
            const auto previousEdit = settings::edit;
            settings::tab = 2;
            settings::status.clear();
            settings::edit = currentConfig;
            settings::edit.graphicsBackend = settings::GraphicsBackend::D3D12;
            settings::edit.antialiasing = 0;
            settings::edit.upscaler = gpu::upscaling::Upscaler::Off;
            settings::edit.frameGenerationProvider = framegen::Provider::Off;
            settings::edit.hdr = false;
            settings::row = int(GraphicsRow::Hdr);
            settings::pending = 0; Tick(base);
            Require(settings::snapshot.rows[int(GraphicsRow::Hdr)].enabled &&
                    settings::snapshot.rows[int(GraphicsRow::HdrPaperWhite)].enabled &&
                    settings::snapshot.rows[int(GraphicsRow::HdrPeak)].enabled,
                    "D3D12 exposes HDR controls");
            Require(!settings::snapshot.rows[int(GraphicsRow::Hdr)].hidden &&
                    settings::snapshot.rows[int(GraphicsRow::HdrPaperWhite)].hidden &&
                    settings::snapshot.rows[int(GraphicsRow::HdrPeak)].hidden,
                    "HDR off hides paper white and peak brightness");
            settings::pending = 2; Tick(base);
            Require(settings::row == int(GraphicsRow::Brightness), "down from HDR off skips the hidden HDR levels");
            settings::pending = 1; Tick(base);
            Require(settings::row == int(GraphicsRow::Hdr), "up from Brightness skips the hidden HDR levels");
            settings::pending = 8; Tick(base);
            Require(settings::edit.hdr && settings::snapshot.rows[int(GraphicsRow::Hdr)].value == L"On",
                    "HDR toggle updates the pending preference");
            Require(!settings::snapshot.rows[int(GraphicsRow::HdrPaperWhite)].hidden &&
                    !settings::snapshot.rows[int(GraphicsRow::HdrPeak)].hidden,
                    "turning HDR on shows its levels before saving");
            settings::pending = 2; Tick(base);
            Require(settings::row == int(GraphicsRow::HdrPaperWhite), "down from HDR on reaches paper white");
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
            // HDR calibration: its own entry row under HDR peak opens the same page.
            {
                const int entry = int(GraphicsRow::HdrCalibration);
                settings::row = int(GraphicsRow::HdrPeak);
                settings::pending = 2; Tick(base);
                Require(settings::row == entry && settings::snapshot.rows[size_t(entry)].pageEntry &&
                        settings::snapshot.rows[size_t(entry)].enabled, "down from HDR peak reaches HDR calibration");
                settings::pending = 0x1000; Tick(base);
                Require(settings::snapshot.calibration.open, "HDR calibration opens the calibration page");
                Require(settings::CalibrationKey(27), "Escape closes calibration opened from its row");
                settings::pending = 0; Tick(base);
                Require(!settings::snapshot.calibration.open && settings::row == entry, "Escape returns to HDR calibration");
                settings::pending = 2; Tick(base);
                Require(settings::row == int(GraphicsRow::Brightness) &&
                        settings::snapshot.rows[int(GraphicsRow::Brightness)].pageEntry, "Brightness follows HDR calibration");
                // HDR and DLSS on is the longest Graphics list; Save at its end stays in view.
                settings::edit.upscaler = gpu::upscaling::Upscaler::Dlss;
                settings::row = int(GraphicsRow::Save);
                settings::pending = 0; Tick(base);
                int visible = 0, savedAt = 0;
                for (int i = 0; i < int(GraphicsRow::Count); ++i)
                {
                    if (settings::snapshot.rows[size_t(i)].hidden) continue;
                    if (i == int(GraphicsRow::Save)) savedAt = visible;
                    ++visible;
                }
                Require(!settings::snapshot.rows[int(GraphicsRow::NeuralRenderingTuning)].hidden &&
                        !settings::snapshot.rows[size_t(entry)].hidden && savedAt - settings::snapshot.scroll < settings::kMenuVisibleRows &&
                        settings::snapshot.scroll + settings::kMenuVisibleRows == visible,
                        "with HDR and DLSS on the list scrolls to Save with both entry rows");
                settings::edit.upscaler = gpu::upscaling::Upscaler::Off;
                settings::edit.hdr = false;
                settings::row = int(GraphicsRow::Hdr);
                settings::pending = 0; Tick(base);
                Require(settings::snapshot.rows[size_t(entry)].hidden, "HDR off hides HDR calibration");
                settings::edit.hdr = true;
                settings::pending = 0; Tick(base);
            }
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
            Require(settings::snapshot.rows[int(GraphicsRow::HdrPaperWhite)].hidden &&
                    settings::snapshot.rows[int(GraphicsRow::HdrPeak)].hidden, "turning HDR off hides its levels");
            // Reopening drops an unsaved HDR choice; focus leaves the now hidden peak row.
            {
                const auto priorCurrent = currentConfig;
                currentConfig.hdr = false;
                settings::edit.hdr = true;
                settings::row = int(GraphicsRow::HdrPeak);
                settings::pending = 0; Tick(base);
                Require(settings::row == int(GraphicsRow::HdrPeak) && !settings::snapshot.rows[int(GraphicsRow::HdrPeak)].hidden,
                        "unsaved HDR on keeps focus on peak brightness");
                settings::active = false;
                PPC_STORE_U32(Menu + 4, 4);
                settings::pending = 0; Tick(base);
                Require(settings::active && !settings::edit.hdr && settings::row == int(GraphicsRow::Brightness),
                        "reopening with HDR off moves focus from peak brightness to Brightness");
                currentConfig = priorCurrent;
            }
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
            writeIni("");
            Require(settings::Read().discordStatus, "Discord status defaults On for existing profiles");
            for (const auto* contents : {"discord_status=2\n", "discord_status=-1\n", "discord_status=invalid\n"})
            {
                writeIni(contents);
                Require(settings::Read().discordStatus, "invalid Discord preference keeps On default");
            }
            for (const bool enabled : {false, true})
            {
                writeIni(enabled ? "discord_status=1\n" : "discord_status=0\n");
                auto discord = settings::Read();
                Require(discord.discordStatus == enabled && settings::SaveConfig(discord) &&
                        settings::GetConfig().discordStatus == enabled && settings::Read().discordStatus == enabled,
                        "Discord preference reads, saves and reloads both modes");
            }
            std::puts("PASS Discord preference default, INI validation and On/Off roundtrip");
            writeIni("");
            Require(settings::Config{}.debugLog && settings::Read().debugLog,
                    "debug log defaults on in new settings");
            for (const auto* contents : {"debug_log=0\n", "debug_log=1\n", "diagnostic_log=2\n",
                                         "diagnostic_log=-1\n", "diagnostic_log=invalid\n"})
            {
                writeIni(contents);
                Require(settings::Read().debugLog, "legacy and invalid keys keep the enabled diagnostic default");
            }
            for (const bool enabled : {false, true})
            {
                writeIni(enabled ? "diagnostic_log=1\ndebug_log=0\n" : "diagnostic_log=0\ndebug_log=1\n");
                auto diagnostic = settings::Read();
                Require(diagnostic.debugLog == enabled, "diagnostic_log wins over the legacy key");
                Require(settings::SaveConfig(diagnostic) && settings::GetConfig().debugLog == enabled &&
                        settings::Read().debugLog == enabled, "diagnostic preference saves and reloads both modes");
                std::ifstream diagnosticIni("settings.ini");
                const std::string text((std::istreambuf_iterator<char>(diagnosticIni)), std::istreambuf_iterator<char>());
                Require(text.find(enabled ? "\ndiagnostic_log=1\n" : "\ndiagnostic_log=0\n") != std::string::npos &&
                        text.find("debug_log=") == std::string::npos, "save writes only the new diagnostic key");
            }
            std::puts("PASS diagnostic log default, legacy migration, validation and On/Off roundtrip");
            writeIni("");
            Require(settings::Read().askCompactGameData, "missing compaction preference offers to free disk space");
            writeIni("ask_compact_game_data=0\n");
            Require(!settings::Read().askCompactGameData, "Don't ask again reads from INI");
            for (const auto* contents : {"ask_compact_game_data=2\n", "ask_compact_game_data=-1\n",
                                         "ask_compact_game_data=invalid\n"})
            {
                writeIni(contents);
                Require(settings::Read().askCompactGameData, "invalid compaction preference keeps enabled default");
            }
            settings::Config compact{};
            compact.askCompactGameData = false;
            Require(settings::SaveConfig(compact) && !settings::GetConfig().askCompactGameData &&
                    !settings::Read().askCompactGameData, "Don't ask again persists through save and reload");
            compact = settings::Read();
            compact.debugLog = false;
            Require(settings::SaveConfig(compact) && !settings::Read().askCompactGameData,
                    "saving another setting preserves Don't ask again");
            {
                std::ifstream compactIni("settings.ini");
                const std::string text((std::istreambuf_iterator<char>(compactIni)), std::istreambuf_iterator<char>());
                Require(text.find("\nask_compact_game_data=0\n") != std::string::npos,
                        "INI writes the compaction preference key");
            }
            compact.askCompactGameData = true;
            Require(settings::SaveConfig(compact) && settings::Read().askCompactGameData,
                    "enabled compaction preference roundtrips");
            std::puts("PASS compaction preference default, validation, Don't ask again save/reload and unrelated save");
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
            writeIni("performance_overlay=2\n");
            Require(settings::Read().performanceOverlay == 2, "performance overlay reads from INI");
            writeIni("performance_overlay=3\n");
            Require(settings::Read().performanceOverlay == 0, "an unknown performance overlay mode reads as Off");
            settings::Config sharpness{};
            sharpness.upscaler = gpu::upscaling::Upscaler::Fsr;
            sharpness.fsrSharpnessPercent = 64;
            sharpness.performanceOverlay = 1;
            Require(settings::SaveConfig(sharpness), "real config save succeeds in sandbox");
            Require(settings::GetConfig().fsrSharpnessPercent == 64 && settings::Read().fsrSharpnessPercent == 64,
                    "real config retains sharpness after save and disk reload");
            std::ifstream persisted("settings.ini");
            const std::string text((std::istreambuf_iterator<char>(persisted)), std::istreambuf_iterator<char>());
            Require(text.find("fsr_sharpness=64\n") != std::string::npos, "INI writes fsr_sharpness key");
            Require(text.find("\nperformance_overlay=1\n") != std::string::npos && settings::Read().performanceOverlay == 1,
                    "INI writes and reloads performance_overlay");
            persisted.close(); // Windows cannot replace settings.ini while this reader holds it.
            sharpness.fsrSharpnessPercent = 102;
            Require(settings::SaveConfig(sharpness) && settings::Read().fsrSharpnessPercent == 100,
                    "real save validates out-of-range sharpness");
            sharpness.fsrSharpnessPercent = 0;
            Require(settings::SaveConfig(sharpness) && settings::Read().fsrSharpnessPercent == 0,
                    "Off roundtrips as zero");
            writeIni("upscaler=1\n");
            Require(settings::Read().dlssModel == 0, "a missing dlss_model defaults to M");
            writeIni("upscaler=1\ndlss_model=2\n");
            Require(settings::Read().dlssModel == 0, "an unknown DLSS model falls back to M");
            settings::Config model{};
            model.upscaler = gpu::upscaling::Upscaler::Dlss;
            model.dlssModel = 1;
            Require(settings::SaveConfig(model) && settings::Read().dlssModel == 1, "DLSS model L roundtrips through the INI");
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
            // Controls layouts: SDL mapping names for the controller, scancodes
            // for the keys; a bad pair keeps that input's default.
            {
                namespace map = hid::input_map;
                writeIni("button_prompts=1\n");
                Require(settings::Read().controllerMap == map::DefaultController &&
                        settings::Read().keyboardMap == map::DefaultKeyboard, "missing layouts read as the defaults");
                settings::Config layout{};
                map::Assign(layout.controllerMap, map::A, 1);
                map::Assign(layout.controllerMap, map::X, 3);
                map::Assign(layout.controllerMap, map::LeftShoulder, map::LeftTriggerSource);
                map::Assign(layout.controllerMap, map::Back, 16);
                map::Assign(layout.keyboardMap, map::A, 44);
                map::Assign(layout.keyboardMap, map::RightThumb, 29);
                Require(layout.controllerMap[map::B] == 0 && layout.controllerMap[map::LeftTrigger] == 9 &&
                        layout.keyboardMap[map::RightThumb] == 29, "Assign swaps a bound value and takes a free one");
                Require(settings::SaveConfig(layout), "layouts save");
                const auto read = settings::Read();
                Require(read.controllerMap == layout.controllerMap && read.keyboardMap == layout.keyboardMap &&
                        settings::GetConfig().controllerMap == layout.controllerMap, "both layouts survive a real save/reload");
                std::ifstream layoutIni("settings.ini");
                const std::string text((std::istreambuf_iterator<char>(layoutIni)), std::istreambuf_iterator<char>());
                layoutIni.close();
                Require(text.find("\ncontroller_map=a:b,b:a,x:y,y:x,leftshoulder:lefttrigger,rightshoulder:rightshoulder,"
                                  "lefttrigger:leftshoulder,righttrigger:righttrigger,back:paddle1,start:start,") != std::string::npos &&
                        text.find("\nkeyboard_map=a:44,b:27,x:4,y:22,") != std::string::npos &&
                        text.find(",leftstick:none,rightstick:29,") != std::string::npos,
                        "INI writes the controller by SDL name and the keys by scancode");
                writeIni("controller_map=a:b,b:bogus,x:x,nonsense,y:paddle1,start:guide\n"
                         "keyboard_map=a:44,b:600,x:41,leftstick:29,rightstick:none,y:44,start:abc,dpup:58\n");
                const auto partial = settings::Read();
                auto controller = map::DefaultController;
                controller[map::A] = 1;
                controller[map::B] = 0;
                controller[map::Y] = 16;
                auto keys = map::DefaultKeyboard;
                keys[map::A] = 44;
                keys[map::LeftThumb] = 29;
                Require(partial.controllerMap == controller,
                        "bad controller pairs keep their defaults; a:b alone reads as a swap; Guide is never bound");
                Require(partial.keyboardMap == keys,
                        "bad key pairs keep their defaults: out of range, Escape, F1, text, a key used twice");
                settings::Config repeated{};
                repeated.controllerMap[map::Start] = repeated.controllerMap[map::A];
                repeated.keyboardMap[map::B] = repeated.keyboardMap[map::A];
                Require(settings::SaveConfig(repeated) && settings::Read().controllerMap == map::DefaultController &&
                        settings::Read().keyboardMap == map::DefaultKeyboard, "a save never writes a button or key twice");
            }
            std::puts("PASS FG settings.ini provider and multiplier validation, FSR normalization, and min/max save/reload");
            std::filesystem::current_path(originalDir);
            std::filesystem::remove_all(sandbox);
            std::puts("PASS FSR sharpness settings.ini read, validation and save/reload roundtrip");
        }
        CaptureHelpRows(base, argc == 3 ? argv[1] : nullptr);
        CaptureSubpageEntries(base, argc == 3 ? argv[1] : nullptr);
        CheckHelpFit(base, argc == 3 ? argv[1] : nullptr);
        {
            // Exercise the same idle report / input polls / consume ordering
            // as the title hook. No guest menu task or SDL event is started.
            settings::active = false;
            settings::controlsOpen = false;
            settings::releaseToParent = false;
            settings::cancelPolls = 0;
            settings::pending = 0;
            PPC_STORE_U32(ConfigData + 4, 0);
            const unsigned beforeQuit = quitEventAttempts;
            const auto titleTick = [&](bool idle) {
                const bool owned = settings::UpdateTitleQuit(base, idle);
                Require(!settings::ConsumeTitleShortcut(idle && !owned), "Quit never opens Settings");
                return owned;
            };
            settings::ConsumeTitleShortcut(false);
            Poll(0x20, false);
            Require(!titleTick(true), "a held Back from outside the idle menu does not open Quit");
            Poll(0x20, false);
            Require(!titleTick(true), "holding Back across the idle gate is not a new press");
            const auto openQuit = [&] {
                Require(!titleTick(true), "title menu is idle before shortcut");
                Poll(0, false); Poll(0x20, false);
                Require(titleTick(true) && settings::IsOpen() && settings::IsTitleQuitOpen() && !settings::active,
                        "fresh Back opens host confirmation without opening retail Settings");
                Require(settings::snapshot.dialogOnly && settings::snapshot.dialogMessage == L"Quit the game?" &&
                        settings::snapshot.dialogChoices == std::vector<std::wstring>{L"Yes", L"No"} &&
                        settings::snapshot.dialogSelection == 1, "Quit defaults to No");
                Poll(0x20, true); Poll(0, true);
            };
            const auto release = [&] { Poll(0, true); Poll(0, false); };
            currentConfig.uiLanguage = 0;
            openQuit();
            settings::menuClockOffset = {};
            settings::menuClockFixed = std::chrono::steady_clock::time_point{};
            std::vector<uint32_t> dialogPixels, replacementPixels;
            uint64_t dialogRevision = 0, replacementRevision = 0;
            Require(!settings::DrawMenu(replacementPixels, replacementRevision, 320, 180) && replacementPixels.empty(),
                    "title confirmation never replaces the live title through DrawMenu");
            Require(settings::DrawTitleQuit(dialogPixels, dialogRevision, 320, 180) && (dialogPixels.front() >> 24) == 0,
                    "title confirmation starts with transparent dim");
            const auto colored = std::find_if(dialogPixels.begin(), dialogPixels.end(), [](uint32_t pixel) {
                return (pixel & 0x00ffffffu) != 0;
            });
            Require(colored != dialogPixels.end(), "transparent opening retains dialog RGB for straight alpha");
            const size_t coloredIndex = size_t(colored - dialogPixels.begin());
            const uint32_t coloredRgb = *colored & 0x00ffffffu;
            const uint64_t snapshotRevision = settings::snapshot.revision, openingRevision = dialogRevision;
            settings::menuClockOffset = std::chrono::milliseconds(50);
            Require(settings::DrawTitleQuit(dialogPixels, dialogRevision, 320, 180) && (dialogPixels.front() >> 24) == 30 &&
                    (dialogPixels[coloredIndex] & 0x00ffffffu) == coloredRgb && dialogRevision > openingRevision &&
                    settings::snapshot.revision == snapshotRevision,
                    "unchanged title snapshot advances its 100 ms dim without fading straight RGB");
            const uint64_t halfOpenRevision = dialogRevision;
            settings::menuClockOffset = std::chrono::milliseconds(100);
            Require(settings::DrawTitleQuit(dialogPixels, dialogRevision, 320, 180) && (dialogPixels.front() >> 24) == 61 &&
                    dialogRevision > halfOpenRevision, "title dim reaches translucent alpha 61 after 100 ms");
            const uint64_t settledRevision = dialogRevision;
            Require(settings::DrawTitleQuit(dialogPixels, dialogRevision, 320, 180) && dialogRevision == settledRevision,
                    "settled title confirmation reuses its raster revision");
            Poll(0x1000, true);
            Require(titleTick(true) && !settings::IsOpen() && !settings::IsTitleQuitOpen() && quitEventAttempts == beforeQuit,
                    "confirming default No holds guest input for its last tick and never quits");
            Require(settings::DrawTitleQuit(dialogPixels, dialogRevision, 320, 180),
                    "dismissed No retains overlay for its closing fade");
            settings::menuClockOffset = std::chrono::milliseconds(150);
            Require(settings::DrawTitleQuit(dialogPixels, dialogRevision, 320, 180) && (dialogPixels.front() >> 24) == 30 &&
                    (dialogPixels[coloredIndex] & 0x00ffffffu) == coloredRgb && dialogRevision > settledRevision,
                    "No fades dim and dialog out over the live title without fading straight RGB");
            const uint64_t halfClosedRevision = dialogRevision;
            settings::menuClockOffset = std::chrono::milliseconds(200);
            Require(!settings::DrawTitleQuit(dialogPixels, dialogRevision, 320, 180) && (dialogPixels.front() >> 24) == 0 &&
                    dialogRevision > halfClosedRevision, "closing completes after 100 ms and publishes its last frame");
            settings::menuClockOffset = {};
            settings::menuClockFixed.reset();
            Poll(0x1000, true); release();
            openQuit();
            Poll(1, true); titleTick(true); Poll(0, true); Poll(0x2000, true);
            Require(titleTick(true) && !settings::IsOpen() && quitEventAttempts == beforeQuit,
                    "B cancels even with Yes selected");
            Poll(0x2000, true); release();
            openQuit();
            Require(settings::CalibrationKey(27), "Escape belongs to title confirmation");
            Require(titleTick(true) && !settings::IsOpen(), "Escape returns to title");
            release();
            openQuit();
            Poll(0x20, true);
            Require(titleTick(true) && !settings::IsOpen(), "fresh Back cancels its confirmation");
            release();
            openQuit();
            settings::PointerClick(500, 420, false);
            Require(titleTick(true) && !settings::IsOpen() && quitEventAttempts == beforeQuit,
                    "mouse No uses the shared dialog hit-test without quitting");
            release();
            titleQuitAccepted = false;
            openQuit();
            Poll(1, true); titleTick(true); Poll(0, true); Poll(0x10, true);
            Require(titleTick(true) && !settings::IsOpen() && quitEventAttempts == beforeQuit + 1,
                    "Yes via keyboard Enter uses shared Quit; rejected event stays on the title");
            release();
            titleQuitAccepted = true;
            openQuit();
            Poll(2, true); titleTick(true); Poll(0, true); Poll(0x1000, true);
            Require(titleTick(true) && !settings::IsOpen() && quitEventAttempts == beforeQuit + 2,
                    "fresh retry Yes queues the same normal quit path once");
            release();
            Require(!titleTick(true), "idle gate rearms after dialog");
            Poll(0, false); Poll(0x20, false);
            Require(!titleTick(false) && !titleTick(true) && !settings::IsOpen(),
                    "queued shortcut is discarded when title leaves MenuIdle");
            Poll(0, false); Poll(0x8000, false);
            Require(settings::ConsumeTitleShortcut(true), "existing Y Settings shortcut remains available");
            settings::ConsumeTitleShortcut(false);
            std::puts("PASS title Quit: overlay-only 100 ms straight-alpha fades, idle edge gate, held input, default No, B/Back/Escape, release consumption, Yes and failed queue retry");
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
