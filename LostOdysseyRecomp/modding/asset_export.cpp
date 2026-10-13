#include "asset_export.h"
#include "fpi_index.h"
#include "gltf_writer.h"
#include "mod_api.h"
#include "text_format.h"
#include "ue3_package.h"
#include "xenos_texture.h"
#include <gpu/shader/cpx_decode.h>
#include <settings/language_selection.h>
#include <settings/quit_text_hook.h>
#include <gpu/texture_layout.h>
#include <lzokay.hpp>
#include <xxhash.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
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

using ue3::kPackageMagic;
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

// index: LO.fpi on a disc, or a DLC's LODLC00x.fpi, whose one archive is the
// .fpi file itself (index and data in one file).
Disc ReadDisc(const fs::path &root, const fs::path &index = "LO.fpi")
{
    Disc disc; disc.root = root;
    const auto fpi = root / index;
    std::error_code error;
    const auto size = fs::file_size(fpi, error);
    Require(!error && size >= 64, "FPI index missing or invalid");
    std::vector<uint8_t> bytes(64, 0);
    std::ifstream input(fpi, std::ios::binary);
    Require(input && input.read(reinterpret_cast<char *>(bytes.data()), 64), "FPI index unreadable");
    // The index is the first `used` sectors; a DLC's file goes on with its data.
    const auto used = fpi::IndexSize(bytes);
    Require(used <= size, "FPI index missing or invalid");
    bytes.resize(size_t(used));
    Require(bool(input.read(reinterpret_cast<char *>(bytes.data()) + 64, std::streamsize(used - 64))), "FPI index unreadable");
    const auto parsed = fpi::Read(bytes);
    disc.number = parsed.disc;
    // Archive names are lower case; match files case-insensitively for case-sensitive file systems.
    std::map<std::string, fs::path> files;
    for (const auto &item : fs::directory_iterator(root, error))
        files.emplace(Lower(Utf8(item.path().filename())), item.path());
    for (const auto &name : parsed.archives)
    {
        const auto it = files.find(name);
        uint64_t archiveSize = 0;
        if (it != files.end()) archiveSize = fs::file_size(it->second, error);
        const bool present = it != files.end() && !error;
        disc.archives.push_back(present ? it->second : fs::path{});
        disc.archiveSizes.push_back(present ? archiveSize : 0);
    }
    for (const auto &file : parsed.files) disc.entries.push_back({file.path, file.archive, file.offset, file.size});
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

// ---- UE3 package -----------------------------------------------------------

using ue3::Reader;
using ue3::Export;
using ue3::Package;
using ue3::ObjectReader;
using ue3::Count;
using ue3::SkipArray;
using ue3::Properties;

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

// decode false (--export-list) checks the same headers and leaves raw empty.
TopMip ReadTopMip(const Package &package, const Export &e, bool decode = true)
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
    Require(r.U32() == uint32_t(width) && r.U32() == uint32_t(height), "top mip size differs from the texture size");
    std::vector<uint8_t> raw;
    if (!decode) Require((top.flags & 0x10) || top.size == top.count, "uncompressed mip size mismatch");
    else if (top.flags & 0x10) raw = DecompressLzo(payload, top.count);
    else { Require(top.size == top.count, "uncompressed mip size mismatch"); raw.assign(payload.begin(), payload.end()); }
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

std::vector<uint8_t> EncodePng(const Image &image)
{
    size_t length = 0;
    void *png = tdefl_write_image_to_png_file_in_memory_ex(image.pixels.data(), int(image.width), int(image.height),
                                                           int(image.channels), &length, kPngLevel, MZ_FALSE);
    if (!png) throw OutputError("PNG encoding failed");
    std::unique_ptr<void, decltype(&mz_free)> owner(png, &mz_free);
    const auto *bytes = static_cast<const uint8_t *>(png);
    return {bytes, bytes + length};
}

size_t WritePng(const fs::path &file, const Image &image)
{
    const auto png = EncodePng(image);
    WriteFile(file, png.data(), png.size());
    return png.size();
}

using ue3::FileStem;

// ---- Models and animations -------------------------------------------------
// LOD 0 of SkeletalMesh, StaticMesh and the game's StaticMeshRPG at package
// version 419, and the AnimSets that move the skeletal ones. Layouts follow UE
// Viewer (umodel, MIT) for this version; StaticMeshRPG serializes as StaticMesh,
// and this game's static vertices are 40 bytes: position, 4 bytes of padding,
// three packed normals, an ARGB colour, then the UV sets. glTF space from
// Unreal's (umodel ExportGLTF): (x, y, z) -> (x, z, y), centimetres to metres;
// reference pose rotations (x, y, z, w) -> (x, z, y, -w), animation keys
// -> (x, z, y, w) except on the root bone, which takes -w as well.

enum class MeshClass { None, Skeletal, Static };
MeshClass ClassifyMesh(const std::string &name)
{
    if (name == "SkeletalMesh") return MeshClass::Skeletal;
    return name == "StaticMesh" || name == "StaticMeshRPG" ? MeshClass::Static : MeshClass::None;
}

constexpr float kMetres = 0.01f;
using Vec3 = std::array<float, 3>;
Vec3 ReadVec(Reader &r) { const float x = r.F32(), y = r.F32(), z = r.F32(); return {x, y, z}; }
Vec3 Position(const Vec3 &v) { return {v[0] * kMetres, v[2] * kMetres, v[1] * kMetres}; }
// FPackedNormal: x in the low byte.
Vec3 Normal(uint32_t packed)
{
    auto axis = [&](int shift) { return float(packed >> shift & 0xff) / 127.5f - 1; };
    return {axis(0), axis(16), axis(8)};
}

struct Model
{
    gltf::Mesh mesh;
    std::vector<int32_t> materials; // object reference of each mesh.materials entry
    uint32_t vertices = 0, triangles = 0;
};

void CheckGeometry(const gltf::Mesh &mesh)
{
    Require(!mesh.positions.empty() && !mesh.indices.empty(), "empty mesh");
    for (const auto index : mesh.indices) Require(index < mesh.positions.size(), "index outside the vertices");
    for (const auto &section : mesh.sections)
        Require(section.material < mesh.materials.size() && section.firstIndex <= mesh.indices.size() &&
                section.indexCount <= mesh.indices.size() - section.firstIndex, "section outside the indices");
}

Model ReadSkeletalMesh(const Package &package, const Export &e)
{
    auto r = ObjectReader(package, e);
    Properties(package, r, true, [](auto &&...) {});
    Model model;
    auto &mesh = model.mesh;
    r.Take(28); // Bounds.
    for (uint32_t i = 0, n = Count(r, 4); i < n; ++i)
    {
        model.materials.push_back(r.I32());
        mesh.materials.push_back({package.RefName(model.materials.back())});
    }
    r.Take(24); // Origin, RotOrigin.
    for (uint32_t i = 0, n = Count(r, 48); i < n; ++i)
    {
        gltf::Bone bone;
        bone.name = package.Name(r);
        r.U32(); // Flags.
        const float x = r.F32(), y = r.F32(), z = r.F32(), w = r.F32();
        bone.rotation = {x, z, y, -w};
        bone.translation = Position(ReadVec(r));
        r.I32(); // Children.
        const auto parent = r.I32();
        bone.parent = i ? parent : -1;
        Require(bone.parent < int(i), "bone parent after the bone");
        mesh.bones.push_back(std::move(bone));
    }
    Require(!mesh.bones.empty(), "no skeleton");
    r.I32(); // SkeletalDepth.
    Require(Count(r, 4) >= 1, "no LOD");
    struct SourceSection { int16_t material; uint32_t first, triangles; };
    std::vector<SourceSection> sections(Count(r, 10));
    for (auto &s : sections)
    {
        s.material = int16_t(r.U16()); r.U16(); // ChunkIndex.
        s.first = r.U32(); s.triangles = r.U16();
    }
    for (uint32_t i = 0, n = Count(r, 2); i < n; ++i) mesh.indices.push_back(r.U16());
    SkipArray(r, 2); SkipArray(r, 2); SkipArray(r, 1); // f68, ActiveBoneIndices, f74.
    struct Chunk { uint32_t first, count; std::vector<uint16_t> bones; };
    std::vector<Chunk> chunks(Count(r, 28));
    for (auto &c : chunks)
    {
        c.first = r.U32();
        SkipArray(r, 33); SkipArray(r, 40); // CPU rigid and soft vertices; the GPU copy below has all.
        for (uint32_t i = 0, n = Count(r, 2); i < n; ++i) c.bones.push_back(r.U16());
        c.count = r.U32(); c.count += r.U32(); r.U32(); // Rigid + soft vertices, MaxInfluences.
    }
    r.U32();
    const auto vertices = r.U32();
    SkipArray(r, 16); SkipArray(r, 1); // Edges, RequiredBones.
    const auto flags = r.U32(); r.U32(); const auto stored = r.U32(); r.U32();
    if (!(flags & 0x21)) r.Take(stored);
    Require(Count(r, 40) == vertices && vertices, "GPU vertices missing");
    mesh.positions.resize(vertices); mesh.normals.resize(vertices);
    mesh.uvs.assign(1, std::vector<std::array<float, 2>>(vertices));
    mesh.joints.resize(vertices); mesh.weights.resize(vertices);
    size_t chunk = 0;
    for (uint32_t v = 0; v < vertices; ++v)
    {
        mesh.positions[v] = Position(ReadVec(r));
        r.U32(); r.U32(); mesh.normals[v] = Normal(r.U32()); // Tangent, binormal, normal.
        mesh.uvs[0][v] = {r.F32(), r.F32()};
        const auto bones = r.Take(4), weights = r.Take(4);
        while (chunk < chunks.size() && v >= chunks[chunk].first + chunks[chunk].count) ++chunk;
        Require(chunk < chunks.size() && v >= chunks[chunk].first, "vertex outside the chunks");
        const auto &map = chunks[chunk].bones;
        float total = 0;
        for (int k = 0; k < 4; ++k) total += weights[k];
        for (int k = 0; k < 4; ++k)
        {
            Require(!weights[k] || (bones[k] < map.size() && map[bones[k]] < mesh.bones.size()), "influence outside the skeleton");
            mesh.joints[v][k] = weights[k] ? map[bones[k]] : 0;
            mesh.weights[v][k] = total > 0 ? weights[k] / total : float(k == 0);
        }
    }
    for (const auto &s : sections)
        mesh.sections.push_back({uint32_t(s.material), s.first, s.triangles * 3});
    CheckGeometry(mesh);
    model.vertices = vertices;
    model.triangles = uint32_t(mesh.indices.size() / 3);
    return model;
}

Model ReadStaticMesh(const Package &package, const Export &e)
{
    auto r = ObjectReader(package, e);
    Properties(package, r, true, [](auto &&...) {});
    r.Take(28); r.I32(); // Bounds, BodySetup.
    SkipArray(r, 36); SkipArray(r, 16); // kDOP nodes and triangles.
    if (r.I32() >= 17) SkipArray(r, 8); // InternalVersion; content tags.
    Require(Count(r, 4) >= 1, "no LOD");
    const auto flags = r.U32(); r.U32(); const auto stored = r.U32(); r.U32();
    if (!(flags & 0x21)) r.Take(stored);
    Model model;
    auto &mesh = model.mesh;
    for (uint32_t i = 0, n = Count(r, 28); i < n; ++i)
    {
        const auto material = r.I32();
        r.I32(); r.I32();
        const auto first = r.U32(), faces = r.U32();
        r.I32(); r.I32();
        auto slot = std::find(model.materials.begin(), model.materials.end(), material);
        if (slot == model.materials.end())
        {
            model.materials.push_back(material);
            mesh.materials.push_back({package.RefName(material)});
            slot = model.materials.end() - 1;
        }
        mesh.sections.push_back({uint32_t(slot - model.materials.begin()), first, faces * 3});
    }
    const auto sets = r.U32(), itemSize = r.U32(), vertices = r.U32();
    Require(sets >= 1 && sets <= 8 && itemSize == 32 + 8 * sets, "unexpected static vertex layout");
    Require(Count(r, itemSize) == vertices && vertices, "vertex count mismatch");
    mesh.positions.resize(vertices); mesh.normals.resize(vertices); mesh.colors.resize(vertices);
    mesh.uvs.assign(sets, std::vector<std::array<float, 2>>(vertices));
    bool coloured = false;
    for (uint32_t v = 0; v < vertices; ++v)
    {
        mesh.positions[v] = Position(ReadVec(r));
        r.U32(); r.U32(); r.U32(); // Padding, tangent, binormal.
        mesh.normals[v] = Normal(r.U32());
        const auto argb = r.U32();
        mesh.colors[v] = {uint8_t(argb >> 16), uint8_t(argb >> 8), uint8_t(argb), uint8_t(argb >> 24)};
        coloured |= argb != 0xffffffffu;
        for (auto &set : mesh.uvs) set[v] = {r.F32(), r.F32()};
    }
    if (!coloured) mesh.colors.clear();
    Require(r.U32() == vertices, "vertex count mismatch");
    for (uint32_t i = 0, n = Count(r, 2); i < n; ++i) mesh.indices.push_back(r.U16());
    CheckGeometry(mesh);
    model.vertices = vertices;
    model.triangles = uint32_t(mesh.indices.size() / 3);
    return model;
}

// Base colour textures of one package, each written once beside its models.
struct TextureCache
{
    struct Entry { std::string file; bool mask = false, tried = false; };
    std::map<int32_t, Entry> entries;
    uint64_t bytes = 0;
};

// The texture a MaterialInstanceConstant of this package draws its colour from:
// of the textures it lists (rpg_TexturePtrs, else TextureParameterValues; else
// its parent's), the one named *_C, else the first not named as a normal (_N),
// specular (_S) or other mask map.
int32_t BaseColorTexture(const Package &package, int32_t material, int depth = 0)
{
    const auto *e = package.ExportAt(material);
    if (!e || depth > 4 || !package.ClassName(*e).starts_with("MaterialInstance")) return 0;
    std::vector<int32_t> pointers, parameters;
    int32_t parent = 0;
    auto r = ObjectReader(package, *e);
    try
    {
        Properties(package, r, true, [&](const std::string &name, const std::string &type, Reader &value, uint32_t) {
            if (name == "Parent" && type == "ObjectProperty") parent = value.I32();
            else if (name == "rpg_TexturePtrs" && type == "ArrayProperty")
                for (uint32_t i = 0, n = Count(value, 4); i < n; ++i) pointers.push_back(value.I32());
            else if (name == "TextureParameterValues" && type == "ArrayProperty")
                for (uint32_t i = 0, n = Count(value, 8); i < n; ++i)
                    Properties(package, value, false, [&](const std::string &field, const std::string &kind, Reader &v, uint32_t) {
                        if (field == "ParameterValue" && kind == "ObjectProperty") parameters.push_back(v.I32());
                    });
        });
    }
    catch (const std::exception &) { return 0; }
    auto &list = pointers.empty() ? parameters : pointers;
    std::erase_if(list, [&](int32_t ref) { const auto *t = package.ExportAt(ref); return !t || package.ClassName(*t) != "Texture2D"; });
    if (list.empty()) return BaseColorTexture(package, parent, depth + 1);
    auto suffix = [&](int32_t ref) {
        const auto name = Lower(package.RefName(ref));
        return name.substr(std::min(name.rfind('_'), name.size()));
    };
    for (const auto ref : list) if (suffix(ref) == "_c") return ref;
    for (const auto ref : list)
        if (const auto s = suffix(ref); s != "_n" && s != "_s" && s != "_h" && s != "_t" && s != "_tm" && s != "_m") return ref;
    return 0;
}

// directory: the package's model folder; textures go to its textures/ folder.
void AttachTextures(const Package &package, Model &model, TextureCache &cache, const fs::path &directory)
{
    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        const auto ref = BaseColorTexture(package, model.materials[i]);
        if (!ref) continue;
        auto &entry = cache.entries[ref];
        if (!entry.tried)
        {
            entry.tried = true;
            try
            {
                const auto &texture = *package.ExportAt(ref);
                const auto image = DecodeTexture(package, texture);
                const auto name = FileStem(texture.name) + "." + std::to_string(ref - 1) + ".png";
                CreateDirectories(directory / "textures");
                cache.bytes += WritePng(directory / "textures" / FromUtf8(name), image);
                entry.file = "textures/" + name;
                // DXT1 alpha is a cut-out; other formats may keep masks in alpha.
                entry.mask = image.channels == 4 && std::string_view(image.format->name) == "DXT1";
            }
            catch (const OutputError &) { throw; }
            catch (const std::exception &) {} // Left untextured.
        }
        if (entry.file.empty()) continue;
        auto &images = model.mesh.images;
        const auto image = std::find(images.begin(), images.end(), entry.file);
        model.mesh.materials[i].baseColor = int(image - images.begin());
        model.mesh.materials[i].mask = entry.mask;
        if (image == images.end()) images.push_back(entry.file);
    }
}

// An AnimSet: track bone names and its sequences, keyed for glTF.
struct AnimSetData
{
    std::vector<std::string> tracks;
    bool rotationOnly = false;
    struct Track { std::vector<Vec3> positions; std::vector<std::array<float, 4>> rotations; };
    struct Sequence { std::string name; float length = 0; uint32_t frames = 0; std::vector<Track> tracks; };
    std::vector<Sequence> sequences;
};

AnimSetData::Sequence ReadSequence(const Package &package, const Export &e, size_t trackCount)
{
    AnimSetData::Sequence sequence;
    sequence.name = e.name;
    int translationFormat = 0, rotationFormat = 0;
    std::vector<uint32_t> offsets;
    auto r = ObjectReader(package, e);
    Properties(package, r, true, [&](const std::string &name, const std::string &type, Reader &value, uint32_t) {
        if (name == "SequenceName" && type == "NameProperty") sequence.name = package.Name(value);
        else if (name == "SequenceLength" && type == "FloatProperty") sequence.length = value.F32();
        else if (name == "NumFrames" && type == "IntProperty") sequence.frames = value.U32();
        else if (name == "TranslationCompressionFormat" && type == "ByteProperty") translationFormat = value.Byte();
        else if (name == "RotationCompressionFormat" && type == "ByteProperty") rotationFormat = value.Byte();
        else if (name == "CompressedTrackOffsets" && type == "ArrayProperty")
            for (uint32_t i = 0, n = Count(value, 4); i < n; ++i) offsets.push_back(value.U32());
    });
    // ACF_None or ACF_Float96NoW translations; ACF_None, ACF_Float96NoW or
    // ACF_Fixed48NoW rotations: what this game's sequences use.
    if (translationFormat > 1 || rotationFormat > 2)
        throw Skip("unsupported_animation", e.name + ": formats " + std::to_string(translationFormat) + "/" + std::to_string(rotationFormat));
    Require(offsets.size() == trackCount * 4, "track offsets do not match the AnimSet");
    const auto size = Count(r, 1);
    Reader stream{r.Take(size)};
    sequence.tracks.resize(trackCount);
    for (size_t t = 0; t < trackCount; ++t)
    {
        auto &track = sequence.tracks[t];
        const auto *o = &offsets[t * 4];
        Require(o[1] <= 65536 && o[3] <= 65536, "too many keys");
        stream.pos = o[0];
        for (uint32_t k = 0; k < o[1]; ++k) track.positions.push_back(ReadVec(stream));
        stream.pos = o[2];
        // Several keys carry a (here unused) range before them.
        const int format = o[3] == 1 ? 1 : rotationFormat;
        if (o[3] > 1) stream.Take(24);
        for (uint32_t k = 0; k < o[3]; ++k)
        {
            float x, y, z, w;
            if (format == 0) { x = stream.F32(); y = stream.F32(); z = stream.F32(); w = stream.F32(); }
            else
            {
                if (format == 1) { x = stream.F32(); y = stream.F32(); z = stream.F32(); }
                else
                {
                    x = (float(stream.U16()) - 32767) / 32767; y = (float(stream.U16()) - 32767) / 32767;
                    z = (float(stream.U16()) - 32767) / 32767;
                }
                w = std::sqrt(std::max(0.0f, 1 - x * x - y * y - z * z));
            }
            track.rotations.push_back({x, y, z, w});
        }
    }
    return sequence;
}

AnimSetData ReadAnimSet(const Package &package, const Export &e)
{
    AnimSetData set;
    std::vector<int32_t> sequences;
    auto r = ObjectReader(package, e);
    Properties(package, r, true, [&](const std::string &name, const std::string &type, Reader &value, uint32_t boolean) {
        if (name == "TrackBoneNames" && type == "ArrayProperty")
            for (uint32_t i = 0, n = Count(value, 8); i < n; ++i) set.tracks.push_back(package.Name(value));
        else if (name == "Sequences" && type == "ArrayProperty")
            for (uint32_t i = 0, n = Count(value, 4); i < n; ++i) sequences.push_back(value.I32());
        else if (name == "bAnimRotationOnly") set.rotationOnly = boolean != 0;
    });
    if (set.tracks.empty()) throw Skip("animation_empty", e.name + ": no tracks");
    for (const auto ref : sequences)
        if (const auto *s = package.ExportAt(ref); s && package.ClassName(*s) == "AnimSequence")
            set.sequences.push_back(ReadSequence(package, *s, set.tracks.size()));
    return set;
}

// glTF animations of `set` on `mesh`'s skeleton, matched by bone name. Keys are
// evenly spaced over the sequence (umodel CAnimTrack). Returns the tracks matched.
size_t Animate(gltf::Mesh &mesh, const AnimSetData &set)
{
    std::map<std::string, uint32_t> bones;
    for (uint32_t i = 0; i < mesh.bones.size(); ++i) bones.emplace(Lower(mesh.bones[i].name), i);
    // A few sets list a bone twice; the first track drives it.
    std::vector<int64_t> target(set.tracks.size(), -1);
    std::vector<bool> driven(mesh.bones.size());
    size_t matched = 0;
    for (size_t t = 0; t < set.tracks.size(); ++t)
        if (const auto it = bones.find(Lower(set.tracks[t])); it != bones.end() && !driven[it->second])
        {
            target[t] = it->second;
            driven[it->second] = true;
            ++matched;
        }
    for (const auto &sequence : set.sequences)
    {
        gltf::Animation animation{sequence.name, {}};
        for (size_t t = 0; t < sequence.tracks.size(); ++t)
        {
            if (target[t] < 0) continue;
            const auto bone = uint32_t(target[t]);
            const auto &track = sequence.tracks[t];
            auto times = [&](size_t keys) {
                const float length = sequence.length > 0 ? sequence.length : float(keys) / 30; // glTF needs rising times
                std::vector<float> out(keys);
                for (size_t k = 0; k < keys; ++k) out[k] = length * float(k) / float(keys);
                return out;
            };
            if (!track.positions.empty() && (!set.rotationOnly || bone == 0))
            {
                gltf::Channel channel{bone, false, times(track.positions.size()), {}};
                for (const auto &p : track.positions) for (const auto value : Position(p)) channel.values.push_back(value);
                animation.channels.push_back(std::move(channel));
            }
            if (!track.rotations.empty())
            {
                gltf::Channel channel{bone, true, times(track.rotations.size()), {}};
                for (const auto &q : track.rotations)
                    channel.values.insert(channel.values.end(), {q[0], q[2], q[1], bone == 0 ? -q[3] : q[3]});
                animation.channels.push_back(std::move(channel));
            }
        }
        mesh.animations.push_back(std::move(animation));
    }
    return matched;
}

// ---- Export run ------------------------------------------------------------

struct Row { std::string key, file; uint32_t width = 0, height = 0; const char *format = ""; uint64_t fingerprint = 0, fingerprintTiled = 0; };
struct TextRow { std::string path, language, source; text::Format format = text::Format::None; size_t entries = 0; bool roundTrip = false; };
struct MovieRow { std::string name, path; uint64_t bytes = 0; };
// Models: vertices, triangles, bones. Animation sets: sequences, tracks, frames
// and the model they were written with.
struct ModelRow { bool animation = false; std::string key, file, className, model; uint32_t a = 0, b = 0, c = 0; };
struct Stats
{
    std::vector<Row> rows;
    std::vector<TextRow> texts;
    std::vector<MovieRow> movieRows;
    std::vector<ModelRow> models;
    std::map<std::string, uint64_t> formats, skipped, examples;
    std::vector<std::string> notes;
    uint64_t textureBytes = 0, movies = 0, movieBytes = 0, modelBytes = 0, packages = 0, notPackages = 0;
    void Skipped(const std::string &reason, const std::string &detail)
    {
        ++skipped[reason];
        if (examples[reason]++ < 5) notes.push_back(reason + ": " + detail);
    }
    void Merge(Stats &&other)
    {
        std::move(other.rows.begin(), other.rows.end(), std::back_inserter(rows));
        std::move(other.texts.begin(), other.texts.end(), std::back_inserter(texts));
        std::move(other.movieRows.begin(), other.movieRows.end(), std::back_inserter(movieRows));
        std::move(other.models.begin(), other.models.end(), std::back_inserter(models));
        for (const auto &[k, v] : other.formats) formats[k] += v;
        for (const auto &[k, v] : other.skipped) skipped[k] += v;
        for (auto &note : other.notes)
            if (examples[note.substr(0, note.find(':'))]++ < 5) notes.push_back(std::move(note));
        textureBytes += other.textureBytes; movies += other.movies; movieBytes += other.movieBytes; modelBytes += other.modelBytes;
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
    enum class Kind { Package, Movie, Text } kind;
    std::string path;     // FPI path, lower case with forward slashes
    fs::path archive;
    uint64_t offset = 0;
    uint32_t size = 0;
    std::string movieName; // output file name for movies
    text::Format textFormat = text::Format::None;
    std::string source = "base"; // text: the disc set, or the DLC index that overrides the file
};

void AppendJsonString(std::string &out, std::string_view value)
{
    out += '"';
    for (const char c : value)
    {
        switch (c)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (uint8_t(c) < 0x20)
            {
                char escape[8];
                std::snprintf(escape, sizeof(escape), "\\u%04x", unsigned(uint8_t(c)));
                out += escape;
            }
            else out += c;
        }
    }
    out += '"';
}

// text/<member path>.json: {"key": "text", ...} in the file's own order. A
// language pack carries the same files with translated values. referenceRoot,
// when set, gets a second copy to compare a translated pack with.
void ExportText(const Job &job, const fs::path &textRoot, const fs::path &referenceRoot, Stats &stats)
{
    std::vector<uint8_t> bytes;
    try
    {
        bytes = ReadExtent(job.archive, job.offset, job.size);
        if (bytes.size() >= 3 && std::memcmp(bytes.data(), "cpx", 3) == 0)
            Require(cpx::Decode(bytes, bytes), "invalid CPX stream");
    }
    catch (const std::exception &error) { stats.Skipped("text_unreadable", job.path + ": " + error.what()); return; }
    std::vector<text::Entry> entries;
    bool roundTrip = false;
    try
    {
        entries = text::Parse(job.textFormat, bytes);
        // Rebuilding with nothing and with every original string must give the
        // file back unchanged, or an import of this file could not be trusted.
        text::Replacements same;
        for (const auto &entry : entries) same.emplace(entry.key, entry.text);
        if (same.size() != entries.size()) throw std::runtime_error("duplicate keys");
        roundTrip = text::Rebuild(job.textFormat, bytes, {}) == bytes && text::Rebuild(job.textFormat, bytes, same) == bytes;
    }
    catch (const std::exception &error) { stats.Skipped("text_unreadable", job.path + ": " + error.what()); return; }
    // Still exported (the text is right), but listed: importing it is not safe.
    if (!roundTrip && stats.examples["text_round_trip_failed"]++ < 5) stats.notes.push_back("text_round_trip_failed: " + job.path);
    // The game shows the port's Quit to Desktop texts for these IDs
    // (settings/quit_text_hook.h); export those, so a translation fits the row.
    if (job.textFormat == text::Format::MenuDat)
    {
        const auto language = settings::language::IdFromCode(text::Language(job.path));
        for (auto &entry : entries)
            for (const auto id : settings::quit_text::TextIds)
                if (language && entry.key == "id." + std::to_string(id))
                    entry.text = text::EncodeUnits(settings::quit_text::Text(id, language));
    }
    std::string json = "{";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        json += i ? ",\n  " : "\n  ";
        AppendJsonString(json, entries[i].key);
        json += ": ";
        AppendJsonString(json, entries[i].text);
    }
    json += entries.empty() ? "}\n" : "\n}\n";
    for (const auto &root : {textRoot, referenceRoot})
    {
        if (root.empty()) continue;
        const auto file = root / FromUtf8(job.path + ".json");
        CreateDirectories(file.parent_path());
        WriteFile(file, json.data(), json.size());
    }
    stats.texts.push_back({job.path, text::Language(job.path), job.source, job.textFormat, entries.size(), roundTrip});
}

std::vector<uint8_t> ReadPackageBytes(const Job &job)
{
    auto bytes = ReadExtent(job.archive, job.offset, job.size);
    if (bytes.size() >= 3 && std::memcmp(bytes.data(), "cpx", 3) == 0) Require(cpx::Decode(bytes, bytes), "invalid CPX stream");
    return bytes;
}
bool IsPackage(const std::vector<uint8_t> &bytes)
{
    return bytes.size() >= 4 && (uint32_t(bytes[0]) << 24 | uint32_t(bytes[1]) << 16 | uint32_t(bytes[2]) << 8 | bytes[3]) == kPackageMagic;
}

// A SkeletalMesh an AnimSet can be written with: its package, export index and
// lower-case bone names.
struct Skeleton { Job job; uint32_t index = 0; std::string root; std::set<std::string> bones; };
struct Context
{
    const Request &request;
    fs::path textures, models, animations;
    const std::set<std::string> *select = nullptr; // list.csv names, or null for everything
    const std::vector<Skeleton> *skeletons = nullptr;
};

// The model an AnimSet moves: the one whose root bone is the set's first track,
// else the one sharing at least 90% of its track names (some sets call the root
// bs_001a_Root for the model's bs_001a0_Root), the longest common root prefix
// breaking ties.
const Skeleton *FindSkeleton(const std::vector<Skeleton> &skeletons, const AnimSetData &set)
{
    const auto root = Lower(set.tracks[0]);
    const Skeleton *best = nullptr;
    size_t bestShared = 0, bestPrefix = 0;
    for (const auto &skeleton : skeletons)
    {
        if (skeleton.root == root) return &skeleton;
        size_t shared = 0, prefix = 0;
        for (const auto &track : set.tracks) shared += skeleton.bones.count(Lower(track));
        while (prefix < root.size() && prefix < skeleton.root.size() && root[prefix] == skeleton.root[prefix]) ++prefix;
        if (shared > bestShared || (shared == bestShared && prefix > bestPrefix)) { best = &skeleton; bestShared = shared; bestPrefix = prefix; }
    }
    return bestShared * 10 >= set.tracks.size() * 9 ? best : nullptr;
}

void ExportModel(const Job &job, const Package &package, size_t index, const std::string &key, MeshClass kind,
                 const Context &context, TextureCache &cache, Stats &stats)
{
    const auto &e = package.exports[index];
    Model model;
    try { model = kind == MeshClass::Skeletal ? ReadSkeletalMesh(package, e) : ReadStaticMesh(package, e); }
    catch (const std::exception &error) { stats.Skipped("model_unreadable", key + ": " + error.what()); return; }
    std::string file = job.path;
    if (!context.request.list)
    {
        const auto name = FileStem(e.name) + "." + std::to_string(index) + ".glb";
        const auto directory = context.models / FromUtf8(job.path);
        CreateDirectories(directory);
        if (context.request.modelTextures) AttachTextures(package, model, cache, directory);
        const auto glb = gltf::Write(model.mesh, e.name);
        WriteFile(directory / FromUtf8(name), glb.data(), glb.size());
        stats.modelBytes += glb.size();
        file = job.path + "/" + name;
    }
    stats.models.push_back({false, key, file, package.ClassName(e), {}, model.vertices, model.triangles,
                            uint32_t(model.mesh.bones.size())});
}

// An AnimSet as a .glb of the model whose root bone is its first track. The
// model goes without textures: a character has a dozen sets, and models/ has
// the textured copy.
void ExportAnimSet(const Job &job, const Package &package, size_t index, const std::string &key, const Context &context,
                   Stats &stats)
{
    const auto &e = package.exports[index];
    AnimSetData set;
    try { set = ReadAnimSet(package, e); }
    catch (const Skip &skip) { stats.Skipped(skip.reason, key + ": " + skip.what()); return; }
    catch (const std::exception &error) { stats.Skipped("animation_unreadable", key + ": " + error.what()); return; }
    uint32_t frames = 0;
    for (const auto &sequence : set.sequences) frames += sequence.frames;
    ModelRow row{true, key, job.path, "AnimSet", {}, uint32_t(set.sequences.size()), uint32_t(set.tracks.size()), frames};
    if (context.request.list) { stats.models.push_back(std::move(row)); return; }
    const auto *source = FindSkeleton(*context.skeletons, set);
    if (!source) { stats.Skipped("animation_without_model", key + " (" + set.tracks[0] + ")"); return; }
    Model model;
    try
    {
        std::vector<uint8_t> bytes;
        const Package *owner = &package;
        std::unique_ptr<Package> other;
        if (source->job.path != job.path)
        {
            bytes = ReadPackageBytes(source->job);
            other = std::make_unique<Package>(bytes);
            owner = other.get();
        }
        Require(source->index < owner->exports.size(), "model export missing");
        model = ReadSkeletalMesh(*owner, owner->exports[source->index]);
        row.model = source->job.path + ":" + owner->exports[source->index].name;
    }
    catch (const OutputError &) { throw; }
    catch (const std::exception &error) { stats.Skipped("animation_model_unreadable", key + ": " + error.what()); return; }
    if (!Animate(model.mesh, set)) { stats.Skipped("animation_without_model", key + " (no matching bones)"); return; }
    const auto name = FileStem(e.name) + "." + std::to_string(index) + ".glb";
    const auto directory = context.animations / FromUtf8(job.path);
    CreateDirectories(directory);
    const auto glb = gltf::Write(model.mesh, e.name);
    WriteFile(directory / FromUtf8(name), glb.data(), glb.size());
    stats.modelBytes += glb.size();
    row.file = job.path + "/" + name;
    stats.models.push_back(std::move(row));
}

// list: the texture's headers only, no decode, fingerprint or PNG.
void ExportPackage(const Job &job, const Context &context, Stats &stats)
{
    const auto &request = context.request;
    const bool pngs = request.pngs, list = request.list;
    std::vector<uint8_t> bytes;
    try { bytes = ReadPackageBytes(job); }
    catch (const std::exception &error) { stats.Skipped("package_unreadable", job.path + ": " + error.what()); return; }
    if (!IsPackage(bytes))
    {
        ++stats.notPackages;
        return;
    }
    std::unique_ptr<Package> package;
    try { package = std::make_unique<Package>(bytes); }
    catch (const std::exception &error) { stats.Skipped("package_unreadable", job.path + ": " + error.what()); return; }
    ++stats.packages;
    const auto directory = context.textures / FromUtf8(job.path);
    bool created = false;
    TextureCache cache;
    for (size_t index = 0; index < package->exports.size(); ++index)
    {
        const auto &e = package->exports[index];
        if (e.name.starts_with("Default__")) continue;
        const auto className = package->ClassName(e);
        const auto kind = request.textures ? Classify(className) : TextureClass::None;
        const auto mesh = request.models ? ClassifyMesh(className) : MeshClass::None;
        const bool animation = request.animations && className == "AnimSet";
        if (kind == TextureClass::None && mesh == MeshClass::None && !animation) continue;
        // Extractor export indices are zero-based, as in settings/menu_assets.cpp.
        const auto key = MakeManifestKey(job.path, uint32_t(index), e.name);
        if (context.select && !context.select->count(key)) continue;
        if (mesh != MeshClass::None || animation)
        {
            if (key.empty()) stats.Skipped("invalid_key", job.path + "#" + std::to_string(index));
            else if (animation) ExportAnimSet(job, *package, index, key, context, stats);
            else ExportModel(job, *package, index, key, mesh, context, cache, stats);
            continue;
        }
        if (kind == TextureClass::Unsupported)
        {
            stats.Skipped("unsupported_class", (key.empty() ? job.path : key) + " (" + package->ClassName(e) + ")");
            continue;
        }
        if (key.empty()) { stats.Skipped("invalid_key", job.path + "#" + std::to_string(index)); continue; }
        if (list)
        {
            try
            {
                const auto top = ReadTopMip(*package, e, false);
                stats.rows.push_back({key, job.path, top.width, top.height, top.format->name});
                ++stats.formats[top.format->name];
            }
            catch (const Skip &skip) { stats.Skipped(skip.reason, key + ": " + skip.what()); }
            catch (const std::exception &error) { stats.Skipped("decode_error", key + ": " + error.what()); }
            continue;
        }
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
    stats.modelBytes += cache.bytes;
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

// The DLC replaces some text files: on the discs the DLC dungeon lines are
// placeholders, and lines only the DLC has are missing.
constexpr const char *kNoDlcWarning =
    "no DLC found, so the DLC dungeon text is the discs' unfinished version; "
    "export from a game with the DLC installed if you have it";

std::optional<Request> ParseArguments(int argc, char **argv)
{
    bool option = false;
    Request parsed;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument = argv[i];
        if (!argument.starts_with("--export-")) continue;
        option = true;
        if (argument == "--export-list") { parsed.list = true; continue; }
        if (argument == "--export-model-textures") { parsed.modelTextures = true; continue; }
        const bool known = argument == "--export-assets" || argument == "--export-kinds" || argument == "--export-filter" ||
                           argument == "--export-select" || argument == "--export-language" ||
                           argument == "--export-language-pack";
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
        else if (argument == "--export-select") parsed.select = FromUtf8(value);
        else if (argument == "--export-language")
        {
            parsed.language = Lower(value);
            if (!settings::language::IdFromCode(parsed.language))
                parsed.error = "unknown language '" + value + "' (expected int, jpn, deu, fra, spa, ita, kor, chi or sch)";
        }
        else if (argument == "--export-language-pack")
        {
            // The id rules of language.ini (modding/mod_api.h).
            parsed.languagePack = Lower(value);
            const auto &id = parsed.languagePack;
            if (id.empty() || id.size() > 128 || id == "." || id == ".." ||
                !std::all_of(id.begin(), id.end(), [](char c) {
                    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
                }))
                parsed.error = "--export-language-pack needs an id of letters, digits, '-', '_' or '.', such as pt-br";
        }
        else
        {
            parsed.textures = parsed.movies = parsed.pngs = parsed.text = parsed.models = parsed.animations = false;
            size_t begin = 0;
            while (begin <= value.size())
            {
                const auto end = std::min(value.find(',', begin), value.size());
                const auto kind = Lower(value.substr(begin, end - begin));
                if (kind == "textures") parsed.textures = parsed.pngs = true;
                else if (kind == "fingerprints") parsed.textures = true; // index.csv without PNG files
                else if (kind == "movies") parsed.movies = true;
                else if (kind == "text") parsed.text = true;
                else if (kind == "models") parsed.models = true;
                else if (kind == "animations") parsed.animations = true;
                else parsed.error = "unknown export kind '" + kind + "' (expected textures,fingerprints,movies,text,models,animations)";
                begin = end + 1;
            }
        }
    }
    if (!option) return std::nullopt;
    if (parsed.output.empty() && parsed.error.empty()) parsed.error = "--export-assets <output folder> is required";
    if (!parsed.languagePack.empty())
    {
        parsed.textures = parsed.movies = parsed.pngs = parsed.models = parsed.animations = false;
        parsed.text = true;
        if (parsed.language.empty()) parsed.language = "int";
    }
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

    // DLC indexes (game/dlc/<content id>/LODLC00x.fpi) override some text files;
    // the game looks them up before the discs. With several, the highest number wins.
    std::vector<Disc> dlcs;
    std::vector<std::string> dlcNames;
    if (request.text)
    {
        std::vector<fs::path> indexes;
        for (const auto &folder : fs::directory_iterator(boot.parent_path() / "dlc", error))
            for (const auto &file : fs::directory_iterator(folder.path(), error))
                if (Lower(Utf8(file.path().extension())) == ".fpi" && file.is_regular_file(error)) indexes.push_back(file.path());
        std::sort(indexes.begin(), indexes.end(), [](const fs::path &a, const fs::path &b) {
            return Lower(Utf8(a.filename())) < Lower(Utf8(b.filename()));
        });
        for (const auto &index : indexes)
        {
            try
            {
                dlcs.push_back(ReadDisc(index.parent_path(), index.filename()));
                dlcNames.push_back(Lower(Utf8(index.stem())));
            }
            catch (const std::exception &failure) { discNotes.push_back(Utf8(index) + ": " + failure.what()); }
        }
    }

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
    // --export-select: list.csv names; a texture, model or animation set key
    // also selects its package.
    std::set<std::string> select, selectedPackages;
    if (!request.select.empty())
    {
        std::ifstream input(request.select, std::ios::binary);
        if (!input) return Fail("cannot read " + Utf8(request.select));
        for (std::string line; std::getline(input, line);)
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.starts_with("\xef\xbb\xbf")) line.erase(0, 3);
            if (line.empty()) continue;
            if (const auto hash = line.find('#'); hash != std::string::npos) selectedPackages.insert(line.substr(0, hash));
            select.insert(std::move(line));
        }
        if (select.empty()) return Fail(Utf8(request.select) + " names nothing to export");
    }
    const auto textures = output / "textures", movies = output / "movies";
    const auto models = output / "models", animations = output / "animations";
    const auto pack = request.languagePack.empty() ? fs::path{} : output / FromUtf8(request.languagePack);
    const auto texts = pack.empty() ? output / "text" : pack / "text";
    // The untranslated copy that language-clean compares the pack with.
    const auto reference = pack.empty() ? fs::path{} : output / "original" / "text";
    try
    {
        CreateDirectories(output);
        if (!request.list)
        {
            if (request.textures) CreateDirectories(textures);
            if (request.movies) CreateDirectories(movies);
            if (request.text) CreateDirectories(texts);
            if (request.models) CreateDirectories(models);
            if (request.animations) CreateDirectories(animations);
        }
    }
    catch (const std::exception &failure) { return Fail(failure.what()); }
    const bool packages = request.textures || request.models || request.animations;

    // One job per FPI path; a path repeated on later discs is the same file
    // when its size matches (the four-disc census found no content variants).
    Stats total;
    std::vector<Job> jobs;
    std::map<std::string, uint32_t> seen; // path -> stored size
    std::set<std::string> movieNames;
    // Text files a DLC replaces: path -> (DLC, its entry); later DLCs win.
    std::map<std::string, std::pair<size_t, const Entry *>> overrides;
    for (size_t i = 0; i < dlcs.size(); ++i)
        for (const auto &entry : dlcs[i].entries)
            if (text::Detect(entry.path) != text::Format::None) overrides[entry.path] = {i, &entry};
    auto addText = [&](const Disc &disc, const Entry &entry, const std::string &source) {
        const auto &archive = disc.archives[entry.archive];
        if (archive.empty() || entry.offset > disc.archiveSizes[entry.archive] ||
            entry.size > disc.archiveSizes[entry.archive] - entry.offset || !SafeRelative(entry.path))
        {
            total.Skipped("text_unreadable", entry.path + ": archive missing, extent out of range or unsafe path");
            return;
        }
        Job job{Job::Kind::Text, entry.path, archive, entry.offset, entry.size, {}};
        job.textFormat = text::Detect(entry.path);
        job.source = source;
        jobs.push_back(std::move(job));
    };
    for (const auto &disc : discs)
        for (const auto &entry : disc.entries)
        {
            const auto extension = entry.path.substr(std::min(entry.path.rfind('.'), entry.path.size()));
            const bool package = extension == ".xxx" || extension == ".upk" || extension == ".umap" || extension == ".u";
            const bool movie = extension == ".wmv";
            const bool textFile = request.text && text::Detect(entry.path) != text::Format::None;
            if (!((package && packages) || (movie && request.movies) || textFile)) continue;
            if (!request.filter.empty() && entry.path.find(request.filter) == std::string::npos) continue;
            if (!select.empty() && package && !selectedPackages.count(entry.path)) continue;
            if (textFile)
            {
                if (!request.language.empty() && text::Language(entry.path) != request.language) continue;
                if (!select.empty() && !select.count(entry.path)) continue;
                // One copy per path, as for packages below.
                if (const auto it = seen.find(entry.path); it != seen.end())
                {
                    if (it->second != entry.size && !overrides.count(entry.path))
                        total.Skipped("text_variant_on_later_disc", "disc" + std::to_string(disc.number) + ":" + entry.path);
                    continue;
                }
                seen.emplace(entry.path, entry.size);
                if (const auto it = overrides.find(entry.path); it != overrides.end())
                    addText(dlcs[it->second.first], *it->second.second, dlcNames[it->second.first]);
                else addText(disc, entry, "base");
                continue;
            }
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
                if (!select.empty() && !select.count(name) && !select.count(entry.path)) continue;
            }
            jobs.push_back(std::move(job));
        }
    // Text files only a DLC has.
    for (const auto &[path, source] : overrides)
        if ((request.filter.empty() || path.find(request.filter) != std::string::npos) && (select.empty() || select.count(path)) &&
            (request.language.empty() || text::Language(path) == request.language) && seen.emplace(path, 0).second)
            addText(dlcs[source.first], *source.second, dlcNames[source.first]);
    // Largest first keeps the four workers busy until the end.
    std::stable_sort(jobs.begin(), jobs.end(), [](const Job &a, const Job &b) { return a.size > b.size; });

    // Animation sets name their skeleton only through bone names; each character's
    // root bone name is unique, and its SkeletalMesh is in a *_m package.
    std::vector<Skeleton> skeletons;
    if (request.animations && !request.list)
    {
        std::set<std::string> visited;
        for (const auto &disc : discs)
            for (const auto &entry : disc.entries)
            {
                const auto &archive = disc.archives[entry.archive];
                if (!entry.path.ends_with("_m.xxx") || !visited.insert(entry.path).second || archive.empty() ||
                    entry.offset > disc.archiveSizes[entry.archive] || entry.size > disc.archiveSizes[entry.archive] - entry.offset)
                    continue;
                try
                {
                    Job job{Job::Kind::Package, entry.path, archive, entry.offset, entry.size, {}};
                    const auto bytes = ReadPackageBytes(job);
                    if (!IsPackage(bytes)) continue;
                    const Package package(bytes);
                    for (size_t index = 0; index < package.exports.size(); ++index)
                    {
                        const auto &e = package.exports[index];
                        if (package.ClassName(e) != "SkeletalMesh" || e.name.starts_with("Default__")) continue;
                        try
                        {
                            const auto model = ReadSkeletalMesh(package, e);
                            Skeleton skeleton{job, uint32_t(index), Lower(model.mesh.bones[0].name), {}};
                            for (const auto &bone : model.mesh.bones) skeleton.bones.insert(Lower(bone.name));
                            skeletons.push_back(std::move(skeleton));
                        }
                        catch (const std::exception &) {} // Counted when the model itself is exported.
                    }
                }
                catch (const std::exception &) {}
            }
    }
    const Context context{request, textures, models, animations, select.empty() ? nullptr : &select, &skeletons};

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
                const auto &job = jobs[index];
                if (job.kind == Job::Kind::Package) ExportPackage(job, context, local);
                else if (job.kind == Job::Kind::Text) ExportText(job, request.list ? fs::path{} : texts, request.list ? fs::path{} : reference, local);
                else if (request.list) local.movieRows.push_back({job.movieName, job.path, job.size});
                else ExportMovie(job, movies, local);
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

    std::sort(total.models.begin(), total.models.end(), [](const ModelRow &a, const ModelRow &b) { return a.key < b.key; });
    size_t modelCount = 0, animationCount = 0;
    for (const auto &row : total.models) ++(row.animation ? animationCount : modelCount);
    if (request.list)
    {
        // kind,name,path,width,height,format,language,entries,bytes: name is the
        // texture, model or animation set key, the movie's output file name or
        // the text member path. Models put vertices, triangles, the class and
        // bones in width, height, format and entries; animation sets put
        // sequences, tracks, AnimSet and frames there.
        std::sort(total.rows.begin(), total.rows.end(), [](const Row &a, const Row &b) { return a.key < b.key; });
        std::sort(total.movieRows.begin(), total.movieRows.end(), [](const MovieRow &a, const MovieRow &b) { return a.path < b.path; });
        std::sort(total.texts.begin(), total.texts.end(), [](const TextRow &a, const TextRow &b) { return a.path < b.path; });
        std::string csv = "kind,name,path,width,height,format,language,entries,bytes\n";
        for (const auto &row : total.rows)
            csv += "texture," + Csv(row.key) + "," + Csv(row.file) + "," + std::to_string(row.width) + "," +
                   std::to_string(row.height) + "," + row.format + ",,,\n";
        for (const auto &row : total.movieRows)
            csv += "movie," + Csv(row.name) + "," + Csv(row.path) + ",,,,,," + std::to_string(row.bytes) + "\n";
        for (const auto &row : total.texts)
            csv += "text," + Csv(row.path) + "," + Csv(row.path) + ",,," + text::Name(row.format) + "," + row.language + "," +
                   std::to_string(row.entries) + ",\n";
        for (const auto &row : total.models)
            csv += std::string(row.animation ? "animation," : "model,") + Csv(row.key) + "," + Csv(row.file) + "," +
                   std::to_string(row.a) + "," + std::to_string(row.b) + "," + row.className + ",," + std::to_string(row.c) + ",\n";
        try { WriteFile(output / "list.csv", csv.data(), csv.size()); }
        catch (const std::exception &failure) { return Fail(failure.what()); }
        std::printf("listed %zu textures, %zu movies, %zu text files, %zu models, %zu animation sets, skipped %llu\n",
                    total.rows.size(), total.movieRows.size(), total.texts.size(), modelCount, animationCount,
                    static_cast<unsigned long long>(total.SkippedTotal()));
        std::fflush(stdout);
        return 0;
    }

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
        if (request.models)
        {
            std::string csv = "key,file,class,vertices,triangles,bones\n";
            for (const auto &row : total.models)
                if (!row.animation)
                    csv += Csv(row.key) + "," + Csv(row.file) + "," + row.className + "," + std::to_string(row.a) + "," +
                           std::to_string(row.b) + "," + std::to_string(row.c) + "\n";
            WriteFile(models / "index.csv", csv.data(), csv.size());
        }
        if (request.animations)
        {
            std::string csv = "key,file,sequences,tracks,frames,model\n";
            for (const auto &row : total.models)
                if (row.animation)
                    csv += Csv(row.key) + "," + Csv(row.file) + "," + std::to_string(row.a) + "," + std::to_string(row.b) + "," +
                           std::to_string(row.c) + "," + Csv(row.model) + "\n";
            WriteFile(animations / "index.csv", csv.data(), csv.size());
        }
        std::map<std::string, size_t> textFormats;
        size_t textEntries = 0, textFailed = 0;
        if (request.text)
        {
            std::sort(total.texts.begin(), total.texts.end(), [](const TextRow &a, const TextRow &b) { return a.path < b.path; });
            std::string csv = "path,language,format,source,entries,round_trip\n";
            for (const auto &row : total.texts)
            {
                csv += Csv(row.path) + "," + row.language + "," + text::Name(row.format) + "," + row.source + "," +
                       std::to_string(row.entries) + "," + (row.roundTrip ? "ok" : "failed") + "\n";
                ++textFormats[text::Name(row.format)];
                textEntries += row.entries;
                textFailed += !row.roundTrip;
            }
            WriteFile(texts / "index.csv", csv.data(), csv.size());
        }
        if (!pack.empty())
        {
            const std::string ini =
                "; A language pack made by --export-assets. Set name to what Settings shows for the\n"
                "; language, translate the values in text/, then copy this folder into the game's mods folder.\n"
                "id=" + request.languagePack + "\nname=" + request.languagePack + "\nbase=" + request.language + "\n";
            WriteFile(pack / "language.ini", ini.data(), ini.size());
        }
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        std::string summary = "Lost Odyssey asset export\n";
        summary += "game: " + Utf8(boot) + "\ndiscs:";
        for (const auto &disc : discs) summary += " " + std::to_string(disc.number) + " (" + Utf8(disc.root) + ")";
        if (!dlcNames.empty())
        {
            summary += "\ndlc:";
            for (const auto &name : dlcNames) summary += " " + name;
        }
        summary += std::string("\nkinds:") + (request.textures ? " textures" : "") + (request.movies ? " movies" : "") +
                   (request.text ? " text" : "") + (request.models ? " models" : "") + (request.animations ? " animations" : "") +
                   (request.modelTextures ? " (with model textures)" : "");
        summary += "\nfilter: " + (request.filter.empty() ? std::string("(none)") : request.filter);
        if (!select.empty()) summary += "\nselection: " + std::to_string(select.size()) + " names from " + Utf8(request.select);
        summary += "\ntext language: " + (request.language.empty() ? std::string("(all)") : request.language);
        if (!pack.empty()) summary += "\nlanguage pack: " + request.languagePack;
        if (!pack.empty() && dlcNames.empty()) summary += std::string("\nwarning: ") + kNoDlcWarning;
        char line[256];
        std::snprintf(line, sizeof(line), "\nelapsed_seconds: %.1f\nthreads: %u\n", elapsed, threads);
        summary += line;
        summary += "\ntextures: " + std::to_string(total.rows.size()) + " (" + std::to_string(total.textureBytes) + " PNG bytes)\n";
        for (const auto &format : kFormats)
            summary += "  " + std::string(format.name) + ": " + std::to_string(total.formats[format.name]) + "\n";
        summary += "movies: " + std::to_string(total.movies) + " (" + std::to_string(total.movieBytes) + " bytes)\n";
        summary += "models: " + std::to_string(modelCount) + ", animation sets: " + std::to_string(animationCount) + " (" +
                   std::to_string(total.modelBytes) + " glTF bytes)\n";
        summary += "packages read: " + std::to_string(total.packages) + " (" + std::to_string(total.notPackages) +
                   " package-named files were not UE3 packages)\n";
        summary += "text files: " + std::to_string(total.texts.size()) + " (" + std::to_string(textEntries) +
                   " strings; rebuild check failed for " + std::to_string(textFailed) + ")\n";
        for (const auto &[format, count] : textFormats) summary += "  " + format + ": " + std::to_string(count) + "\n";
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

    std::printf("exported %zu textures, %llu movies, %zu text files, %zu models, %zu animation sets, skipped %llu\n",
                total.rows.size(), static_cast<unsigned long long>(total.movies), total.texts.size(), modelCount, animationCount,
                static_cast<unsigned long long>(total.SkippedTotal()));
    if (!pack.empty() && dlcNames.empty()) std::printf("warning: %s\n", kNoDlcWarning);
    if (!pack.empty()) std::printf("language pack ready to translate: %s\n", Utf8(pack).c_str());
    std::fflush(stdout);
    return 0;
}
}
