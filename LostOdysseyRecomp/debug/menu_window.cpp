#include <stdafx.h>
#include "battle_menu.h"
#include <os/logger.h>

#ifdef _WIN32
namespace
{
    HWND menu = nullptr;
    HWND statusLabel = nullptr;

    LRESULT CALLBACK MenuProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_COMMAND)
        {
            if (LOWORD(wparam) == 1) debug_menu::RequestVictory();
            if (LOWORD(wparam) == 2) debug_menu::CancelVictory();
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
        HWND control = CreateWindowW(type, text, WS_CHILD | WS_VISIBLE | style,
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
        menu = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"Lost Odyssey — Debug Menu (F1)",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
            500, 270, nullptr, nullptr, wc.hInstance, nullptr);
        if (!menu) { LOG_ERROR("debug menu: CreateWindow failed {}", GetLastError()); return; }
        Control(L"STATIC", L"剧情调试 / Story debug", 0, 20, 18, 440, 24);
        statusLabel = Control(L"STATIC", L"", 0, 20, 52, 440, 45);
        Control(L"BUTTON", L"当前战斗判胜 / Win battle", BS_PUSHBUTTON, 20, 105, 265, 34, 1);
        Control(L"BUTTON", L"取消请求", BS_PUSHBUTTON, 300, 105, 155, 34, 2);
        Control(L"STATIC", L"一次性请求；在战斗判定点执行。\n窗口不会暂停游戏。F1 打开/关闭。", 0, 20, 158, 440, 48);
    }
    ShowWindow(menu, IsWindowVisible(menu) ? SW_HIDE : SW_SHOW);
    LOG_INFO("debug menu: window visible {}", IsWindowVisible(menu) != FALSE);
    Update();
#endif
}

void debug_menu::Update()
{
#ifdef _WIN32
    if (statusLabel && IsWindowVisible(menu)) SetWindowTextW(statusLabel, Status());
#endif
}
