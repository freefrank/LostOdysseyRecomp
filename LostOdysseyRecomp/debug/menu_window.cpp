#include <stdafx.h>
#include "battle_menu.h"
#include "teleport.h"
#include "map_info.h"
#include "save_anywhere.h"
#include "translations.h"
#include <settings/config.h>
#include <settings/desktop_ui.h>
#include <settings/window_chrome.h>
#include <os/logger.h>
#include <gpu/renderer.h>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <algorithm>

#ifdef _WIN32
namespace
{
    HWND menu = nullptr;
    HWND viewport = nullptr;
    settings::window_chrome::State chrome;
    namespace ui = settings::window_chrome;
    struct ScopedDpi
    {
        DPI_AWARENESS_CONTEXT previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        ~ScopedDpi() { SetThreadDpiAwarenessContext(previous); }
    };
    HFONT uiFont = nullptr, sectionFont = nullptr;
    HWND languageList = nullptr, languageStatus = nullptr;
    bool chinese = false;
    struct LocalizedControl { HWND window; std::wstring key; };
    std::vector<LocalizedControl> localizedControls;
    std::unordered_map<HWND, std::wstring> displayedLabels;
    const wchar_t* Tr(const wchar_t* key) { return debug_menu::translations::Text(key, chinese); }

    HWND statusLabel = nullptr;
    HWND mapLabel = nullptr;
    HWND positionLabel = nullptr;
    HWND teleportStatus = nullptr;
    HWND coordinates[3]{};
    HWND teleportButtons[10]{};
    bool invalidCoordinates = false;
    HWND poiList = nullptr, poiButton = nullptr, poiDetails = nullptr;
    uint64_t poiRevision = ~uint64_t(0);
    std::vector<debug_menu::MapPoi> displayedPois;
    HWND saveToggle = nullptr;
    HWND captureButton = nullptr, captureStatus = nullptr;
    HWND pages[2]{};
    int activePage = 0;
    struct LayoutControl { HWND window; int x, y, width, height, page; bool section; };
    std::vector<LayoutControl> layoutControls;
    constexpr int contentWidth = 552;
    constexpr int pageHeights[] = {492, 486};
    int scrollX = 0, scrollY = 0, wheelRemainder = 0;
    LONG layoutWidth = -1, layoutHeight = -1;
    UINT layoutDpi = 0;

    void Layout(HWND window)
    {
        if (!viewport) return;
        RECT client{};
        GetClientRect(menu, &client);
        const int edge = ui::Px(menu, 20), top = ui::Px(menu, 144);
        if (layoutWidth != client.right || layoutHeight != client.bottom || layoutDpi != GetDpiForWindow(menu))
        {
            layoutWidth = client.right; layoutHeight = client.bottom; layoutDpi = GetDpiForWindow(menu);
            ui::Layout(menu, chrome);
            MoveWindow(languageList, client.right - ui::Px(menu, 180), ui::Px(menu, 68), ui::Px(menu, 160), ui::Px(menu, 150), TRUE);
            MoveWindow(languageStatus, edge, ui::Px(menu, 110), client.right - 2*edge, ui::Px(menu, 24), TRUE);
            for (int i = 0; i < 2; ++i)
                MoveWindow(pages[i], edge + ui::Px(menu, i*140), ui::Px(menu, 68), ui::Px(menu, 132), ui::Px(menu, 34), TRUE);
            MoveWindow(viewport, edge, top, std::max(1L, client.right-2*edge), std::max(1L, client.bottom-top-edge), TRUE);
        }
        GetClientRect(viewport, &client);
        const int width = ui::Px(menu, contentWidth), height = ui::Px(menu, pageHeights[activePage]);
        // SetScrollInfo does not consistently reset a retained position when
        // the resized client becomes larger than the virtual surface.
        scrollX = std::clamp(scrollX, 0, std::max(0, width - int(client.right)));
        scrollY = std::clamp(scrollY, 0, std::max(0, height - int(client.bottom)));
        // Keep both bars present so adding one cannot change the other axis's range.
        SCROLLINFO horizontal{sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL,
            0, width - 1, UINT(client.right), scrollX};
        SCROLLINFO vertical{sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL,
            0, height - 1, UINT(client.bottom), scrollY};
        SetScrollInfo(viewport, SB_HORZ, &horizontal, TRUE);
        SetScrollInfo(viewport, SB_VERT, &vertical, TRUE);
        scrollX = GetScrollPos(viewport, SB_HORZ);
        scrollY = GetScrollPos(viewport, SB_VERT);
        for (const auto& control : layoutControls)
        {
            ShowWindow(control.window, control.page == activePage ? SW_SHOWNOACTIVATE : SW_HIDE);
            MoveWindow(control.window, ui::Px(menu, control.x) - scrollX, ui::Px(menu, control.y) - scrollY,
                ui::Px(menu, control.width), ui::Px(menu, control.height), FALSE);
        }
        RedrawWindow(viewport, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }

    void RevealFocus()
    {
        const HWND focus = GetFocus();
        RECT client{};
        GetClientRect(viewport, &client);
        for (const auto& control : layoutControls)
        {
            if (control.window != focus || control.page != activePage) continue;
            // A combo's creation height includes its popup, not its closed field.
            RECT visible{};
            GetWindowRect(focus, &visible);
            const int height = visible.bottom - visible.top;
            const int x = ui::Px(menu, control.x), y = ui::Px(menu, control.y), width = ui::Px(menu, control.width);
            if (x < scrollX) scrollX = x;
            else if (x + width > scrollX + client.right) scrollX = x + width - client.right;
            if (y < scrollY) scrollY = y;
            else if (y + height > scrollY + client.bottom) scrollY = y + height - client.bottom;
            Layout(menu);
            break;
        }
    }

    void SelectPage(int page)
    {
        activePage = std::clamp(page, 0, 1);
        scrollX = scrollY = wheelRemainder = 0;
        Layout(menu);
        for (HWND button : pages) InvalidateRect(button, nullptr, FALSE);
    }

    void RefreshFonts()
    {
        HFONT previousUi = uiFont, previousSection = sectionFont;
        uiFont = ui::Font(menu, 15);
        sectionFont = ui::Font(menu, 18, FW_SEMIBOLD);
        for (const auto& control : layoutControls)
            SendMessageW(control.window, WM_SETFONT, reinterpret_cast<WPARAM>(control.section ? sectionFont : uiFont), TRUE);
        for (HWND control : {languageList, languageStatus, pages[0], pages[1]})
            if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
        if (previousUi) DeleteObject(previousUi);
        if (previousSection) DeleteObject(previousSection);
    }

    void SetLabel(HWND label, const wchar_t* text)
    {
        // Repeated WM_SETTEXT invalidates static controls even when unchanged.
        // Keep editable coordinate fields outside this display-only cache.
        text = Tr(text);
        auto& previous = displayedLabels[label];
        if (previous == text) return;
        previous = text;
        SetWindowTextW(label, text);
    }

    void RefreshLanguage()
    {
        chinese = settings::GetConfig().debugLanguage == 1;
        SetWindowTextW(menu, Tr(L"Lost Odyssey — Debug Menu (F1)"));
        for (const auto& control : localizedControls)
            SetLabel(control.window, control.key.c_str());
        SendMessageW(languageList, CB_SETCURSEL, chinese ? 1 : 0, 0);
        SetWindowTextW(pages[0], chinese ? L"常用" : L"Overview");
        SetWindowTextW(pages[1], chinese ? L"传送" : L"Teleport");
        SetWindowTextW(chrome.minimize, chinese ? L"最小化" : L"Minimize");
        SetWindowTextW(chrome.maximize, chinese ? L"最大化 / 还原" : L"Maximize / Restore");
        SetWindowTextW(chrome.close, chinese ? L"关闭" : L"Close");
        poiRevision = ~uint64_t(0); // Rebuild display names while preserving POI identity.
    }

    bool ReadPosition(debug_menu::Position& position)
    {
        float values[3]{};
        for (int i = 0; i < 3; ++i)
        {
            wchar_t text[96]{};
            GetWindowTextW(coordinates[i], text, 96);
            wchar_t* end = nullptr;
            values[i] = std::wcstof(text, &end);
            if (end == text) return false;
            while (std::iswspace(*end)) ++end;
            if (*end || !std::isfinite(values[i]) || std::abs(values[i]) > debug_menu::MaxTeleportCoordinate) return false;
        }
        position = {values[0], values[1], values[2]};
        return true;
    }

    LRESULT CALLBACK MenuProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        constexpr WPARAM GamepadA = 0xC3, GamepadB = 0xC4, GamepadUp = 0xCB, GamepadDown = 0xCC;
        LRESULT chromeResult{};
        if (ui::HandleMessage(window, message, wparam, lparam, chrome, chromeResult, 560, 380)) return chromeResult;
        if (message == WM_SIZE) { Layout(window); return 0; }
        if (message == WM_DPICHANGED)
        {
            const auto *suggested = reinterpret_cast<RECT *>(lparam);
            SetWindowPos(window, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            RefreshFonts();
            Layout(window);
            return 0;
        }
        if (message == WM_VSCROLL || message == WM_HSCROLL)
        {
            const int bar = message == WM_VSCROLL ? SB_VERT : SB_HORZ;
            SCROLLINFO info{sizeof(SCROLLINFO), SIF_ALL};
            GetScrollInfo(viewport, bar, &info);
            int position = info.nPos;
            switch (LOWORD(wparam))
            {
            case SB_LINEUP: position -= ui::Px(menu, 28); break;
            case SB_LINEDOWN: position += ui::Px(menu, 28); break;
            case SB_PAGEUP: position -= int(info.nPage); break;
            case SB_PAGEDOWN: position += int(info.nPage); break;
            case SB_THUMBTRACK: case SB_THUMBPOSITION: position = info.nTrackPos; break;
            case SB_TOP: position = 0; break;
            case SB_BOTTOM: position = info.nMax; break;
            }
            (bar == SB_VERT ? scrollY : scrollX) = position;
            Layout(window);
            return 0;
        }
        if (message == WM_MOUSEWHEEL)
        {
            wheelRemainder += GET_WHEEL_DELTA_WPARAM(wparam);
            scrollY -= (wheelRemainder / WHEEL_DELTA) * ui::Px(menu, 84);
            wheelRemainder %= WHEEL_DELTA;
            Layout(window);
            return 0;
        }
        if (message == WM_COMMAND)
        {
            if (LOWORD(wparam) == 200 || LOWORD(wparam) == 201)
            {
                SelectPage(LOWORD(wparam) - 200);
                return 0;
            }
            if (LOWORD(wparam) == 104 && HIWORD(wparam) == CBN_SELCHANGE)
            {
                const auto selected = SendMessageW(languageList, CB_GETCURSEL, 0, 0);
                if (selected == 0 || selected == 1)
                {
                    const bool saved = settings::SaveDebugLanguage(uint32_t(selected));
                    RefreshLanguage();
                    SetLabel(languageStatus, saved ? L"" : L"Could not save language. Check settings.ini permissions.");
                }
                return 0;
            }
            if (LOWORD(wparam) == 103) gpu::renderer::RequestDebugCapture();
            if (LOWORD(wparam) == 100) debug_menu::RequestVictory();
            if (LOWORD(wparam) == 101) debug_menu::CancelVictory();
            if (LOWORD(wparam) == 102)
                debug_menu::SetSaveAnywhereEnabled(
                    SendMessageW(reinterpret_cast<HWND>(lparam), BM_GETCHECK, 0, 0) == BST_CHECKED);
            const int id = LOWORD(wparam);
            if (id >= 10) invalidCoordinates = false;
            if (id == 10) debug_menu::RequestSavePosition();
            if (id == 11) debug_menu::RequestRestorePosition();
            if (id == 12)
            {
                const auto snapshot = debug_menu::GetTeleportSnapshot();
                if (snapshot.available)
                {
                    const float values[] = {snapshot.current.x, snapshot.current.y, snapshot.current.z};
                    for (int i = 0; i < 3; ++i)
                    {
                        wchar_t value[64]{};
                        std::swprintf(value, 64, L"%.3f", values[i]);
                        SetWindowTextW(coordinates[i], value);
                    }
                }
            }
            if (id == 13)
            {
                debug_menu::Position position{};
                if (ReadPosition(position)) debug_menu::RequestTeleport(position);
                else invalidCoordinates = true;
            }
            if (id >= 20 && id <= 25)
            {
                debug_menu::Position offset{};
                const float delta = (id % 2 == 0) ? -100.0f : 100.0f;
                if (id < 22) offset.x = delta;
                else if (id < 24) offset.y = delta;
                else offset.z = delta;
                debug_menu::RequestTeleportOffset(offset);
            }
            if (id == 31)
            {
                const auto selection = SendMessageW(poiList, CB_GETCURSEL, 0, 0);
                if (selection != CB_ERR && size_t(selection) < displayedPois.size())
                    debug_menu::RequestPoiTeleport(displayedPois[size_t(selection)].id);
            }
            return 0;
        }
        if (message == WM_DRAWITEM)
        {
            const auto& item = *reinterpret_cast<DRAWITEMSTRUCT *>(lparam);
            ui::DrawButton(item, item.CtlID == 103 || item.CtlID == UINT(200 + activePage));
            return TRUE;
        }
        if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLORBTN)
            return reinterpret_cast<LRESULT>(ui::ColorControl(reinterpret_cast<HDC>(wparam)));
        if (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX)
            return reinterpret_cast<LRESULT>(ui::ColorControl(reinterpret_cast<HDC>(wparam), true));
        if (message == WM_ERASEBKGND)
        {
            RECT client{};
            GetClientRect(window, &client);
            ui::Fill(reinterpret_cast<HDC>(wparam), client, ui::Surface);
            return TRUE;
        }
        if (message == WM_PAINT)
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            ui::Paint(window, dc, chrome);
            EndPaint(window, &paint);
            return 0;
        }
        if (message == WM_PRINTCLIENT)
        {
            ui::Paint(window, reinterpret_cast<HDC>(wparam), chrome);
            return 0;
        }
        if (message == WM_DESTROY)
        {
            ui::Destroy(chrome);
            if (uiFont) DeleteObject(uiFont);
            if (sectionFont) DeleteObject(sectionFont);
            uiFont = sectionFont = nullptr;
            menu = viewport = nullptr;
            localizedControls.clear(); layoutControls.clear(); displayedLabels.clear();
            displayedPois.clear(); poiRevision = ~uint64_t(0);
            scrollX = scrollY = activePage = wheelRemainder = 0;
            layoutWidth = layoutHeight = -1; layoutDpi = 0;
            return 0;
        }
        if (message == WM_KEYDOWN && (wparam == GamepadUp || wparam == GamepadDown))
        {
            if (HWND next = GetNextDlgTabItem(menu, GetFocus(), wparam == GamepadUp)) SetFocus(next);
            RevealFocus();
            return 0;
        }
        if (message == WM_KEYDOWN && wparam == GamepadA)
        {
            if (HWND focus = GetFocus()) SendMessageW(focus, BM_CLICK, 0, 0);
            return 0;
        }
        if (message == WM_CLOSE || (message == WM_KEYDOWN &&
            (wparam == VK_F1 || wparam == VK_ESCAPE || wparam == GamepadB)))
        {
            ShowWindow(window, SW_HIDE);
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    LRESULT CALLBACK ViewportProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        // Keep native scrollbars on this child while the root owns custom chrome.
        if (message == WM_COMMAND || message == WM_DRAWITEM || message == WM_CTLCOLORSTATIC ||
            message == WM_CTLCOLORBTN || message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX ||
            message == WM_VSCROLL || message == WM_HSCROLL || message == WM_MOUSEWHEEL || message == WM_KEYDOWN)
            return SendMessageW(menu, message, wparam, lparam);
        if (message == WM_ERASEBKGND)
        {
            RECT rect{}; GetClientRect(window, &rect);
            ui::Fill(reinterpret_cast<HDC>(wparam), rect, ui::Surface);
            return TRUE;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    HWND Control(const wchar_t* type, const wchar_t* text, DWORD style,
        int x, int y, int width, int height, int id = 0, int page = 0, bool section = false)
    {
        const DWORD tabStop = (std::wcscmp(type, L"STATIC") == 0 || (style & 0xf) == BS_GROUPBOX) ? 0 : WS_TABSTOP;
        const bool pushButton = std::wcscmp(type, L"BUTTON") == 0 && (style & 0xf) == BS_PUSHBUTTON;
        if (pushButton) style |= BS_OWNERDRAW;
        if (std::wcscmp(type, L"STATIC") == 0) style |= SS_NOPREFIX;
        HWND control = CreateWindowW(type, Tr(text), WS_CHILD | WS_VISIBLE | tabStop | style,
            0, 0, 0, 0, viewport, reinterpret_cast<HMENU>(intptr_t(id)), GetModuleHandleW(nullptr), nullptr);
        settings::desktop_ui::StyleControl(control, section ? sectionFont : uiFont);
        layoutControls.push_back({control, x, y, width, height, page, section});
        if (*text && std::wcscmp(type, L"EDIT") != 0 && std::wcscmp(type, L"COMBOBOX") != 0)
            localizedControls.push_back({control, text});
        return control;
    }
}
#endif

void debug_menu::Toggle()
{
#ifdef _WIN32
    const ScopedDpi dpi;
    if (!menu)
    {
        chinese = settings::GetConfig().debugLanguage == 1;
        WNDCLASSW wc{};
        wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), L"IDI_LOST_ODYSSEY_RECOMP");
        wc.lpfnWndProc = MenuProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = ui::Brush();
        wc.lpszClassName = L"LostOdysseyDebugMenu";
        RegisterClassW(&wc);
        menu = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, Tr(L"Lost Odyssey — Debug Menu (F1)"),
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
            660, 720, nullptr, nullptr, wc.hInstance, nullptr);
        if (!menu) { LOG_ERROR("debug menu: CreateWindow failed {}", GetLastError()); return; }
        ui::Create(menu, chrome);
        wc.lpfnWndProc = ViewportProc;
        wc.lpszClassName = L"LostOdysseyDebugViewport";
        RegisterClassW(&wc);
        viewport = CreateWindowExW(WS_EX_CONTROLPARENT, wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN,
            0, 0, 1, 1, menu, nullptr, wc.hInstance, nullptr);
        ui::StyleControl(viewport, nullptr);
        uiFont = settings::desktop_ui::Font(menu, 15);
        sectionFont = settings::desktop_ui::Font(menu, 18, FW_SEMIBOLD);
        auto fixed = [&](const wchar_t* kind, const wchar_t* text, DWORD style, int id) {
            HWND child = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1,
                menu, reinterpret_cast<HMENU>(intptr_t(id)), wc.hInstance, nullptr);
            ui::StyleControl(child, uiFont);
            return child;
        };
        pages[0] = fixed(L"BUTTON", chinese ? L"常用" : L"Overview", BS_OWNERDRAW | WS_TABSTOP, 200);
        pages[1] = fixed(L"BUTTON", chinese ? L"传送" : L"Teleport", BS_OWNERDRAW | WS_TABSTOP, 201);
        languageList = fixed(L"COMBOBOX", L"Language", CBS_DROPDOWNLIST | WS_TABSTOP, 104);
        SendMessageW(languageList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
        SendMessageW(languageList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"简体中文"));
        SendMessageW(languageList, CB_SETCURSEL, chinese ? 1 : 0, 0);
        languageStatus = fixed(L"STATIC", L"", SS_NOPREFIX, 0);
        Control(L"STATIC", L"Diagnostics", 0, 8, 8, 520, 28, 0, 0, true);
        captureButton = Control(L"BUTTON", L"截取渲染状态 / Capture render state", BS_PUSHBUTTON, 8, 48, 520, 38, 103);
        captureStatus = Control(L"EDIT", L"", ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
            8, 98, 520, 74);
        Control(L"STATIC", L"常用 / Quick settings", 0, 8, 188, 520, 28, 0, 0, true);
        saveToggle = Control(L"BUTTON", L"随时存档 / Save anywhere", BS_AUTOCHECKBOX,
            8, 226, 320, 28, 102);
        Control(L"STATIC", L"System → Save", 0, 346, 230, 190, 24);
        mapLabel = Control(L"STATIC", L"", 0, 8, 278, 520, 58);
        Control(L"STATIC", L"战斗 / Battle", 0, 8, 354, 520, 28, 0, 0, true);
        statusLabel = Control(L"STATIC", L"", 0, 8, 392, 520, 36);
        Control(L"BUTTON", L"当前战斗判胜 / Win battle", BS_PUSHBUTTON, 8, 440, 328, 36, 100);
        Control(L"BUTTON", L"取消请求", BS_PUSHBUTTON, 348, 440, 188, 36, 101);
        Control(L"STATIC", L"Position", 0, 8, 8, 520, 28, 0, 1, true);
        positionLabel = Control(L"STATIC", L"", 0, 8, 46, 520, 26, 0, 1);
        teleportButtons[0] = Control(L"BUTTON", L"记住当前位置", BS_PUSHBUTTON, 8, 86, 168, 36, 10, 1);
        teleportButtons[1] = Control(L"BUTTON", L"返回记录位置", BS_PUSHBUTTON, 188, 86, 168, 36, 11, 1);
        teleportButtons[2] = Control(L"BUTTON", L"填入当前坐标", BS_PUSHBUTTON, 368, 86, 168, 36, 12, 1);
        const wchar_t* axes[] = {L"X", L"Y", L"Z"};
        for (int i = 0; i < 3; ++i)
        {
            Control(L"STATIC", axes[i], 0, 8 + i * 138, 144, 18, 26, 0, 1);
            coordinates[i] = Control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 28 + i * 138, 140, 106, 30, 0, 1);
        }
        teleportButtons[3] = Control(L"BUTTON", L"传送到坐标", BS_PUSHBUTTON, 422, 140, 114, 32, 13, 1);
        const wchar_t* offsets[] = {L"X −100", L"X +100", L"Y −100", L"Y +100", L"Z −100", L"Z +100"};
        for (int i = 0; i < 6; ++i)
            teleportButtons[4+i] = Control(L"BUTTON", offsets[i], BS_PUSHBUTTON, 8+i*90, 188, 78, 32, 20+i, 1);
        teleportStatus = Control(L"STATIC", L"", 0, 8, 236, 520, 56, 0, 1);
        Control(L"STATIC", L"Points of interest", 0, 8, 316, 520, 28, 0, 1, true);
        poiList = Control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 8, 360, 370, 220, 30, 1);
        poiButton = Control(L"BUTTON", L"传送到此 POI", BS_PUSHBUTTON, 390, 360, 146, 32, 31, 1);
        poiDetails = Control(L"STATIC", L"", 0, 8, 410, 520, 58, 0, 1);
        // Fit the initial window to the current monitor; scrolling keeps every control reachable.
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (GetMonitorInfoW(MonitorFromWindow(menu, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            const auto& area = monitor.rcWork;
            const int width = std::min(LONG(ui::Px(menu, 624)), area.right - area.left);
            const int height = std::min(LONG(ui::Px(menu, 700)), area.bottom - area.top);
            SetWindowPos(menu, nullptr, area.left + (area.right - area.left - width) / 2,
                area.top + (area.bottom - area.top - height) / 2, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        RefreshLanguage();
        Layout(menu);
    }
    const bool opening = !IsWindowVisible(menu);
    ShowWindow(menu, opening ? (getenv("LO_BACKGROUND") ? SW_SHOWNOACTIVATE : SW_SHOW) : SW_HIDE);
    if (opening && !getenv("LO_BACKGROUND")) SetFocus(pages[activePage]);
    LOG_INFO("debug menu: window visible {}", IsWindowVisible(menu) != FALSE);
    Update();
#endif
}

void debug_menu::Update()
{
#ifdef _WIN32
    const ScopedDpi dpi;
    if (captureStatus && IsWindowVisible(menu))
    {
        const auto status = gpu::renderer::DebugCaptureStatus();
        if (!status.empty()) SetLabel(captureStatus, translations::Capture(status, chinese).c_str());
        EnableWindow(captureButton, !gpu::renderer::DebugCaptureBusy());
    }
    // Optional startup visibility for repeatable UI validation without injected keys.
    static bool openOnStartup = getenv("LO_DEBUG_MENU_OPEN") != nullptr;
    if (openOnStartup) { openOnStartup = false; Toggle(); }
    // Drain only this tool window's messages before SDL pumps the game window.
    // IsDialogMessage supplies Tab navigation without injecting system input.
    if (menu)
    {
        MSG message{};
        while (PeekMessageW(&message, menu, 0, 0, PM_REMOVE))
        {
            // Close shortcuts also apply while a child edit/button owns focus.
            if (message.message == WM_KEYDOWN && (message.wParam == VK_F1 ||
                message.wParam == VK_ESCAPE || message.wParam == 0xC4 /* Gamepad B */))
            {
                ShowWindow(menu, SW_HIDE);
                continue;
            }
            if (message.message == WM_KEYDOWN && (message.wParam == 0xCB || message.wParam == 0xCC || message.wParam == 0xC3))
            {
                SendMessageW(menu, message.message, message.wParam, message.lParam);
                continue;
            }
            if (!IsDialogMessageW(menu, &message))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (message.message == WM_KEYDOWN && message.wParam == VK_TAB) RevealFocus();
        }
    }
    if (saveToggle && IsWindowVisible(menu))
        SendMessageW(saveToggle, BM_SETCHECK, SaveAnywhereEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
    if (mapLabel && IsWindowVisible(menu)) {
        const auto map = GetMapInfo();
        std::wstring text = Tr(L"当前地图 / Map: 加载中或尚未识别");
        if (map.available) text = std::wstring(Tr(L"地图 ID / Map ID: ")) + std::to_wstring(map.id) + L"  [" + map.package + L"]\n" +
            (map.name.empty() ? Tr(L"地图名称尚未加载 / Name unavailable") : map.name);
        SetLabel(mapLabel, text.c_str());
    }
    if (statusLabel && IsWindowVisible(menu)) SetLabel(statusLabel, Status());
    if (positionLabel && IsWindowVisible(menu))
    {
        const auto snapshot = GetTeleportSnapshot();
        wchar_t text[160]{};
        if (snapshot.available)
            std::swprintf(text, 160, L"X %.2f    Y %.2f    Z %.2f", snapshot.current.x, snapshot.current.y, snapshot.current.z);
        else std::swprintf(text, 160, L"%ls", Tr(L"等待可控制的地图角色 / No controllable map character"));
        SetLabel(positionLabel, text);
        SetLabel(teleportStatus, invalidCoordinates ? L"坐标须为 −1000000 到 1000000 范围内的数值。" : snapshot.status.c_str());
        for (int i = 0; i < 10; ++i)
            EnableWindow(teleportButtons[i], snapshot.available && (i != 1 || snapshot.bookmarkAvailable));
        if (poiRevision != snapshot.poiRevision || displayedPois.size() != snapshot.pois.size())
        {
            const auto oldSelection = SendMessageW(poiList, CB_GETCURSEL, 0, 0);
            const uint64_t selectedId = oldSelection != CB_ERR && size_t(oldSelection) < displayedPois.size()
                ? displayedPois[size_t(oldSelection)].id : 0;
            SendMessageW(poiList, CB_RESETCONTENT, 0, 0);
            size_t selected = 0;
            for (size_t i = 0; i < snapshot.pois.size(); ++i)
            {
                SendMessageW(poiList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(translations::Poi(snapshot.pois[i].label, chinese).c_str()));
                if (snapshot.pois[i].id == selectedId) selected = i;
            }
            if (!snapshot.pois.empty()) SendMessageW(poiList, CB_SETCURSEL, selected, 0);
            poiRevision = snapshot.poiRevision;
        }
        displayedPois = snapshot.pois;
        const auto selection = SendMessageW(poiList, CB_GETCURSEL, 0, 0);
        const bool selected = snapshot.available && selection != CB_ERR && size_t(selection) < displayedPois.size();
        EnableWindow(poiList, snapshot.available && !displayedPois.empty());
        EnableWindow(poiButton, selected);
        if (selected)
        {
            const auto p = displayedPois[size_t(selection)].position;
            const auto c = snapshot.current;
            const float distance = std::sqrt((p.x-c.x)*(p.x-c.x)+(p.y-c.y)*(p.y-c.y)+(p.z-c.z)*(p.z-c.z));
            wchar_t details[180]{};
            std::swprintf(details, 180, Tr(L"落点 X %.1f  Y %.1f  Z %.1f\n距离 %.0f（游戏单位）"), p.x,p.y,p.z,distance);
            SetLabel(poiDetails, details);
        }
        else SetLabel(poiDetails, snapshot.available ? L"当前地图没有识别到 POI" : L"地图控制恢复后自动更新 POI");
    }
#endif
}
