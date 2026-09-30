#include <debug/menu_overlay.h>
#include <debug/teleport.h>
#include <debug/battle_menu.h>
#include <debug/map_info.h>
#include <debug/save_anywhere.h>
#include <host_ui/host_ui.h>
#include <host_ui/rasterizer.h>
#include <debug/translations.h>
#include <settings/config.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "REQUIRE FAILED: %s\n", message);
        std::exit(1);
    }
}

struct MockServices
{
    uint32_t selectedLanguage = 1;
    bool saveLanguageSuccess = true;
    bool captureRequested = false;
    bool saveAnywhere = false;
    bool allowVictory = true;
    bool victoryRequested = false;
    bool victoryCancelled = false;

    bool allowTeleport = true;
    bool allowSavePos = true;
    bool allowRestorePos = true;
    bool allowPoiTeleport = true;

    int teleportCalls = 0;
    int savePosCalls = 0;
    int restorePosCalls = 0;
    int poiTeleportCalls = 0;

    debug_menu::Position lastTeleport{};
    debug_menu::TeleportSnapshot snapshot;
} g_mock;
} // namespace

namespace hid { bool UsesPlayStationPrompts() { return false; } }

namespace settings
{
Config GetConfig()
{
    Config c;
    c.debugLanguage = g_mock.selectedLanguage;
    return c;
}
bool SaveDebugLanguage(uint32_t language)
{
    if (!g_mock.saveLanguageSuccess) return false;
    g_mock.selectedLanguage = language;
    return true;
}
}

namespace gpu::renderer
{
void RequestDebugCapture() { g_mock.captureRequested = true; }
std::wstring DebugCaptureStatus() { return L"Ready"; }
}

namespace debug_menu
{
MapInfo GetMapInfo()
{
    return {true, 42, L"Test Wasteland", L"wasteland_01"};
}
bool SaveAnywhereEnabled() { return g_mock.saveAnywhere; }
void SetSaveAnywhereEnabled(bool enabled) { g_mock.saveAnywhere = enabled; }

bool RequestVictory()
{
    if (!g_mock.allowVictory) return false;
    g_mock.victoryRequested = true;
    return true;
}
void CancelVictory()
{
    g_mock.victoryCancelled = true;
}
const wchar_t* Status()
{
    return g_mock.victoryRequested ? L"Waiting for safe phase" : L"Ready";
}

TeleportSnapshot GetTeleportSnapshot()
{
    return g_mock.snapshot;
}

bool RequestTeleport(Position position)
{
    if (!g_mock.allowTeleport) return false;
    g_mock.lastTeleport = position;
    ++g_mock.teleportCalls;
    return true;
}
bool RequestTeleportOffset(Position) { return true; }

bool RequestSavePosition()
{
    if (!g_mock.allowSavePos) return false;
    ++g_mock.savePosCalls;
    return true;
}
bool RequestRestorePosition()
{
    if (!g_mock.allowRestorePos) return false;
    ++g_mock.restorePosCalls;
    return true;
}
bool RequestPoiTeleport(uint64_t)
{
    if (!g_mock.allowPoiTeleport) return false;
    ++g_mock.poiTeleportCalls;
    return true;
}
}

int main()
{
    // Setup initial mock environment
    g_mock.snapshot.available = true;
    g_mock.snapshot.current = {100.0f, 200.0f, 300.0f};
    g_mock.snapshot.bookmarkAvailable = true;
    g_mock.snapshot.bookmark = {50.0f, 60.0f, 70.0f};
    g_mock.snapshot.poiRevision = 1;
    g_mock.snapshot.pois = {
        {1, L"Save Point 1", {100.0f, 200.0f, 300.0f}},
        {2, L"Chest 1", {150.0f, 250.0f, 350.0f}}
    };

    Require(!debug_menu::IsOverlayVisible(), "Overlay should be initially hidden");
    Require(!host_ui::IsGamePaused(), "Game should not be initially paused");

    // 1. Toggle Overlay & Pause State
    debug_menu::ToggleOverlay();
    Require(debug_menu::IsOverlayVisible(), "ToggleOverlay did not make overlay visible");
    Require(host_ui::IsGamePaused(), "ToggleOverlay did not pause the game");

    // 2. Tab 0 (Overview) Row 0: Language Toggle & failure handling
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.selectedLanguage == 0, "Language toggle did not change to English");

    g_mock.saveLanguageSuccess = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    g_mock.saveLanguageSuccess = true;

    // Row 1: Render Capture
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.captureRequested, "Capture was not requested");

    // Row 2: Save Anywhere
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.saveAnywhere, "Save anywhere was not enabled");

    // Row 3: Win Battle (Success and Rejection)
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.victoryRequested, "Victory request not registered");

    g_mock.allowVictory = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);

    // Row 4: Cancel Victory
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.victoryCancelled, "Cancel victory was not called");

    // 3. Tab Switching: NextTab -> Teleport Tab
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    debug_menu::UpdateOverlaySnapshot();

    // 4. Tab 1 (Teleport) Row 0: Save Position
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.savePosCalls == 1, "Save position was not called");

    g_mock.allowSavePos = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.savePosCalls == 1, "Save position should be rejected without increment");

    // Row 1: Restore Position
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.restorePosCalls == 1, "Restore position was not called");

    g_mock.allowRestorePos = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.restorePosCalls == 1, "Restore position should be rejected without increment");

    // Row 2: Fill Current Coordinates
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);

    // Row 3: Coordinate Axis selection & adjustment
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Left);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Switch to Y axis
    debug_menu::HandleInput(debug_menu::InputAction::Right);

    // Row 4: Teleport to XYZ
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.teleportCalls == 1, "Teleport XYZ was not called");
    Require(g_mock.lastTeleport.x == 100.0f && g_mock.lastTeleport.y == 300.0f &&
                g_mock.lastTeleport.z == 300.0f,
            "Axis adjustment did not submit the expected XYZ coordinates");

    // Dirty Z, then refill without closing/reopening the overlay. Opening also
    // initializes XYZ, so testing only the initial fill misses a stale Z value.
    debug_menu::HandleInput(debug_menu::InputAction::Up); // Row 3, Y selected
    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Switch to Z
    debug_menu::HandleInput(debug_menu::InputAction::Right); // Z = 400
    debug_menu::HandleInput(debug_menu::InputAction::Up); // Row 2: Fill
    g_mock.snapshot.current = {-125.0f, 450.0f, 875.0f};
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Down); // Row 4: Teleport
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.teleportCalls == 2, "Teleport after refilling coordinates was not called");
    Require(g_mock.lastTeleport.x == -125.0f && g_mock.lastTeleport.y == 450.0f &&
                g_mock.lastTeleport.z == 875.0f,
            "Fill Coordinates did not replace every edited axis with the current position");

    // An unavailable snapshot must preserve the user's target, not partially
    // replace it with invalid coordinates.
    debug_menu::HandleInput(debug_menu::InputAction::Up);
    debug_menu::HandleInput(debug_menu::InputAction::Up); // Row 2: Fill
    g_mock.snapshot.available = false;
    g_mock.snapshot.current = {1.0f, 2.0f, 3.0f};
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.teleportCalls == 3, "Teleport after an unavailable fill was not called");
    Require(g_mock.lastTeleport.x == -125.0f && g_mock.lastTeleport.y == 450.0f &&
                g_mock.lastTeleport.z == 875.0f,
            "An unavailable Fill Coordinates request changed the target");
    g_mock.snapshot.available = true;

    g_mock.allowTeleport = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.teleportCalls == 3, "Teleport XYZ should be rejected without increment");

    // Row 5: Step Size Switch
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Right);

    // Row 6: POI Selection Cycling
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Left);

    // Row 7: Teleport to POI
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.poiTeleportCalls == 1, "POI teleport was not called");

    g_mock.allowPoiTeleport = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.poiTeleportCalls == 1, "POI teleport should be rejected without increment");

    // 5. Render overlay into PixelBuffer with Rasterizer
    host_ui::PixelBuffer frame;
    Require(frame.Resize(), "Logical overlay allocation failed");
    host_ui::Rasterizer rasterizer(frame);
    debug_menu::RenderOverlay(rasterizer);

    bool hasNonZeroPixel = false;
    for (uint32_t px : frame.pixels)
    {
        if (px != 0) { hasNonZeroPixel = true; break; }
    }
    Require(hasNonZeroPixel, "RenderOverlay produced completely blank frame");

    // Verify accurate text measurement (ASCII 9px, CJK 17px)
    Require(rasterizer.MeasureWString(L"A") == 9, "MeasureWString ASCII glyph width should be 9px");
    Require(rasterizer.MeasureWString(L"中") == 17, "MeasureWString CJK glyph width should be 17px");
    Require(rasterizer.MeasureWString(L"中文测试") == 17 * 4, "MeasureWString 4 CJK glyphs width should be 68px");

    // Verify translations bidirectional clean mapping
    Require(std::wstring(debug_menu::translations::Text(L"当前战斗判胜", false)) == L"Win Current Battle", "Win battle translation to English");
    Require(std::wstring(debug_menu::translations::Text(L"Win Current Battle", true)) == L"当前战斗判胜", "Win battle translation to Chinese");
    Require(std::wstring(debug_menu::translations::Poi(L"存档点 1", false)) == L"Save point 1", "POI translation to English");
    Require(std::wstring(debug_menu::translations::Poi(L"Save Point 1", true)) == L"存档点 1", "POI translation to Chinese");

    // 6. Dismiss overlay with Cancel (B / Esc)
    debug_menu::HandleInput(debug_menu::InputAction::Cancel);
    Require(!debug_menu::IsOverlayVisible(), "Cancel input did not hide overlay");
    Require(!host_ui::IsGamePaused(), "Cancel input did not unpause game");

    std::puts("PASS debug menu interaction: modal overlay, pause sync, navigation, error reporting, and rasterizer rendering");
    return 0;
}
