#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <SDL.h>

namespace install::ui
{
struct GlyphInfo
{
    const uint8_t* bitmap = nullptr;
    int width = 0;  // 8 or 16
    int height = 16;
};

// Look up glyph bitmap from bundled SIL OFL Unifont (supports ASCII + CJK)
GlyphInfo GetGlyph(uint32_t codepoint);

// Decode next UTF-8 codepoint from string_view, advancing offset
uint32_t DecodeUtf8(std::string_view text, size_t& offset);

// Draws a single character glyph at (x, y) with scaling, returning width advanced
int DrawGlyph(SDL_Renderer* renderer, int x, int y, uint32_t codepoint,
              uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255, float scale = 1.0f);

// Draws UTF-8 string (ASCII + Chinese/Japanese/Korean)
void DrawString(SDL_Renderer* renderer, int x, int y, std::string_view text,
                uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255, float scale = 1.0f);

// Measures total width of UTF-8 string at given scale
int MeasureTextWidth(std::string_view text, float scale = 1.0f);

// Line height at given scale (base unifont glyph height is 16px)
int TextLineHeight(float scale = 1.0f);

// Truncates a UTF-8 string so that its measured width does not exceed maxWidth.
// If truncated, appends ellipsis ("..."). Never cuts in the middle of a UTF-8 sequence.
std::string TruncateTextWidth(std::string_view text, int maxWidth, float scale = 1.0f, std::string_view ellipsis = "...");
}
