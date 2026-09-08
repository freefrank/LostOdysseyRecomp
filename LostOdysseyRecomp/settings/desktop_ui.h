#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <string_view>

namespace settings::desktop_ui
{
inline constexpr COLORREF Canvas = RGB(18, 22, 25);
inline constexpr COLORREF Surface = RGB(29, 34, 38);
inline constexpr COLORREF Raised = RGB(42, 48, 52);
inline constexpr COLORREF Border = RGB(67, 73, 76);
inline constexpr COLORREF Text = RGB(236, 232, 222);
inline constexpr COLORREF Muted = RGB(164, 165, 160);
inline constexpr COLORREF Accent = RGB(205, 166, 91);

inline int Px(HWND window, int value)
{
    return MulDiv(value, int(GetDpiForWindow(window)), 96);
}

inline HFONT Font(HWND window, int points, int weight = FW_NORMAL)
{
    return CreateFontW(-Px(window, points), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

inline void EnableDarkFrame(HWND window)
{
    // Resolve these optional presentation APIs at runtime so the helper stays
    // reusable without adding link dependencies to host-only fixtures.
    if (HMODULE dwm = LoadLibraryW(L"dwmapi.dll"))
    {
        using SetAttribute = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
        if (auto set = reinterpret_cast<SetAttribute>(GetProcAddress(dwm, "DwmSetWindowAttribute")))
        {
            const BOOL dark = TRUE;
            set(window, 20, &dark, sizeof(dark)); // DWMWA_USE_IMMERSIVE_DARK_MODE
        }
        FreeLibrary(dwm);
    }
}

inline void StyleControl(HWND control, HFONT font)
{
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    if (HMODULE theme = LoadLibraryW(L"uxtheme.dll"))
    {
        using SetTheme = HRESULT(WINAPI *)(HWND, LPCWSTR, LPCWSTR);
        if (auto set = reinterpret_cast<SetTheme>(GetProcAddress(theme, "SetWindowTheme")))
            set(control, L"DarkMode_Explorer", nullptr);
        FreeLibrary(theme);
    }
}

inline HBRUSH CanvasBrush()
{
    static HBRUSH brush = CreateSolidBrush(Canvas);
    return brush;
}

inline HBRUSH SurfaceBrush()
{
    static HBRUSH brush = CreateSolidBrush(Surface);
    return brush;
}

inline HBRUSH EditBrush()
{
    static HBRUSH brush = CreateSolidBrush(Raised);
    return brush;
}

inline HBRUSH ColorControl(HDC dc, bool editable = false)
{
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, Text);
    SetBkColor(dc, editable ? Raised : Surface);
    return editable ? EditBrush() : SurfaceBrush();
}

inline void Fill(HDC dc, const RECT &rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

inline void DrawButton(const DRAWITEMSTRUCT &item, bool primary = false)
{
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    COLORREF background = primary ? Accent : Raised;
    if (pressed) background = primary ? RGB(176, 137, 66) : RGB(52, 59, 63);
    if (disabled) background = RGB(35, 39, 42);
    Fill(item.hDC, item.rcItem, background);
    SetDCBrushColor(item.hDC, primary ? Accent : Border);
    FrameRect(item.hDC, &item.rcItem, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    wchar_t label[256]{};
    GetWindowTextW(item.hwndItem, label, int(std::size(label)));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, disabled ? Muted : (primary ? Canvas : Text));
    DrawTextW(item.hDC, label, -1, const_cast<RECT *>(&item.rcItem),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (item.itemState & ODS_FOCUS)
    {
        RECT focus = item.rcItem;
        InflateRect(&focus, -3, -3);
        DrawFocusRect(item.hDC, &focus);
    }
}
} // namespace settings::desktop_ui
#endif
