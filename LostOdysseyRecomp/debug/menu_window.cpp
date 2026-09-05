#include <stdafx.h>
#include "battle_menu.h"
#include "teleport.h"
#include <os/logger.h>
#include <cmath>
#include <cwchar>
#include <cwctype>

#ifdef _WIN32
namespace
{
    HWND menu = nullptr;
    HWND statusLabel = nullptr;
    HWND positionLabel = nullptr;
    HWND teleportStatus = nullptr;
    HWND coordinates[3]{};
    HWND teleportButtons[10]{};
    bool invalidCoordinates = false;
    HWND poiList = nullptr, poiButton = nullptr, poiDetails = nullptr;
    uint64_t poiRevision = ~uint64_t(0);
    std::vector<debug_menu::MapPoi> displayedPois;

    void SetLabel(HWND label, const wchar_t* text)
    {
        // Repeated WM_SETTEXT invalidates static controls even when unchanged.
        // Keep editable coordinate fields outside this display-only cache.
        static std::unordered_map<HWND, std::wstring> displayed;
        auto& previous = displayed[label];
        if (previous == text) return;
        previous = text;
        SetWindowTextW(label, text);
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
        if (message == WM_COMMAND)
        {
            if (LOWORD(wparam) == 100) debug_menu::RequestVictory();
            if (LOWORD(wparam) == 101) debug_menu::CancelVictory();
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
        if (message == WM_CLOSE || (message == WM_KEYDOWN && wparam == VK_F1))
        {
            ShowWindow(window, SW_HIDE);
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    HWND Control(const wchar_t* type, const wchar_t* text, DWORD style,
        int x, int y, int width, int height, int id = 0)
    {
        const DWORD tabStop = (std::wcscmp(type, L"STATIC") == 0) ? 0 : WS_TABSTOP;
        HWND control = CreateWindowW(type, text, WS_CHILD | WS_VISIBLE | tabStop | style,
            x, y, width, height, menu, reinterpret_cast<HMENU>(intptr_t(id)), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        return control;
    }
}
#endif

void debug_menu::Toggle()
{
#ifdef _WIN32
    if (!menu)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = MenuProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"LostOdysseyDebugMenu";
        RegisterClassW(&wc);
        menu = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"Lost Odyssey — Debug Menu (F1)",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
            560, 680, nullptr, nullptr, wc.hInstance, nullptr);
        if (!menu) { LOG_ERROR("debug menu: CreateWindow failed {}", GetLastError()); return; }
        Control(L"STATIC", L"剧情调试 / Story debug", 0, 20, 18, 440, 24);
        statusLabel = Control(L"STATIC", L"", 0, 20, 52, 440, 45);
        Control(L"BUTTON", L"当前战斗判胜 / Win battle", BS_PUSHBUTTON, 20, 105, 265, 34, 100);
        Control(L"BUTTON", L"取消请求", BS_PUSHBUTTON, 300, 105, 155, 34, 101);
        Control(L"STATIC", L"一次性请求；在战斗判定点执行。\n窗口不会暂停游戏。F1 打开/关闭。", 0, 20, 158, 440, 48);
        Control(L"STATIC", L"人物传送 / Teleport（仅当前地图）", 0, 20, 212, 500, 24);
        positionLabel = Control(L"STATIC", L"", 0, 20, 242, 500, 24);
        teleportButtons[0] = Control(L"BUTTON", L"记住当前位置", BS_PUSHBUTTON, 20, 275, 150, 30, 10);
        teleportButtons[1] = Control(L"BUTTON", L"返回记录位置", BS_PUSHBUTTON, 180, 275, 150, 30, 11);
        teleportButtons[2] = Control(L"BUTTON", L"填入当前坐标", BS_PUSHBUTTON, 340, 275, 180, 30, 12);
        const wchar_t* axes[] = {L"X", L"Y", L"Z"};
        for (int i = 0; i < 3; ++i)
        {
            Control(L"STATIC", axes[i], 0, 20 + i * 125, 320, 20, 24);
            coordinates[i] = Control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 40 + i * 125, 315, 100, 28);
        }
        teleportButtons[3] = Control(L"BUTTON", L"传送到坐标", BS_PUSHBUTTON, 405, 315, 115, 30, 13);
        const wchar_t* offsets[] = {L"X −100", L"X +100", L"Y −100", L"Y +100", L"Z −100", L"Z +100"};
        for (int i = 0; i < 6; ++i)
            teleportButtons[4+i] = Control(L"BUTTON", offsets[i], BS_PUSHBUTTON, 20+i*84, 355, 80, 30, 20+i);
        teleportStatus = Control(L"STATIC", L"", 0, 20, 397, 500, 40);
        Control(L"STATIC", L"仅同地图坐标；到达目标仍会触发游戏事件。", 0, 20, 445, 500, 24);
        Control(L"STATIC", L"当前地图 POI / 兴趣点", 0, 20, 482, 500, 24);
        poiList = Control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 20, 510, 355, 220, 30);
        poiButton = Control(L"BUTTON", L"传送到此 POI", BS_PUSHBUTTON, 385, 510, 135, 30, 31);
        poiDetails = Control(L"STATIC", L"", 0, 20, 550, 500, 42);
        Control(L"STATIC", L"自动读取已加载地图；列表不包含尚未加载的区域。", 0, 20, 601, 500, 24);
    }
    ShowWindow(menu, IsWindowVisible(menu) ? SW_HIDE : SW_SHOW);
    LOG_INFO("debug menu: window visible {}", IsWindowVisible(menu) != FALSE);
    Update();
#endif
}

void debug_menu::Update()
{
#ifdef _WIN32
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
            if (message.message == WM_KEYDOWN && message.wParam == VK_F1)
            {
                ShowWindow(menu, SW_HIDE);
                continue;
            }
            if (!IsDialogMessageW(menu, &message))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
    }
    if (statusLabel && IsWindowVisible(menu)) SetLabel(statusLabel, Status());
    if (positionLabel && IsWindowVisible(menu))
    {
        const auto snapshot = GetTeleportSnapshot();
        wchar_t text[160]{};
        if (snapshot.available)
            std::swprintf(text, 160, L"X %.2f    Y %.2f    Z %.2f", snapshot.current.x, snapshot.current.y, snapshot.current.z);
        else std::swprintf(text, 160, L"等待可控制的地图角色 / No controllable map character");
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
                SendMessageW(poiList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(snapshot.pois[i].label.c_str()));
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
            std::swprintf(details, 180, L"落点 X %.1f  Y %.1f  Z %.1f\n距离 %.0f（游戏单位）", p.x,p.y,p.z,distance);
            SetLabel(poiDetails, details);
        }
        else SetLabel(poiDetails, snapshot.available ? L"当前地图没有识别到 POI" : L"地图控制恢复后自动更新 POI");
    }
#endif
}
