#include <debug/menu_overlay.h>
#include <debug/teleport.h>
#include <debug/map_info.h>
#include <debug/controller_hint.h>
#include <host_ui/host_ui.h>
#include <host_ui/rasterizer.h>
#include <settings/config.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
std::mutex g_fixtureMutex;
debug_menu::TeleportSnapshot g_teleport;
std::atomic<bool> g_saveAnywhere{false};
std::atomic<bool> g_playStation{false};

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
    config.debugLanguage = 0;
    return config;
}
bool SaveDebugLanguage(uint32_t) { return true; }
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
MapInfo GetMapInfo() { return {}; }
bool SaveAnywhereEnabled() { return g_saveAnywhere.load(); }
void SetSaveAnywhereEnabled(bool enabled) { g_saveAnywhere = enabled; }
bool RequestVictory() { return true; }
void CancelVictory() {}
const wchar_t* Status() { return L""; }
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

    const auto overviewReference = renderFrame();
    capture("overview-reference.ppm");
    g_playStation = true;
    const auto overviewPlayStation = renderFrame();
    capture("overview-playstation.ppm");
    Require(RegionDiffers(overviewReference, overviewPlayStation, 260, 594, 760, 22),
            "debug footer did not update for PlayStation prompts");
    g_playStation = false;
    Require(renderFrame() == overviewReference, "debug footer did not restore after controller switch");

    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Chinese debug UI
    const auto chineseReference = renderFrame();
    g_playStation = true;
    const auto chinesePlayStation = renderFrame();
    capture("overview-zh-playstation.ppm");
    Require(RegionDiffers(chineseReference, chinesePlayStation, 260, 594, 760, 22),
            "Chinese debug footer did not update for PlayStation prompts");
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    renderFrame();
    capture("cheats-zh-playstation.ppm");
    debug_menu::HandleInput(debug_menu::InputAction::PrevTab);
    debug_menu::HandleInput(debug_menu::InputAction::PrevTab);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Restore English
    g_playStation = false;

    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
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
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Memory edits confirmation
    g_playStation = false;
    const auto confirmReference = renderFrame();
    g_playStation = true;
    const auto confirmPlayStation = renderFrame();
    capture("confirm-playstation.ppm");
    Require(RegionDiffers(confirmReference, confirmPlayStation, 300, 480, 680, 22),
            "cheat confirmation did not update for PlayStation prompts");
    debug_menu::HandleInput(debug_menu::InputAction::Cancel); // Dismiss confirmation
    g_playStation = false;

    std::thread updates([] {
        for (uint64_t revision = 1; revision <= 1000; ++revision)
        {
            {
                std::lock_guard lock(g_fixtureMutex);
                g_teleport.poiRevision = revision;
                g_teleport.pois = {{revision, L"POI", {float(revision), 2.0f, 3.0f}}};
            }
            debug_menu::UpdateOverlaySnapshot();
        }
    });
    std::thread input([] {
        for (int iteration = 0; iteration < 1000; ++iteration)
        {
            debug_menu::HandleInput(debug_menu::InputAction::NextTab);
            debug_menu::HandleInput(debug_menu::InputAction::Down);
            debug_menu::HandleInput(debug_menu::InputAction::Right);
            debug_menu::HandleInput(debug_menu::InputAction::Up);
            debug_menu::HandleInput(debug_menu::InputAction::PrevTab);
        }
    });
    std::thread render([&] {
        for (int iteration = 0; iteration < 1000; ++iteration)
        {
            frame.Clear();
            host_ui::Rasterizer rasterizer(frame);
            debug_menu::RenderOverlay(rasterizer);
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
