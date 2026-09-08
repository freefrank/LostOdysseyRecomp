#include <stdafx.h>
#include "battle_menu.h"
#include "teleport.h"
#include "map_info.h"
#include "save_anywhere.h"
#include "translations.h"
#include <settings/config.h>
#include <settings/desktop_ui.h>
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
    HFONT uiFont = nullptr, sectionFont = nullptr;
    HWND languageList = nullptr, languageStatus = nullptr;
    bool chinese = false;
    struct LocalizedControl { HWND window; std::wstring key; };
    std::vector<LocalizedControl> localizedControls;
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
    struct LayoutControl { HWND window; int x, y, width, height; };
    std::vector<LayoutControl> layoutControls;
    constexpr int contentWidth = 540, contentHeight = 900;
    int scrollX = 0, scrollY = 0, wheelRemainder = 0;

    void Layout(HWND window)
    {
        RECT client{};
        GetClientRect(window, &client);
        // SetScrollInfo does not consistently reset a retained position when
        // the resized client becomes larger than the virtual surface.
        scrollX = std::clamp(scrollX, 0, std::max(0, contentWidth - int(client.right)));
        scrollY = std::clamp(scrollY, 0, std::max(0, contentHeight - int(client.bottom)));
        // Keep both bars present so adding one cannot change the other axis's range.
        SCROLLINFO horizontal{sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL,
            0, contentWidth - 1, UINT(client.right), scrollX};
        SCROLLINFO vertical{sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL,
            0, contentHeight - 1, UINT(client.bottom), scrollY};
        SetScrollInfo(window, SB_HORZ, &horizontal, TRUE);
        SetScrollInfo(window, SB_VERT, &vertical, TRUE);
        scrollX = GetScrollPos(window, SB_HORZ);
        scrollY = GetScrollPos(window, SB_VERT);
        for (const auto& control : layoutControls)
            MoveWindow(control.window, control.x - scrollX, control.y - scrollY,
                control.width, control.height, FALSE);
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }

    void RevealFocus()
    {
        const HWND focus = GetFocus();
        RECT client{};
        GetClientRect(menu, &client);
        for (const auto& control : layoutControls)
        {
            if (control.window != focus) continue;
            // A combo's creation height includes its popup, not its closed field.
            RECT visible{};
            GetWindowRect(focus, &visible);
            const int height = visible.bottom - visible.top;
            if (control.x < scrollX) scrollX = control.x;
            else if (control.x + control.width > scrollX + client.right)
                scrollX = control.x + control.width - client.right;
            if (control.y < scrollY) scrollY = control.y;
            else if (control.y + height > scrollY + client.bottom)
                scrollY = control.y + height - client.bottom;
            Layout(menu);
            break;
        }
    }

    void SetLabel(HWND label, const wchar_t* text)
    {
        // Repeated WM_SETTEXT invalidates static controls even when unchanged.
        // Keep editable coordinate fields outside this display-only cache.
        static std::unordered_map<HWND, std::wstring> displayed;
        text = Tr(text);
        auto& previous = displayed[label];
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
        if (message == WM_SIZE) { Layout(window); return 0; }
        if (message == WM_DPICHANGED)
        {
            const auto *suggested = reinterpret_cast<RECT *>(lparam);
            SetWindowPos(window, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            Layout(window);
            return 0;
        }
        if (message == WM_VSCROLL || message == WM_HSCROLL)
        {
            const int bar = message == WM_VSCROLL ? SB_VERT : SB_HORZ;
            SCROLLINFO info{sizeof(SCROLLINFO), SIF_ALL};
            GetScrollInfo(window, bar, &info);
            int position = info.nPos;
            switch (LOWORD(wparam))
            {
            case SB_LINEUP: position -= 28; break;
            case SB_LINEDOWN: position += 28; break;
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
            scrollY -= (wheelRemainder / WHEEL_DELTA) * 84;
            wheelRemainder %= WHEEL_DELTA;
            Layout(window);
            return 0;
        }
        if (message == WM_COMMAND)
        {
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
            settings::desktop_ui::DrawButton(*reinterpret_cast<DRAWITEMSTRUCT *>(lparam),
                                             reinterpret_cast<DRAWITEMSTRUCT *>(lparam)->CtlID == 103);
            return TRUE;
        }
        if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLORBTN)
            return reinterpret_cast<LRESULT>(settings::desktop_ui::ColorControl(reinterpret_cast<HDC>(wparam)));
        if (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX)
            return reinterpret_cast<LRESULT>(settings::desktop_ui::ColorControl(reinterpret_cast<HDC>(wparam), true));
        if (message == WM_ERASEBKGND)
        {
            RECT client{};
            GetClientRect(window, &client);
            settings::desktop_ui::Fill(reinterpret_cast<HDC>(wparam), client, settings::desktop_ui::Surface);
            RECT accent{0, 0, 4, client.bottom};
            settings::desktop_ui::Fill(reinterpret_cast<HDC>(wparam), accent, settings::desktop_ui::Accent);
            return TRUE;
        }
        if (message == WM_KEYDOWN && (wparam == GamepadUp || wparam == GamepadDown))
        {
            SendMessageW(window, WM_NEXTDLGCTL, wparam == GamepadUp, FALSE);
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

    HWND Control(const wchar_t* type, const wchar_t* text, DWORD style,
        int x, int y, int width, int height, int id = 0)
    {
        const DWORD tabStop = (std::wcscmp(type, L"STATIC") == 0 || (style & 0xf) == BS_GROUPBOX) ? 0 : WS_TABSTOP;
        y += 190; // Capture action/status and language selector precede the original layout.
        const bool pushButton = std::wcscmp(type, L"BUTTON") == 0 && (style & 0xf) == BS_PUSHBUTTON;
        if (pushButton) style |= BS_OWNERDRAW;
        HWND control = CreateWindowW(type, Tr(text), WS_CHILD | WS_VISIBLE | tabStop | style,
            x, y, width, height, menu, reinterpret_cast<HMENU>(intptr_t(id)), GetModuleHandleW(nullptr), nullptr);
        const bool section = std::wcscmp(type, L"STATIC") == 0 && (y == 232 || y == 326 || y == 426 || y == 568);
        settings::desktop_ui::StyleControl(control, section ? sectionFont : uiFont);
        layoutControls.push_back({control, x, y, width, height});
        if (*text && std::wcscmp(type, L"EDIT") != 0 && std::wcscmp(type, L"COMBOBOX") != 0)
            localizedControls.push_back({control, text});
        return control;
    }
}
#endif

void debug_menu::Toggle()
{
#ifdef _WIN32
    if (!menu)
    {
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        chinese = settings::GetConfig().debugLanguage == 1;
        WNDCLASSW wc{};
        wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), L"IDI_LOST_ODYSSEY_RECOMP");
        wc.lpfnWndProc = MenuProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = settings::desktop_ui::SurfaceBrush();
        wc.lpszClassName = L"LostOdysseyDebugMenu";
        RegisterClassW(&wc);
        menu = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, Tr(L"Lost Odyssey — Debug Menu (F1)"),
            WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
            660, 780, nullptr, nullptr, wc.hInstance, nullptr);
        if (!menu) { LOG_ERROR("debug menu: CreateWindow failed {}", GetLastError()); return; }
        settings::desktop_ui::EnableDarkFrame(menu);
        uiFont = settings::desktop_ui::Font(menu, 15);
        sectionFont = settings::desktop_ui::Font(menu, 17, FW_SEMIBOLD);
        captureButton = Control(L"BUTTON", L"截取渲染状态 / Capture render state", BS_PUSHBUTTON, 24, -176, 490, 30, 103);
        captureStatus = Control(L"STATIC", L"截取下一完整帧；导出期间可能短暂停顿。", 0, 24, -140, 490, 80);
        Control(L"STATIC", L"Language", 0, 20, -36, 110, 24);
        languageList = Control(L"COMBOBOX", L"", CBS_DROPDOWNLIST, 140, -40, 180, 100, 104);
        SendMessageW(languageList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
        SendMessageW(languageList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"简体中文"));
        SendMessageW(languageList, CB_SETCURSEL, chinese ? 1 : 0, 0);
        languageStatus = Control(L"STATIC", L"", 0, 20, -12, 500, 24);
        Control(L"STATIC", L"F1 打开/关闭 · 本窗口不会暂停游戏", 0, 20, 14, 500, 24);
        Control(L"STATIC", L"常用 / Quick settings", 0, 20, 42, 500, 20);
        saveToggle = Control(L"BUTTON", L"随时存档 / Save anywhere", BS_AUTOCHECKBOX,
            24, 66, 490, 25, 102);
        Control(L"STATIC", L"开启后重新进入 System 菜单，再选择 Save。", 0, 24, 97, 490, 24);
        Control(L"STATIC", L"当前地图 / Map", 0, 20, 136, 500, 20);
        mapLabel = Control(L"STATIC", L"", 0, 24, 160, 490, 60);
        Control(L"STATIC", L"战斗 / Battle", 0, 20, 236, 500, 20);
        statusLabel = Control(L"STATIC", L"", 0, 24, 258, 490, 40);
        Control(L"BUTTON", L"当前战斗判胜 / Win battle", BS_PUSHBUTTON, 24, 300, 300, 30, 100);
        Control(L"BUTTON", L"取消请求", BS_PUSHBUTTON, 334, 300, 180, 30, 101);
        Control(L"STATIC", L"一次性请求；在战斗判定点执行。", 0, 24, 339, 490, 24);
        Control(L"STATIC", L"人物传送 / Teleport（仅当前地图）", 0, 20, 378, 500, 20);
        positionLabel = Control(L"STATIC", L"", 0, 24, 402, 490, 24);
        teleportButtons[0] = Control(L"BUTTON", L"记住当前位置", BS_PUSHBUTTON, 24, 432, 150, 30, 10);
        teleportButtons[1] = Control(L"BUTTON", L"返回记录位置", BS_PUSHBUTTON, 184, 432, 150, 30, 11);
        teleportButtons[2] = Control(L"BUTTON", L"填入当前坐标", BS_PUSHBUTTON, 344, 432, 170, 30, 12);
        const wchar_t* axes[] = {L"X", L"Y", L"Z"};
        for (int i = 0; i < 3; ++i)
        {
            Control(L"STATIC", axes[i], 0, 24 + i * 124, 478, 18, 24);
            coordinates[i] = Control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 42 + i * 124, 474, 98, 28);
        }
        teleportButtons[3] = Control(L"BUTTON", L"传送到坐标", BS_PUSHBUTTON, 402, 474, 112, 30, 13);
        const wchar_t* offsets[] = {L"X −100", L"X +100", L"Y −100", L"Y +100", L"Z −100", L"Z +100"};
        for (int i = 0; i < 6; ++i)
            teleportButtons[4+i] = Control(L"BUTTON", offsets[i], BS_PUSHBUTTON, 24+i*83, 514, 75, 30, 20+i);
        teleportStatus = Control(L"STATIC", L"", 0, 24, 550, 490, 36);
        poiList = Control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 24, 592, 345, 220, 30);
        poiButton = Control(L"BUTTON", L"传送到此 POI", BS_PUSHBUTTON, 379, 592, 135, 30, 31);
        poiDetails = Control(L"STATIC", L"", 0, 24, 628, 490, 36);
        Control(L"STATIC", L"POI 仅含已加载区域；传送到达后仍会触发游戏事件。", 0, 24, 672, 490, 24);
        // Fit the initial window to the current monitor; scrolling keeps every control reachable.
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (GetMonitorInfoW(MonitorFromWindow(menu, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            const auto& area = monitor.rcWork;
            const int width = std::min(580L, area.right - area.left);
            const int height = std::min(760L, area.bottom - area.top);
            SetWindowPos(menu, nullptr, area.left + (area.right - area.left - width) / 2,
                area.top + (area.bottom - area.top - height) / 2, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        Layout(menu);
    }
    ShowWindow(menu, IsWindowVisible(menu) ? SW_HIDE : SW_SHOW);
    LOG_INFO("debug menu: window visible {}", IsWindowVisible(menu) != FALSE);
    Update();
#endif
}

void debug_menu::Update()
{
#ifdef _WIN32
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
