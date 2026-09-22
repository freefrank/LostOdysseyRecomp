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
        settings::tab = 2; settings::row = 9;
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
        settings::tab = 2; settings::row = 10;
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
            settings::row = 2;
            settings::Publish(base, ConfigData);
            Require(settings::snapshot.rows.size() == 11, "graphics tab has 11 rows");
            const auto& wsRow = settings::snapshot.rows[2];
            Require(wsRow.name == L"Widescreen" && wsRow.value == L"On" && wsRow.selectedChoice == 0,
                    "initial 3440x1440 automatically enables Widescreen switch");
            const auto& resRow = settings::snapshot.rows[3];
            Require(resRow.name == L"Output resolution", "row 3 is Output resolution");
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
            settings::row = 2;
            settings::Publish(base, ConfigData);
            Require(settings::snapshot.rows[2].value == L"Off", "1280x720 starts with Widescreen Off");
            Require(settings::snapshot.rows[3].choices.size() == 5 &&
                    settings::snapshot.rows[3].choices[0] == L"1280 × 720" &&
                    settings::snapshot.rows[3].choices[4] == L"3840 × 2160",
                    "16:9 resolution choices present");

            // Toggle switch ON (delta +1)
            settings::pending = 0x1008; Tick(base);
            Require(settings::edit.width == 1720 && settings::edit.height == 720,
                    "toggle ON from 1280x720 maps to 1720x720");
            Require(settings::snapshot.rows[2].value == L"On", "Widescreen switch is now On");
            Require(settings::snapshot.rows[3].selectedChoice == 0 &&
                    settings::snapshot.rows[3].value == L"1720 × 720", "1720x720 selected");

            // Move to row 3 (resolution) and cycle forward through all 5 ultrawide tiers
            settings::row = 3;
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
            Require(settings::snapshot.rows[3].selectedChoice == 4 &&
                    settings::snapshot.rows[3].value == L"5120 × 2160", "5120x2160 tier verified");

            // Switch back to 16:9 on row 2: height 2160 preserves height and maps to 3840x2160
            settings::row = 2;
            settings::pending = 0x1008; Tick(base);
            Require(settings::edit.width == 3840 && settings::edit.height == 2160,
                    "switch to 16:9 preserves 2160 height mapping to 3840x2160");
            Require(settings::snapshot.rows[2].value == L"Off", "switch is now Off");
            Require(settings::snapshot.rows[3].choices[4] == L"3840 × 2160", "16:9 4K selected");

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

            // Switch to 3440x1440 and Save on row 9: goes through display change state machine
            settings::tab = 2;
            settings::row = 2;
            settings::pending = 0x1008; Tick(base); // Switch ON -> 1720x720
            settings::row = 3;
            settings::pending = 0x1008; Tick(base); // 2560x1080
            settings::pending = 0x1008; Tick(base); // 3440x1440
            Require(settings::edit.width == 3440 && settings::edit.height == 1440, "selected 3440x1440");
            settings::row = 10; // Save graphics settings
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
            settings::row = 0;
            settings::edit.upscaler = gpu::upscaling::Upscaler::Off;
            settings::Publish(base, ConfigData);

            // DLSS Quality row (row 6) is hidden when upscaler is Off
            Require(settings::snapshot.rows.size() == 11, "graphics tab has 11 rows");
            Require(settings::snapshot.rows[6].hidden, "DLSS quality is hidden when upscaler is Off");

            // Navigation skipping hidden row 6
            settings::row = 5; // Upscaler row
            settings::pending = 2; Tick(base); // D-pad down
            Require(settings::row == 7, "down from Upscaler skips hidden DLSS quality to Scaling quality (row 7)");
            settings::pending = 1; Tick(base); // D-pad up
            Require(settings::row == 5, "up from Scaling quality skips hidden DLSS quality to Upscaler (row 5)");

            // Start (0x10) jumps focus to Save graphics settings (row 10) without saving
            settings::pending = 0x10; Tick(base);
            Require(settings::row == 10, "Start jumps focus to Save row (row 10)");
            Require(saves == savesBefore, "Start jump does not save immediately");

            // Simultaneous Start (0x10) + Confirm (0x1000) does NOT save on the same tick
            settings::row = 0;
            settings::pending = 0x1010; Tick(base);
            Require(settings::row == 10, "simultaneous Start+A still focuses Save row");
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
            for (int selected : {5, 7})
            {
                settings::row = selected; settings::Publish(base, ConfigData);
                auto preview = settings::snapshot; preview.assets = assets;
                for (int label : {5, 7, 9})
                    Require(covers(assets->body, preview.rows[label].name), "changed label must use original body face");
                std::vector<uint32_t> pixels;
                Require(settings::RasterizeMenu(preview, 1280, 720, pixels), "translated Graphics preview");
                WriteBmp(std::filesystem::path(argv[2]) / (selected == 5 ? "graphics-aa.bmp" : "graphics-rate.bmp"), pixels);
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
