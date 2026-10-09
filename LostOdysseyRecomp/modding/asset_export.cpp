#include "asset_export.h"
#include "mod_api.h"
#include "xenos_texture.h"
#include <gpu/shader/cpx_decode.h>
#include <gpu/texture_layout.h>
#include <lzokay.hpp>
#include <xxhash.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <span>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Header-only miniz; the runtime links its definitions from updater/stage.cpp.
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

namespace modding::asset_export
{
namespace
{
namespace fs = std::filesystem;
namespace cpx = xenos::resources::cpx;

constexpr uint32_t kPackageMagic = 0x9e2a83c1;
constexpr unsigned kMaxThreads = 4;
// The export is bound by deflate time: level 6 took twice as long as level 1
// for 13% smaller files.
constexpr unsigned kPngLevel = 1;
constexpr uint8_t kAsfHeader[16] = {0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
                                    0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};

void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }

// An unwritable output stops the export; anything wrong with game data is a counted skip.
struct OutputError : std::runtime_error { using std::runtime_error::runtime_error; };
struct Skip : std::runtime_error
{
    const char *reason;
    Skip(const char *why, const std::string &detail) : std::runtime_error(detail), reason(why) {}
};

std::string Utf8(const fs::path &path)
{
    const auto value = path.u8string();
    return {reinterpret_cast<const char *>(value.data()), value.size()};
}
fs::path FromUtf8(std::string_view value)
{
    return fs::path(std::u8string(reinterpret_cast<const char8_t *>(value.data()), value.size()));
}
std::string Lower(std::string value)
{
    for (auto &c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}
// Deep package folders under a deep output folder can pass MAX_PATH.
fs::path Writable(fs::path path)
{
#ifdef _WIN32
    path.make_preferred();
    const std::wstring native = path.native();
    if (native.size() < 240 || native.rfind(L"\\\\?\\", 0) == 0 || !path.is_absolute()) return path;
    if (native.rfind(L"\\\\", 0) == 0) return fs::path(L"\\\\?\\UNC\\" + native.substr(2));
    return fs::path(L"\\\\?\\" + native);
#else
    return path;
#endif
}
void CreateDirectories(const fs::path &directory)
{
    std::error_code error;
    fs::create_directories(Writable(directory), error);
    if (error) throw OutputError("cannot create " + Utf8(directory) + ": " + error.message());
}
void WriteFile(const fs::path &file, const void *data, size_t size)
{
    std::ofstream output(Writable(file), std::ios::binary | std::ios::trunc);
    if (!output || !output.write(static_cast<const char *>(data), std::streamsize(size)) || !output.flush())
        throw OutputError("cannot write " + Utf8(file));
}
uint32_t Le32(std::span<const uint8_t> b, size_t p)
{
    Require(p <= b.size() && 4 <= b.size() - p, "FPI read out of bounds");
    return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24;
}
uint16_t Le16(std::span<const uint8_t> b, size_t p)
{
    Require(p <= b.size() && 2 <= b.size() - p, "FPI read out of bounds");
    return uint16_t(b[p] | uint16_t(b[p + 1]) << 8);
}

// ---- Disc index (LO.fpi) -------------------------------------------------

struct Entry { std::string path; uint32_t archive = 0; uint64_t offset = 0; uint32_t size = 0; };
struct Disc
{
    fs::path root;
    uint32_t number = 0;
    std::vector<fs::path> archives; // empty path: archive file missing
    std::vector<uint64_t> archiveSizes;
    std::vector<Entry> entries;
};

struct FpiNames
{
    std::span<const uint8_t> bytes;
    uint32_t dictionary = 0, extensions = 0;
    std::string Unpack(size_t offset) const
    {
        // Base-40 filename alphabet of the FPI dictionary (see settings/menu_assets.cpp).
        constexpr char alphabet[] = "\0" "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_.\\";
        auto word = Le16(bytes, offset); const auto count = word % 40;
        Require(word / 1600 < 40, "invalid FPI name");
        std::string value;
        value += alphabet[word / 40 % 40]; value += alphabet[word / 1600];
        for (unsigned i = 0; i < count; ++i)
        {
            word = Le16(bytes, offset + 2 + 2 * i); Require(word / 1600 < 40, "invalid FPI name");
            value += alphabet[word % 40]; value += alphabet[word / 40 % 40]; value += alphabet[word / 1600];
        }
        if (const auto end = value.find('\0'); end != std::string::npos) value.resize(end);
        return Lower(std::move(value));
    }
    std::string Name(uint32_t bits) const
    {
        if (!(bits & 0x3ffff)) return {};
        constexpr const char *suffix[] = {"", "_sndw", "_scrw", "_mapw", "_lvdw", "_navw", "_colw", "_camw",
            "_map", "_cam", "_bx", "_mw", "_a", "_d", "_f", "_m", "_p", "_u", "_w", "_0", "_1", "_2", "_00", "_01",
            "_0mw", "_nav", "_elgt", "_000a0", "_010a0", "_020a0", "_030a0", "_040a0"};
        auto name = Unpack(size_t(dictionary) + (bits & 0x3ffff) * 2) + suffix[(bits >> 18) & 31];
        if (const auto ext = (bits >> 23) & 31)
            name += "." + Unpack(size_t(dictionary) + Le16(bytes, size_t(extensions) + (ext - 1) * 2) * 2);
        return name;
    }
};

bool SafeRelative(std::string_view path)
{
    if (path.empty() || path.front() == '/') return false;
    size_t begin = 0;
    while (begin <= path.size())
    {
        const auto end = std::min(path.find('/', begin), path.size());
        const auto part = path.substr(begin, end - begin);
        if (part.empty() || part == "." || part == "..") return false;
        begin = end + 1;
    }
    return true;
}

Disc ReadDisc(const fs::path &root)
{
    Disc disc; disc.root = root;
    const auto fpi = root / "LO.fpi";
    std::error_code error;
    const auto size = fs::file_size(fpi, error);
    Require(!error && size >= 64 && size <= 4 * 1024 * 1024, "LO.fpi missing or invalid");
    std::vector<uint8_t> bytes(size_t(size), 0);
    {
        std::ifstream input(fpi, std::ios::binary);
        Require(input && input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size())), "LO.fpi unreadable");
    }
    Require(Le32(bytes, 8) == 0x10000, "unsupported LO.fpi version");
    disc.number = bytes[20];
    FpiNames names{bytes, Le32(bytes, 40), Le32(bytes, 44)};
    const uint32_t count = Le16(bytes, 26), begin = Le32(bytes, 32);
    Require(count && count <= 64 && begin >= 64 && begin <= bytes.size() && size_t(count) * 48 <= bytes.size() - begin,
            "invalid LO.fpi archive table");
    // Archive names are lower case; match files case-insensitively for case-sensitive file systems.
    std::map<std::string, fs::path> files;
    for (const auto &item : fs::directory_iterator(root, error))
        files.emplace(Lower(Utf8(item.path().filename())), item.path());
    for (uint32_t i = 0; i < count; ++i)
    {
        const auto ar = size_t(begin) + i * 48;
        const auto it = files.find(names.Name(Le32(bytes, ar + 24)));
        uint64_t archiveSize = 0;
        if (it != files.end()) archiveSize = fs::file_size(it->second, error);
        const bool present = it != files.end() && !error;
        disc.archives.push_back(present ? it->second : fs::path{});
        disc.archiveSizes.push_back(present ? archiveSize : 0);
        const auto base = ar + Le32(bytes, ar + 4);
        Require(base <= bytes.size(), "invalid LO.fpi archive record");
        struct Folder { uint32_t index, count, depth; std::string path; };
        const auto prefix = names.Name(Le32(bytes, ar + 20));
        std::vector<Folder> pending{{0, Le16(bytes, ar + 2), 0, prefix.empty() ? "" : prefix + "/"}};
        std::set<uint32_t> visited;
        while (!pending.empty())
        {
            const auto folder = std::move(pending.back()); pending.pop_back();
            Require(folder.depth < 64 && uint64_t(folder.index) + folder.count <= 65536, "invalid LO.fpi folder");
            for (uint32_t j = 0; j < folder.count; ++j)
            {
                const auto index = folder.index + j;
                Require(visited.insert(index).second, "LO.fpi folder cycle");
                const auto p = base + size_t(index) * 24;
                Require(p <= bytes.size() && 24 <= bytes.size() - p, "LO.fpi record out of bounds");
                const auto bits = Le32(bytes, p);
                auto path = folder.path + names.Name(bits);
                std::replace(path.begin(), path.end(), '\\', '/');
                if (bits & 0x10000000)
                    pending.push_back({Le32(bytes, p + 20), Le16(bytes, p + 14), folder.depth + 1, path + "/"});
                else
                    disc.entries.push_back({std::move(path), i, uint64_t(Le32(bytes, p + 8) & 0xffffff) * 2048, Le32(bytes, p + 16)});
            }
        }
    }
    return disc;
}

std::vector<uint8_t> ReadExtent(const fs::path &archive, uint64_t offset, size_t size)
{
    std::ifstream input(archive, std::ios::binary);
    Require(bool(input), "archive unreadable");
    input.seekg(std::streamoff(offset));
    std::vector<uint8_t> bytes(size);
    Require(input && input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(size)), "archive short read");
    return bytes;
}

// ---- UE3 package (big endian, version 0x1a3; see tools/asset_inventory/decode.cpp) ----

struct Reader
{
    std::span<const uint8_t> bytes;
    size_t pos = 0;
    std::span<const uint8_t> Take(size_t count)
    {
        Require(pos <= bytes.size() && count <= bytes.size() - pos, "truncated package data");
        const auto value = bytes.subspan(pos, count); pos += count; return value;
    }
    uint8_t Byte() { return Take(1)[0]; }
    uint32_t U32()
    {
        const auto b = Take(4);
        return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
    }
    int32_t I32() { return int32_t(U32()); }
    std::string String()
    {
        const auto size = I32();
        Require(size != 0 && size >= -32768 && size <= 32768, "invalid UE string length");
        if (size > 0)
        {
            const auto b = Take(size_t(size)); Require(b.back() == 0, "unterminated UE string");
            return {reinterpret_cast<const char *>(b.data()), b.size() - 1};
        }
        // FString UTF-16 payloads are little endian even in Xbox 360 packages.
        const auto b = Take(size_t(-int64_t(size)) * 2);
        Require(b[b.size() - 1] == 0 && b[b.size() - 2] == 0, "unterminated UTF-16 UE string");
        std::string result;
        for (size_t i = 0; i + 2 < b.size(); i += 2)
        {
            uint32_t point = uint32_t(b[i]) | uint32_t(b[i + 1]) << 8;
            if (point >= 0xd800 && point <= 0xdbff)
            {
                Require(i + 4 < b.size(), "invalid UTF-16 surrogate");
                const uint32_t low = uint32_t(b[i + 2]) | uint32_t(b[i + 3]) << 8;
                Require(low >= 0xdc00 && low <= 0xdfff, "invalid UTF-16 surrogate");
                point = 0x10000 + ((point - 0xd800) << 10) + low - 0xdc00; i += 2;
            }
            else Require(point < 0xdc00 || point > 0xdfff, "invalid UTF-16 surrogate");
            if (point < 0x80) result += char(point);
            else if (point < 0x800) { result += char(0xc0 | point >> 6); result += char(0x80 | (point & 63)); }
            else if (point < 0x10000)
            {
                result += char(0xe0 | point >> 12); result += char(0x80 | (point >> 6 & 63)); result += char(0x80 | (point & 63));
            }
            else
            {
                result += char(0xf0 | point >> 18); result += char(0x80 | (point >> 12 & 63));
                result += char(0x80 | (point >> 6 & 63)); result += char(0x80 | (point & 63));
            }
        }
        return result;
    }
};

struct Export { std::string name; int32_t classRef = 0; uint32_t offset = 0, size = 0; };
struct Package
{
    std::span<const uint8_t> bytes;
    std::vector<std::string> names, imports;
    std::vector<Export> exports;
    std::string Name(Reader &r) const
    {
        const auto index = r.U32(), number = r.U32();
        Require(index < names.size() && number <= 1000000, "invalid UE name reference");
        return names[index] + (number ? "_" + std::to_string(number - 1) : "");
    }
    explicit Package(std::span<const uint8_t> data) : bytes(data)
    {
        Reader r{data};
        Require(data.size() >= 44 && r.U32() == kPackageMagic, "not a UE3 package");
        const auto version = r.U32();
        Require(version == 0x002a01a3 || version == 0x002901a3, "unsupported UE3 package version");
        r.U32(); r.String(); r.U32(); // Header size, folder name, package flags.
        const auto nameCount = r.U32(), nameOffset = r.U32(), exportCount = r.U32(), exportOffset = r.U32();
        const auto importCount = r.U32(), importOffset = r.U32(), dependsOffset = r.U32();
        Require(nameCount <= 1000000 && exportCount <= 1000000 && importCount <= 1000000, "UE3 table count exceeds limit");
        Require(r.pos <= nameOffset && nameOffset <= importOffset && importOffset <= exportOffset &&
                exportOffset <= dependsOffset && dependsOffset <= data.size(), "invalid UE3 table offsets");
        r = {data.first(importOffset), nameOffset};
        for (uint32_t i = 0; i < nameCount; ++i) { names.push_back(r.String()); r.Take(8); }
        r = {data.first(exportOffset), importOffset};
        for (uint32_t i = 0; i < importCount; ++i) { Name(r); Name(r); r.I32(); imports.push_back(Name(r)); }
        r = {data.first(dependsOffset), exportOffset};
        for (uint32_t i = 0; i < exportCount; ++i)
        {
            Export e; e.classRef = r.I32(); r.I32(); r.I32(); e.name = Name(r);
            r.I32(); r.Take(8); e.size = r.U32(); e.offset = r.U32();
            // Version 419: ComponentMap, ExportFlags, NetObjects, package GUID.
            const auto components = r.U32();
            Require(components <= (r.bytes.size() - r.pos) / 12, "invalid export component map");
            for (uint32_t c = 0; c < components; ++c) { Name(r); r.I32(); }
            r.U32();
            const auto netObjects = r.U32();
            Require(netObjects <= (r.bytes.size() - r.pos) / 4, "invalid export net objects");
            r.Take(size_t(netObjects) * 4 + 16);
            Require(!e.size || (e.offset <= data.size() && e.size <= data.size() - e.offset), "export data outside package");
            Require(e.classRef >= 0 ? size_t(e.classRef) <= exportCount : uint64_t(-int64_t(e.classRef)) <= imports.size(),
                    "export class outside package tables");
            exports.push_back(std::move(e));
        }
    }
    std::string ClassName(const Export &e) const
    {
        if (e.classRef < 0) return imports[size_t(-int64_t(e.classRef)) - 1];
        if (e.classRef > 0) return exports[size_t(e.classRef) - 1].name;
        return "Class";
    }
};

// ---- Textures --------------------------------------------------------------

struct Format { int32_t id; const char *name; uint32_t block, bytes, bc; };
// UE3 EPixelFormat values present in the game's Texture2D, LightMapTexture2D
// and ShadowMapTexture2D exports. Endian swaps follow the Xenos fetch formats:
// DXT 8in16, k_8_8_8_8 8in32, k_8 none.
constexpr Format kFormats[] = {
    {5, "DXT1", 4, 8, 1}, {6, "DXT3", 4, 16, 2}, {7, "DXT5", 4, 16, 3}, {2, "A8R8G8B8", 1, 4, 0}, {3, "G8", 1, 1, 0}};

enum class TextureClass { None, Exportable, Unsupported };
TextureClass Classify(const std::string &name)
{
    // Texture2D subclasses serialize the same mip chain.
    static const std::set<std::string> exportable{"Texture2D", "LightMapTexture2D", "ShadowMapTexture2D",
                                                  "TextureFlipBook", "TerrainWeightMapTexture"};
    static const std::set<std::string> unsupported{"Texture", "TextureCube", "TextureMovie", "Texture2DComposite",
                                                   "TextureRenderTarget", "TextureRenderTarget2D", "TextureRenderTargetCube"};
    return exportable.count(name) ? TextureClass::Exportable
         : unsupported.count(name) ? TextureClass::Unsupported : TextureClass::None;
}

struct Image
{
    uint32_t width = 0, height = 0, channels = 0;
    const Format *format = nullptr;
    std::vector<uint8_t> pixels;
    uint64_t fingerprint = 0, fingerprintTiled = 0;
};

std::vector<uint8_t> DecompressLzo(std::span<const uint8_t> stored, uint32_t expected)
{
    // UE3 compressed bulk data: magic, block size, totals, then a chunk table.
    Reader s{stored};
    Require(s.U32() == kPackageMagic, "invalid LZO bulk header");
    const auto blockSize = s.U32(), compressed = s.U32(), decoded = s.U32();
    Require(blockSize && blockSize <= 1024 * 1024 && decoded == expected && decoded && decoded <= 256u * 1024 * 1024,
            "invalid LZO bulk sizes");
    const auto count = (decoded + uint64_t(blockSize) - 1) / blockSize;
    std::vector<std::pair<uint32_t, uint32_t>> chunks;
    uint64_t storedTotal = 0, decodedTotal = 0;
    for (uint64_t i = 0; i < count; ++i)
    {
        const auto a = s.U32(), b = s.U32();
        Require(a > 0 && b == std::min<uint64_t>(blockSize, decoded - i * blockSize), "invalid LZO chunk table");
        chunks.emplace_back(a, b); storedTotal += a; decodedTotal += b;
    }
    Require(storedTotal == compressed && decodedTotal == decoded && s.pos + storedTotal == stored.size(),
            "LZO chunk table does not match its payload");
    std::vector<uint8_t> raw(decoded);
    size_t written = 0;
    for (const auto [a, b] : chunks)
    {
        const auto input = s.Take(a); size_t actual = 0;
        Require(lzokay::decompress(input.data(), input.size(), raw.data() + written, b, actual) == lzokay::EResult::Success &&
                actual == b, "LZO chunk does not decode");
        written += actual;
    }
    return raw;
}

// Top mip bytes as stored: tiled, big endian.
struct TopMip { const Format *format = nullptr; uint32_t width = 0, height = 0, mips = 0; std::vector<uint8_t> raw; };

TopMip ReadTopMip(const Package &package, const Export &e)
{
    Require(e.size >= 4, "empty texture object");
    Reader r{package.bytes.first(size_t(e.offset) + e.size), e.offset};
    r.I32(); // Net index.
    int32_t width = -1, height = -1, formatId = -1;
    bool noTiling = false;
    for (size_t count = 0;; ++count)
    {
        Require(count < 65536, "texture property count exceeds limit");
        const auto property = package.Name(r);
        if (property == "None") break;
        const auto type = package.Name(r);
        const auto size = r.U32(); r.U32();
        if (type == "StructProperty") package.Name(r);
        uint32_t boolean = 0;
        if (type == "BoolProperty") boolean = r.U32();
        Reader value{r.Take(size)};
        if (property == "bNoTiling") noTiling = boolean != 0;
        else if (property == "SizeX" && type == "IntProperty") width = value.I32();
        else if (property == "SizeY" && type == "IntProperty") height = value.I32();
        else if (property == "Format" && type == "ByteProperty" && size == 1) formatId = value.Byte();
    }
    const auto format = std::find_if(std::begin(kFormats), std::end(kFormats), [&](const Format &f) { return f.id == formatId; });
    if (format == std::end(kFormats)) throw Skip("unsupported_format", "format " + std::to_string(formatId));
    if (noTiling) throw Skip("linear_layout", "bNoTiling texture");
    Require(width > 0 && height > 0 && width <= 8192 && height <= 8192 && uint64_t(width) * height <= 16u * 1024 * 1024,
            "invalid texture size");
    // Bulk data header: flags, element count, size on disk, offset in file.
    struct Bulk { uint32_t flags, count, size, offset; };
    auto bulk = [&] { Bulk b; b.flags = r.U32(); b.count = r.U32(); b.size = r.U32(); b.offset = r.U32(); return b; };
    auto inlinePayload = [](const Bulk &b) { return !(b.flags & 0x21) && b.size != 0xffffffffu; };
    const auto source = bulk(); // Editor source art; never present in console packages.
    if (inlinePayload(source)) r.Take(source.size);
    const auto mips = r.U32();
    Require(mips >= 1 && mips <= 16, "invalid mip count");
    const auto top = bulk();
    // 0x01 separate file, 0x20 not stored (UE3 BULKDATA flags).
    if (!inlinePayload(top)) throw Skip("top_mip_not_stored", "mip 0 flags " + std::to_string(top.flags));
    if (top.flags & 0x82) throw Skip("unsupported_compression", "mip 0 flags " + std::to_string(top.flags));
    const auto payload = r.Take(top.size);
    std::vector<uint8_t> raw;
    if (top.flags & 0x10) raw = DecompressLzo(payload, top.count);
    else { Require(top.size == top.count, "uncompressed mip size mismatch"); raw.assign(payload.begin(), payload.end()); }
    Require(r.U32() == uint32_t(width) && r.U32() == uint32_t(height), "top mip size differs from the texture size");
    return {&*format, uint32_t(width), uint32_t(height), mips, std::move(raw)};
}

// One w x h level whose first block sits at `origin` in a tiled surface of
// `pitch` blocks. RGBA8, or one grey channel for G8.
Image Untile(const Format &format, uint32_t w, uint32_t h, std::span<const uint8_t> raw,
             gpu::TextureBlockOffset origin, uint32_t pitch)
{
    const uint32_t bw = format.block, bpb = format.bytes;
    const uint32_t blocksX = (w + bw - 1) / bw, blocksY = (h + bw - 1) / bw, log2 = uint32_t(std::countr_zero(bpb));
    Image image{w, h, format.id == 3 ? 1u : 4u, &format, {}};
    image.pixels.resize(size_t(w) * h * image.channels);
    uint8_t block[16];
    uint32_t texels[16];
    for (uint32_t by = 0; by < blocksY; ++by)
        for (uint32_t bx = 0; bx < blocksX; ++bx)
        {
            const size_t address = xenos_texture::TiledOffset2D(bx + origin.x, by + origin.y, pitch, log2);
            Require(address <= raw.size() && bpb <= raw.size() - address, "texture data shorter than its tiled layout");
            std::memcpy(block, raw.data() + address, bpb);
            if (format.bc)
            {
                for (uint32_t i = 0; i < bpb; i += 2) std::swap(block[i], block[i + 1]);
                xenos_texture::DecodeBcBlock(block, format.bc, texels);
                for (unsigned i = 0; i < 16; ++i)
                {
                    const uint32_t x = bx * 4 + i % 4, y = by * 4 + i / 4;
                    if (x >= w || y >= h) continue;
                    auto *p = image.pixels.data() + (size_t(y) * w + x) * 4;
                    p[0] = uint8_t(texels[i] >> 16); p[1] = uint8_t(texels[i] >> 8); p[2] = uint8_t(texels[i]); p[3] = uint8_t(texels[i] >> 24);
                }
            }
            else if (bpb == 4)
            {
                // 8in32 leaves B, G, R, A (D3DFMT_A8R8G8B8) in memory order.
                std::swap(block[0], block[3]); std::swap(block[1], block[2]);
                auto *p = image.pixels.data() + (size_t(by) * w + bx) * 4;
                p[0] = block[2]; p[1] = block[1]; p[2] = block[0]; p[3] = block[3];
            }
            else image.pixels[size_t(by) * w + bx] = block[0];
        }
    return image;
}

// The texture-import identity, as the renderer logs it with
// LO_TEXTURE_FINGERPRINT_LOG (gpu/renderer.cpp GetTexture): XXH3-64 of the
// base level's blocks in row order after the guest endian swap (DXT 8in16,
// A8R8G8B8 8in32, G8 none), and of the tiled extent as stored (0 when the
// package holds less than that extent).
std::pair<uint64_t, uint64_t> Fingerprints(const Format &format, uint32_t w, uint32_t h, std::span<const uint8_t> raw,
                                           gpu::TextureBlockOffset origin, uint32_t pitch)
{
    const uint32_t bw = format.block, bpb = format.bytes;
    const uint32_t blocksX = (w + bw - 1) / bw, blocksY = (h + bw - 1) / bw, log2 = uint32_t(std::countr_zero(bpb));
    std::vector<uint8_t> row(size_t(blocksX) * bpb);
    XXH3_state_t *state = XXH3_createState();
    XXH3_64bits_reset(state);
    for (uint32_t by = 0; by < blocksY; ++by)
    {
        for (uint32_t bx = 0; bx < blocksX; ++bx)
        {
            const size_t address = xenos_texture::TiledOffset2D(bx + origin.x, by + origin.y, pitch, log2);
            uint8_t *block = row.data() + size_t(bx) * bpb;
            std::memcpy(block, raw.data() + address, bpb); // Untile already checked the extent.
            if (format.bc) for (uint32_t i = 0; i < bpb; i += 2) std::swap(block[i], block[i + 1]);
            else if (bpb == 4) { std::swap(block[0], block[3]); std::swap(block[1], block[2]); }
        }
        XXH3_64bits_update(state, row.data(), row.size());
    }
    const uint64_t linear = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    const uint64_t tiledBytes = uint64_t(pitch) * ((origin.y + blocksY + 31) & ~31u) * bpb;
    return {linear, raw.size() >= tiledBytes ? XXH3_64bits(raw.data(), size_t(tiledBytes)) : 0};
}

Image DecodeTexture(const Package &package, const Export &e)
{
    const auto top = ReadTopMip(package, e);
    // With a mip chain, level 0 sits inside the packed mip tail when the
    // shorter side is <= 16 texels (renderer.cpp GetTexture, Xenia
    // GetPackedMipOffset). Single-level textures start at block 0: in the
    // game data the rest of their tile is zero.
    const auto &format = *top.format;
    const auto origin = top.mips > 1 ? gpu::PackedMipOffset2D(top.width, top.height, 0, format.block, format.block)
                                     : gpu::TextureBlockOffset{};
    const uint32_t pitch = (origin.x + (top.width + format.block - 1) / format.block + 31) & ~31u;
    auto image = Untile(format, top.width, top.height, top.raw, origin, pitch);
    std::tie(image.fingerprint, image.fingerprintTiled) = Fingerprints(format, top.width, top.height, top.raw, origin, pitch);
    if (image.channels == 4)
    {
        // Opaque images are written as RGB.
        bool opaque = true;
        for (size_t i = 3; opaque && i < image.pixels.size(); i += 4) opaque = image.pixels[i] == 255;
        if (opaque)
        {
            for (size_t i = 0, j = 0; i < image.pixels.size(); i += 4, j += 3)
            {
                image.pixels[j] = image.pixels[i]; image.pixels[j + 1] = image.pixels[i + 1]; image.pixels[j + 2] = image.pixels[i + 2];
            }
            image.channels = 3;
            image.pixels.resize(size_t(image.width) * image.height * 3);
        }
    }
    return image;
}

size_t WritePng(const fs::path &file, const Image &image)
{
    size_t length = 0;
    void *png = tdefl_write_image_to_png_file_in_memory_ex(image.pixels.data(), int(image.width), int(image.height),
                                                           int(image.channels), &length, kPngLevel, MZ_FALSE);
    if (!png) throw OutputError("PNG encoding failed for " + Utf8(file));
    std::unique_ptr<void, decltype(&mz_free)> owner(png, &mz_free);
    WriteFile(file, png, length);
    return length;
}

std::string FileStem(std::string_view object)
{
    std::string name;
    for (const unsigned char c : object) name += c < 32 || std::strchr("<>:\"/\\|?*", c) ? '_' : char(c);
    // Device names stay reserved on Windows whatever the extension.
    std::string upper = name;
    for (auto &c : upper) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    static const std::set<std::string> reserved{"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
        "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    return reserved.count(upper) ? "_" + name : name;
}

// ---- Export run ------------------------------------------------------------

struct Row { std::string key, file; uint32_t width = 0, height = 0; const char *format = ""; uint64_t fingerprint = 0, fingerprintTiled = 0; };
struct Stats
{
    std::vector<Row> rows;
    std::map<std::string, uint64_t> formats, skipped, examples;
    std::vector<std::string> notes;
    uint64_t textureBytes = 0, movies = 0, movieBytes = 0, packages = 0, notPackages = 0;
    void Skipped(const std::string &reason, const std::string &detail)
    {
        ++skipped[reason];
        if (examples[reason]++ < 5) notes.push_back(reason + ": " + detail);
    }
    void Merge(Stats &&other)
    {
        std::move(other.rows.begin(), other.rows.end(), std::back_inserter(rows));
        for (const auto &[k, v] : other.formats) formats[k] += v;
        for (const auto &[k, v] : other.skipped) skipped[k] += v;
        for (auto &note : other.notes)
            if (examples[note.substr(0, note.find(':'))]++ < 5) notes.push_back(std::move(note));
        textureBytes += other.textureBytes; movies += other.movies; movieBytes += other.movieBytes;
        packages += other.packages; notPackages += other.notPackages;
    }
    uint64_t SkippedTotal() const
    {
        uint64_t total = 0;
        for (const auto &[k, v] : skipped) total += v;
        return total;
    }
};

struct Job
{
    enum class Kind { Package, Movie } kind;
    std::string path;     // FPI path, lower case with forward slashes
    fs::path archive;
    uint64_t offset = 0;
    uint32_t size = 0;
    std::string movieName; // output file name for movies
};

void ExportPackage(const Job &job, const fs::path &textures, Stats &stats, bool pngs)
{
    std::vector<uint8_t> bytes;
    try
    {
        bytes = ReadExtent(job.archive, job.offset, job.size);
        if (bytes.size() >= 3 && std::memcmp(bytes.data(), "cpx", 3) == 0)
            Require(cpx::Decode(bytes, bytes), "invalid CPX stream");
    }
    catch (const std::exception &error) { stats.Skipped("package_unreadable", job.path + ": " + error.what()); return; }
    if (bytes.size() < 4 || (uint32_t(bytes[0]) << 24 | uint32_t(bytes[1]) << 16 | uint32_t(bytes[2]) << 8 | bytes[3]) != kPackageMagic)
    {
        ++stats.notPackages;
        return;
    }
    std::unique_ptr<Package> package;
    try { package = std::make_unique<Package>(bytes); }
    catch (const std::exception &error) { stats.Skipped("package_unreadable", job.path + ": " + error.what()); return; }
    ++stats.packages;
    const auto directory = textures / FromUtf8(job.path);
    bool created = false;
    for (size_t index = 0; index < package->exports.size(); ++index)
    {
        const auto &e = package->exports[index];
        const auto kind = Classify(package->ClassName(e));
        if (kind == TextureClass::None || e.name.starts_with("Default__")) continue;
        // Extractor export indices are zero-based, as in settings/menu_assets.cpp.
        const auto key = MakeManifestKey(job.path, uint32_t(index), e.name);
        if (kind == TextureClass::Unsupported)
        {
            stats.Skipped("unsupported_class", (key.empty() ? job.path : key) + " (" + package->ClassName(e) + ")");
            continue;
        }
        if (key.empty()) { stats.Skipped("invalid_key", job.path + "#" + std::to_string(index)); continue; }
        Image image;
        try { image = DecodeTexture(*package, e); }
        catch (const Skip &skip) { stats.Skipped(skip.reason, key + ": " + skip.what()); continue; }
        catch (const std::exception &error) { stats.Skipped("decode_error", key + ": " + error.what()); continue; }
        std::string file;
        if (pngs)
        {
            if (!created) { CreateDirectories(directory); created = true; }
            const auto name = FileStem(e.name) + "." + std::to_string(index) + ".png";
            stats.textureBytes += WritePng(directory / FromUtf8(name), image);
            file = job.path + "/" + name;
        }
        stats.rows.push_back({key, file, image.width, image.height, image.format->name,
                              image.fingerprint, image.fingerprintTiled});
        ++stats.formats[image.format->name];
    }
}

void ExportMovie(const Job &job, const fs::path &movies, Stats &stats)
{
    const auto file = movies / FromUtf8(job.movieName);
    try
    {
        std::ifstream input(job.archive, std::ios::binary);
        Require(input && input.seekg(std::streamoff(job.offset)), "archive unreadable");
        uint8_t head[16]{};
        Require(job.size >= sizeof(head) && input.read(reinterpret_cast<char *>(head), sizeof(head)), "movie too short");
        if (std::memcmp(head, "cpx", 3) == 0)
        {
            // event_kari.wmv is stored CPX-compressed; the others are plain ASF.
            Require(job.size <= cpx::kMaxStoredSize, "CPX movie too large");
            auto bytes = ReadExtent(job.archive, job.offset, job.size);
            Require(cpx::Decode(bytes, bytes), "invalid CPX stream");
            if (bytes.size() < 16 || std::memcmp(bytes.data(), kAsfHeader, 16) != 0) throw Skip("movie_not_asf", job.path);
            WriteFile(file, bytes.data(), bytes.size());
            stats.movieBytes += bytes.size();
        }
        else
        {
            if (std::memcmp(head, kAsfHeader, 16) != 0) throw Skip("movie_not_asf", job.path);
            std::ofstream output(Writable(file), std::ios::binary | std::ios::trunc);
            if (!output || !output.write(reinterpret_cast<const char *>(head), sizeof(head))) throw OutputError("cannot write " + Utf8(file));
            std::vector<char> buffer(4 * 1024 * 1024);
            for (uint64_t left = job.size - sizeof(head); left;)
            {
                const auto chunk = size_t(std::min<uint64_t>(left, buffer.size()));
                if (!input.read(buffer.data(), std::streamsize(chunk)))
                {
                    output.close(); std::error_code ignored; fs::remove(Writable(file), ignored);
                    throw std::runtime_error("archive short read");
                }
                if (!output.write(buffer.data(), std::streamsize(chunk))) throw OutputError("cannot write " + Utf8(file));
                left -= chunk;
            }
            if (!output.flush()) throw OutputError("cannot write " + Utf8(file));
            stats.movieBytes += job.size;
        }
        ++stats.movies;
    }
    catch (const OutputError &) { throw; }
    catch (const Skip &skip) { stats.Skipped(skip.reason, skip.what()); }
    catch (const std::exception &error) { stats.Skipped("movie_unreadable", job.path + ": " + error.what()); }
}

bool Inside(const fs::path &child, const fs::path &parent)
{
    std::error_code error;
    auto c = fs::weakly_canonical(child, error);
    if (error) c = child.lexically_normal();
    auto p = fs::weakly_canonical(parent, error);
    if (error) p = parent.lexically_normal();
    auto ci = c.begin();
    for (auto pi = p.begin(); pi != p.end(); ++pi, ++ci)
    {
        if (pi->empty() && std::next(pi) == p.end()) break; // trailing separator
        if (ci == c.end()) return false;
#ifdef _WIN32
        if (Lower(Utf8(*ci)) != Lower(Utf8(*pi))) return false;
#else
        if (*ci != *pi) return false;
#endif
    }
    return true;
}

void AttachParentConsole()
{
#ifdef _WIN32
    // The runtime is a GUI-subsystem program. Pipes from a parent (QProcess,
    // subprocess) arrive as inherited handles; a plain console launch does not.
    auto usable = [](DWORD id) {
        const HANDLE handle = GetStdHandle(id);
        return handle && handle != INVALID_HANDLE_VALUE && GetFileType(handle) != FILE_TYPE_UNKNOWN;
    };
    if (usable(STD_OUTPUT_HANDLE) || !AttachConsole(ATTACH_PARENT_PROCESS)) return;
    FILE *stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    if (!usable(STD_ERROR_HANDLE)) freopen_s(&stream, "CONOUT$", "w", stderr);
#endif
}

int Fail(const std::string &message)
{
    std::fprintf(stderr, "error: %s\n", message.c_str());
    std::fflush(stderr);
    return 1;
}

std::string Csv(std::string_view value)
{
    if (value.find_first_of(",\"\r\n") == std::string_view::npos) return std::string(value);
    std::string quoted = "\"";
    for (const char c : value) { if (c == '"') quoted += '"'; quoted += c; }
    return quoted + "\"";
}
}

std::optional<Request> ParseArguments(int argc, char **argv)
{
    bool option = false;
    Request parsed;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument = argv[i];
        if (!argument.starts_with("--export-")) continue;
        option = true;
        const bool known = argument == "--export-assets" || argument == "--export-kinds" || argument == "--export-filter";
        if (!known) { parsed.error = "unknown option " + std::string(argument); continue; }
        if (i + 1 >= argc || std::string_view(argv[i + 1]).starts_with("--"))
        {
            parsed.error = std::string(argument) + " needs a value";
            continue;
        }
        const std::string value = argv[++i];
        if (argument == "--export-assets")
        {
            if (value.empty()) parsed.error = "--export-assets needs an output folder";
            parsed.output = FromUtf8(value);
        }
        else if (argument == "--export-filter") parsed.filter = Lower(value);
        else
        {
            parsed.textures = parsed.movies = parsed.pngs = false;
            size_t begin = 0;
            while (begin <= value.size())
            {
                const auto end = std::min(value.find(',', begin), value.size());
                const auto kind = Lower(value.substr(begin, end - begin));
                if (kind == "textures") parsed.textures = parsed.pngs = true;
                else if (kind == "fingerprints") parsed.textures = true; // index.csv without PNG files
                else if (kind == "movies") parsed.movies = true;
                else parsed.error = "unknown export kind '" + kind + "' (expected textures,fingerprints,movies)";
                begin = end + 1;
            }
        }
    }
    if (!option) return std::nullopt;
    if (parsed.output.empty() && parsed.error.empty()) parsed.error = "--export-assets <output folder> is required";
    return parsed;
}

int Run(const Request &request, const fs::path &gameRoot)
{
    AttachParentConsole();
    const auto started = std::chrono::steady_clock::now();
    if (!request.error.empty()) return Fail(request.error);

    // Discs: the boot disc plus disc1..disc4 beside it, ordered by FPI disc number.
    std::error_code error;
    auto boot = gameRoot.empty() ? gameRoot : fs::absolute(gameRoot, error).lexically_normal();
    if (boot.has_relative_path() && boot.filename().empty()) boot = boot.parent_path(); // trailing separator
    if (!fs::is_regular_file(boot / "LO.fpi", error) && fs::is_regular_file(boot / "disc1" / "LO.fpi", error)) boot /= "disc1";
    if (boot.empty() || !fs::is_regular_file(boot / "LO.fpi", error))
        return Fail("no game data: " + Utf8(boot) + " has no LO.fpi (use --game <disc1 folder>)");
    std::vector<Disc> discs;
    std::vector<std::string> discNotes;
    try { discs.push_back(ReadDisc(boot)); }
    catch (const std::exception &failure) { return Fail("cannot read " + Utf8(boot / "LO.fpi") + ": " + failure.what()); }
    for (int number = 1; number <= 4; ++number)
    {
        const auto sibling = boot.parent_path() / ("disc" + std::to_string(number));
        if (!fs::is_regular_file(sibling / "LO.fpi", error) || fs::equivalent(sibling, boot, error)) continue;
        try
        {
            auto disc = ReadDisc(sibling);
            if (std::none_of(discs.begin(), discs.end(), [&](const Disc &d) { return d.number == disc.number; }))
                discs.push_back(std::move(disc));
        }
        catch (const std::exception &failure) { discNotes.push_back(Utf8(sibling) + ": " + failure.what()); }
    }
    std::stable_sort(discs.begin(), discs.end(), [](const Disc &a, const Disc &b) { return a.number < b.number; });

    // Output: a new or empty folder outside the game data.
    auto output = fs::absolute(request.output, error).lexically_normal();
    if (error) return Fail("invalid output folder " + Utf8(request.output));
    for (const auto &disc : discs)
        if (Inside(output, disc.root)) return Fail("output folder " + Utf8(output) + " is inside the game data");
    if (fs::exists(output, error))
    {
        if (!fs::is_directory(output, error)) return Fail("output " + Utf8(output) + " is not a folder");
        if (fs::directory_iterator(output, error) != fs::directory_iterator() || error)
            return Fail("output folder " + Utf8(output) + " is not empty");
    }
    const auto textures = output / "textures", movies = output / "movies";
    try
    {
        CreateDirectories(output);
        if (request.textures) CreateDirectories(textures);
        if (request.movies) CreateDirectories(movies);
    }
    catch (const std::exception &failure) { return Fail(failure.what()); }

    // One job per FPI path; a path repeated on later discs is the same file
    // when its size matches (the four-disc census found no content variants).
    Stats total;
    std::vector<Job> jobs;
    std::map<std::string, uint32_t> seen; // path -> stored size
    std::set<std::string> movieNames;
    for (const auto &disc : discs)
        for (const auto &entry : disc.entries)
        {
            const auto extension = entry.path.substr(std::min(entry.path.rfind('.'), entry.path.size()));
            const bool package = extension == ".xxx" || extension == ".upk" || extension == ".umap" || extension == ".u";
            const bool movie = extension == ".wmv";
            if (!((package && request.textures) || (movie && request.movies))) continue;
            if (!request.filter.empty() && entry.path.find(request.filter) == std::string::npos) continue;
            if (const auto it = seen.find(entry.path); it != seen.end())
            {
                if (it->second != entry.size)
                    total.Skipped(movie ? "movie_variant_on_later_disc" : "package_variant_on_later_disc",
                                  "disc" + std::to_string(disc.number) + ":" + entry.path);
                continue;
            }
            seen.emplace(entry.path, entry.size);
            const auto &archive = disc.archives[entry.archive];
            if (archive.empty() || entry.offset > disc.archiveSizes[entry.archive] ||
                entry.size > disc.archiveSizes[entry.archive] - entry.offset || !SafeRelative(entry.path))
            {
                total.Skipped(movie ? "movie_unreadable" : "package_unreadable",
                              entry.path + ": archive missing, extent out of range or unsafe path");
                continue;
            }
            Job job{movie ? Job::Kind::Movie : Job::Kind::Package, entry.path, archive, entry.offset, entry.size, {}};
            if (movie)
            {
                // Keep the original file name; disambiguate equal names from different folders.
                auto name = entry.path.substr(entry.path.rfind('/') + 1);
                if (movieNames.count(Lower(name)))
                {
                    const auto folder = entry.path.substr(0, entry.path.rfind('/'));
                    name = folder.substr(folder.rfind('/') + 1) + "_" + name;
                    for (int n = 2; movieNames.count(Lower(name)); ++n) name = std::to_string(n) + "_" + name;
                }
                movieNames.insert(Lower(name));
                job.movieName = name;
            }
            jobs.push_back(std::move(job));
        }
    // Largest first keeps the four workers busy until the end.
    std::stable_sort(jobs.begin(), jobs.end(), [](const Job &a, const Job &b) { return a.size > b.size; });

    std::mutex mutex;
    std::atomic<size_t> next{0};
    std::atomic<bool> abort{false};
    size_t done = 0;
    std::string fatal;
    auto lastProgress = std::chrono::steady_clock::now();
    std::printf("progress 0/%zu\n", jobs.size());
    std::fflush(stdout);
    auto worker = [&] {
        while (!abort)
        {
            const size_t index = next++;
            if (index >= jobs.size()) return;
            Stats local;
            try
            {
                if (jobs[index].kind == Job::Kind::Package) ExportPackage(jobs[index], textures, local, request.pngs);
                else ExportMovie(jobs[index], movies, local);
            }
            catch (const std::exception &failure)
            {
                std::lock_guard lock(mutex);
                if (fatal.empty()) fatal = failure.what();
                abort = true;
                return;
            }
            std::lock_guard lock(mutex);
            total.Merge(std::move(local));
            ++done;
            const auto now = std::chrono::steady_clock::now();
            if (done == jobs.size() || now - lastProgress >= std::chrono::seconds(1))
            {
                lastProgress = now;
                std::printf("progress %zu/%zu\n", done, jobs.size());
                std::fflush(stdout);
            }
        }
    };
    const unsigned threads = std::max(1u, std::min(kMaxThreads, std::thread::hardware_concurrency()));
    std::vector<std::thread> pool;
    for (unsigned i = 0; i < threads; ++i) pool.emplace_back(worker);
    for (auto &thread : pool) thread.join();
    if (!fatal.empty()) return Fail(fatal);

    try
    {
        if (request.textures)
        {
            std::sort(total.rows.begin(), total.rows.end(), [](const Row &a, const Row &b) { return a.key < b.key; });
            std::string csv = "key,file,width,height,format,fingerprint,fingerprint_tiled\n";
            char hex[40];
            for (const auto &row : total.rows)
            {
                std::snprintf(hex, sizeof(hex), "%016llx,%016llx", static_cast<unsigned long long>(row.fingerprint),
                              static_cast<unsigned long long>(row.fingerprintTiled));
                csv += Csv(row.key) + "," + Csv(row.file) + "," + std::to_string(row.width) + "," +
                       std::to_string(row.height) + "," + row.format + "," + hex + "\n";
            }
            WriteFile(textures / "index.csv", csv.data(), csv.size());
        }
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        std::string summary = "Lost Odyssey asset export\n";
        summary += "game: " + Utf8(boot) + "\ndiscs:";
        for (const auto &disc : discs) summary += " " + std::to_string(disc.number) + " (" + Utf8(disc.root) + ")";
        summary += std::string("\nkinds:") + (request.textures ? " textures" : "") + (request.movies ? " movies" : "");
        summary += "\nfilter: " + (request.filter.empty() ? std::string("(none)") : request.filter);
        char line[256];
        std::snprintf(line, sizeof(line), "\nelapsed_seconds: %.1f\nthreads: %u\n", elapsed, threads);
        summary += line;
        summary += "\ntextures: " + std::to_string(total.rows.size()) + " (" + std::to_string(total.textureBytes) + " PNG bytes)\n";
        for (const auto &format : kFormats)
            summary += "  " + std::string(format.name) + ": " + std::to_string(total.formats[format.name]) + "\n";
        summary += "movies: " + std::to_string(total.movies) + " (" + std::to_string(total.movieBytes) + " bytes)\n";
        summary += "packages read: " + std::to_string(total.packages) + " (" + std::to_string(total.notPackages) +
                   " package-named files were not UE3 packages)\n";
        summary += "\nskipped: " + std::to_string(total.SkippedTotal()) + "\n";
        for (const auto &[reason, count] : total.skipped) summary += "  " + reason + ": " + std::to_string(count) + "\n";
        if (!total.notes.empty() || !discNotes.empty())
        {
            summary += "\nexamples (up to 5 per reason):\n";
            for (const auto &note : discNotes) summary += "  disc_unreadable: " + note + "\n";
            for (const auto &note : total.notes) summary += "  " + note + "\n";
        }
        WriteFile(output / "export-summary.txt", summary.data(), summary.size());
    }
    catch (const std::exception &failure) { return Fail(failure.what()); }

    std::printf("exported %zu textures, %llu movies, skipped %llu\n", total.rows.size(),
                static_cast<unsigned long long>(total.movies), static_cast<unsigned long long>(total.SkippedTotal()));
    std::fflush(stdout);
    return 0;
}
}
