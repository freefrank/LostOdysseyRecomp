#include "menu_assets.h"
#include <gpu/shader/cpx_decode.h>
#include <lzokay.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace settings::menu_assets
{
namespace
{
constexpr size_t MaxPackage = 32 * 1024 * 1024;
void Check(bool ok) { if (!ok) throw std::runtime_error("unsupported menu asset"); }
struct Reader
{
    std::span<const uint8_t> bytes;
    size_t pos = 0;
    std::span<const uint8_t> Take(size_t count)
    {
        Check(pos <= bytes.size() && count <= bytes.size() - pos);
        const auto value = bytes.subspan(pos, count); pos += count; return value;
    }
    uint32_t U32()
    {
        const auto b = Take(4);
        return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
    }
    uint16_t U16() { const auto b = Take(2); return uint16_t(uint16_t(b[0]) << 8 | b[1]); }
    int32_t I32() { return int32_t(U32()); }
    uint8_t Byte() { return Take(1)[0]; }
    std::string String()
    {
        const auto size = I32(); Check(size > 0 && size <= 4096);
        const auto b = Take(size); Check(b.back() == 0);
        Check(std::all_of(b.begin(), b.end() - 1, [](uint8_t c) { return c >= 32 && c < 127; }));
        return {reinterpret_cast<const char *>(b.data()), b.size() - 1};
    }
};
struct Export { int32_t cls = 0, outer = 0; std::string name; uint32_t offset = 0, size = 0; };
struct Object
{
    Reader native;
    std::map<std::string, int32_t> integers;
};
struct Package
{
    std::span<const uint8_t> bytes;
    std::vector<std::string> names, imports;
    std::vector<Export> exports;
    std::string Name(Reader &r) const
    {
        const auto index = r.U32(), number = r.U32(); Check(index < names.size() && number < 65536);
        return names[index] + (number ? "_" + std::to_string(number - 1) : "");
    }
    explicit Package(std::span<const uint8_t> b) : bytes(b)
    {
        Check(b.size() >= 64 && b.size() <= MaxPackage);
        Reader r{b}; Check(r.U32() == 0x9e2a83c1 && r.U32() == 0x002a01a3);
        const auto header = r.U32(); r.String(); r.U32();
        const auto nc = r.U32(), no = r.U32(), ec = r.U32(), eo = r.U32();
        const auto ic = r.U32(), io = r.U32(), dep = r.U32();
        Check(nc && nc <= 4096 && ec && ec <= 2048 && ic && ic <= 256);
        Check(no <= io && io <= eo && eo <= dep && dep <= header && header <= b.size());
        r = {b.first(io), no};
        for (uint32_t i = 0; i < nc; ++i) { names.push_back(r.String()); r.Take(8); }
        Check(r.pos == io);
        r = {b.first(eo), io};
        for (uint32_t i = 0; i < ic; ++i)
        {
            Name(r); Name(r); r.I32(); imports.push_back(Name(r));
        }
        Check(r.pos == eo);
        r = {b.first(dep), eo};
        for (uint32_t i = 0; i < ec; ++i)
        {
            Export e; e.cls = r.I32(); r.I32(); e.outer = r.I32(); e.name = Name(r);
            r.I32(); r.Take(8); e.size = r.U32(); e.offset = r.U32(); r.U32();
            Check(r.U32() == 0); r.Take(20);
            Check(e.offset >= header && e.offset <= b.size() && e.size <= b.size() - e.offset);
            Check(e.cls < 0 && int64_t(-int64_t(e.cls)) <= int64_t(imports.size()));
            exports.push_back(std::move(e));
        }
        Check(r.pos == dep);
    }
    const Export &Find(const std::string &name, const char *cls) const
    {
        const auto it = std::find_if(exports.begin(), exports.end(), [&](const auto &e) {
            return e.name == name && imports[size_t(-int64_t(e.cls) - 1)] == cls;
        });
        Check(it != exports.end()); return *it;
    }
    Object Read(const Export &e) const
    {
        Reader r{bytes.first(size_t(e.offset) + e.size), e.offset}; r.I32();
        Object result;
        for (size_t count = 0; count < 4096; ++count)
        {
            const auto name = Name(r);
            if (name == "None") { result.native = r; return result; }
            const auto type = Name(r); const auto size = r.U32(); r.U32();
            if (type == "StructProperty") Name(r);
            if (type == "BoolProperty") result.integers[name] = int32_t(r.U32());
            Reader value{r.Take(size)};
            if (type == "IntProperty") result.integers[name] = value.I32();
            else if (type == "ByteProperty") result.integers[name] = value.Byte();
        }
        throw std::runtime_error("menu property count");
    }
};
size_t TiledOffset(uint32_t x, uint32_t y, uint32_t pitch)
{
    // Same Xenos 2D addressing as gpu/video.cpp; pitch is in 16-byte BC3 blocks.
    const uint32_t outer = (((y >> 5) * (pitch >> 5)) + (x >> 5)) << 6;
    const uint32_t inner = (((y >> 1) & 7) << 3) | (x & 7);
    const uint32_t v = (outer | inner) << 4;
    const uint32_t bank = (y >> 4) & 1, pipe = ((x >> 3) & 3) ^ (((y >> 3) & 1) << 1);
    return ((y & 1) << 4) | (pipe << 6) | (bank << 11) | (v & 15) |
           (((v >> 4) & 1) << 5) | (((v >> 5) & 7) << 8) | ((v >> 8) << 12);
}
void Bc3(const uint8_t *b, Image &image, uint32_t x, uint32_t y)
{
    uint32_t alpha[8] = {b[0], b[1]};
    if (alpha[0] > alpha[1])
        for (uint32_t i = 1; i <= 6; ++i) alpha[i + 1] = ((7 - i) * alpha[0] + i * alpha[1]) / 7;
    else
    {
        for (uint32_t i = 1; i <= 4; ++i) alpha[i + 1] = ((5 - i) * alpha[0] + i * alpha[1]) / 5;
        alpha[6] = 0; alpha[7] = 255;
    }
    uint64_t abits = 0;
    for (unsigned i = 0; i < 6; ++i) abits |= uint64_t(b[i + 2]) << (8 * i);
    uint32_t colors[4][3]{};
    for (unsigned i = 0; i < 2; ++i)
    {
        const auto c = uint32_t(b[8 + 2 * i]) | uint32_t(b[9 + 2 * i]) << 8;
        const auto r = (c >> 11) & 31, g = (c >> 5) & 63, bl = c & 31;
        colors[i][0] = (r << 3) | (r >> 2); colors[i][1] = (g << 2) | (g >> 4); colors[i][2] = (bl << 3) | (bl >> 2);
    }
    for (unsigned c = 0; c < 3; ++c)
    {
        colors[2][c] = (2 * colors[0][c] + colors[1][c]) / 3;
        colors[3][c] = (colors[0][c] + 2 * colors[1][c]) / 3;
    }
    uint32_t cbits = uint32_t(b[12]) | uint32_t(b[13]) << 8 | uint32_t(b[14]) << 16 | uint32_t(b[15]) << 24;
    for (unsigned i = 0; i < 16; ++i)
    {
        const auto *c = colors[(cbits >> (2 * i)) & 3];
        image.pixels[size_t(y + i / 4) * image.width + x + i % 4] =
            alpha[(abits >> (3 * i)) & 7] << 24 | c[0] << 16 | c[1] << 8 | c[2];
    }
}
Image Texture(const Package &package, const Export &e)
{
    auto object = package.Read(e); auto r = object.native;
    const auto width = object.integers.at("SizeX"), height = object.integers.at("SizeY");
    Check(object.integers.at("Format") == 7 && width >= 128 && height >= 128 && width <= 2048 && height <= 2048);
    Check((width & (width - 1)) == 0 && (height & (height - 1)) == 0);
    Check(r.U32() == 0 && r.U32() == 0 && r.U32() == 0); const auto sourceOffset = r.U32();
    Check(sourceOffset == r.pos);
    const auto mipCount = r.U32(); Check(mipCount > 0 && mipCount <= 12);
    const auto flags = r.U32(), rawSize = r.U32(), storedSize = r.U32(), offset = r.U32();
    Check(flags == 0x10 && rawSize == uint32_t(width * height) && offset == r.pos && storedSize <= MaxPackage);
    Reader stream{r.Take(storedSize)};
    Check(stream.U32() == 0x9e2a83c1); const auto blockSize = stream.U32();
    const auto compressed = stream.U32(), decoded = stream.U32();
    Check(blockSize == 131072 && decoded == rawSize);
    const auto count = (decoded + blockSize - 1) / blockSize;
    std::vector<std::pair<uint32_t, uint32_t>> chunks;
    uint64_t storedTotal = 0, decodedTotal = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        const auto a = stream.U32(), b = stream.U32();
        Check(a > 0 && b == std::min(blockSize, decoded - i * blockSize));
        chunks.emplace_back(a, b); storedTotal += a; decodedTotal += b;
    }
    Check(storedTotal == compressed && decodedTotal == decoded && stream.pos + storedTotal == stream.bytes.size());
    std::vector<uint8_t> raw(decoded); size_t written = 0;
    for (const auto [a, b] : chunks)
    {
        const auto input = stream.Take(a); size_t actual = 0;
        Check(lzokay::decompress(input.data(), input.size(), raw.data() + written, b, actual) == lzokay::EResult::Success && actual == b);
        written += actual;
    }
    Check(r.U32() == uint32_t(width) && r.U32() == uint32_t(height));
    Image image{uint32_t(width), uint32_t(height), std::vector<uint32_t>(size_t(width) * height)};
    for (uint32_t by = 0; by < uint32_t(height) / 4; ++by)
        for (uint32_t bx = 0; bx < uint32_t(width) / 4; ++bx)
        {
            const auto address = TiledOffset(bx, by, uint32_t(width) / 4);
            Check(address <= raw.size() && 16 <= raw.size() - address);
            uint8_t block[16];
            for (unsigned i = 0; i < 16; ++i) block[i] = raw[address + (i ^ 1)];
            Bc3(block, image, bx * 4, by * 4);
        }
    return image;
}

uint32_t Le32(std::span<const uint8_t> b, size_t p)
{
    Check(p <= b.size() && 4 <= b.size() - p);
    return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24;
}
uint16_t Le16(std::span<const uint8_t> b, size_t p)
{
    Check(p <= b.size() && 2 <= b.size() - p); return uint16_t(b[p] | uint16_t(b[p + 1]) << 8);
}
std::vector<uint8_t> ReadFile(const std::filesystem::path &file, uint64_t offset, size_t size)
{
    Check(size && size <= MaxPackage);
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    Check(bool(input)); const auto length = input.tellg(); Check(length >= 0 && offset <= uint64_t(length) && size <= uint64_t(length) - offset);
    std::vector<uint8_t> bytes(size); input.seekg(std::streamoff(offset));
    Check(bool(input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(size)))); return bytes;
}
struct Extent { std::string archive; uint64_t offset; uint32_t size; };
struct Index
{
    std::vector<uint8_t> bytes;
    uint32_t dictionary, extensions;
    std::map<std::string, Extent> files;
    std::string Unpack(size_t offset) const
    {
        // FPI base-40 filename alphabet. This is format metadata, not artwork.
        constexpr char alphabet[] = "\0" "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_.\\";
        auto word = Le16(bytes, offset); const auto count = word % 40;
        Check(word / 1600 < 40); std::string value;
        value += alphabet[word / 40 % 40]; value += alphabet[word / 1600];
        for (unsigned i = 0; i < count; ++i)
        {
            word = Le16(bytes, offset + 2 + 2 * i); Check(word / 1600 < 40);
            value += alphabet[word % 40]; value += alphabet[word / 40 % 40]; value += alphabet[word / 1600];
        }
        if (const auto end = value.find('\0'); end != std::string::npos) value.resize(end);
        for (auto &c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return value;
    }
    std::string Name(uint32_t bits) const
    {
        if (!(bits & 0x3ffff)) return {};
        constexpr const char *suffix[] = {"", "_sndw", "_scrw", "_mapw", "_lvdw", "_navw", "_colw", "_camw",
            "_map", "_cam", "_bx", "_mw", "_a", "_d", "_f", "_m", "_p", "_u", "_w", "_0", "_1", "_2", "_00", "_01",
            "_0mw", "_nav", "_elgt", "_000a0", "_010a0", "_020a0", "_030a0", "_040a0"};
        auto name = Unpack(size_t(dictionary) + (bits & 0x3ffff) * 2) + suffix[(bits >> 18) & 31];
        const auto ext = (bits >> 23) & 31;
        if (ext) name += "." + Unpack(size_t(dictionary) + Le16(bytes, size_t(extensions) + (ext - 1) * 2) * 2);
        return name;
    }
    explicit Index(const std::filesystem::path &file)
    {
        const auto size = std::filesystem::file_size(file); Check(size >= 64 && size <= 2 * 1024 * 1024);
        bytes = ReadFile(file, 0, size); Check(Le32(bytes, 8) == 0x10000);
        dictionary = Le32(bytes, 40); extensions = Le32(bytes, 44);
        const auto count = Le16(bytes, 26);
        const auto begin = Le32(bytes, 32);
        Check(count && count <= 64 && begin >= 64 && begin <= bytes.size() && size_t(count) * 48 <= bytes.size() - begin);
        for (uint32_t i = 0; i < count; ++i)
        {
            const auto ar = size_t(begin) + i * 48;
            const auto archive = Name(Le32(bytes, ar + 24));
            // Only this resource archive can supply the selected localized menu packages.
            if (archive != "xenon_loc.fpd") continue;
            const auto base = ar + Le32(bytes, ar + 4);
            Check(base <= bytes.size());
            struct Folder { uint32_t index, count, depth; std::string path; };
            const auto prefix = Name(Le32(bytes, ar + 20));
            std::vector<Folder> pending{{0, Le16(bytes, ar + 2), 0, prefix.empty() ? "" : prefix + "\\"}};
            std::set<uint32_t> visited;
            while (!pending.empty())
            {
                const auto folder = std::move(pending.back()); pending.pop_back(); Check(folder.depth < 16);
                Check(uint64_t(folder.index) + folder.count <= 65536);
                for (uint32_t j = 0; j < folder.count; ++j)
                {
                    const auto index = folder.index + j; Check(visited.insert(index).second && visited.size() <= 65536);
                    const auto p = base + size_t(index) * 24; Check(p <= bytes.size() && 24 <= bytes.size() - p);
                    const auto bits = Le32(bytes, p); const auto path = folder.path + Name(bits);
                    if (bits & 0x10000000)
                        pending.push_back({Le32(bytes, p + 20), Le16(bytes, p + 14), folder.depth + 1, path + "\\"});
                    else if (path.find("\\menu\\rp") != std::string::npos)
                        files.emplace(path, Extent{archive, uint64_t(Le32(bytes, p + 8) & 0xffffff) * 2048, Le32(bytes, p + 16)});
                }
            }
        }
    }
    std::vector<uint8_t> ReadPackage(const std::filesystem::path &disc, const std::string &language, const char *family) const
    {
        const auto path = "bin\\xenon\\loc\\" + language + "\\menu\\" + family + "_" + language + ".xxx";
        const auto it = files.find(path); Check(it != files.end()); const auto &e = it->second;
        auto bytes = ReadFile(disc / e.archive, e.offset, e.size);
        if (bytes.size() >= 3 && bytes[0] == 'c' && bytes[1] == 'p' && bytes[2] == 'x')
        {
            xenos::resources::cpx::Header header;
            Check(xenos::resources::cpx::ReadHeader(bytes, header) && header.decodedSize <= MaxPackage);
            Check(xenos::resources::cpx::Decode(bytes, bytes));
        }
        return bytes;
    }
};
}

Font DecodeFont(std::span<const uint8_t> bytes, const std::string &name)
{
    Package package(bytes); const auto &e = package.Find(name, "Font");
    auto r = package.Read(e).native; const auto count = r.U32(); Check(count && count <= 16384);
    std::vector<Glyph> characters(count);
    for (auto &g : characters)
    {
        g.x = r.U32(); g.y = r.U32(); g.width = r.U32(); g.height = r.U32(); g.page = r.Byte();
    }
    const auto pageCount = r.U32(); Check(pageCount && pageCount <= 32);
    std::vector<uint32_t> refs;
    for (uint32_t i = 0; i < pageCount; ++i) { const auto n = r.U32(); Check(n && n <= package.exports.size()); refs.push_back(n); }
    Font font; font.kerning = r.I32(); Check(font.kerning >= -32 && font.kerning <= 32);
    const auto remapCount = r.U32(); Check(remapCount && remapCount <= 65536);
    for (uint32_t i = 0; i < remapCount; ++i)
    {
        const auto cp = r.U16(), index = r.U16(); Check(index < characters.size());
        Check(font.glyphs.emplace(cp, characters[index]).second);
    }
    Check(r.U32() == 1 && r.pos == r.bytes.size());
    size_t totalPixels = 0;
    for (const auto ref : refs)
    {
        const auto &page = package.exports[ref - 1]; Check(page.outer == int32_t(&e - package.exports.data() + 1));
        auto image = Texture(package, page); totalPixels += image.pixels.size(); Check(totalPixels <= 16 * 1024 * 1024);
        font.pages.push_back(std::move(image));
    }
    for (const auto &[cp, g] : font.glyphs)
    {
        Check(g.page < font.pages.size()); const auto &image = font.pages[g.page];
        Check(g.width && g.height && g.x < image.width && g.y < image.height && g.width <= image.width - g.x && g.height <= image.height - g.y);
        Check(int(g.width) + font.kerning > 0); font.height = std::max(font.height, g.height);
    }
    return font;
}
Image DecodeTexture(std::span<const uint8_t> bytes, const std::string &name)
{
    Package package(bytes); return Texture(package, package.Find(name, "Texture2D"));
}
std::shared_ptr<const Assets> Load(const std::filesystem::path &gameRoot, uint32_t language) noexcept
{
    try
    {
        if (gameRoot.empty()) return {};
        // The installation root normally contains disc1; direct disc roots are also supported.
        auto disc = gameRoot / "disc1";
        if (!std::filesystem::exists(disc / "LO.fpi")) disc = gameRoot;
        Index index(disc / "LO.fpi");
        constexpr const char *languages[] = {"int", "chi", "jpn", "kor", "sch"};
        const std::string selected = languages[language < std::size(languages) ? language : 0];
        auto common = index.ReadPackage(disc, selected, "rpfontscommon");
        auto assets = std::make_shared<Assets>(); assets->language = selected;
        assets->body = DecodeFont(common, "Maru23");
        try { assets->title = DecodeFont(common, "LocTit1"); } catch (...) { /* Body face remains available. */ }
        try { assets->fallback = DecodeFont(common, "Abc"); } catch (...) { /* Missing glyphs retain GDI fallback. */ }
        auto menu = index.ReadPackage(disc, selected, "rpmenurescommon");
        assets->menu = DecodeTexture(menu, "UI_MAIN_00");
        // The UV layout below is the verified 512x1024 common menu atlas.
        Check(assets->menu.width == 512 && assets->menu.height == 1024);
        return assets;
    }
    catch (...) { return {}; }
}
std::shared_ptr<const Assets> Cached(const std::filesystem::path &gameRoot, uint32_t language) noexcept
{
    // Only the presentation thread calls this. A changed root discards the old cache.
    static std::filesystem::path root;
    static std::map<uint32_t, std::shared_ptr<const Assets>> cache;
    try
    {
        if (root != gameRoot) { root = gameRoot; cache.clear(); }
        auto [entry, inserted] = cache.try_emplace(language);
        if (inserted) entry->second = Load(gameRoot, language);
        return entry->second;
    }
    catch (...) { return {}; }
}
}
