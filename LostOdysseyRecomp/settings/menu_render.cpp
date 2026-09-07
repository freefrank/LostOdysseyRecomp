#include "menu_render.h"
#include "translations.h"
#include <algorithm>
#include <cmath>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

bool settings::RasterizeMenu(const MenuSnapshot &current, uint32_t width, uint32_t height,
                             std::vector<uint32_t> &pixels)
{
    if (!width || !height || width > 16384 || height > 16384 || uint64_t(width) * height > 7680ull * 4320)
        return false;
#ifdef _WIN32
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = int(width);
    info.bmiHeader.biHeight = -int(height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dc || !bitmap)
    {
        if (bitmap)
            DeleteObject(bitmap);
        if (dc)
            DeleteDC(dc);
        return false;
    }
    auto oldBitmap = SelectObject(dc, bitmap);
    SetBkMode(dc, TRANSPARENT);
    const double scale = std::min(width / 1280.0, height / 720.0);
    const double offsetX = (width - 1280 * scale) * 0.5;
    const double offsetY = (height - 720 * scale) * 0.5;
    auto rect = [&](int x, int y, int w, int h) -> RECT {
        return {LONG(std::lround(offsetX + x * scale)), LONG(std::lround(offsetY + y * scale)),
                LONG(std::lround(offsetX + (x + w) * scale)), LONG(std::lround(offsetY + (y + h) * scale))};
    };
    RECT canvas{0, 0, LONG(width), LONG(height)};
    FillRect(dc, &canvas, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    auto fill = [&](int x, int y, int w, int h, COLORREF color) {
        RECT r = rect(x, y, w, h);
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &r, brush);
        DeleteObject(brush);
    };
    auto text = [&](int x, int y, int w, int h, const std::wstring &value, int size, COLORREF color,
                    bool bold = false) {
        HFONT font =
            CreateFontW(-std::max(1, int(std::lround(size * scale))), 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        auto old = SelectObject(dc, font);
        SetTextColor(dc, color);
        RECT r = rect(x, y, w, h);
        DrawTextW(dc, value.c_str(), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, old);
        DeleteObject(font);
    };
    const COLORREF ink = RGB(229, 224, 213), muted = RGB(153, 153, 147), gold = RGB(207, 172, 108);
    fill(0, 0, 1280, 720, RGB(22, 26, 29));
    text(64, 26, 300, 24, L"LOST ODYSSEY", 17, gold, true);
    text(64, 57, 900, 60, Translate(current.language, L"Settings", L"設定"), 38, ink, true);
    text(1080, 66, 136, 42, L"LB / RB", 20, gold);
    const wchar_t *en[] = {L"Gameplay", L"Audio", L"Graphics", L"Language"};
    const wchar_t *zh[] = {L"遊戲", L"聲音", L"圖像", L"語言"};
    for (int i = 0; i < 4; i++)
    {
        int x = 64 + i * 288;
        if (i == current.tab)
        {
            fill(x, 128, 288, 56, RGB(49, 52, 51));
            fill(x, 180, 288, 4, gold);
        }
        text(x + 20, 128, 248, 52, Translate(current.language, en[i], zh[i]), 23, i == current.tab ? ink : muted,
             i == current.tab);
    }
    const int rowHeight = current.rows.size() > 7 ? 49 : 56;
    for (size_t i = 0; i < current.rows.size(); i++)
    {
        int y = 208 + int(i) * rowHeight;
        const auto &r = current.rows[i];
        if (int(i) == current.row)
            fill(64, y, 1152, rowHeight - 4, RGB(49, 52, 51));
        text(84, y, 696, rowHeight - 4, r.name, 22, r.enabled ? ink : muted);
        text(820, y, 376, rowHeight - 4, r.value, 21, r.enabled ? gold : muted);
    }
    fill(64, 626, 1152, 1, RGB(65, 66, 62));
    text(64, 644, 1152, 48, current.help, 16, muted);
    GdiFlush();
    pixels.resize(size_t(width) * height);
    const auto *source = static_cast<uint32_t *>(bits);
    for (size_t i = 0; i < pixels.size(); i++)
    {
        uint32_t p = source[i];
        pixels[i] = 0xff000000u | ((p & 255) << 16) | (p & 0xff00) | ((p >> 16) & 255);
    }
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return true;
#else
    return false;
#endif
}
