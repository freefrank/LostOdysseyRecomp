#include "font_patch.h"
#include "ue3_package.h"
#include "xenos_texture.h"

#include <lzokay.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>

namespace modding::font_patch
{
namespace
{
using ue3::Package;
using ue3::Reader;

void Require(bool ok, const std::string &message) { if (!ok) throw std::runtime_error(message); }

constexpr uint32_t kMaxSide = 4096;        // the largest page the game's decoders take
constexpr uint32_t kMaxGlyphs = 16384;     // per font, as the menu reader allows
constexpr uint32_t kBulkLzo = 0x10;        // UE3 bulk data stored LZO compressed
constexpr uint32_t kBulkChunk = 131072;    // LZO chunk size of the game's bulk data
constexpr int32_t kFormatDxt5 = 7;         // EPixelFormat PF_DXT5
constexpr int32_t kFormatArgb = 2;         // PF_A8R8G8B8 (Japanese Yume24, Yume66)

uint32_t Le32(std::span<const uint8_t> b, size_t p) { return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24; }
uint16_t Le16(std::span<const uint8_t> b, size_t p) { return uint16_t(b[p] | b[p + 1] << 8); }

struct Writer
{
    std::vector<uint8_t> bytes;
    void U8(uint8_t v) { bytes.push_back(v); }
    void U16(uint16_t v) { bytes.insert(bytes.end(), {uint8_t(v >> 8), uint8_t(v)}); }
    void U32(uint32_t v) { bytes.insert(bytes.end(), {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}); }
    void Raw(std::span<const uint8_t> b) { bytes.insert(bytes.end(), b.begin(), b.end()); }
};
void PutBe32(std::vector<uint8_t> &out, size_t at, uint32_t v)
{
    for (int i = 0; i < 4; ++i) out[at + size_t(i)] = uint8_t(v >> (24 - 8 * i));
}
uint32_t GetBe32(std::span<const uint8_t> b, size_t at)
{
    return uint32_t(b[at]) << 24 | uint32_t(b[at + 1]) << 16 | uint32_t(b[at + 2]) << 8 | b[at + 3];
}

// ---- Tagged properties with their byte positions ---------------------------

struct Property
{
    std::string name, type;
    size_t begin = 0, sizeField = 0, value = 0, size = 0; // package offsets
};
struct Properties
{
    std::vector<Property> list;
    size_t end = 0; // after the "None" name: where the native data starts
    const Property *Find(std::string_view name) const
    {
        for (const auto &p : list) if (p.name == name) return &p;
        return nullptr;
    }
};
// From an object's first byte (its net index) or, object = false, a struct inside an array.
Properties ReadProperties(const Package &package, Reader &r, bool object)
{
    Properties result;
    if (object) r.I32();
    for (size_t count = 0;; ++count)
    {
        Require(count < 4096, "too many properties");
        Property p;
        p.begin = r.pos;
        p.name = package.Name(r);
        if (p.name == "None") { result.end = r.pos; return result; }
        p.type = package.Name(r);
        p.sizeField = r.pos;
        p.size = r.U32();
        r.U32(); // array index
        if (p.type == "StructProperty") package.Name(r);
        if (p.type == "BoolProperty") r.U32();
        p.value = r.pos;
        r.Take(p.size);
        result.list.push_back(std::move(p));
    }
}
int32_t IntValue(const Package &package, const Properties &props, std::string_view name)
{
    const auto *p = props.Find(name);
    Require(p && p->type == "IntProperty" && p->size == 4, std::string("no ") + std::string(name) + " property");
    return int32_t(GetBe32(package.bytes, p->value));
}

// ---- The Font object ---------------------------------------------------------

struct FontGlyph { uint32_t x = 0, y = 0, width = 0, height = 0; uint8_t page = 0; };
struct Font
{
    Properties props;
    // The template element of Characters: its bytes and where its values are, relative to it.
    std::vector<uint8_t> element;
    size_t startU = 0, startV = 0, uSize = 0, vSize = 0, textureIndex = 0;
    std::vector<FontGlyph> glyphs;
    std::vector<uint32_t> pages; // object references of the page textures
    int32_t kerning = 0;
    std::vector<std::pair<uint16_t, uint16_t>> remap;
    uint32_t remapped = 0;
};

Font ReadFont(const Package &package, const ue3::Export &e)
{
    Font font;
    Reader r = ue3::ObjectReader(package, e);
    font.props = ReadProperties(package, r, true);
    const auto *characters = font.props.Find("Characters");
    Require(characters && characters->type == "ArrayProperty", "no Characters property");
    Reader array{package.bytes.first(characters->value + characters->size), characters->value};
    const auto propertyCount = array.U32();
    Require(propertyCount > 0 && propertyCount <= kMaxGlyphs, "unsupported Characters count");
    // The first element is the template for new ones.
    const size_t first = array.pos;
    const auto element = ReadProperties(package, array, false);
    auto offset = [&](std::string_view name, std::string_view type, size_t size) {
        for (const auto &p : element.list)
            if (p.name == name)
            {
                Require(p.type == type && p.size == size, "unexpected type of Characters." + std::string(name));
                return p.value - first;
            }
        throw std::runtime_error("Characters has no " + std::string(name));
    };
    font.startU = offset("StartU", "IntProperty", 4);
    font.startV = offset("StartV", "IntProperty", 4);
    font.uSize = offset("USize", "IntProperty", 4);
    font.vSize = offset("VSize", "IntProperty", 4);
    font.textureIndex = offset("TextureIndex", "ByteProperty", 1);
    font.element.assign(package.bytes.begin() + std::ptrdiff_t(first), package.bytes.begin() + std::ptrdiff_t(array.pos));
    for (uint32_t i = 1; i < propertyCount; ++i) ReadProperties(package, array, false);
    Require(array.pos == characters->value + characters->size, "Characters size mismatch");

    // Native: the glyph table again, the page references, kerning, CharRemap, IsRemapped.
    Reader n{package.bytes.first(size_t(e.offset) + e.size), font.props.end};
    const auto count = ue3::Count(n, 17);
    Require(count == propertyCount, "the two glyph tables differ in size");
    font.glyphs.resize(count);
    for (auto &g : font.glyphs)
    {
        g.x = n.U32(); g.y = n.U32(); g.width = n.U32(); g.height = n.U32(); g.page = n.Byte();
    }
    const auto pageCount = ue3::Count(n, 4);
    Require(pageCount > 0 && pageCount <= 32, "unsupported page count");
    for (uint32_t i = 0; i < pageCount; ++i) font.pages.push_back(n.U32());
    font.kerning = n.I32();
    const auto remapCount = ue3::Count(n, 4);
    for (uint32_t i = 0; i < remapCount; ++i)
    {
        const auto cp = n.U16(), index = n.U16();
        Require(index < count, "CharRemap points past the glyph table");
        font.remap.emplace_back(cp, index);
    }
    font.remapped = n.U32();
    Require(font.remapped == 1, "font is not remapped");
    Require(n.pos == size_t(e.offset) + e.size, "unexpected data after the font");
    return font;
}

// ---- The page texture -----------------------------------------------------------

struct Page
{
    Properties props;
    uint32_t width = 0, height = 0;
    int32_t format = kFormatDxt5;
    std::vector<uint8_t> raw; // tiled DXT5 blocks (8-in-16 swapped) or A, R, G, B pixels, as stored
};

// Bytes of a tiled surface: rows and columns of blocks (DXT5) or pixels (A8R8G8B8) padded to 32 (Xenos tiles).
size_t TiledSize(uint32_t width, uint32_t height, int32_t format = kFormatDxt5)
{
    const uint32_t side = format == kFormatDxt5 ? 4 : 1, bytes = format == kFormatDxt5 ? 16 : 4;
    const size_t pitch = (width / side + 31) / 32 * 32, rows = (height / side + 31) / 32 * 32;
    return pitch * rows * bytes;
}

// The byte offset of block (x, y) in a tiled surface pitch blocks wide (tools/ui_assets/decode.cpp).
size_t TiledOffset(uint32_t x, uint32_t y, uint32_t pitch)
{
    const uint32_t outer = (((y >> 5) * (pitch >> 5)) + (x >> 5)) << 6;
    const uint32_t inner = (((y >> 1) & 7) << 3) | (x & 7);
    const uint32_t v = (outer | inner) << 4;
    const uint32_t bank = (y >> 4) & 1, pipe = ((x >> 3) & 3) ^ (((y >> 3) & 1) << 1);
    return ((y & 1) << 4) | (pipe << 6) | (bank << 11) | (v & 15) |
           (((v >> 4) & 1) << 5) | (((v >> 5) & 7) << 8) | ((v >> 8) << 12);
}

Page ReadPage(const Package &package, const ue3::Export &e)
{
    Require(package.ClassName(e) == "Texture2D", e.name + " is not a Texture2D");
    Page page;
    Reader r = ue3::ObjectReader(package, e);
    page.props = ReadProperties(package, r, true);
    const auto width = IntValue(package, page.props, "SizeX"), height = IntValue(package, page.props, "SizeY");
    const auto *format = page.props.Find("Format");
    Require(format && format->type == "ByteProperty" && format->size == 1 &&
            (package.bytes[format->value] == kFormatDxt5 || package.bytes[format->value] == kFormatArgb),
            e.name + " is neither DXT5 nor A8R8G8B8");
    page.format = package.bytes[format->value];
    Require(width >= 4 && height >= 4 && uint32_t(width) <= kMaxSide && uint32_t(height) <= kMaxSide &&
            width % 4 == 0 && height % 4 == 0, e.name + ": unsupported size");
    page.width = uint32_t(width); page.height = uint32_t(height);
    // SourceArt (empty bulk data), then one mip.
    Require(r.U32() == 0 && r.U32() == 0 && r.U32() == 0, e.name + ": unexpected source art");
    r.U32(); // its offset in the file
    Require(r.U32() == 1, e.name + ": more than one mip");
    const auto flags = r.U32(), rawSize = r.U32(), storedSize = r.U32();
    r.U32(); // payload offset in the file
    Require(flags == kBulkLzo && rawSize == TiledSize(page.width, page.height, page.format), e.name + ": unexpected mip layout");
    Reader stream{r.Take(storedSize)};
    Require(stream.U32() == ue3::kPackageMagic && stream.U32() == kBulkChunk, e.name + ": unexpected bulk header");
    const auto compressed = stream.U32(), decoded = stream.U32();
    Require(decoded == rawSize, e.name + ": bulk size mismatch");
    std::vector<std::pair<uint32_t, uint32_t>> chunks;
    for (uint32_t done = 0; done < decoded; done += kBulkChunk)
    {
        const auto a = stream.U32(), b = stream.U32();
        Require(b == std::min(kBulkChunk, decoded - done), e.name + ": unexpected bulk chunk");
        chunks.emplace_back(a, b);
    }
    page.raw.resize(decoded);
    size_t written = 0, total = 0;
    for (const auto [a, b] : chunks)
    {
        const auto input = stream.Take(a);
        size_t actual = 0;
        Require(lzokay::decompress(input.data(), input.size(), page.raw.data() + written, b, actual) == lzokay::EResult::Success &&
                actual == b, e.name + ": LZO data does not decode");
        written += actual; total += a;
    }
    Require(total == compressed && stream.pos == stream.bytes.size(), e.name + ": bulk size mismatch");
    Require(r.U32() == page.width && r.U32() == page.height, e.name + ": mip size mismatch");
    Require(r.pos == size_t(e.offset) + e.size, e.name + ": unexpected data after the mip");
    return page;
}

// One DXT5 block from 16 RGBA pixels, in the stored byte order.
std::array<uint8_t, 16> EncodeDxt5(const std::array<std::array<uint8_t, 4>, 16> &px)
{
    std::array<uint8_t, 16> b{};
    uint8_t aMax = 0, aMin = 255;
    for (const auto &p : px) { aMax = std::max(aMax, p[3]); aMin = std::min(aMin, p[3]); }
    b[0] = aMax; b[1] = aMin;
    if (aMax != aMin)
    {
        int palette[8] = {aMax, aMin};
        for (int i = 1; i <= 6; ++i) palette[i + 1] = ((7 - i) * aMax + i * aMin) / 7;
        uint64_t bits = 0;
        for (size_t i = 0; i < 16; ++i)
        {
            int best = 0;
            for (int k = 1; k < 8; ++k)
                if (std::abs(palette[k] - px[i][3]) < std::abs(palette[best] - px[i][3])) best = k;
            bits |= uint64_t(best) << (3 * i);
        }
        for (int i = 0; i < 6; ++i) b[2 + size_t(i)] = uint8_t(bits >> (8 * i));
    }
    // Colour: the box of the visible pixels' colours, its corners as the endpoints.
    std::array<int, 3> lo{255, 255, 255}, hi{0, 0, 0};
    bool any = false;
    for (const auto &p : px)
        if (p[3])
        {
            any = true;
            for (int c = 0; c < 3; ++c) { lo[c] = std::min<int>(lo[c], p[c]); hi[c] = std::max<int>(hi[c], p[c]); }
        }
    if (!any) lo = hi = {0, 0, 0};
    auto pack = [](const std::array<int, 3> &c) { return uint16_t((c[0] * 31 + 127) / 255 << 11 | (c[1] * 63 + 127) / 255 << 5 | (c[2] * 31 + 127) / 255); };
    auto unpack = [](uint16_t v) {
        const int r = v >> 11 & 31, g = v >> 5 & 63, bl = v & 31;
        return std::array<int, 3>{r << 3 | r >> 2, g << 2 | g >> 4, bl << 3 | bl >> 2};
    };
    uint16_t c0 = pack(hi), c1 = pack(lo);
    uint32_t indices = 0;
    if (c0 != c1)
    {
        const auto e0 = unpack(c0), e1 = unpack(c1);
        std::array<std::array<int, 3>, 4> palette{e0, e1};
        for (int c = 0; c < 3; ++c)
        {
            palette[2][c] = (2 * e0[c] + e1[c]) / 3;
            palette[3][c] = (e0[c] + 2 * e1[c]) / 3;
        }
        for (size_t i = 0; i < 16; ++i)
        {
            int best = 0, bestError = 1 << 30;
            for (int k = 0; k < 4; ++k)
            {
                int error = 0;
                for (int c = 0; c < 3; ++c) error += (palette[k][c] - px[i][c]) * (palette[k][c] - px[i][c]);
                if (error < bestError) { best = k; bestError = error; }
            }
            indices |= uint32_t(best) << (2 * i);
        }
    }
    b[8] = uint8_t(c0); b[9] = uint8_t(c0 >> 8); b[10] = uint8_t(c1); b[11] = uint8_t(c1 >> 8);
    for (int i = 0; i < 4; ++i) b[12 + size_t(i)] = uint8_t(indices >> (8 * i));
    std::array<uint8_t, 16> stored{};
    for (size_t i = 0; i < 16; ++i) stored[i ^ 1] = b[i]; // the GPU's 8-in-16 swap
    return stored;
}

uint32_t NextPowerOfTwo(uint32_t v)
{
    uint32_t p = 4;
    while (p < v) p <<= 1;
    return p;
}

// An LZO1X stream that stores bytes as one literal run, then the end marker.
// The vendored lzokay has no compressor; any LZO1X decoder reads this.
std::vector<uint8_t> StoreLzo(std::span<const uint8_t> bytes)
{
    Require(bytes.size() >= 4, "LZO literal run too short");
    std::vector<uint8_t> out;
    const size_t run = bytes.size() - 3;
    if (run <= 15) out.push_back(uint8_t(run));
    else
    {
        // 0, then 255 per zero byte, then the rest (1..255) on top of 15.
        size_t rest = run - 15;
        out.push_back(0);
        while (rest > 255) { out.push_back(0); rest -= 255; }
        out.push_back(uint8_t(rest));
    }
    out.insert(out.end(), bytes.begin(), bytes.end());
    out.insert(out.end(), {0x11, 0x00, 0x00});
    return out;
}

std::vector<uint8_t> CompressBulk(const std::vector<uint8_t> &raw)
{
    std::vector<std::vector<uint8_t>> chunks;
    for (size_t done = 0; done < raw.size(); done += kBulkChunk)
        chunks.push_back(StoreLzo(std::span(raw).subspan(done, std::min<size_t>(kBulkChunk, raw.size() - done))));
    Writer w;
    w.U32(ue3::kPackageMagic);
    w.U32(kBulkChunk);
    size_t compressed = 0;
    for (const auto &c : chunks) compressed += c.size();
    w.U32(uint32_t(compressed));
    w.U32(uint32_t(raw.size()));
    for (size_t i = 0; i < chunks.size(); ++i)
    {
        w.U32(uint32_t(chunks[i].size()));
        w.U32(uint32_t(std::min<size_t>(kBulkChunk, raw.size() - i * kBulkChunk)));
    }
    for (const auto &c : chunks) w.Raw(c);
    return std::move(w.bytes);
}

// The page serialized at base with the atlas at (0, top); its height becomes height.
std::vector<uint8_t> WritePage(const Package &package, const ue3::Export &e, const Page &page, const Glyphs &atlas,
                               uint32_t top, uint32_t height, size_t base)
{
    std::vector<uint8_t> raw = page.raw;
    raw.resize(TiledSize(page.width, height, page.format), 0);
    if (page.format == kFormatArgb)
    {
        // One pixel per element, stored A, R, G, B (modding/asset_export.cpp Untile reads it back).
        const uint32_t pitch = (page.width + 31) / 32 * 32;
        for (uint32_t y = 0; y < atlas.height; ++y)
            for (uint32_t x = 0; x < page.width; ++x)
            {
                uint8_t px[4] = {};
                if (x < atlas.width) std::memcpy(px, &atlas.rgba[(size_t(y) * atlas.width + x) * 4], 4);
                const uint8_t stored[4] = {px[3], px[0], px[1], px[2]};
                std::memcpy(&raw[xenos_texture::TiledOffset2D(x, top + y, pitch, 2)], stored, 4);
            }
    }
    const uint32_t pitch = (page.width / 4 + 31) / 32 * 32;
    for (uint32_t by = top / 4; page.format == kFormatDxt5 && by < (top + atlas.height + 3) / 4; ++by)
        for (uint32_t bx = 0; bx < page.width / 4; ++bx)
        {
            std::array<std::array<uint8_t, 4>, 16> px{};
            for (uint32_t i = 0; i < 16; ++i)
            {
                const uint32_t x = bx * 4 + i % 4, y = by * 4 + i / 4 - top;
                if (x < atlas.width && y < atlas.height)
                    std::memcpy(px[i].data(), &atlas.rgba[(size_t(y) * atlas.width + x) * 4], 4);
            }
            const auto block = EncodeDxt5(px);
            std::memcpy(&raw[TiledOffset(bx, by, pitch)], block.data(), 16);
        }
    const auto bulk = CompressBulk(raw);

    // Properties as they were, with the new SizeY; then the native data at its new place.
    std::vector<uint8_t> out(package.bytes.begin() + e.offset, package.bytes.begin() + std::ptrdiff_t(page.props.end));
    PutBe32(out, page.props.Find("SizeY")->value - e.offset, height);
    Writer w;
    w.Raw(out);
    w.U32(0); w.U32(0); w.U32(0);
    w.U32(uint32_t(base + w.bytes.size() + 4));
    w.U32(1);
    w.U32(kBulkLzo);
    w.U32(uint32_t(raw.size()));
    w.U32(uint32_t(bulk.size()));
    w.U32(uint32_t(base + w.bytes.size() + 4));
    w.Raw(bulk);
    w.U32(page.width);
    w.U32(height);
    return std::move(w.bytes);
}

std::vector<uint8_t> WriteFont(const Package &package, const ue3::Export &e, const Font &font, const std::vector<FontGlyph> &glyphs,
                               const std::vector<std::pair<uint16_t, uint16_t>> &remap)
{
    const auto *characters = font.props.Find("Characters");
    Writer w;
    w.Raw(package.bytes.subspan(e.offset, characters->begin - e.offset));
    // Characters: the original elements, then new ones from the first as a template.
    Writer value;
    value.U32(uint32_t(glyphs.size()));
    value.Raw(package.bytes.subspan(characters->value + 4, characters->size - 4));
    for (size_t i = font.glyphs.size(); i < glyphs.size(); ++i)
    {
        auto element = font.element;
        PutBe32(element, font.startU, glyphs[i].x);
        PutBe32(element, font.startV, glyphs[i].y);
        PutBe32(element, font.uSize, glyphs[i].width);
        PutBe32(element, font.vSize, glyphs[i].height);
        element[font.textureIndex] = glyphs[i].page;
        value.Raw(element);
    }
    std::vector<uint8_t> header(package.bytes.begin() + std::ptrdiff_t(characters->begin),
                                package.bytes.begin() + std::ptrdiff_t(characters->value));
    PutBe32(header, characters->sizeField - characters->begin, uint32_t(value.bytes.size()));
    w.Raw(header);
    w.Raw(value.bytes);
    const size_t after = characters->value + characters->size;
    w.Raw(package.bytes.subspan(after, font.props.end - after)); // the other properties and None

    w.U32(uint32_t(glyphs.size()));
    for (const auto &g : glyphs) { w.U32(g.x); w.U32(g.y); w.U32(g.width); w.U32(g.height); w.U8(g.page); }
    w.U32(uint32_t(font.pages.size()));
    for (const auto page : font.pages) w.U32(page);
    w.U32(uint32_t(font.kerning));
    w.U32(uint32_t(remap.size()));
    for (const auto &[cp, index] : remap) { w.U16(cp); w.U16(index); }
    w.U32(font.remapped);
    return std::move(w.bytes);
}

void Append(std::vector<uint8_t> &out, const ue3::Export &e, const std::vector<uint8_t> &data, size_t base)
{
    out.insert(out.end(), data.begin(), data.end());
    PutBe32(out, e.record + 32, uint32_t(data.size()));
    PutBe32(out, e.record + 36, uint32_t(base));
    // Nothing refers to the old bytes now; zeros pack to almost nothing.
    std::fill_n(out.begin() + std::ptrdiff_t(e.offset), e.size, uint8_t(0));
}
size_t Align(std::vector<uint8_t> &out)
{
    while (out.size() % 16) out.push_back(0);
    Require(out.size() < 0xf0000000u, "package too large");
    return out.size();
}
}

Glyphs Parse(std::span<const uint8_t> file)
{
    static constexpr uint8_t kMagic[9] = {'L', 'O', 'F', 'O', 'N', 'T', '1', '\r', '\n'};
    Require(file.size() >= 32 && std::memcmp(file.data(), kMagic, 9) == 0, "not a LOFONT1 file");
    // The magic is 9 bytes; the header words start at 12 after three zero bytes.
    Require(file[9] == 0 && file[10] == 0 && file[11] == 0, "not a LOFONT1 file");
    const auto header = Le32(file, 12), count = Le32(file, 16), width = Le32(file, 20), height = Le32(file, 24);
    Require(header == 32 && Le32(file, 28) == 0, "unsupported LOFONT1 header");
    Require(count > 0 && count <= kMaxGlyphs, "glyph count out of range");
    Require(width > 0 && height > 0 && width <= kMaxSide && height <= kMaxSide, "atlas size out of range");
    const size_t glyphBytes = size_t(count) * 12, pixels = size_t(width) * height * 4;
    Require(file.size() == header + glyphBytes + pixels, "LOFONT1 size does not match its header");
    Glyphs result;
    result.width = width; result.height = height;
    for (uint32_t i = 0; i < count; ++i)
    {
        const size_t p = header + size_t(i) * 12;
        Glyph g{Le32(file, p), Le16(file, p + 4), Le16(file, p + 6), Le16(file, p + 8), Le16(file, p + 10)};
        Require(g.codepoint > 0 && g.codepoint <= 0xffff, "code point out of range");
        Require(g.width > 0 && g.height > 0 && g.x + g.width <= width && g.y + g.height <= height, "glyph outside the atlas");
        result.glyphs.push_back(g);
    }
    result.rgba.assign(file.begin() + std::ptrdiff_t(header + glyphBytes), file.end());
    return result;
}

std::vector<std::string> Check(std::span<const uint8_t> bytes)
{
    const Package package(bytes);
    std::vector<std::string> lines;
    for (const auto &e : package.exports)
    {
        if (package.ClassName(e) != "Font") continue;
        const auto font = ReadFont(package, e);
        // The Characters property, element by element, against the native table.
        const auto *characters = font.props.Find("Characters");
        Reader array{package.bytes.first(characters->value + characters->size), characters->value + 4};
        for (size_t i = 0; i < font.glyphs.size(); ++i)
        {
            const auto element = ReadProperties(package, array, false);
            const auto &g = font.glyphs[i];
            const bool same = uint32_t(IntValue(package, element, "StartU")) == g.x && uint32_t(IntValue(package, element, "StartV")) == g.y &&
                              uint32_t(IntValue(package, element, "USize")) == g.width && uint32_t(IntValue(package, element, "VSize")) == g.height &&
                              package.bytes[element.Find("TextureIndex")->value] == g.page;
            Require(same, e.name + ": glyph " + std::to_string(i) + " differs between the two tables");
        }
        std::string pages;
        std::vector<std::pair<uint32_t, uint32_t>> sizes;
        for (const auto ref : font.pages)
        {
            const auto *page = package.ExportAt(int32_t(ref));
            Require(page, e.name + ": page reference outside the package");
            const auto decoded = ReadPage(package, *page);
            sizes.emplace_back(decoded.width, decoded.height);
            pages += (pages.empty() ? "" : ", ") + std::to_string(decoded.width) + "x" + std::to_string(decoded.height);
        }
        // Some original fonts (Icon) have glyphs past their page; they are reported, not refused.
        size_t outside = 0;
        for (const auto &g : font.glyphs)
        {
            Require(g.page < sizes.size(), e.name + ": glyph on a page the font does not have");
            outside += g.x + g.width > sizes[g.page].first || g.y + g.height > sizes[g.page].second;
        }
        lines.push_back(e.name + ": " + std::to_string(font.glyphs.size()) + " glyphs, " + std::to_string(font.remap.size()) +
                        " code points, kerning " + std::to_string(font.kerning) + ", pages " + pages +
                        (outside ? ", " + std::to_string(outside) + " glyphs past their page" : ""));
    }
    return lines;
}

std::vector<FontInfo> ReadFonts(std::span<const uint8_t> bytes)
{
    const Package package(bytes);
    std::vector<FontInfo> fonts;
    for (size_t index = 0; index < package.exports.size(); ++index)
    {
        const auto &e = package.exports[index];
        if (package.ClassName(e) != "Font" || e.name.starts_with("Default__")) continue;
        const auto font = ReadFont(package, e);
        FontInfo info{e.name, uint32_t(index), font.kerning};
        for (const auto ref : font.pages)
        {
            const auto *page = package.ExportAt(int32_t(ref));
            Require(page && package.ClassName(*page) == "Texture2D", e.name + ": page is not a Texture2D");
            Reader r = ue3::ObjectReader(package, *page);
            const auto props = ReadProperties(package, r, true);
            const auto *format = props.Find("Format");
            info.pages.push_back({ref - 1, page->name, uint32_t(IntValue(package, props, "SizeX")), uint32_t(IntValue(package, props, "SizeY")),
                                  format && format->type == "ByteProperty" && format->size == 1 ? package.bytes[format->value] : 0u});
        }
        for (const auto &[cp, i] : font.remap)
        {
            const auto &g = font.glyphs[i];
            info.glyphs.push_back({cp, g.x, g.y, g.width, g.height, g.page});
        }
        std::sort(info.glyphs.begin(), info.glyphs.end(), [](const auto &a, const auto &b) { return a.codepoint < b.codepoint; });
        fonts.push_back(std::move(info));
    }
    return fonts;
}

Result RebuildPackage(std::span<const uint8_t> bytes, const std::vector<Addition> &additions)
{
    Result result;
    std::unique_ptr<Package> package;
    try { package = std::make_unique<Package>(bytes); }
    catch (const std::exception &error) { result.notes.push_back(std::string("package unreadable: ") + error.what()); return result; }
    std::vector<uint8_t> out(bytes.begin(), bytes.end());
    // Each font is read from the original package: a second font on the same
    // last page would write over the first one's glyphs. No shipped package has one.
    std::set<uint32_t> written;
    for (const auto &addition : additions)
    {
        try
        {
            const ue3::Export *e = nullptr;
            for (const auto &candidate : package->exports)
                if (candidate.name == addition.font && package->ClassName(candidate) == "Font") e = &candidate;
            Require(e, "the package has no font " + addition.font);
            const auto font = ReadFont(*package, *e);
            const auto pageRef = font.pages.back();
            const auto *pageExport = package->ExportAt(int32_t(pageRef));
            Require(pageExport, "page reference outside the package");
            Require(!written.count(pageRef), "its last page " + pageExport->name + " already took another font's glyphs");
            const auto page = ReadPage(*package, *pageExport);
            const auto &atlas = addition.glyphs;
            Require(atlas.width <= page.width, "atlas is " + std::to_string(atlas.width) + " wide, the page " + std::to_string(page.width));
            Require(font.glyphs.size() + atlas.glyphs.size() <= kMaxGlyphs, "too many glyphs");

            // Below every glyph of the last page, at a block row of its own.
            const auto pageIndex = uint8_t(font.pages.size() - 1);
            uint32_t bottom = 0;
            for (const auto &g : font.glyphs)
                if (g.page == pageIndex) bottom = std::max(bottom, g.y + g.height);
            const uint32_t top = (bottom + 3) / 4 * 4;
            const uint32_t height = top + atlas.height <= page.height ? page.height : NextPowerOfTwo(top + atlas.height);
            Require(height <= kMaxSide, "the glyphs do not fit on a " + std::to_string(kMaxSide) + " page");

            auto glyphs = font.glyphs;
            auto remap = font.remap;
            std::map<uint16_t, size_t> where;
            for (size_t i = 0; i < remap.size(); ++i) where[remap[i].first] = i;
            size_t replaced = 0;
            for (const auto &g : atlas.glyphs)
            {
                const auto index = uint16_t(glyphs.size());
                glyphs.push_back({g.x, g.y + top, g.width, g.height, pageIndex});
                const auto cp = uint16_t(g.codepoint);
                if (const auto it = where.find(cp); it != where.end()) { remap[it->second].second = index; ++replaced; }
                else { where[cp] = remap.size(); remap.emplace_back(cp, index); }
            }

            auto base = Align(out);
            Append(out, *pageExport, WritePage(*package, *pageExport, page, atlas, top, height, base), base);
            base = Align(out);
            Append(out, *e, WriteFont(*package, *e, font, glyphs, remap), base);
            written.insert(pageRef);
            ++result.patched;
            result.notes.push_back(addition.font + ": " + std::to_string(atlas.glyphs.size()) + " glyphs (" + std::to_string(replaced) +
                                   " replaced) on " + pageExport->name + ", now " + std::to_string(page.width) + "x" +
                                   std::to_string(height));
        }
        catch (const std::exception &error)
        {
            result.notes.push_back(addition.font + " kept original (" + addition.source + "): " + error.what());
        }
    }
    if (result.patched) result.package = std::move(out);
    return result;
}
}
