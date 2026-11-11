#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace settings::menu_assets
{
struct Image
{
    uint32_t width = 0, height = 0;
    // Straight-alpha BGRA bytes, matching the Win32 menu DIB.
    std::vector<uint32_t> pixels;
    // Pixels per original texel: a mod's HD font page is 2, 4 or 8.
    uint32_t scale = 1;
};
// Glyph rectangles are in original texels; multiply by the page scale.
struct Glyph { uint32_t x = 0, y = 0, width = 0, height = 0, page = 0; };
struct Font
{
    std::unordered_map<uint32_t, Glyph> glyphs;
    std::vector<Image> pages;
    // With HD pages, the same pages at half their size, for small text.
    std::vector<Image> smallPages;
    int kerning = 0;
    uint32_t height = 0;
};
// The page to sample for a glyph drawn at `outputPerTexel` output pixels per
// original texel: the half-size HD page while it is still as sharp as the
// output, otherwise the full page.
inline const Image &GlyphPage(const Font &font, const Glyph &g, double outputPerTexel)
{
    if (g.page < font.smallPages.size() && !font.smallPages[g.page].pixels.empty() &&
        font.smallPages[g.page].scale >= outputPerTexel)
        return font.smallPages[g.page];
    return font.pages[g.page];
}
struct Assets
{
    Font body, title, fallback;
    Image menu;
    // The retail menus' window parts (empty when missing): the list cursor.
    Image window;
    // The game's button prompt tiles (Icon_Page_0, 36x36 cells; empty when
    // missing) and the same atlas with the PlayStation artwork the game uses,
    // both in the menu raster's byte order (host_ui::PackRgba).
    Image icons, iconsPlayStation;
    std::string language;
};
// Reads selected FPI package extents and optional, validated image overlays.
// No game writes, guest memory or renderer dependency.
std::shared_ptr<const Assets> Load(const std::filesystem::path &gameRoot, uint32_t language) noexcept;
// Presentation-thread cache, invalidated by root or mod generation changes.
std::shared_ptr<const Assets> Cached(const std::filesystem::path &gameRoot, uint32_t language) noexcept;

// Empty packagePath keeps the pure original-resource readers used by fixtures.
// With a package identity, Texture2D pixels (including font pages) may be replaced.
Font DecodeFont(std::span<const uint8_t> package, const std::string &name, std::string_view packagePath = {});
Image DecodeTexture(std::span<const uint8_t> package, const std::string &name, std::string_view packagePath = {});
}
