// Production native window/message pump, with GPU/game state supplied by stubs.
// Runs on a non-input desktop and never injects global keyboard/mouse input.
#include "../../LostOdysseyRecomp/debug/menu_window.cpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
namespace {
void CaptureWindow(const std::filesystem::path& path)
{
    RECT rect{}; GetWindowRect(menu, &rect);
    const int width = rect.right-rect.left, height = rect.bottom-rect.top;
    HDC screen = GetDC(menu), memory = CreateCompatibleDC(screen);
    BITMAPINFO info{}; info.bmiHeader = {sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB};
    void* pixels{}; HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    assert(bitmap && pixels); HGDIOBJ old = SelectObject(memory, bitmap);
    RedrawWindow(menu, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    // Match the validated inactive-desktop updater capture contract.
    SendMessageW(menu, WM_PRINT, reinterpret_cast<WPARAM>(memory), PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    BITMAPFILEHEADER file{}; file.bfType = 0x4D42; file.bfOffBits = sizeof(file) + sizeof(info.bmiHeader);
    file.bfSize = file.bfOffBits + width*height*4;
    FILE* output{}; _wfopen_s(&output, path.c_str(), L"wb"); assert(output);
    fwrite(&file, sizeof(file), 1, output); fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, output);
    fwrite(pixels, size_t(width)*height*4, 1, output); fclose(output);
    SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(menu, screen);
}
uint32_t selectedLanguage = 1;
int captures = 0, teleports = 0;
}
namespace debug_menu {
void RequestVictory() {} void CancelVictory() {} const wchar_t* Status() { return L"Ready"; }
bool SaveAnywhereEnabled() { return false; } void SetSaveAnywhereEnabled(bool) {}
MapInfo GetMapInfo() { return {true, 13, L"Fixture map", L"map_13"}; }
TeleportSnapshot GetTeleportSnapshot() { TeleportSnapshot s; s.available = true; s.current = {10,20,30}; return s; }
bool RequestTeleport(Position) { ++teleports; return true; } bool RequestTeleportOffset(Position) { return true; }
bool RequestSavePosition() { return true; } bool RequestRestorePosition() { return true; }
bool RequestPoiTeleport(uint64_t) { return true; }
}
namespace settings {
Config GetConfig() { Config value; value.debugLanguage = selectedLanguage; return value; }
bool SaveDebugLanguage(uint32_t value) { selectedLanguage = value; return true; }
}
namespace gpu::renderer {
bool busy = false;
std::wstring status = L"Ready";
void RequestDebugCapture() { ++captures; }
std::wstring DebugCaptureStatus() { return status; }
bool DebugCaptureBusy() { return busy; }
}

int main(int argc, char** argv)
{
    HDESK desktop = CreateDesktopW(L"LO_DebugMenu_Interaction_Test", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    assert(desktop && SetThreadDesktop(desktop));
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    _putenv_s("LO_BACKGROUND", ""); // Exercise normal product focus on this non-input desktop.
    _putenv_s("LO_DEBUG_MENU_OPEN", "1");
    debug_menu::Update();
    assert(menu && IsWindowVisible(menu));
    std::filesystem::path output = argc > 1 ? argv[1] : "out/v0.5.0/ui-modernization/native/debug";
    std::filesystem::create_directories(output);
    if (argc > 2 && std::string_view(argv[2]) == "--capture-only")
    {
        selectedLanguage = 0;
        RefreshLanguage();
        debug_menu::Update();
        CaptureWindow(output / "overview-en.bmp");
        gpu::renderer::status = L"ZIP 失败，原始文件保留 / ZIP failed: C:\\LostOdyssey\\captures\\long-path-for-render-capture\\2026-09-08\\frame-and-resource-details\\retained-original-files\\capture-render-state-with-a-long-filename-for-inspection.zip";
        debug_menu::Update();
        CaptureWindow(output / "long-error-en.bmp");
        DestroyWindow(menu);
        std::printf("CAPTURE ONLY: current-language overview and long error; no repeated functional checks\n");
        return 0;
    }
    assert(IsWindowVisible(captureButton) && !IsWindowVisible(coordinates[0]));
    assert(GetParent(captureButton) == viewport);
    assert(GetWindowLongPtrW(viewport, GWL_EXSTYLE) & WS_EX_CONTROLPARENT);
    CaptureWindow(output / "overview-zh.bmp");
    SendMessageW(captureButton, BM_CLICK, 0, 0); assert(captures == 1);
    SendMessageW(pages[1], BM_CLICK, 0, 0);
    assert(activePage == 1 && IsWindowVisible(coordinates[0]) && !IsWindowVisible(captureButton));
    SendMessageW(teleportButtons[2], BM_CLICK, 0, 0);
    SendMessageW(teleportButtons[3], BM_CLICK, 0, 0); assert(teleports == 1);
    CaptureWindow(output / "teleport-zh.bmp");
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
    const HFONT titleFont = chrome.font;
    RECT titleBefore{}; GetWindowRect(chrome.close, &titleBefore);
    for (int i = 0; i < 5; ++i) SendMessageW(viewport, WM_VSCROLL, SB_BOTTOM, 0);
    RECT titleAfter{}; GetWindowRect(chrome.close, &titleAfter);
    assert(chrome.font == titleFont && EqualRect(&titleBefore, &titleAfter));
    SetWindowPos(menu, nullptr, 0, 0, ui::Px(menu, 560), ui::Px(menu, 380), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    SetFocus(teleportButtons[3]); RevealFocus();
    RECT controlRect{}, clipRect{}; GetWindowRect(teleportButtons[3], &controlRect);
    GetClientRect(viewport, &clipRect); MapWindowPoints(viewport, nullptr, reinterpret_cast<POINT*>(&clipRect), 2);
    assert(controlRect.top >= clipRect.top && controlRect.bottom <= clipRect.bottom);
    CaptureWindow(output / "small-focus.bmp");
    SetWindowPos(menu, nullptr, 0, 0, ui::Px(menu, 720), ui::Px(menu, 800), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    assert(scrollX == 0 && scrollY == 0);
    POINT hit{ui::Px(menu, 30), ui::Px(menu, 24)}; ClientToScreen(menu, &hit);
    assert(SendMessageW(menu, WM_NCHITTEST, 0, MAKELPARAM(hit.x, hit.y)) == HTCAPTION);
    hit = {1, 1}; ClientToScreen(menu, &hit);
    assert(SendMessageW(menu, WM_NCHITTEST, 0, MAKELPARAM(hit.x, hit.y)) == HTTOPLEFT);
    SendMessageW(chrome.maximize, BM_CLICK, 0, 0); assert(IsZoomed(menu));
    SendMessageW(chrome.maximize, BM_CLICK, 0, 0); assert(!IsZoomed(menu));
    SendMessageW(chrome.minimize, BM_CLICK, 0, 0); assert(IsIconic(menu));
    SendMessageW(menu, WM_SYSCOMMAND, SC_RESTORE, 0); assert(!IsIconic(menu));
    RECT suggested{}; GetWindowRect(menu, &suggested);
    SendMessageW(menu, WM_DPICHANGED, MAKELONG(GetDpiForWindow(menu), GetDpiForWindow(menu)), reinterpret_cast<LPARAM>(&suggested));
    LOGFONTW actual{}; assert(GetObjectW(uiFont, sizeof(actual), &actual));
    assert(actual.lfHeight == -ui::Px(menu, 15));
    SendMessageW(pages[0], BM_CLICK, 0, 0);
    SendMessageW(languageList, CB_SETCURSEL, 0, 0);
    SendMessageW(menu, WM_COMMAND, MAKEWPARAM(104, CBN_SELCHANGE), reinterpret_cast<LPARAM>(languageList));
    assert(!chinese && activePage == 0);
    CaptureWindow(output / "overview-en.bmp");
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
    assert(std::wcsstr(text, L"fixture-raw") && std::wcsstr(text, L"failed"));
    CaptureWindow(output / "capture-error.bmp");
    SendMessageW(chrome.close, BM_CLICK, 0, 0); assert(!IsWindowVisible(menu));
    DestroyWindow(menu);
    assert(!menu && !viewport && !chrome.font && !uiFont && layoutControls.empty());
    std::printf("PASS new chrome hit regions/close; paged callbacks; viewport focus/reclamp; stable title font; current-DPI layout; bilingual/status; child F1/Esc/B dismissal; cleanup\n");
}
