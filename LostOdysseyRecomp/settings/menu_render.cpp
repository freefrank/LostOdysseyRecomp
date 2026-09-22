#include "menu_render.h"
#include "menu_assets.h"
#include "translations.h"
#include "../host_ui/rasterizer.h"
#include <algorithm>
#include <array>
#include <cmath>

#ifdef LO_MENU_RENDER_TRACE
// Only the direct raster fixture enables this observer; no runtime tracing.
extern void LoMenuRenderTrace(const std::wstring &value, bool original);
#endif

namespace
{
    using host_ui::MakeColor;
    using host_ui::ColorBlend;

    inline uint32_t Shade(uint32_t col, int amount)
    {
        uint32_t a = (col >> 24) & 0xFF;
        int r = std::clamp(int(col & 0xFF) + amount, 0, 255);
        int g = std::clamp(int((col >> 8) & 0xFF) + amount, 0, 255);
        int b = std::clamp(int((col >> 16) & 0xFF) + amount, 0, 255);
        return host_ui::PackRgba(uint8_t(r), uint8_t(g), uint8_t(b), uint8_t(a));
    }
}

bool settings::RasterizeMenu(const MenuSnapshot &current, uint32_t width, uint32_t height,
                             std::vector<uint32_t> &pixels)
{
    if (!width || !height || width > 16384 || height > 16384 || uint64_t(width) * height > 7680ull * 4320)
        return false;

    // Cross-platform software rasterizer: logical design canvas is 1280x720.
    // If output dimensions differ, scale and center onto the output buffer.
    pixels.assign(size_t(width) * height, 0xFF000000u);
    uint32_t *dib = pixels.data();

    const double scale = std::min(width / 1280.0, height / 720.0);
    const double offsetX = (width - 1280 * scale) * 0.5;
    const double offsetY = (height - 720 * scale) * 0.5;

    auto rect = [&](int x, int y, int w, int h) {
        int x0 = std::clamp(int(std::lround(offsetX + x * scale)), 0, int(width));
        int y0 = std::clamp(int(std::lround(offsetY + y * scale)), 0, int(height));
        int x1 = std::clamp(int(std::lround(offsetX + (x + w) * scale)), 0, int(width));
        int y1 = std::clamp(int(std::lround(offsetY + (y + h) * scale)), 0, int(height));
        return std::array<int, 4>{x0, y0, x1, y1};
    };

    auto fill = [&](int x, int y, int w, int h, uint32_t color) {
        auto [x0, y0, x1, y1] = rect(x, y, w, h);
        if (x0 >= x1 || y0 >= y1) return;
        uint32_t alpha = (color >> 24) & 0xFF;
        if (alpha == 255)
        {
            for (int py = y0; py < y1; ++py)
            {
                uint32_t *row = &dib[size_t(py) * width + x0];
                std::fill_n(row, x1 - x0, color);
            }
        }
        else
        {
            for (int py = y0; py < y1; ++py)
            {
                for (int px = x0; px < x1; ++px)
                {
                    size_t idx = size_t(py) * width + px;
                    dib[idx] = ColorBlend(dib[idx], color);
                }
            }
        }
    };

    auto line = [&](int x1, int y1, int x2, int y2, uint32_t color, int thickness = 1) {
        int sx1 = int(std::lround(offsetX + x1 * scale));
        int sy1 = int(std::lround(offsetY + y1 * scale));
        int sx2 = int(std::lround(offsetX + x2 * scale));
        int sy2 = int(std::lround(offsetY + y2 * scale));
        int th = std::max(1, int(std::lround(thickness * scale)));

        if (sy1 == sy2) // Horizontal line
        {
            int left = std::clamp(std::min(sx1, sx2), 0, int(width));
            int right = std::clamp(std::max(sx1, sx2), 0, int(width));
            int top = std::clamp(sy1 - th / 2, 0, int(height));
            int bottom = std::clamp(sy1 + (th + 1) / 2, 0, int(height));
            for (int py = top; py < bottom; ++py)
            {
                uint32_t *row = &dib[size_t(py) * width + left];
                std::fill_n(row, right - left, color);
            }
            return;
        }
        if (sx1 == sx2) // Vertical line
        {
            int left = std::clamp(sx1 - th / 2, 0, int(width));
            int right = std::clamp(sx1 + (th + 1) / 2, 0, int(width));
            int top = std::clamp(std::min(sy1, sy2), 0, int(height));
            int bottom = std::clamp(std::max(sy1, sy2), 0, int(height));
            for (int py = top; py < bottom; ++py)
            {
                uint32_t *row = &dib[size_t(py) * width + left];
                std::fill_n(row, right - left, color);
            }
            return;
        }

        // Generic Bresenham line
        int dx = std::abs(sx2 - sx1), sx = sx1 < sx2 ? 1 : -1;
        int dy = -std::abs(sy2 - sy1), sy = sy1 < sy2 ? 1 : -1;
        int err = dx + dy, e2;
        int cx = sx1, cy = sy1;
        while (true)
        {
            if (cx >= 0 && cx < int(width) && cy >= 0 && cy < int(height))
                dib[size_t(cy) * width + cx] = color;
            if (cx == sx2 && cy == sy2) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; cx += sx; }
            if (e2 <= dx) { err += dx; cy += sy; }
        }
    };

    auto sprite = [&](const menu_assets::Image &image, int sx, int sy, int sw, int sh,
                      double x, double y, double w, double h, int brightness = 255,
                      bool opaque = false, uint32_t face = 0, uint32_t edge = 0) {
        if (sx < 0 || sy < 0 || sw <= 0 || sh <= 0 || sx + sw > int(image.width) || sy + sh > int(image.height) || w <= 0 || h <= 0)
            return;
        const double left = offsetX + x * scale, top = offsetY + y * scale;
        const double dw = w * scale, dh = h * scale;
        const int x0 = std::max(0, int(std::floor(left))), y0 = std::max(0, int(std::floor(top)));
        const int x1 = std::min(int(width), int(std::ceil(left + dw))), y1 = std::min(int(height), int(std::ceil(top + dh)));

        for (int py = y0; py < y1; ++py)
        {
            for (int px = x0; px < x1; ++px)
            {
                const double u = std::clamp((px + .5 - left) * sw / dw - .5, 0.0, double(sw - 1));
                const double v = std::clamp((py + .5 - top) * sh / dh - .5, 0.0, double(sh - 1));
                const int ux = int(u), vy = int(v), ux1 = std::min(ux + 1, sw - 1), vy1 = std::min(vy + 1, sh - 1);
                const double fx = u - ux, fy = v - vy;
                const uint32_t samples[] = {
                    image.pixels[size_t(sy + vy) * image.width + sx + ux],
                    image.pixels[size_t(sy + vy) * image.width + sx + ux1],
                    image.pixels[size_t(sy + vy1) * image.width + sx + ux],
                    image.pixels[size_t(sy + vy1) * image.width + sx + ux1]};
                const double weights[] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
                double alpha = 0, color[3]{};
                for (int i = 0; i < 4; ++i)
                {
                    const double a = (opaque ? 255 : (samples[i] >> 24) & 255) * weights[i];
                    alpha += a;
                    for (int c = 0; c < 3; ++c)
                    {
                        double channel = (samples[i] >> (8 * c)) & 255;
                        if (face != 0 && edge != 0)
                        {
                            const int shift = c * 8;
                            const double outlineChannel = (edge >> shift) & 255;
                            channel = outlineChannel + (((face >> shift) & 255) - outlineChannel) * channel / 255;
                        }
                        color[c] += channel * a;
                    }
                }
                auto &destination = dib[size_t(py) * width + px];
                uint32_t result = 0;
                for (int c = 0; c < 3; ++c)
                {
                    const auto previous = (destination >> (8 * c)) & 255;
                    const auto channel = uint32_t(std::clamp(std::lround(color[c] * brightness / (255.0 * 255.0) + previous * (1 - alpha / 255)), 0l, 255l));
                    result |= channel << (8 * c);
                }
                destination = result | 0xFF000000u;
            }
        }
    };

    auto metal = [&](int x, int y, int w, int h, uint32_t base, int grain) {
        if (current.assets && !current.assets->menu.pixels.empty())
        {
            for (int yy = 0; yy < h;)
            {
                const int v = (y + yy) % 135, th = std::min(h - yy, 135 - v);
                for (int xx = 0; xx < w;)
                {
                    const int u = (x + xx) % 402, tw = std::min(w - xx, 402 - u);
                    sprite(current.assets->menu, 3 + u, 62 + v, tw, th, x + xx, y + yy, tw, th);
                    xx += tw;
                }
                yy += th;
            }
            return;
        }
        auto [x0, y0, x1, y1] = rect(x, y, w, h);
        const int streakWidth = std::max(12, int(std::lround(53 * scale)));
        for (int py = y0; py < y1; ++py)
        {
            uint32_t rowNoise = uint32_t(py) + 0x9e3779b9u;
            rowNoise ^= rowNoise >> 16;
            rowNoise *= 0x7feb352du;
            rowNoise ^= rowNoise >> 15;
            rowNoise *= 0x846ca68bu;
            rowNoise ^= rowNoise >> 16;
            const int band = int(rowNoise % 7) - 3;
            for (int px = x0; px < x1; ++px)
            {
                uint32_t noise = uint32_t(px / streakWidth) * 0x27d4eb2du ^ rowNoise;
                noise ^= noise >> 15;
                const int streak = int((noise >> 29) & 3) - 1;
                const int delta = std::clamp(band + streak, -grain, grain);
                const int red = std::clamp(int(base & 255) + delta, 0, 255);
                const int green = std::clamp(int((base >> 8) & 255) + delta, 0, 255);
                const int blue = std::clamp(int((base >> 16) & 255) + delta, 0, 255);
                dib[size_t(py) * width + px] = MakeColor(255, red, green, blue);
            }
        }
    };

    const uint32_t steel = MakeColor(255, 100, 103, 103);
    const uint32_t steelDark = MakeColor(255, 68, 71, 71);
    const uint32_t rail = MakeColor(255, 94, 97, 97);
    const uint32_t ink = MakeColor(255, 242, 242, 237);
    const uint32_t muted = MakeColor(255, 165, 166, 163);
    const uint32_t disabled = MakeColor(255, 126, 128, 126);
    const uint32_t selectedSurface = MakeColor(255, 194, 196, 194);
    const uint32_t selectedInk = MakeColor(255, 35, 36, 36);
    const uint32_t outline = MakeColor(255, 39, 40, 40);

    metal(0, 0, 1280, 720, steel, 7);
    metal(0, 0, 1280, 100, MakeColor(255, 107, 110, 110), 6);
    metal(0, 104, 366, 536, rail, 7);
    metal(366, 104, 727, 536, MakeColor(255, 99, 102, 102), 7);
    metal(1094, 104, 186, 536, MakeColor(255, 96, 99, 99), 6);
    metal(0, 643, 1280, 77, MakeColor(255, 105, 108, 108), 6);

    auto text = [&](int x, int y, int w, int h, const std::wstring &value, int size, uint32_t color,
                    bool bold = false, int alignment = 0 /* 0=left, 1=center, 2=right */,
                    uint32_t edge = MakeColor(255, 39, 40, 40), int minimum = 13) {
        if (current.assets && !value.empty())
        {
            const auto covers = [&](const menu_assets::Font &font) {
                return font.height && std::all_of(value.begin(), value.end(), [&](wchar_t c) {
                    return c == L' ' || font.glyphs.contains(uint32_t(c));
                });
            };
            const auto *selectedFont = size >= 32 && !current.assets->title.glyphs.empty() ? &current.assets->title : &current.assets->body;
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
                if (alignment == 1) left += (w - advance * ratio) * .5;
                else if (alignment == 2) left += w - advance * ratio;
                const double top = y + (h - font.height * ratio) * .5;
                for (const auto c : value)
                {
                    if (c == L' ') { left += (font.height * 0.3) * ratio; continue; }
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
        // Unifont-based fallback text
        host_ui::Rasterizer r(dib, width, height);

        // Auto-scale font down to fit available width w without overflowing
        float fontScale = float(scale * (size / 16.0f));
        auto measureWidth = [&](float fscale) {
            int tw = 0;
            for (wchar_t c : value)
            {
                auto g = host_ui::font::GetGlyph(uint32_t(c));
                tw += int((g.width + 1) * fscale);
            }
            return tw;
        };

        int totalWidth = measureWidth(fontScale);
        const double maxAllowedWidth = std::max(8.0, double(w - 4) * scale);
        if (totalWidth > maxAllowedWidth)
        {
            float shrinkRatio = float(maxAllowedWidth / totalWidth);
            fontScale *= shrinkRatio;
            totalWidth = measureWidth(fontScale);
        }

        const double screenX = offsetX + x * scale;
        const double screenY = offsetY + y * scale;
        const double screenW = w * scale;
        const double screenH = h * scale;

        int left = int(std::lround(screenX));
        if (alignment == 1) left = int(std::lround(screenX + (screenW - totalWidth) * 0.5));
        else if (alignment == 2) left = int(std::lround(screenX + screenW - totalWidth));

        // Exact vertical centering inside button height
        const double glyphHeight = 16.0 * fontScale;
        int top = int(std::lround(screenY + (screenH - glyphHeight) * 0.5));

        // Draw outline if edge requested
        if (edge != 0)
        {
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx || dy) r.DrawWString(left + dx, top + dy, value, edge, fontScale);
        }
        r.DrawWString(left, top, value, color, fontScale);
    };

    auto brushedCell = [&](int x, int y, int w, int h, uint32_t base) {
        if (current.assets && !current.assets->menu.pixels.empty())
        {
            if (base == selectedSurface)
                sprite(current.assets->menu, 98, 259, 96, 36, x, y, w, h, 255, true);
            else
                metal(x, y, w, h, base, 5);
            return;
        }
        fill(x, y, w, h, base);
        for (int yy = 2; yy < h; yy += 4)
            line(x + 1, y + yy, x + w - 1, y + yy, Shade(base, (yy % 8) ? -3 : 3));
    };

    auto cell = [&](int x, int y, int w, int h, bool selected) {
        if (selected) brushedCell(x, y, w, h, selectedSurface);
        line(x, y, x + w, y, selected ? MakeColor(255, 229, 230, 226) : MakeColor(255, 137, 139, 138));
        line(x, y, x, y + h, selected ? MakeColor(255, 218, 219, 216) : MakeColor(255, 113, 115, 114));
        line(x, y + h - 1, x + w, y + h - 1, selected ? MakeColor(255, 55, 56, 56) : MakeColor(255, 68, 70, 70));
        line(x + w - 1, y, x + w - 1, y + h, selected ? MakeColor(255, 70, 71, 71) : MakeColor(255, 76, 78, 78));
    };

    auto arrow = [&](int x, int y) {
        auto draw = [&](int dx, int dy, uint32_t fillColor, uint32_t edgeColor) {
            for (int row = 0; row <= 10; ++row)
            {
                line(x + dx, y + dy + row, x + 22 + dx + row, y + dy + row, fillColor);
                line(x + dx, y + dy + 20 - row, x + 22 + dx + row, y + dy + 20 - row, fillColor);
            }
            line(x + dx, y + dy, x + 22 + dx, y + dy, edgeColor);
            line(x + 22 + dx, y + dy, x + 34 + dx, y + 10 + dy, edgeColor);
            line(x + 34 + dx, y + 10 + dy, x + 22 + dx, y + 20 + dy, edgeColor);
            line(x + 22 + dx, y + 20 + dy, x + dx, y + 20 + dy, edgeColor);
            line(x + dx, y + 20 + dy, x + dx, y + dy, edgeColor);
        };
        draw(2, 2, MakeColor(255, 46, 47, 47), MakeColor(255, 46, 47, 47));
        draw(0, 0, MakeColor(255, 239, 240, 236), MakeColor(255, 49, 50, 50));
        line(x + 2, y + 2, x + 21, y + 2, MakeColor(255, 255, 255, 251));
    };

    auto controllerButton = [&](int x, int y, wchar_t letter, bool green, bool bright) {
        const uint32_t base = green ? (bright ? MakeColor(255, 116, 177, 43) : MakeColor(255, 91, 116, 72))
                                    : (bright ? MakeColor(255, 190, 62, 49) : MakeColor(255, 124, 79, 74));
        fill(x + 2, y + 2, 24, 24, MakeColor(255, 45, 46, 46));
        fill(x, y, 24, 24, base);
        line(x, y, x + 24, y, MakeColor(255, 42, 43, 43));
        line(x, y + 24, x + 24, y + 24, MakeColor(255, 42, 43, 43));
        line(x, y, x, y + 24, MakeColor(255, 42, 43, 43));
        line(x + 24, y, x + 24, y + 24, MakeColor(255, 42, 43, 43));
        line(x + 6, y + 4, x + 17, y + 4, Shade(base, 58));
        text(x, y, 24, 24, std::wstring(1, letter), 15, ink, true, 1, outline, 12);
    };

    line(0, 97, 1280, 97, MakeColor(255, 228, 229, 225), 2);
    line(0, 101, 1280, 101, MakeColor(255, 30, 31, 31), 4);
    line(0, 105, 1280, 105, MakeColor(255, 151, 153, 152));
    if (current.assets && !current.assets->menu.pixels.empty())
    {
        metal(0, 96, 365, 34, rail, 7);
        line(0, 120, 280, 120, MakeColor(255, 42, 43, 43), 2);
        sprite(current.assets->menu, 410, 4, 89, 32, 277, 98, 89, 32);
    }
    line(365, 0, 365, 640, MakeColor(255, 38, 39, 39), 2);
    line(368, 0, 368, 640, MakeColor(255, 151, 153, 152));
    line(1093, 104, 1093, 640, MakeColor(255, 42, 43, 43), 2);
    line(1096, 104, 1096, 640, MakeColor(255, 144, 146, 145));
    line(0, 639, 1280, 639, MakeColor(255, 34, 35, 35), 4);
    line(0, 644, 1280, 644, MakeColor(255, 153, 155, 154));

    if (current.assets && !current.assets->menu.pixels.empty())
        sprite(current.assets->menu, 373, 777, 41, 41, 82, 43, 42, 42);
    else
    {
        fill(91, 52, 26, 26, steelDark);
        line(91, 52, 117, 52, ink);
        fill(99, 60, 10, 10, MakeColor(255, 127, 130, 130));
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
             selected ? selectedInk : ink, selected, 1,
             selected ? MakeColor(255, 222, 223, 219) : outline, 12);
    }

    constexpr int rowTop = 150;
    constexpr int rowHeight = 43;
    constexpr int labelLeft = 65;
    constexpr int labelWidth = 299;
    constexpr int choiceLeft = 386;
    constexpr int choiceWidth = 640;
    int visible = 0;
    for (size_t index = 0; index < current.rows.size(); ++index)
    {
        const auto &row = current.rows[index];
        if (row.hidden)
            continue;
        const int slot = visible++ - current.scroll;
        if (slot < 0)
            continue;
        const int y = rowTop + slot * rowHeight;
        if (y + rowHeight > 640) break;
        const bool focused = int(index) == current.row;

        if (focused)
        {
            brushedCell(labelLeft, y, labelWidth, rowHeight - 2, selectedSurface);
            arrow(38, y + 10);
        }
        line(labelLeft, y, labelLeft + labelWidth, y, focused ? MakeColor(255, 244, 244, 239) : MakeColor(255, 145, 147, 146));
        line(labelLeft, y + rowHeight - 2, labelLeft + labelWidth, y + rowHeight - 2,
             focused ? MakeColor(255, 56, 57, 57) : MakeColor(255, 69, 71, 71));
        text(82, y, 274, rowHeight - 2, row.name, 24,
             !row.enabled ? disabled : focused ? selectedInk : ink, false, 0,
             focused ? MakeColor(255, 222, 223, 219) : outline, 16);

        if (row.sliderPercent >= 0)
        {
            text(425, y, 62, rowHeight - 2, L"Min", 20, row.enabled ? ink : disabled, false, 1);
            text(930, y, 74, rowHeight - 2, L"Max", 20, row.enabled ? ink : disabled, false, 1);
            cell(495, y + 15, 420, 13, false);
            fill(499, y + 18, 412, 7, MakeColor(255, 53, 57, 58));
            const int extent = int(std::lround(412 * std::clamp(row.sliderPercent, 0, 100) / 100.0));
            if (extent > 0)
            {
                fill(499, y + 18, extent, 7, row.enabled ? MakeColor(255, 178, 203, 209) : MakeColor(255, 115, 124, 125));
                line(499, y + 18, 499 + extent, y + 18, MakeColor(255, 222, 234, 235));
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
                const uint32_t legendColor = !row.enabled ? disabled : currentChoice ? ink : muted;
                text(left + 62, y, 60, rowHeight - 2, L"OK", 18, legendColor, false, 0, outline, 14);
                text(left + 182, y, 125, rowHeight - 2,
                     Translate(current.language, L"Cancel", L"取消"), 18, legendColor,
                     false, 0, outline, 13);
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
                 row.enabled ? muted : disabled, false, 1);
            text(choiceLeft + arrowWidth + 8, y, choiceWidth - arrowWidth * 2 - 16, rowHeight - 2,
                 (*choices)[selected], 22, !row.enabled ? disabled : focused ? selectedInk : ink,
                 false, 1, focused ? MakeColor(255, 222, 223, 219) : outline, 14);
            text(choiceLeft + choiceWidth - arrowWidth, y, arrowWidth, rowHeight - 2, L"▶", 16,
                 row.enabled ? muted : disabled, false, 1);
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
                 false, 1, focused && currentChoice ? MakeColor(255, 222, 223, 219) : outline, 13);
        }
    }

    // Understated overflow indicators when rows exceed the visible viewport.
    if (current.scroll > 0)
        text( choiceLeft + choiceWidth - 40, rowTop - 24, 40, 20, L"▲", 13, muted, false, 1);
    if (visible - current.scroll > kMenuVisibleRows)
        text( choiceLeft + choiceWidth - 40, 622, 40, 20, L"▼", 13, muted, false, 1);

    text(65, 652, 52, 43, L"Help", 20, ink, false);
    fill(116, 650, 1094, 45, MakeColor(255, 70, 73, 73));
    line(116, 650, 1210, 650, MakeColor(255, 154, 156, 155));
    line(116, 694, 1210, 694, MakeColor(255, 55, 56, 56));
    text(131, 650, 1063, 45, current.help, 20, ink, false, 0, outline, 14);

    if (!current.dialogChoices.empty())
    {
        brushedCell(280, 208, 720, 306, MakeColor(255, 73, 76, 76));
        line(280, 208, 1000, 208, MakeColor(255, 231, 232, 228), 2);
        line(280, 208, 280, 514, MakeColor(255, 194, 196, 193), 2);
        line(280, 513, 1000, 513, MakeColor(255, 35, 36, 36), 3);
        line(999, 208, 999, 514, MakeColor(255, 39, 40, 40), 3);
        text(325, 226, 630, 48, current.dialogTitle, 27, ink, false, 1);

        std::vector<std::wstring> messageLines;
        std::wstring remaining = current.dialogMessage;
        while (!remaining.empty()) {
            size_t cut = 0, space = std::wstring::npos; unsigned units = 0;
            while (cut < remaining.size() && units + (remaining[cut] >= 0x2e80 ? 2 : 1) <= 70) {
                if (remaining[cut] == L' ') space = cut;
                units += remaining[cut] >= 0x2e80 ? 2 : 1; ++cut;
            }
            if (cut < remaining.size() && space != std::wstring::npos && space > cut / 2) cut = space;
            messageLines.push_back(remaining.substr(0, cut)); remaining.erase(0, cut);
            while (!remaining.empty() && remaining.front() == L' ') remaining.erase(0, 1);
        }
        const int lineHeight = std::min(22, 85 / int(std::max<size_t>(1, messageLines.size())));
        for (size_t i = 0; i < messageLines.size(); ++i)
            text(330, 267 + int(i) * lineHeight, 620, lineHeight, messageLines[i], 16, ink, false, 1, outline, 13);
        for (size_t i = 0; i < current.dialogChoices.size(); ++i)
        {
            const int y = 360 + int(i) * 43;
            const bool focused = int(i) == current.dialogSelection;
            cell(390, y, 500, 37, focused);
            if (focused) arrow(354, y + 8);
            text(405, y, 470, 37, current.dialogChoices[i], 20,
                 focused ? selectedInk : ink, false, 1,
                 focused ? MakeColor(255, 222, 223, 219) : outline, 13);
        }
    }

    return true;
}
