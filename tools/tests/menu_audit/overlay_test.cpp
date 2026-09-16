#include <debug/menu_overlay.h>
#include <debug/teleport.h>
#include <host_ui/host_ui.h>
#include <cstdio>
#include <cstdlib>
#include <mutex>

static void Require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
namespace settings
{
struct Config { int debugLanguage = 0; };
Config GetConfig() { return {}; }
bool SaveDebugLanguage(uint32_t) { return true; }
}
namespace gpu::renderer { void RequestDebugCapture() {} }
namespace debug_menu
{
static TeleportSnapshot fixture;
static Position submitted{};
static bool submittedRequest = false;
TeleportSnapshot GetTeleportSnapshot() { return fixture; }
bool RequestTeleport(Position p) { submitted = p; submittedRequest = true; return true; }
bool RequestSavePosition() { return true; }
bool RequestRestorePosition() { return true; }
bool RequestPoiTeleport(uint64_t) { return true; }
bool RequestVictory() { return true; }
void CancelVictory() {}
bool SaveAnywhereEnabled() { return false; }
void SetSaveAnywhereEnabled(bool) {}
}
// Complete production model and HandleInput, excluding only the rasterizer.
#include "overlay_logic.inc"

int main()
{
    using namespace debug_menu;
    fixture.available = true;
    fixture.current = {100, 200, 300};
    ToggleOverlay();
    Require(host_ui::IsGamePaused(), "overlay did not pause");
    HandleInput(InputAction::NextTab);
    for (int i = 0; i < 3; ++i) HandleInput(InputAction::Down); // axis editing
    HandleInput(InputAction::Confirm); // Y
    HandleInput(InputAction::Confirm); // Z
    for (int i = 0; i < 6; ++i) HandleInput(InputAction::Right); // Z=900
    Require(GetOverlayStateSnapshot().editCoordinates[2] == 900, "fixture did not change Z");
    HandleInput(InputAction::Up); // fill coordinates
    HandleInput(InputAction::Confirm);
    auto snapshot = GetOverlayStateSnapshot();
    Require(snapshot.editCoordinates[0] == 100 && snapshot.editCoordinates[1] == 200 &&
            snapshot.editCoordinates[2] == 300, "Fill Coordinates left a stale component");
    HandleInput(InputAction::Down);
    HandleInput(InputAction::Down); // submit XYZ
    HandleInput(InputAction::Confirm);
    Require(submittedRequest && submitted.x == 100 && submitted.y == 200 && submitted.z == 300,
            "RequestTeleport did not receive the fully restored XYZ");
    HandleInput(InputAction::Cancel);
    Require(!IsOverlayVisible() && !host_ui::IsGamePaused(), "cancel did not resume");
    std::puts("PASS: actual overlay navigation, edit Z=900, refill Z=300, exact RequestTeleport payload, resume");
}
