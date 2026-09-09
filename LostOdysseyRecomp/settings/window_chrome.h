#pragma once

// Opt-in desktop chrome. Other settings windows keep desktop_ui's existing style.
#ifdef _WIN32
#include "desktop_ui.h"
#include <windowsx.h>
#include <string>

namespace settings::window_chrome
{
inline constexpr COLORREF Canvas = RGB(12, 21, 32);
inline constexpr COLORREF Surface = RGB(18, 30, 44);
inline constexpr COLORREF Raised = RGB(28, 44, 61);
inline constexpr COLORREF Border = RGB(53, 72, 90);
inline constexpr COLORREF Text = RGB(235, 240, 246);
inline constexpr COLORREF Muted = RGB(158, 177, 197);
inline constexpr COLORREF Accent = RGB(209, 174, 110);
inline constexpr int TitleHeight = 52;
inline constexpr int MinimizeId = 0x7F01, MaximizeId = 0x7F02, CloseId = 0x7F03;
using desktop_ui::Px;
using desktop_ui::Font;
using desktop_ui::StyleControl;
using desktop_ui::Fill;

inline HBRUSH Brush(bool editable = false)
{
    static HBRUSH surface = CreateSolidBrush(Surface), raised = CreateSolidBrush(Raised);
    return editable ? raised : surface;
}
inline HBRUSH ColorControl(HDC dc, bool editable = false)
{
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, Text);
    SetBkColor(dc, editable ? Raised : Surface);
    return Brush(editable);
}
struct State
{
    HWND minimize{}, maximize{}, close{};
    HFONT font{};
    bool resizable = true;
    UINT dpi = 0;
    LONG width = -1;
};

inline void DrawButton(const DRAWITEMSTRUCT& item, bool primary = false)
{
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool caption = item.CtlID >= MinimizeId && item.CtlID <= CloseId;
    Fill(item.hDC, item.rcItem, disabled ? (caption ? Canvas : Surface) : pressed ? Border : primary ? Accent : caption ? Canvas : Raised);
    if (!caption)
    {
        SetDCBrushColor(item.hDC, primary ? Accent : Border);
        FrameRect(item.hDC, &item.rcItem, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, disabled ? Muted : primary ? Canvas : Text);
    if (caption)
    {
        const int x = (item.rcItem.left + item.rcItem.right) / 2;
        const int y = (item.rcItem.top + item.rcItem.bottom) / 2;
        const int r = Px(item.hwndItem, 5);
        HPEN pen = CreatePen(PS_SOLID, std::max(1, Px(item.hwndItem, 1)), disabled ? Muted : Text);
        HGDIOBJ old = SelectObject(item.hDC, pen);
        if (item.CtlID == MinimizeId) { MoveToEx(item.hDC, x-r, y+2, nullptr); LineTo(item.hDC, x+r+1, y+2); }
        else if (item.CtlID == CloseId)
        {
            MoveToEx(item.hDC, x-r, y-r, nullptr); LineTo(item.hDC, x+r+1, y+r+1);
            MoveToEx(item.hDC, x+r, y-r, nullptr); LineTo(item.hDC, x-r-1, y+r+1);
        }
        else
        {
            HGDIOBJ brush = SelectObject(item.hDC, GetStockObject(HOLLOW_BRUSH));
            Rectangle(item.hDC, x-r, y-r, x+r+1, y+r+1);
            SelectObject(item.hDC, brush);
        }
        SelectObject(item.hDC, old); DeleteObject(pen);
    }
    else
    {
        const auto font = reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
        const HGDIOBJ previousFont = font ? SelectObject(item.hDC, font) : nullptr;
        const int length = GetWindowTextLengthW(item.hwndItem);
        std::wstring text(size_t(length) + 1, L'\0');
        GetWindowTextW(item.hwndItem, text.data(), length + 1);
        RECT rect = item.rcItem; InflateRect(&rect, -Px(item.hwndItem, 8), 0);
        DrawTextW(item.hDC, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (previousFont) SelectObject(item.hDC, previousFont);
    }
    if ((item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT))
    {
        RECT focus = item.rcItem; InflateRect(&focus, -4, -4); DrawFocusRect(item.hDC, &focus);
    }
}

inline void Layout(HWND window, State& state)
{
    RECT client{}; GetClientRect(window, &client);
    const UINT dpi = GetDpiForWindow(window);
    if (state.width == client.right && state.dpi == dpi && state.font) return;
    state.width = client.right;
    int right = client.right - Px(window, 6);
    for (HWND button : {state.close, state.maximize, state.minimize})
    {
        if (!button) continue;
        right -= Px(window, 44);
        SetWindowPos(button, nullptr, right, Px(window, 6), Px(window, 44), Px(window, 40),
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (!state.font || state.dpi != dpi)
    {
        if (state.font) DeleteObject(state.font);
        state.font = Font(window, 14, FW_SEMIBOLD);
    }
    state.dpi = dpi;
    RECT title{0, 0, client.right, Px(window, TitleHeight)};
    InvalidateRect(window, &title, FALSE);
}

inline void Create(HWND window, State& state, bool resizable = true)
{
    state.resizable = resizable;
    auto button = [&](int id, const wchar_t* label) {
        return CreateWindowExW(0, L"BUTTON", label, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(intptr_t(id)), GetModuleHandleW(nullptr), nullptr);
    };
    state.minimize = button(MinimizeId, L"Minimize");
    if (resizable) state.maximize = button(MaximizeId, L"Maximize / Restore");
    state.close = button(CloseId, L"Close");
    desktop_ui::EnableDarkFrame(window);
    SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    Layout(window, state);
}
inline void Destroy(State& state)
{
    if (state.font) DeleteObject(state.font);
    state = {};
}
inline void Paint(HWND window, HDC dc, const State& state)
{
    RECT client{}; GetClientRect(window, &client);
    RECT title{0, 0, client.right, Px(window, TitleHeight)};
    Fill(dc, title, Canvas);
    RECT line{Px(window, 20), title.bottom - 1, client.right - Px(window, 20), title.bottom};
    Fill(dc, line, Border);
    title.left = Px(window, 24);
    title.right -= Px(window, state.maximize ? 150 : 106);
    wchar_t text[512]{}; GetWindowTextW(window, text, int(std::size(text)));
    HGDIOBJ old = state.font ? SelectObject(dc, state.font) : nullptr;
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, Text);
    DrawTextW(dc, text, -1, &title, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (old) SelectObject(dc, old);
    SetDCBrushColor(dc, Border);
    FrameRect(dc, &client, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

inline bool HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp,
                          State& state, LRESULT& result, int minWidth = 420, int minHeight = 240)
{
    result = 0;
    if (message == WM_NCCALCSIZE) return true;
    if (message == WM_NCHITTEST)
    {
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; ScreenToClient(window, &p);
        RECT r{}; GetClientRect(window, &r);
        const int edge = Px(window, 6);
        if (state.resizable && !IsZoomed(window))
        {
            const bool l = p.x < edge, t = p.y < edge, b = p.y >= r.bottom-edge, right = p.x >= r.right-edge;
            result = t ? (l ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP) : b ? (l ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM)
                : l ? HTLEFT : right ? HTRIGHT : HTCLIENT;
            if (result != HTCLIENT) return true;
        }
        result = p.y < Px(window, TitleHeight) && p.x < r.right - Px(window, state.maximize ? 144 : 100) ? HTCAPTION : HTCLIENT;
        return true;
    }
    if (message == WM_GETMINMAXINFO)
    {
        auto info = reinterpret_cast<MINMAXINFO*>(lp);
        info->ptMinTrackSize = {Px(window, minWidth), Px(window, minHeight)};
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            const auto& work = monitor.rcWork; const auto& screen = monitor.rcMonitor;
            info->ptMaxPosition = {work.left-screen.left, work.top-screen.top};
            info->ptMaxSize = {work.right-work.left, work.bottom-work.top};
        }
        return true;
    }
    if (message == WM_COMMAND)
    {
        if (LOWORD(wp) == CloseId) { SendMessageW(window, WM_CLOSE, 0, 0); return true; }
        if (LOWORD(wp) == MinimizeId) { ShowWindow(window, SW_MINIMIZE); return true; }
        if (LOWORD(wp) == MaximizeId)
        {
            SendMessageW(window, WM_SYSCOMMAND, IsZoomed(window) ? SC_RESTORE : SC_MAXIMIZE, 0); return true;
        }
    }
    if (message == WM_SETTEXT)
    {
        RECT r{}; GetClientRect(window, &r); r.bottom = Px(window, TitleHeight);
        InvalidateRect(window, &r, FALSE);
    }
    return false;
}
} // namespace settings::window_chrome
#endif
