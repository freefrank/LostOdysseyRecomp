#include <debug/menu_overlay.h>
#include <debug/teleport.h>
#include <debug/map_info.h>
#include <debug/controller_hint.h>
#include <host_ui/host_ui.h>
#include <host_ui/rasterizer.h>
#include <settings/config.h>

#include <atomic>
#include <barrier>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
std::mutex g_fixtureMutex;
debug_menu::TeleportSnapshot g_teleport;
std::atomic<bool> g_saveAnywhere{false};
std::atomic<bool> g_playStation{false};
std::atomic<settings::EncounterRate> g_encounterRate{settings::EncounterRate::Normal};
std::atomic<bool> g_everyStep{false};
std::atomic<uint32_t> g_debugLanguage{0};
std::atomic<const wchar_t*> g_resetRefusal{nullptr};
std::atomic<const wchar_t*> g_resetStatus{L"Return to title unavailable"};

struct ResetNotice
{
    const wchar_t* key;
    const wchar_t* chinese;
    const char* filename;
};
constexpr ResetNotice ResetNotices[] = {
    {L"Return to title unavailable", L"当前无法返回标题", "unavailable"},
    {L"Not available during battle", L"战斗期间不可用", "battle"},
    {L"Please wait for loading to finish", L"请等待读取完成", "loading"},
    {L"Please wait for saving to finish", L"请等待保存完成", "saving"},
    {L"Close the save/load screen first", L"先关闭存档/读档画面", "storage"},
    {L"Already at the title screen", L"当前已在标题画面", "title"},
    {L"Returning to title", L"正在返回标题", "returning"},
};

void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

bool RegionDiffers(const std::vector<uint32_t>& left, const std::vector<uint32_t>& right,
                   int x, int y, int width, int height)
{
    for (int py = y; py < y + height; ++py)
        for (int px = x; px < x + width; ++px)
            if (left[size_t(py) * host_ui::kOverlayWidth + px] != right[size_t(py) * host_ui::kOverlayWidth + px])
                return true;
    return false;
}

void SavePpm(const std::filesystem::path& path, const host_ui::PixelBuffer& frame)
{
    std::ofstream out(path, std::ios::binary);
    Require(bool(out), "cannot open debug prompt screenshot");
    out << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (uint32_t pixel : frame.pixels)
    {
        const char rgb[] = {char(pixel), char(pixel >> 8), char(pixel >> 16)};
        out.write(rgb, 3);
    }
    Require(bool(out), "cannot write debug prompt screenshot");
}
}

namespace hid { bool UsesPlayStationPrompts() { return g_playStation.load(); } }

namespace settings
{
Config GetConfig()
{
    Config config;
    config.debugLanguage = g_debugLanguage.load();
    return config;
}
bool SaveDebugLanguage(uint32_t language) { g_debugLanguage = language; return true; }
bool SaveFastForward(bool, uint32_t, uint32_t, bool) { return true; }
bool SaveMenuAnimationRate(uint32_t) { return true; }
}

namespace gpu::renderer
{
void RequestDebugCapture() {}
std::wstring DebugCaptureStatus() { return {}; }
}

namespace debug_menu
{
TeleportSnapshot GetTeleportSnapshot()
{
    std::lock_guard lock(g_fixtureMutex);
    return g_teleport;
}
bool RequestTeleport(Position) { return true; }
bool RequestTeleportOffset(Position) { return true; }
bool RequestSavePosition() { return true; }
bool RequestRestorePosition() { return true; }
bool RequestPoiTeleport(uint64_t) { return true; }
const wchar_t* RequestMapJump(const char*) { return nullptr; }
MapInfo GetMapInfo() { return {}; }
bool SaveAnywhereEnabled() { return g_saveAnywhere.load(); }
void SetSaveAnywhereEnabled(bool enabled) { g_saveAnywhere = enabled; }
void RequestPartySwitch() {}
settings::EncounterRate GetEncounterRate() { return g_encounterRate.load(); }
void SetEncounterRate(settings::EncounterRate rate) { g_encounterRate = rate; g_everyStep = false; }
bool BattleExpDoubleEnabled() { return false; }
void SetBattleExpDoubleEnabled(bool) {}
bool EncounterEveryStepEnabled() { return g_everyStep.load(); }
void SetEncounterEveryStepEnabled(bool enabled) { g_everyStep = enabled; }
bool RequestVictory() { return true; }
void CancelVictory() {}
const wchar_t* Status() { return L""; }
namespace soft_reset
{
bool Request() { return false; }
const wchar_t* Status() { return g_resetStatus.load(); }
const wchar_t* TakeRefusal() { return g_resetRefusal.exchange(nullptr); }
}
}

int main(int argc, char** argv)
{
    using debug_menu::controller_hint::ShoulderLabels;
    Require(ShoulderLabels(L"LB/RB: tabs  LT+RT: categories",true)==L"L1/R1: tabs  L2+R2: categories",
            "PS shoulder combinations lost physical mapping");
    Require(ShoulderLabels(L"LB/RB: tabs  LT+RT: categories",false)==L"LB/RB: tabs  LT+RT: categories",
            "non-PS shoulder labels changed");
    Require(ShoulderLabels(L"VOLT ALBATROSS XLT LT2 : LT + RB",true)==L"VOLT ALBATROSS XLT LT2 : L2 + R1",
            "shoulder replacement matched part of a word");
    Require(ShoulderLabels(L"按住 LT 加速；LT+RT 不加速",true)==L"按住 L2 加速；L2+R2 不加速",
            "Chinese shoulder labels did not update");
    {
        std::lock_guard lock(g_fixtureMutex);
        g_teleport.available = true;
        g_teleport.current = {1.0f, 2.0f, 3.0f};
    }
    debug_menu::ToggleOverlay();
    Require(debug_menu::IsOverlayVisible() && host_ui::IsGamePaused(), "overlay did not open and pause");

    host_ui::PixelBuffer frame;
    Require(frame.Resize(), "overlay frame allocation failed");
    auto renderFrame = [&] {
        frame.Clear();
        host_ui::Rasterizer rasterizer(frame);
        debug_menu::RenderOverlay(rasterizer);
        return frame.pixels;
    };
    const auto capture = [&](const char* name) {
        if (argc < 2) return;
        std::filesystem::create_directories(argv[1]);
        SavePpm(std::filesystem::path(argv[1]) / name, frame);
    };
    const auto requireResetNotice = [&](const wchar_t* text) {
        host_ui::PixelBuffer expected;
        Require(expected.Resize(), "reset notice reference allocation failed");
        host_ui::Rasterizer rasterizer(expected);
        const uint32_t color = host_ui::MakeColor(255, 120, 220, 150);
        rasterizer.DrawWString((1280 - rasterizer.MeasureWString(text)) / 2, 572, text, color);
        for (int y = 572; y < 590; ++y)
            for (int x = 260; x < 1020; ++x)
            {
                const auto pixel = size_t(y) * host_ui::kOverlayWidth + x;
                Require((frame.pixels[pixel] == color) == (expected.pixels[pixel] == color),
                    "guest-thread refusal did not render the complete localized notice");
            }
    };
    const auto checkResetNotices = [&](bool chinese) {
        for (const auto& notice : ResetNotices)
        {
            g_resetRefusal = notice.key;
            debug_menu::PollResetNotice();
            renderFrame();
            const std::string filename = std::string("return-to-title-notice-") + notice.filename +
                (chinese ? "-zh.ppm" : "-en.ppm");
            capture(filename.c_str());
            requireResetNotice(chinese ? notice.chinese : notice.key);
        }
    };

    const auto overviewReference = renderFrame();
    capture("overview-reference.ppm");
    g_playStation = true;
    const auto overviewPlayStation = renderFrame();
    capture("overview-playstation.ppm");
    Require(RegionDiffers(overviewReference, overviewPlayStation, 260, 594, 760, 22),
            "debug footer did not update for PlayStation prompts");
    g_playStation = false;
    Require(renderFrame() == overviewReference, "debug footer did not restore after controller switch");

    debug_menu::HandleInput(debug_menu::InputAction::Cancel);
    debug_menu::PollResetNotice();
    Require(!debug_menu::IsOverlayVisible() && !host_ui::IsGamePaused(),
        "empty reset notice unexpectedly opened the overlay");
    g_resetRefusal = L"Please wait for saving to finish";
    debug_menu::PollResetNotice();
    Require(debug_menu::IsOverlayVisible() && host_ui::IsGamePaused(),
        "late refusal did not reopen and pause the overlay");
    const auto refusalReference = renderFrame();
    capture("return-to-title-late-refusal-en.ppm");
    requireResetNotice(L"Please wait for saving to finish");
    debug_menu::PollResetNotice();
    Require(renderFrame() == refusalReference, "consumed reset notice changed the overlay again");

    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    debug_menu::HandleInput(debug_menu::InputAction::NextTab); // Cheats already open when refusal arrives
    g_resetRefusal = L"Please wait for loading to finish";
    debug_menu::PollResetNotice();
    renderFrame();
    capture("return-to-title-visible-cheats-refusal-en.ppm");
    requireResetNotice(L"Please wait for loading to finish");
    g_resetRefusal = L"Close the save/load screen first";
    debug_menu::PollResetNotice();
    renderFrame();
    capture("return-to-title-storage-screen-refusal-en.ppm");
    requireResetNotice(L"Close the save/load screen first");
    checkResetNotices(false);

    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    const auto resetNoReference = renderFrame();
    capture("return-to-title-en-no.ppm");
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    const auto resetYesReference = renderFrame();
    capture("return-to-title-en-yes.ppm");
    Require(RegionDiffers(resetNoReference, resetYesReference, 400, 393, 480, 38),
        "Return to title Yes/No selection did not render");
    debug_menu::HandleInput(debug_menu::InputAction::Cancel);
    Require(debug_menu::IsOverlayVisible(), "cancel closed overlay instead of Return to title confirmation");
    g_resetStatus = L"Not available during battle";
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    renderFrame();
    capture("return-to-title-battle-refusal-immediate-en.ppm");
    requireResetNotice(L"Not available during battle");
    Require(debug_menu::IsOverlayVisible() && host_ui::IsGamePaused(),
        "battle refusal did not keep the menu open and paused");
    for (int i = 0; i < 4; ++i) debug_menu::HandleInput(debug_menu::InputAction::Up);

    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Chinese debug UI
    const auto chineseReference = renderFrame();
    g_playStation = true;
    const auto chinesePlayStation = renderFrame();
    capture("overview-zh-playstation.ppm");
    Require(RegionDiffers(chineseReference, chinesePlayStation, 260, 594, 760, 22),
            "Chinese debug footer did not update for PlayStation prompts");
    debug_menu::HandleInput(debug_menu::InputAction::Cancel);
    g_resetRefusal = L"Please wait for loading to finish";
    debug_menu::PollResetNotice();
    Require(debug_menu::IsOverlayVisible() && host_ui::IsGamePaused(),
        "Chinese late refusal did not reopen and pause the overlay");
    renderFrame();
    capture("return-to-title-late-refusal-zh-playstation.ppm");
    requireResetNotice(L"请等待读取完成");
    g_resetRefusal = L"Close the save/load screen first";
    debug_menu::PollResetNotice();
    renderFrame();
    capture("return-to-title-storage-screen-refusal-zh-playstation.ppm");
    requireResetNotice(L"先关闭存档/读档画面");
    checkResetNotices(true);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    renderFrame();
    capture("return-to-title-battle-refusal-immediate-zh.ppm");
    requireResetNotice(L"战斗期间不可用");
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    renderFrame();
    capture("return-to-title-zh-playstation.ppm");
    debug_menu::HandleInput(debug_menu::InputAction::Cancel);
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    renderFrame();
    capture("cheats-zh-playstation.ppm");
    for (int i = 0; i < 4; ++i) debug_menu::HandleInput(debug_menu::InputAction::Down);
    renderFrame();
    capture("fast-forward-audio-zh-playstation.ppm");
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    renderFrame();
    capture("encounter-rate-zh-playstation.ppm");
    for (int i = 0; i < 3; ++i) debug_menu::HandleInput(debug_menu::InputAction::Right);
    renderFrame();
    capture("encounter-every-step-zh-playstation.ppm");
    Require(g_everyStep && g_encounterRate == settings::EncounterRate::None,
        "Chinese encounter row did not select session-only Every step");
    debug_menu::HandleInput(debug_menu::InputAction::Right); // Restore Normal
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    renderFrame();
    capture("battle-exp-zh-playstation.ppm");
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    renderFrame();
    capture("menu-animations-zh-playstation.ppm");
    for (int i = 0; i < 7; ++i) debug_menu::HandleInput(debug_menu::InputAction::Up);
    debug_menu::HandleInput(debug_menu::InputAction::PrevTab);
    debug_menu::HandleInput(debug_menu::InputAction::PrevTab);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Restore English
    g_playStation = false;

    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    for (int i = 0; i < 4; ++i) debug_menu::HandleInput(debug_menu::InputAction::Down);
    renderFrame();
    capture("fast-forward-audio-en.ppm");
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    renderFrame();
    capture("encounter-rate-en.ppm");
    for (int i = 0; i < 3; ++i) debug_menu::HandleInput(debug_menu::InputAction::Right);
    renderFrame();
    capture("encounter-every-step-en.ppm");
    Require(g_everyStep && g_encounterRate == settings::EncounterRate::None,
        "English encounter row did not select session-only Every step");
    debug_menu::HandleInput(debug_menu::InputAction::Right); // Restore Normal
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    renderFrame();
    capture("battle-exp-en.ppm");
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    renderFrame();
    capture("menu-animations-en-faster.ppm");
    debug_menu::HandleInput(debug_menu::InputAction::NextCategory);
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Character picker
    const auto pickerReference = renderFrame();
    g_playStation = true;
    const auto pickerPlayStation = renderFrame();
    capture("picker-playstation.ppm");
    Require(RegionDiffers(pickerReference, pickerPlayStation, 300, 510, 680, 22),
            "cheat picker did not update for PlayStation prompts");

    debug_menu::HandleInput(debug_menu::InputAction::Cancel); // Dismiss picker
    debug_menu::HandleInput(debug_menu::InputAction::PrevCategory);
    for(int i=0;i<8;++i) debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Row 8: memory edits confirmation
    g_playStation = false;
    const auto confirmReference = renderFrame();
    g_playStation = true;
    const auto confirmPlayStation = renderFrame();
    capture("confirm-playstation.ppm");
    Require(RegionDiffers(confirmReference, confirmPlayStation, 300, 480, 680, 22),
            "cheat confirmation did not update for PlayStation prompts");
    debug_menu::HandleInput(debug_menu::InputAction::Cancel); // Dismiss confirmation
    Require(debug_menu::IsOverlayVisible(), "cancel closed overlay instead of the cheat confirmation");
    g_playStation = false;

    // Keep all three workers participating in every round, without making the
    // rasterizer draw hundreds of frames after the mutation workers finish.
    constexpr int stressRounds = 64;
    constexpr int mutationsPerRound = 16;
    std::barrier stressRound(3);
    std::thread updates([&] {
        for (int round = 0; round < stressRounds; ++round)
        {
            stressRound.arrive_and_wait();
            for (int iteration = 0; iteration < mutationsPerRound; ++iteration)
            {
                const uint64_t revision = uint64_t(round) * mutationsPerRound + iteration + 1;
                {
                    std::lock_guard lock(g_fixtureMutex);
                    g_teleport.poiRevision = revision;
                    g_teleport.pois = {{revision, L"POI", {float(revision), 2.0f, 3.0f}}};
                }
                debug_menu::UpdateOverlaySnapshot();
            }
            stressRound.arrive_and_wait();
        }
    });
    std::thread input([&] {
        for (int round = 0; round < stressRounds; ++round)
        {
            stressRound.arrive_and_wait();
            for (int iteration = 0; iteration < mutationsPerRound; ++iteration)
            {
                debug_menu::HandleInput(debug_menu::InputAction::NextTab);
                debug_menu::HandleInput(debug_menu::InputAction::Down);
                debug_menu::HandleInput(debug_menu::InputAction::Right);
                debug_menu::HandleInput(debug_menu::InputAction::Up);
                debug_menu::HandleInput(debug_menu::InputAction::PrevTab);
            }
            stressRound.arrive_and_wait();
        }
    });
    std::thread render([&] {
        for (int round = 0; round < stressRounds; ++round)
        {
            stressRound.arrive_and_wait();
            frame.Clear();
            host_ui::Rasterizer rasterizer(frame);
            debug_menu::RenderOverlay(rasterizer);
            stressRound.arrive_and_wait();
        }
    });
    updates.join();
    input.join();
    render.join();

    Require(frame.pixels.size() == size_t(host_ui::kOverlayWidth) * host_ui::kOverlayHeight,
            "render changed overlay frame dimensions");
    Require(debug_menu::IsOverlayVisible(), "concurrent menu actions closed overlay");
    debug_menu::HandleInput(debug_menu::InputAction::Cancel);
    Require(!debug_menu::IsOverlayVisible() && !host_ui::IsGamePaused(), "cancel did not close and resume");
    std::puts("debug overlay concurrent update, input, render, and cancel passed");
    return 0;
}
