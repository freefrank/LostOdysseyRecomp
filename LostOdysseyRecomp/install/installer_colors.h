#pragma once

#include <cstdint>
#include <SDL.h>

namespace install::ui
{
struct Color
{
    uint8_t r, g, b, a = 255;
};

constexpr Color COLOR_STEEL{ 50, 53, 55, 255 };          // Background dark steel slate
constexpr Color COLOR_STEEL_PANEL{ 42, 45, 47, 255 };    // Panel interior
constexpr Color COLOR_RAIL{ 32, 34, 36, 255 };           // Dark well / inset
constexpr Color COLOR_BORDER_LIGHT{ 125, 128, 130, 255 };// Top/Left bevel highlight
constexpr Color COLOR_BORDER_DARK{ 20, 21, 22, 255 };    // Bottom/Right bevel shadow
constexpr Color COLOR_BORDER_LINE{ 65, 68, 70, 255 };    // Subtle divider line

constexpr Color COLOR_ACCENT_GOLD{ 235, 205, 130, 255 }; // Lost Odyssey Title Gold
constexpr Color COLOR_GOLD_MUTED{ 190, 168, 115, 255 };  // Muted gold subtitle
constexpr Color COLOR_CYAN{ 120, 215, 235, 255 };        // Directory / path cyan
constexpr Color COLOR_GREEN{ 140, 205, 150, 255 };       // Verified / success green
constexpr Color COLOR_RED{ 225, 85, 75, 255 };           // Alert red
constexpr Color COLOR_INK{ 245, 245, 242, 255 };         // Clean white/cream text
constexpr Color COLOR_MUTED{ 165, 170, 172, 255 };       // Subdued secondary text

constexpr Color COLOR_SEL_SURFACE{ 210, 215, 216, 255 }; // Bright brushed steel surface
constexpr Color COLOR_SEL_INK{ 25, 28, 30, 255 };        // Dark text on bright highlight
constexpr Color COLOR_SEL_TOP{ 250, 252, 254, 255 };     // Crisp top highlight line
constexpr Color COLOR_SEL_BOTTOM{ 130, 135, 138, 255 };  // Shadow bottom bevel line
constexpr Color COLOR_FOLDER_ICON{ 235, 195, 95, 255 };  // Warm amber/gold folder tab
constexpr Color COLOR_FILE_ICON{ 140, 150, 155, 255 };   // Clean subtle file icon

inline void SetDrawColor(SDL_Renderer* renderer, Color c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

inline void DrawRect(SDL_Renderer* renderer, int x, int y, int w, int h, Color c)
{
    SetDrawColor(renderer, c);
    SDL_Rect r{ x, y, w, h };
    SDL_RenderDrawRect(renderer, &r);
}

inline void FillRect(SDL_Renderer* renderer, int x, int y, int w, int h, Color c)
{
    SetDrawColor(renderer, c);
    SDL_Rect r{ x, y, w, h };
    SDL_RenderFillRect(renderer, &r);
}

inline void DrawBevelPanel(SDL_Renderer* renderer, int x, int y, int w, int h, Color fillCol)
{
    FillRect(renderer, x, y, w, h, fillCol);
    SetDrawColor(renderer, COLOR_BORDER_LIGHT);
    SDL_RenderDrawLine(renderer, x, y, x + w - 1, y);
    SDL_RenderDrawLine(renderer, x, y, x, y + h - 1);
    SetDrawColor(renderer, COLOR_BORDER_DARK);
    SDL_RenderDrawLine(renderer, x, y + h - 1, x + w - 1, y + h - 1);
    SDL_RenderDrawLine(renderer, x + w - 1, y, x + w - 1, y + h - 1);
}

inline void DrawSelectionBar(SDL_Renderer* renderer, int x, int y, int w, int h)
{
    FillRect(renderer, x, y, w, h, COLOR_SEL_SURFACE);
    SetDrawColor(renderer, COLOR_SEL_TOP);
    SDL_RenderDrawLine(renderer, x, y, x + w - 1, y);
    SDL_RenderDrawLine(renderer, x, y + 1, x + w - 1, y + 1);
    SetDrawColor(renderer, COLOR_SEL_BOTTOM);
    SDL_RenderDrawLine(renderer, x, y + h - 1, x + w - 1, y + h - 1);
    SDL_RenderDrawLine(renderer, x, y + h - 2, x + w - 1, y + h - 2);
}
} // namespace install::ui
