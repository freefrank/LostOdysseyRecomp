#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Glyphs a language pack adds to the game's fonts (UE3 UFont objects in
// rpfonts*_<language>.xxx), for letters the original fonts do not have. A pack
// holds one LOFONT1 file per font, <pack>/fonts/<package path>/<font>.lofont:
// an RGBA atlas of the new glyphs and where each one is in it. The package is
// rebuilt when the game opens its archive (modding/text_overlay.h), like a
// model replacement: the atlas goes below the glyphs of the font's last page
// (the page grows to the next power of two when it does not fit), and the
// font's two glyph tables (the Characters property and the native table) and
// its CharRemap get the new glyphs. The changed Font and Texture2D objects are
// serialized at the end of the package and the export table points there; the
// old bytes are zeroed. A glyph for a code point the font already has replaces
// it. See docs/wiki/Modding-API.md#fonts-lofont1.
namespace modding::font_patch
{
struct Glyph
{
    uint32_t codepoint = 0;
    uint32_t x = 0, y = 0, width = 0, height = 0; // in the atlas
};
struct Glyphs
{
    std::vector<Glyph> glyphs;
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgba; // width * height * 4, rows top to bottom
};
// LOFONT1, little endian: "LOFONT1" CR LF and three zero bytes, u32 header size
// (32), u32 glyph count, u32 atlas width, u32 atlas height, u32 zero; per glyph
// u32 code point (1..0xFFFF), u16 x, y, width, height; then the RGBA atlas.
// Throws std::runtime_error on anything else.
Glyphs Parse(std::span<const uint8_t> file);

// Every Font of a package: its two glyph tables must agree and its pages must
// decode. One line per font; throws std::runtime_error on the first problem.
std::vector<std::string> Check(std::span<const uint8_t> package);

// The fonts of a package as --export-assets writes them (fonts kind): what a
// tool needs to draw glyphs that fit. Throws std::runtime_error on a font it
// cannot read.
struct FontInfo
{
    struct Page { uint32_t exportIndex = 0; std::string name; uint32_t width = 0, height = 0, format = 0; };
    struct Entry { uint32_t codepoint = 0, x = 0, y = 0, width = 0, height = 0, page = 0; };
    std::string name;
    uint32_t exportIndex = 0;
    int32_t kerning = 0;
    std::vector<Page> pages;
    std::vector<Entry> glyphs; // by code point
};
std::vector<FontInfo> ReadFonts(std::span<const uint8_t> package);

struct Addition
{
    std::string font;   // the Font object's name (Maru23)
    Glyphs glyphs;
    std::string source; // for messages
};
struct Result
{
    std::vector<uint8_t> package;   // empty when nothing was added
    std::vector<std::string> notes; // one line per font, done or skipped
    size_t patched = 0;
};
// package: the decoded package bytes (no CPX).
Result RebuildPackage(std::span<const uint8_t> package, const std::vector<Addition> &additions);
}
