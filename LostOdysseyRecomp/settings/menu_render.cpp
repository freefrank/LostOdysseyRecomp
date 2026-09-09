#include "menu_render.h"
#include "menu_assets.h"
#include "translations.h"
#include <algorithm>
#include <array>
#include <cmath>
#ifdef LO_MENU_RENDER_TRACE
// Only the direct raster fixture enables this observer; no runtime tracing.
extern void LoMenuRenderTrace(const std::wstring &value, bool original);
#endif
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
    if (!dc)
        return false;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = int(width);
    info.bmiHeader.biHeight = -int(height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap)
    {
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
    auto shade = [](COLORREF color, int amount) {
        const int red = std::clamp(int(GetRValue(color)) + amount, 0, 255);
        const int green = std::clamp(int(GetGValue(color)) + amount, 0, 255);
        const int blue = std::clamp(int(GetBValue(color)) + amount, 0, 255);
        return RGB(red, green, blue);
    };
    auto fill = [&](int x, int y, int w, int h, COLORREF color) {
        RECT area = rect(x, y, w, h);
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &area, brush);
        DeleteObject(brush);
    };
    auto line = [&](int x1, int y1, int x2, int y2, COLORREF color, int thickness = 1) {
        HPEN pen = CreatePen(PS_SOLID, std::max(1, int(std::lround(thickness * scale))), color);
        auto old = SelectObject(dc, pen);
        MoveToEx(dc, int(std::lround(offsetX + x1 * scale)), int(std::lround(offsetY + y1 * scale)), nullptr);
        LineTo(dc, int(std::lround(offsetX + x2 * scale)), int(std::lround(offsetY + y2 * scale)));
        SelectObject(dc, old);
        DeleteObject(pen);
    };

    auto *dib = static_cast<uint32_t *>(bits);
    std::fill_n(dib, size_t(width) * height, 0u);
    auto sprite = [&](const menu_assets::Image &image, int sx, int sy, int sw, int sh,
                      double x, double y, double w, double h, int brightness = 255,
                      bool opaque = false, COLORREF face = CLR_INVALID, COLORREF edge = CLR_INVALID) {
        if (sx < 0 || sy < 0 || sw <= 0 || sh <= 0 || sx + sw > int(image.width) || sy + sh > int(image.height) || w <= 0 || h <= 0)
            return;
        const double left = offsetX + x * scale, top = offsetY + y * scale;
        const double dw = w * scale, dh = h * scale;
        const int x0 = std::max(0, int(std::floor(left))), y0 = std::max(0, int(std::floor(top)));
        const int x1 = std::min(int(width), int(std::ceil(left + dw))), y1 = std::min(int(height), int(std::ceil(top + dh)));
        // Flush queued GDI operations before touching the shared DIB pixels.
        GdiFlush();
        for (int py = y0; py < y1; ++py)
            for (int px = x0; px < x1; ++px)
            {
                const double u = std::clamp((px + .5 - left) * sw / dw - .5, 0.0, double(sw - 1));
                const double v = std::clamp((py + .5 - top) * sh / dh - .5, 0.0, double(sh - 1));
                const int ux = int(u), vy = int(v), ux1 = std::min(ux + 1, sw - 1), vy1 = std::min(vy + 1, sh - 1);
                const double fx = u - ux, fy = v - vy;
                const uint32_t samples[] = {image.pixels[size_t(sy + vy) * image.width + sx + ux],
                    image.pixels[size_t(sy + vy) * image.width + sx + ux1],
                    image.pixels[size_t(sy + vy1) * image.width + sx + ux],
                    image.pixels[size_t(sy + vy1) * image.width + sx + ux1]};
                const double weights[] = {(1-fx)*(1-fy), fx*(1-fy), (1-fx)*fy, fx*fy};
                double alpha = 0, color[3]{};
                for (int i = 0; i < 4; ++i)
                {
                    const double a = (opaque ? 255 : samples[i] >> 24) * weights[i]; alpha += a;
                    for (int c = 0; c < 3; ++c)
                    {
                        double channel = (samples[i] >> (8*c)) & 255;
                        if (face != CLR_INVALID && edge != CLR_INVALID)
                        {
                            // Font pages contain white faces and black outlines. Preserve
                            // coverage while recoloring both for a light selected cell.
                            const int shift = (2-c)*8;
                            const double outlineChannel = (edge >> shift) & 255;
                            channel = outlineChannel + (((face >> shift) & 255) - outlineChannel) * channel / 255;
                        }
                        color[c] += channel * a;
                    }
                }
                auto &destination = dib[size_t(py) * width + px]; uint32_t result = 0;
                for (int c = 0; c < 3; ++c)
                {
                    const auto previous = (destination >> (8*c)) & 255;
                    const auto channel = uint32_t(std::clamp(std::lround(color[c] * brightness / (255.0*255.0) + previous * (1-alpha/255)), 0l, 255l));
                    result |= channel << (8*c);
                }
                destination = result;
            }
    };
    auto metal = [&](int x, int y, int w, int h, COLORREF base, int grain) {
        if (current.assets && !current.assets->menu.pixels.empty())
        {
            // Tile the interior at native 720p density; stretch only independent
            // borders/corners, never the fine horizontal grain of the full panel.
            for (int yy = 0; yy < h; )
            {
                const int v = (y + yy) % 135, th = std::min(h - yy, 135 - v);
                for (int xx = 0; xx < w; )
                {
                    const int u = (x + xx) % 402, tw = std::min(w - xx, 402 - u);
                    sprite(current.assets->menu, 3 + u, 62 + v, tw, th, x + xx, y + yy, tw, th);
                    xx += tw;
                }
                yy += th;
            }
            return;
        }
        RECT area = rect(x, y, w, h);
        area.left = std::clamp<LONG>(area.left, 0, LONG(width));
        area.right = std::clamp<LONG>(area.right, 0, LONG(width));
        area.top = std::clamp<LONG>(area.top, 0, LONG(height));
        area.bottom = std::clamp<LONG>(area.bottom, 0, LONG(height));
        const int streakWidth = std::max(12, int(std::lround(53 * scale)));
        for (LONG py = area.top; py < area.bottom; ++py)
        {
            uint32_t rowNoise = uint32_t(py) + 0x9e3779b9u;
            rowNoise ^= rowNoise >> 16;
            rowNoise *= 0x7feb352du;
            rowNoise ^= rowNoise >> 15;
            rowNoise *= 0x846ca68bu;
            rowNoise ^= rowNoise >> 16;
            const int band = int(rowNoise % 7) - 3;
            for (LONG px = area.left; px < area.right; ++px)
            {
                uint32_t noise = uint32_t(px / streakWidth) * 0x27d4eb2du ^ rowNoise;
                noise ^= noise >> 15;
                const int streak = int((noise >> 29) & 3) - 1;
                const int delta = std::clamp(band + streak, -grain, grain);
                const int red = std::clamp(int(GetRValue(base)) + delta, 0, 255);
                const int green = std::clamp(int(GetGValue(base)) + delta, 0, 255);
                const int blue = std::clamp(int(GetBValue(base)) + delta, 0, 255);
                dib[size_t(py) * width + px] = uint32_t(blue) | (uint32_t(green) << 8) | (uint32_t(red) << 16);
            }
        }
    };

    const COLORREF steel = RGB(100, 103, 103);
    const COLORREF steelDark = RGB(68, 71, 71);
    const COLORREF rail = RGB(94, 97, 97);
    const COLORREF ink = RGB(242, 242, 237);
    const COLORREF muted = RGB(165, 166, 163);
    const COLORREF disabled = RGB(126, 128, 126);
    const COLORREF selectedSurface = RGB(194, 196, 194);
    const COLORREF selectedInk = RGB(35, 36, 36);
    const COLORREF outline = RGB(39, 40, 40);

    metal(0, 0, 1280, 720, steel, 7);
    metal(0, 0, 1280, 100, RGB(107, 110, 110), 6);
    metal(0, 104, 366, 536, rail, 7);
    metal(366, 104, 727, 536, RGB(99, 102, 102), 7);
    metal(1094, 104, 186, 536, RGB(96, 99, 99), 6);
    metal(0, 643, 1280, 77, RGB(105, 108, 108), 6);

    auto faceFor = [&](const std::wstring &value) {
        if (std::none_of(value.begin(), value.end(), [](wchar_t c) { return c > 0x7f; }))
            return L"Trebuchet MS";
        if (current.language == 2) return L"Yu Gothic UI";
        if (current.language == 3) return L"Malgun Gothic";
        return current.language == 4 ? L"Microsoft YaHei UI" : L"Microsoft JhengHei UI";
    };
    auto makeFont = [&](const std::wstring &value, int size, bool bold) {
        return CreateFontW(-std::max(1, int(std::lround(size * scale))), 0, 0, 0,
                           bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, faceFor(value));
    };
    auto text = [&](int x, int y, int w, int h, const std::wstring &value, int size, COLORREF color,
                    bool bold = false, UINT alignment = DT_LEFT, COLORREF edge = RGB(39, 40, 40), int minimum = 13) {
        if (current.assets && !value.empty())
        {
            const auto covers = [&](const menu_assets::Font &font) {
                return font.height && std::all_of(value.begin(), value.end(), [&](wchar_t c) { return font.glyphs.contains(uint32_t(c)); });
            };
            const auto *selectedFont = size >= 32 && !current.assets->title.glyphs.empty() ? &current.assets->title : &current.assets->body;
            // Switch the whole string, retaining each original face's metrics.
            // Never combine unrelated glyph baselines inside a resolution label.
            if (!covers(*selectedFont) && covers(current.assets->fallback)) selectedFont = &current.assets->fallback;
            if (covers(*selectedFont))
            {
                const auto &font = *selectedFont;
#ifdef LO_MENU_RENDER_TRACE
                LoMenuRenderTrace(value, true);
#endif
                double advance = 0;
                for (const auto c : value) advance += int(font.glyphs.at(uint32_t(c)).width) + font.kerning;
                const double desired = size >= 32 ? size * 1.25 : size * 1.30;
                const double ratio = std::min({desired / font.height, double(h) / font.height, std::max(1.0, double(w - 8)) / std::max(1.0, advance)});
                double left = x;
                if (alignment & DT_CENTER) left += (w - advance * ratio) * .5;
                else if (alignment & DT_RIGHT) left += w - advance * ratio;
                const double top = y + (h - font.height * ratio) * .5;
                for (const auto c : value)
                {
                    const auto &g = font.glyphs.at(uint32_t(c));
                    sprite(font.pages[g.page], int(g.x), int(g.y), int(g.width), int(g.height),
                           left, top, g.width * ratio, g.height * ratio, 255, false, color, edge);
                    left += (int(g.width) + font.kerning) * ratio;
                }
                return;
            }
        }
#ifdef LO_MENU_RENDER_TRACE
        LoMenuRenderTrace(value, false);
#endif
        RECT area = rect(x, y, w, h);
        HFONT font = nullptr;
        for (int candidate = size; candidate >= minimum; --candidate)
        {
            font = makeFont(value, candidate, bold);
            auto old = SelectObject(dc, font);
            SIZE measured{};
            GetTextExtentPoint32W(dc, value.c_str(), int(value.size()), &measured);
            SelectObject(dc, old);
            if (measured.cx <= area.right - area.left - std::max(2, int(std::lround(8 * scale))) ||
                candidate == minimum)
                break;
            DeleteObject(font);
            font = nullptr;
        }
        auto old = SelectObject(dc, font);
        const UINT flags = alignment | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
        if (edge != CLR_INVALID)
        {
            const int stroke = std::max(1, int(std::lround(scale)));
            SetTextColor(dc, edge);
            for (const auto [dx, dy] : std::array<std::pair<int, int>, 8>{
                     std::pair{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}})
            {
                RECT edged = area;
                OffsetRect(&edged, dx * stroke, dy * stroke);
                DrawTextW(dc, value.c_str(), -1, &edged, flags);
            }
        }
        SetTextColor(dc, color);
        DrawTextW(dc, value.c_str(), -1, &area, flags);
        SelectObject(dc, old);
        DeleteObject(font);
    };
    auto brushedCell = [&](int x, int y, int w, int h, COLORREF base) {
        if (current.assets && !current.assets->menu.pixels.empty())
        {
            // This RGB panel is used by the original opaque material although
            // its atlas alpha is zero. Other sprites retain their alpha coverage.
            if (base == selectedSurface)
                sprite(current.assets->menu, 98, 259, 96, 36, x, y, w, h, 255, true);
            else
                metal(x, y, w, h, base, 5);
            return;
        }
        fill(x, y, w, h, base);
        for (int yy = 2; yy < h; yy += 4)
            line(x + 1, y + yy, x + w - 1, y + yy, shade(base, (yy % 8) ? -3 : 3));
    };
    auto cell = [&](int x, int y, int w, int h, bool selected) {
        if (selected) brushedCell(x, y, w, h, selectedSurface);
        line(x, y, x + w, y, selected ? RGB(229, 230, 226) : RGB(137, 139, 138));
        line(x, y, x, y + h, selected ? RGB(218, 219, 216) : RGB(113, 115, 114));
        line(x, y + h - 1, x + w, y + h - 1, selected ? RGB(55, 56, 56) : RGB(68, 70, 70));
        line(x + w - 1, y, x + w - 1, y + h, selected ? RGB(70, 71, 71) : RGB(76, 78, 78));
    };
    auto arrow = [&](int x, int y) {
        auto draw = [&](int dx, int dy, COLORREF fillColor, COLORREF edgeColor) {
            POINT points[] = {{int(std::lround(offsetX + (x + dx) * scale)), int(std::lround(offsetY + (y + dy) * scale))},
                              {int(std::lround(offsetX + (x + 22 + dx) * scale)), int(std::lround(offsetY + (y + dy) * scale))},
                              {int(std::lround(offsetX + (x + 34 + dx) * scale)), int(std::lround(offsetY + (y + 10 + dy) * scale))},
                              {int(std::lround(offsetX + (x + 22 + dx) * scale)), int(std::lround(offsetY + (y + 20 + dy) * scale))},
                              {int(std::lround(offsetX + (x + dx) * scale)), int(std::lround(offsetY + (y + 20 + dy) * scale))}};
            HBRUSH brush = CreateSolidBrush(fillColor);
            HPEN pen = CreatePen(PS_SOLID, std::max(1, int(std::lround(scale))), edgeColor);
            auto oldBrush = SelectObject(dc, brush);
            auto oldPen = SelectObject(dc, pen);
            Polygon(dc, points, int(std::size(points)));
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
        };
        draw(2, 2, RGB(46, 47, 47), RGB(46, 47, 47));
        draw(0, 0, RGB(239, 240, 236), RGB(49, 50, 50));
        line(x + 2, y + 2, x + 21, y + 2, RGB(255, 255, 251));
    };
    auto controllerButton = [&](int x, int y, wchar_t letter, bool green, bool bright) {
        const COLORREF base = green ? (bright ? RGB(116, 177, 43) : RGB(91, 116, 72))
                                    : (bright ? RGB(190, 62, 49) : RGB(124, 79, 74));
        HBRUSH shadowBrush = CreateSolidBrush(RGB(45, 46, 46));
        auto previousBrush = SelectObject(dc, shadowBrush);
        RECT shadow = rect(x + 2, y + 2, 24, 24);
        Ellipse(dc, shadow.left, shadow.top, shadow.right, shadow.bottom);
        SelectObject(dc, previousBrush);
        DeleteObject(shadowBrush);
        HBRUSH brush = CreateSolidBrush(base);
        HPEN pen = CreatePen(PS_SOLID, std::max(1, int(std::lround(scale))), RGB(42, 43, 43));
        previousBrush = SelectObject(dc, brush);
        auto previousPen = SelectObject(dc, pen);
        RECT button = rect(x, y, 24, 24);
        Ellipse(dc, button.left, button.top, button.right, button.bottom);
        SelectObject(dc, previousBrush);
        SelectObject(dc, previousPen);
        DeleteObject(brush);
        DeleteObject(pen);
        line(x + 6, y + 4, x + 17, y + 4, shade(base, 58));
        text(x, y, 24, 24, std::wstring(1, letter), 15, ink, true, DT_CENTER, outline, 12);
    };

    line(0, 97, 1280, 97, RGB(228, 229, 225), 2);
    line(0, 101, 1280, 101, RGB(30, 31, 31), 4);
    line(0, 105, 1280, 105, RGB(151, 153, 152));
    if (current.assets && !current.assets->menu.pixels.empty())
    {
        metal(0, 96, 365, 34, rail, 7);
        line(0, 120, 280, 120, RGB(42, 43, 43), 2);
        sprite(current.assets->menu, 410, 4, 89, 32, 277, 98, 89, 32);
    }
    line(365, 0, 365, 640, RGB(38, 39, 39), 2);
    line(368, 0, 368, 640, RGB(151, 153, 152));
    line(1093, 104, 1093, 640, RGB(42, 43, 43), 2);
    line(1096, 104, 1096, 640, RGB(144, 146, 145));
    line(0, 639, 1280, 639, RGB(34, 35, 35), 4);
    line(0, 644, 1280, 644, RGB(153, 155, 154));

    const int gearX = 104, gearY = 65;
    auto drawGear = [&](int offset, COLORREF fillColor, COLORREF edgeColor) {
        std::array<POINT, 48> points{};
        for (int i = 0; i < int(points.size()); ++i)
        {
            const double angle = i * 2.0 * 3.141592653589793 / points.size();
            const int radius = (i % 4 == 1 || i % 4 == 2) ? 23 : 18;
            points[i] = {int(std::lround(offsetX + (gearX + offset + std::cos(angle) * radius) * scale)),
                         int(std::lround(offsetY + (gearY + offset + std::sin(angle) * radius) * scale))};
        }
        HBRUSH brush = CreateSolidBrush(fillColor);
        HPEN pen = CreatePen(PS_SOLID, std::max(1, int(std::lround(scale))), edgeColor);
        auto previousBrush = SelectObject(dc, brush);
        auto previousPen = SelectObject(dc, pen);
        Polygon(dc, points.data(), int(points.size()));
        SelectObject(dc, previousBrush);
        SelectObject(dc, previousPen);
        DeleteObject(brush);
        DeleteObject(pen);
    };
    if (current.assets && !current.assets->menu.pixels.empty())
        sprite(current.assets->menu, 373, 777, 41, 41, 82, 43, 42, 42);
    else
    {
    drawGear(2, RGB(43, 44, 44), RGB(43, 44, 44));
    drawGear(0, RGB(218, 219, 215), RGB(43, 44, 44));
    HBRUSH gearBrush = CreateSolidBrush(steelDark);
    HPEN gearPen = CreatePen(PS_SOLID, std::max(1, int(std::lround(2 * scale))), ink);
    auto oldBrush = SelectObject(dc, gearBrush);
    auto oldPen = SelectObject(dc, gearPen);
    RECT gear = rect(gearX - 13, gearY - 13, 26, 26);
    Ellipse(dc, gear.left, gear.top, gear.right, gear.bottom);
    HBRUSH holeBrush = CreateSolidBrush(RGB(127, 130, 130));
    SelectObject(dc, holeBrush);
    RECT hole = rect(gearX - 5, gearY - 5, 10, 10);
    Ellipse(dc, hole.left, hole.top, hole.right, hole.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(holeBrush);
    DeleteObject(gearBrush);
    DeleteObject(gearPen);
    }
    text(130, 42, 234, 43, Translate(current.language, L"Settings", L"設定"), 31, ink, false);

    text(70, 122, 260, 28, L"Menu", 18, ink, false);
    const wchar_t *enTabs[] = {L"Gameplay", L"Audio", L"Graphics", L"Language"};
    const wchar_t *zhTabs[] = {L"遊戲", L"聲音", L"圖像", L"語言"};
    for (int i = 0; i < 4; ++i)
    {
        constexpr int tabWidth = 160;
        const int x = 386 + i * tabWidth;
        const bool selected = i == current.tab;
        cell(x, 110, tabWidth, 32, selected);
        text(x + 8, 110, tabWidth - 16, 32, Translate(current.language, enTabs[i], zhTabs[i]), 17,
             selected ? selectedInk : ink, selected, DT_CENTER,
             selected ? RGB(222, 223, 219) : outline, 12);
    }

    constexpr int rowTop = 150;
    constexpr int rowHeight = 43;
    constexpr int labelLeft = 65;
    constexpr int labelWidth = 299;
    constexpr int choiceLeft = 386;
    constexpr int choiceWidth = 640;
    for (size_t index = 0; index < current.rows.size(); ++index)
    {
        const int y = rowTop + int(index) * rowHeight;
        if (y + rowHeight > 580) break;
        const auto &row = current.rows[index];
        const bool focused = int(index) == current.row;

        if (focused)
        {
            brushedCell(labelLeft, y, labelWidth, rowHeight - 2, selectedSurface);
            arrow(38, y + 10);
        }
        line(labelLeft, y, labelLeft + labelWidth, y, focused ? RGB(244, 244, 239) : RGB(145, 147, 146));
        line(labelLeft, y + rowHeight - 2, labelLeft + labelWidth, y + rowHeight - 2,
             focused ? RGB(56, 57, 57) : RGB(69, 71, 71));
        text(82, y, 274, rowHeight - 2, row.name, 24,
             !row.enabled ? disabled : focused ? selectedInk : ink, false, DT_LEFT,
             focused ? RGB(222, 223, 219) : outline, 16);

        if (row.sliderPercent >= 0)
        {
            text(425, y, 62, rowHeight - 2, L"Min", 20, row.enabled ? ink : disabled, false, DT_CENTER);
            text(930, y, 74, rowHeight - 2, L"Max", 20, row.enabled ? ink : disabled, false, DT_CENTER);
            cell(495, y + 15, 420, 13, false);
            fill(499, y + 18, 412, 7, RGB(53, 57, 58));
            const int extent = int(std::lround(412 * std::clamp(row.sliderPercent, 0, 100) / 100.0));
            if (extent > 0)
            {
                fill(499, y + 18, extent, 7, row.enabled ? RGB(178, 203, 209) : RGB(115, 124, 125));
                line(499, y + 18, 499 + extent, y + 18, RGB(222, 234, 235));
            }
            continue;
        }

        std::vector<std::wstring> fallback;
        const std::vector<std::wstring> *choices = &row.choices;
        if (choices->empty())
        {
            fallback.push_back(row.value);
            choices = &fallback;
        }
        const int selected = std::clamp(row.selectedChoice, 0, int(choices->size()) - 1);
        if (row.controllerButtons && choices->size() >= 2)
        {
            for (int option = 0; option < 2; ++option)
            {
                const int left = choiceLeft + option * choiceWidth / 2;
                const bool currentChoice = option == selected;
                cell(left, y, choiceWidth / 2, rowHeight - 2, focused && currentChoice);
                const bool swap = option == 1;
                controllerButton(left + 34, y + 8, swap ? L'B' : L'A', !swap, currentChoice);
                controllerButton(left + 154, y + 8, swap ? L'A' : L'B', swap, currentChoice);
                const COLORREF legendColor = !row.enabled ? disabled : currentChoice ? ink : muted;
                text(left + 62, y, 60, rowHeight - 2, L"OK", 18, legendColor, false, DT_LEFT, outline, 14);
                text(left + 182, y, 125, rowHeight - 2,
                     Translate(current.language, L"Cancel", L"取消"), 18, legendColor,
                     false, DT_LEFT, outline, 13);
            }
            continue;
        }
        if (choices->size() > 5)
        {
            constexpr int arrowWidth = 72;
            cell(choiceLeft, y, arrowWidth, rowHeight - 2, false);
            cell(choiceLeft + arrowWidth, y, choiceWidth - arrowWidth * 2, rowHeight - 2, focused);
            cell(choiceLeft + choiceWidth - arrowWidth, y, arrowWidth, rowHeight - 2, false);
            text(choiceLeft, y, arrowWidth, rowHeight - 2, L"◀", 16,
                 row.enabled ? muted : disabled, false, DT_CENTER);
            text(choiceLeft + arrowWidth + 8, y, choiceWidth - arrowWidth * 2 - 16, rowHeight - 2,
                 (*choices)[selected], 22, !row.enabled ? disabled : focused ? selectedInk : ink,
                 false, DT_CENTER, focused ? RGB(222, 223, 219) : outline, 14);
            text(choiceLeft + choiceWidth - arrowWidth, y, arrowWidth, rowHeight - 2, L"▶", 16,
                 row.enabled ? muted : disabled, false, DT_CENTER);
            continue;
        }

        const int count = int(choices->size());
        const int usedWidth = count == 1 ? choiceWidth / 2 : choiceWidth;
        const int startX = choiceLeft + (choiceWidth - usedWidth) / 2;
        for (int option = 0; option < count; ++option)
        {
            const int left = startX + usedWidth * option / count;
            const int right = startX + usedWidth * (option + 1) / count;
            const bool currentChoice = option == selected;
            cell(left, y, right - left, rowHeight - 2, focused && currentChoice);
            text(left + 7, y, right - left - 14, rowHeight - 2, (*choices)[option], 22,
                 !row.enabled ? disabled : currentChoice ? (focused ? selectedInk : ink) : muted,
                 false, DT_CENTER, focused && currentChoice ? RGB(222, 223, 219) : outline, 13);
        }
    }

    text(65, 652, 52, 43, L"Help", 20, ink, false);
    fill(116, 650, 1094, 45, RGB(70, 73, 73));
    line(116, 650, 1210, 650, RGB(154, 156, 155));
    line(116, 694, 1210, 694, RGB(55, 56, 56));
    text(131, 650, 1063, 45, current.help, 20, ink, false, DT_LEFT, outline, 14);

    if (!current.dialogChoices.empty())
    {
        brushedCell(280, 208, 720, 306, RGB(73, 76, 76));
        line(280, 208, 1000, 208, RGB(231, 232, 228), 2);
        line(280, 208, 280, 514, RGB(194, 196, 193), 2);
        line(280, 513, 1000, 513, RGB(35, 36, 36), 3);
        line(999, 208, 999, 514, RGB(39, 40, 40), 3);
        text(325, 226, 630, 48, current.dialogTitle, 27, ink, false, DT_CENTER);
        // Consent text must remain readable with both bitmap and system fonts.
        // The normal text helper fits one line; wrap before passing it paragraphs.
        std::vector<std::wstring> messageLines;
        std::wstring remaining = current.dialogMessage;
        while (!remaining.empty()) {
            size_t cut=0, space=std::wstring::npos; unsigned units=0;
            while (cut<remaining.size() && units+(remaining[cut]>=0x2e80?2:1)<=70) {
                if (remaining[cut]==L' ') space=cut;
                units+=remaining[cut]>=0x2e80?2:1; ++cut;
            }
            if (cut<remaining.size() && space!=std::wstring::npos && space>cut/2) cut=space;
            messageLines.push_back(remaining.substr(0,cut)); remaining.erase(0,cut);
            while (!remaining.empty() && remaining.front()==L' ') remaining.erase(0,1);
        }
        const int lineHeight=std::min(22,85/int(std::max<size_t>(1,messageLines.size())));
        for(size_t i=0;i<messageLines.size();++i)
            text(330,267+int(i)*lineHeight,620,lineHeight,messageLines[i],16,ink,false,DT_CENTER,outline,13);
        for (size_t i = 0; i < current.dialogChoices.size(); ++i)
        {
            const int y = 360 + int(i) * 43;
            const bool focused = int(i) == current.dialogSelection;
            cell(390, y, 500, 37, focused);
            if (focused) arrow(354, y + 8);
            text(405, y, 470, 37, current.dialogChoices[i], 20,
                 focused ? selectedInk : ink, false, DT_CENTER,
                 focused ? RGB(222, 223, 219) : outline, 13);
        }
    }

    GdiFlush();
    pixels.resize(size_t(width) * height);
    const auto *source = static_cast<uint32_t *>(bits);
    for (size_t i = 0; i < pixels.size(); ++i)
    {
        const uint32_t pixel = source[i];
        pixels[i] = 0xff000000u | ((pixel & 255) << 16) | (pixel & 0xff00) | ((pixel >> 16) & 255);
    }
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return true;
#else
    return false;
#endif
}
