// Exercise the production hook, substituting only its guest-call boundaries.
// No game, renderer device, window, save or profile is started by this fixture.
#include <stdafx.h>
#include <gpu/video.h>
#include <settings/config.h>
#include <fstream>
#include <stdexcept>
namespace gpu::video { plume::RenderDevice* MenuFlowTestDevice(); }
namespace settings {
Config MenuFlowGetConfig();
bool MenuFlowSaveConfig(const Config&);
void MenuFlowPreviewConfig(const Config&);
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
        settings::tab = 2; settings::row = 7;
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
        settings::tab = 2; settings::row = 8;
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
        if (argc == 3)
        {
            settings::status.clear();
            settings::edit.uiLanguage = 4; settings::tab = 2;
            auto assets = settings::menu_assets::Cached(argv[1], 4);
            Require(bool(assets), "installed SCH assets");
            std::filesystem::create_directories(argv[2]);
            for (int selected : {4, 6})
            {
                settings::row = selected; settings::Publish(base, ConfigData);
                auto preview = settings::snapshot; preview.assets = assets;
                for (int label : {4, 6, 8})
                    Require(covers(assets->body, preview.rows[label].name), "changed label must use original body face");
                std::vector<uint32_t> pixels;
                Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "translated Graphics preview");
                WriteBmp(std::filesystem::path(argv[2]) / (selected == 4 ? "graphics-aa.bmp" : "graphics-rate.bmp"), pixels);
            }
            std::puts("PASS two Graphics previews from actual Publish/Translate, normal and selected changed labels");
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
