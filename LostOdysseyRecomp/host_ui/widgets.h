#pragma once

#include "rasterizer.h"
#include <string>
#include <vector>

namespace host_ui
{
    // High-level common widget functions for overlays
    inline void DrawPanel(Rasterizer& r, int x, int y, int w, int h, uint32_t bg = MakeColor(220, 25, 27, 30), uint32_t border = MakeColor(255, 65, 75, 85))
    {
        r.FillRect(x, y, w, h, bg);
        r.DrawRect(x, y, w, h, border, 2);
    }

    inline void DrawHeader(Rasterizer& r, int x, int y, int w, int h, const std::wstring& title, uint32_t bg = MakeColor(255, 38, 42, 48), uint32_t textCol = MakeColor(255, 230, 235, 240))
    {
        r.FillRect(x, y, w, h, bg);
        r.DrawHLine(x, y + h, w, MakeColor(255, 70, 80, 95), 1);
        int titleW = r.MeasureWString(title);
        int textX = (titleW < w) ? (x + (w - titleW) / 2) : (x + 12);
        r.DrawWString(textX, y + (h - 16) / 2, title, textCol, 1.0f);
    }

    inline void DrawButton(Rasterizer& r, int x, int y, int w, int h, const std::wstring& text, bool focused, bool active = false)
    {
        uint32_t bg = focused ? MakeColor(255, 60, 90, 130) : (active ? MakeColor(255, 45, 55, 65) : MakeColor(255, 35, 40, 45));
        uint32_t border = focused ? MakeColor(255, 120, 170, 230) : MakeColor(255, 65, 70, 75);
        uint32_t textCol = focused ? MakeColor(255, 255, 255, 255) : MakeColor(255, 200, 205, 210);

        r.FillRect(x, y, w, h, bg);
        r.DrawRect(x, y, w, h, border, focused ? 2 : 1);
        int textW = r.MeasureWString(text);
        int textX = (textW < w) ? (x + (w - textW) / 2) : (x + 6);
        r.DrawWString(textX, y + (h - 16) / 2, text, textCol, 1.0f);
    }
}
