#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace settings::menu_assets
{
struct Image
{
    uint32_t width = 0, height = 0;
    // Straight-alpha BGRA bytes, matching the Win32 menu DIB.
    std::vector<uint32_t> pixels;
};
struct Glyph { uint32_t x = 0, y = 0, width = 0, height = 0, page = 0; };
struct Font
{
    std::unordered_map<uint32_t, Glyph> glyphs;
    std::vector<Image> pages;
    int kerning = 0;
    uint32_t height = 0;
};
struct Assets
{
    Font body, title, fallback;
    Image menu;
    std::string language;
};
// Reads the small FPI index, then only the selected menu package extents.
// No game writes, guest memory, renderer dependency or external asset cache.
std::shared_ptr<const Assets> Load(const std::filesystem::path &gameRoot, uint32_t language) noexcept;
// Presentation-thread cache, including failed loads. No per-frame disk scans.
std::shared_ptr<const Assets> Cached(const std::filesystem::path &gameRoot, uint32_t language) noexcept;

// Bounded pure readers, also exercised by the focused asset fixture.
Font DecodeFont(std::span<const uint8_t> package, const std::string &name);
Image DecodeTexture(std::span<const uint8_t> package, const std::string &name);
}
