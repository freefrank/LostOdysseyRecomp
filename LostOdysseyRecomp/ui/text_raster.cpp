#include <stdafx.h>
#include "text_raster.h"
#include <host_ui/unifont.h>
#include <settings/menu_assets.h>
#include <settings/menu_render.h>

#include <algorithm>
#include <cmath>

namespace ui::text_raster {
namespace {
namespace menu_assets = settings::menu_assets;

// The text's characters in drawing order: Arabic (from a language pack) joined and right to left.
std::u32string Codepoints(const std::string& text) {
    std::u32string out;
    for (size_t at = 0; at < text.size();)
        if (const uint32_t cp = host_ui::font::DecodeUtf8(text, at)) out += char32_t(cp);
    if (std::any_of(out.begin(), out.end(), [](char32_t c) { return c > 0xFFFF; })) return out;
    const std::wstring ordered = settings::ArabicDrawOrder(std::wstring(out.begin(), out.end()));
    return std::u32string(ordered.begin(), ordered.end());
}
}

uint32_t UiLanguageFor(uint32_t game) {
    switch (game) {
    case 2: return 2; // Japanese
    case 7: return 3; // Korean
    case 8: return 1; // Traditional Chinese
    case 9: return 4; // Simplified Chinese
    default: return 0;
    }
}

Raster DrawText(const menu_assets::Assets* assets, const std::string& utf8, double height, uint32_t face, bool fallbackFirst) {
    const auto text = Codepoints(utf8);
    const auto covers = [&](const menu_assets::Font& font) {
        return font.height && std::all_of(text.begin(), text.end(), [&](char32_t c) { return c == U' ' || font.glyphs.contains(uint32_t(c)); });
    };
    const menu_assets::Font* font = nullptr;
    if (assets && fallbackFirst && covers(assets->fallback)) font = &assets->fallback;
    else if (assets) font = covers(assets->body) ? &assets->body : covers(assets->fallback) ? &assets->fallback : nullptr;
    Raster out;
    if (font) {
        const double ratio = height / font->height;
        double advance = 0;
        for (const char32_t c : text) advance += c == U' ' ? font->height * 0.3 : int(font->glyphs.at(uint32_t(c)).width) + font->kerning;
        out.advance = advance * ratio;
        out.width = uint32_t(std::ceil(advance * ratio)) + 2;
        out.height = uint32_t(std::ceil(font->height * ratio)) + 2;
        std::vector<double> color(size_t(out.width) * out.height * 3), alpha(size_t(out.width) * out.height);
        double left = 1;
        for (const char32_t c : text) {
            if (c == U' ') { left += font->height * 0.3 * ratio; continue; }
            const auto& g = font->glyphs.at(uint32_t(c));
            const auto& page = font->pages[g.page];
            const double w = g.width * ratio, h = g.height * ratio;
            const int x0 = std::max(0, int(left)), x1 = std::min(int(out.width), int(std::ceil(left + w)));
            const int y1 = std::min(int(out.height), int(std::ceil(1 + h)));
            for (int py = 1; py < y1; ++py)
                for (int px = x0; px < x1; ++px) {
                    const double u = std::clamp((px + .5 - left) * g.width / w - .5, 0.0, double(g.width - 1));
                    const double v = std::clamp((py + .5 - 1) * g.height / h - .5, 0.0, double(g.height - 1));
                    const int ux = int(u), vy = int(v), ux1 = std::min<int>(ux + 1, g.width - 1), vy1 = std::min<int>(vy + 1, g.height - 1);
                    const double fx = u - ux, fy = v - vy;
                    const uint32_t s[] = {page.pixels[size_t(g.y + vy) * page.width + g.x + ux], page.pixels[size_t(g.y + vy) * page.width + g.x + ux1],
                                          page.pixels[size_t(g.y + vy1) * page.width + g.x + ux], page.pixels[size_t(g.y + vy1) * page.width + g.x + ux1]};
                    const double wt[] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
                    const size_t at = size_t(py) * out.width + px;
                    for (int i = 0; i < 4; ++i) {
                        const double a = ((s[i] >> 24) & 255) * wt[i];
                        // The page's grey level blends the dark edge into the face colour.
                        const double level = ((s[i] >> 16) & 255) / 255.0;
                        alpha[at] += a;
                        for (int k = 0; k < 3; ++k) {
                            const double ink = double((face >> (8 * k)) & 255);
                            color[at * 3 + k] += (16 + (ink - 16) * level) * a;
                        }
                    }
                }
            left += (int(g.width) + font->kerning) * ratio;
        }
        out.pixels.resize(alpha.size());
        for (size_t i = 0; i < alpha.size(); ++i) {
            const double a = std::min(alpha[i], 255.0);
            if (a <= 0) continue;
            uint32_t pixel = uint32_t(a) << 24;
            for (int k = 0; k < 3; ++k) pixel |= uint32_t(std::clamp(color[i * 3 + k] / alpha[i], 0.0, 255.0)) << (8 * k);
            out.pixels[i] = pixel;
        }
        return out;
    }
    // Unifont: 16 px bitmaps scaled to the height, with a one-pixel dark shadow.
    const double scale = height / 16.0;
    double advance = 0;
    // Joined Arabic forms touch; other glyphs keep a one-pixel gap.
    const auto step = [](char32_t c) { return host_ui::font::GetGlyph(uint32_t(c)).width + (settings::ArabicJoinedForm(wchar_t(c)) ? 0 : 1); };
    for (const char32_t c : text) advance += step(c) * scale;
    out.advance = advance;
    out.width = uint32_t(std::ceil(advance)) + 2;
    out.height = uint32_t(std::ceil(height)) + 2;
    out.pixels.assign(size_t(out.width) * out.height, 0);
    for (int pass = 0; pass < 2; ++pass) {
        double left = 0;
        const int shift = pass == 0 ? 1 : 0;
        const uint32_t ink = pass == 0 ? 0xFF101010u : 0xFF000000u | (face & 0xFFFFFFu);
        for (const char32_t c : text) {
            const auto g = host_ui::font::GetGlyph(uint32_t(c));
            for (uint32_t py = 0; py + shift < out.height; ++py)
                for (int px = int(left); px < int(left + g.width * scale) && px + shift < int(out.width); ++px) {
                    const int gx = std::min(g.width - 1, int((px - left) / scale)), gy = std::min(15, int(py / scale));
                    if (!g.bitmap) continue;
                    const uint16_t row = g.width == 8 ? uint16_t(g.bitmap[gy]) << 8 : uint16_t(g.bitmap[gy * 2] << 8 | g.bitmap[gy * 2 + 1]);
                    if (row & (0x8000 >> gx)) out.pixels[size_t(py + shift) * out.width + px + shift] = ink;
                }
            left += step(c) * scale;
        }
    }
    return out;
}
}
