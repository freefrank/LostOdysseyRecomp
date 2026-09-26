#pragma once

#include "../hid/controller_glyphs.h"
#include "../host_ui/rasterizer.h"
#include <cstdlib>
#include <string>

namespace debug_menu::controller_hint
{
    inline std::wstring ShoulderLabels(std::wstring text, bool playStation)
    {
        if (!playStation) return text;
        const auto word = [](wchar_t c) {
            return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9');
        };
        for (size_t i = 0; i + 1 < text.size(); ++i)
        {
            const bool isolated = (i == 0 || !word(text[i - 1])) &&
                                  (i + 2 == text.size() || !word(text[i + 2]));
            if (!isolated || (text[i] != L'L' && text[i] != L'R')) continue;
            if (text[i + 1] == L'B') text[i + 1] = L'1';
            else if (text[i + 1] == L'T') text[i + 1] = L'2';
        }
        return text;
    }

    // Draw the shared face artwork directly into the debug overlay's pixel buffer.
    // The overlay bitmap font need not contain PlayStation symbols.
    inline void DrawFace(host_ui::Rasterizer& r, hid::prompts::Face face, int x, int y, uint32_t color)
    {
        hid::prompts::DrawFace(face, x, y, 16, [&](int ax, int ay, int bx, int by) {
            const int dx = std::abs(bx - ax), sx = ax < bx ? 1 : -1;
            const int dy = -std::abs(by - ay), sy = ay < by ? 1 : -1;
            int error = dx + dy;
            for (;;) {
                r.PutPixel(ax, ay, color);
                r.PutPixel(ax + 1, ay, color);
                if (ax == bx && ay == by) break;
                const int twice = 2 * error;
                if (twice >= dy) { error += dy; ax += sx; }
                if (twice <= dx) { error += dx; ay += sy; }
            }
        });
    }

    inline int Width(const host_ui::Rasterizer& r, bool playStation, const wchar_t* suffix)
    {
        return (playStation ? 20 : r.MeasureWString(L"A")) + r.MeasureWString(suffix);
    }

    // Returns the x position immediately after the hint, for aligned inline legends.
    inline int Draw(host_ui::Rasterizer& r, int x, int y, bool playStation,
                    hid::prompts::Face face, const wchar_t* suffix, uint32_t color)
    {
        if (playStation) {
            DrawFace(r, face, x, y, color);
            x += 20;
        } else {
            wchar_t letter = L'A';
            switch (face) {
            case hid::prompts::Face::A: letter = L'A'; break;
            case hid::prompts::Face::B: letter = L'B'; break;
            case hid::prompts::Face::X: letter = L'X'; break;
            case hid::prompts::Face::Y: letter = L'Y'; break;
            }
            x += r.DrawWString(x, y, std::wstring(1, letter), color);
        }
        return x + r.DrawWString(x, y, suffix, color);
    }
} // namespace debug_menu::controller_hint
