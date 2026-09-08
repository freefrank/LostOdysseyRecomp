// Production native window/message pump, with GPU/game state supplied by stubs.
// Runs on a non-input desktop and never injects global keyboard/mouse input.
#include "../../LostOdysseyRecomp/debug/menu_window.cpp"
#include <cassert>
#include <cstdio>
namespace debug_menu {
void RequestVictory() {} void CancelVictory() {} const wchar_t* Status() { return L"Ready"; }
bool SaveAnywhereEnabled() { return false; } void SetSaveAnywhereEnabled(bool) {}
MapInfo GetMapInfo() { return {true, 13, L"Fixture map", L"map_13"}; }
TeleportSnapshot GetTeleportSnapshot() { TeleportSnapshot s; s.available = true; s.current = {10,20,30}; return s; }
bool RequestTeleport(Position) { return true; } bool RequestTeleportOffset(Position) { return true; }
bool RequestSavePosition() { return true; } bool RequestRestorePosition() { return true; }
bool RequestPoiTeleport(uint64_t) { return true; }
}
namespace settings {
Config GetConfig() { Config value; value.debugLanguage = 1; return value; }
bool SaveDebugLanguage(uint32_t) { return true; }
}
namespace gpu::renderer {
bool busy = false;
std::wstring status = L"Ready";
void RequestDebugCapture() {}
std::wstring DebugCaptureStatus() { return status; }
bool DebugCaptureBusy() { return busy; }
}

int main()
{
    HDESK desktop = CreateDesktopW(L"LO_DebugMenu_Interaction_Test", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    assert(desktop && SetThreadDesktop(desktop));
    _putenv_s("LO_DEBUG_MENU_OPEN", "1");
    debug_menu::Update();
    assert(menu && IsWindowVisible(menu));
    for (const WPARAM key : {WPARAM(VK_F1), WPARAM(VK_ESCAPE), WPARAM(0xC4)})
    {
        SetFocus(coordinates[0]);
        assert(GetFocus() == coordinates[0]);
        PostMessageW(coordinates[0], WM_KEYDOWN, key, 0);
        debug_menu::Update();
        assert(!IsWindowVisible(menu));
        debug_menu::Toggle();
        assert(IsWindowVisible(menu));
    }
    gpu::renderer::busy = true;
    gpu::renderer::status = L"正在截取 / Capturing: fixture-frame";
    debug_menu::Update();
    assert(!IsWindowEnabled(captureButton));
    wchar_t text[512]{};
    GetWindowTextW(captureStatus, text, int(std::size(text)));
    assert(std::wcsstr(text, L"fixture-frame"));
    gpu::renderer::busy = false;
    gpu::renderer::status = L"ZIP 失败，原始文件保留 / ZIP failed: fixture-raw";
    debug_menu::Update();
    assert(IsWindowEnabled(captureButton));
    GetWindowTextW(captureStatus, text, int(std::size(text)));
    assert(std::wcsstr(text, L"fixture-raw") && std::wcsstr(text, L"失败"));
    DestroyWindow(menu);
    std::printf("PASS startup open; child-focused F1/Esc/Gamepad B hide; busy disabled/status; failure status/re-enabled\n");
}
