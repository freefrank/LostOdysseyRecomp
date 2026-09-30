#include <settings/menu_assets.h>
#include <gpu/shader/cpx_decode.h>
#include <lzokay.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>
#include <span>
#include <map>
#include <algorithm>

namespace inspect {
    constexpr size_t MaxPackage = 128 * 1024 * 1024;
#define Check(ok) do { if (!(ok)) throw std::runtime_error(std::string("unsupported package: ") + #ok + " at line " + std::to_string(__LINE__)); } while (0)
    struct Reader {
        std::span<const uint8_t> bytes;
        size_t pos = 0;
        std::span<const uint8_t> Take(size_t count) {
            Check(pos <= bytes.size() && count <= bytes.size() - pos);
            const auto value = bytes.subspan(pos, count); pos += count; return value;
        }
        uint32_t U32() {
            const auto b = Take(4);
            return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
        }
        uint16_t U16() { const auto b = Take(2); return uint16_t(uint16_t(b[0]) << 8 | b[1]); }
        int32_t I32() { return int32_t(U32()); }
        uint8_t Byte() { return Take(1)[0]; }
        std::string String() {
            const auto size = I32(); Check(size > 0 && size <= 4096);
            const auto b = Take(size); Check(b.back() == 0);
            return {reinterpret_cast<const char *>(b.data()), b.size() - 1};
        }
    };
    struct Export { int32_t cls = 0, outer = 0; std::string name; uint32_t offset = 0, size = 0; };
    struct Object {
        Reader native;
        std::map<std::string, int32_t> integers;
    };
    struct Package {
        std::span<const uint8_t> bytes;
        std::vector<std::string> names, imports;
        std::vector<Export> exports;
        std::string Name(Reader &r) const {
            const auto index = r.U32(), number = r.U32(); Check(index < names.size() && number < 65536);
            return names[index] + (number ? "_" + std::to_string(number - 1) : "");
        }
        explicit Package(std::span<const uint8_t> b) : bytes(b) {
            Check(b.size() >= 64 && b.size() <= MaxPackage);
            Reader r{b}; Check(r.U32() == 0x9e2a83c1 && r.U32() == 0x002a01a3);
            const auto header = r.U32(); r.String(); r.U32();
            const auto nc = r.U32(), no = r.U32(), ec = r.U32(), eo = r.U32();
            const auto ic = r.U32(), io = r.U32(), dep = r.U32();
            // Some localized subtitle package slots are intentionally empty.
            if (!nc && !ec && !ic) return;
            if (!nc || nc > 65536 || !ec || ec > 16384 || ic > 2048)
                throw std::runtime_error("package table limits: names=" + std::to_string(nc) +
                    " exports=" + std::to_string(ec) + " imports=" + std::to_string(ic));
            Check(no <= io && io <= eo && eo <= dep && dep <= b.size());
            r = {b.first(io), no};
            for (uint32_t i = 0; i < nc; ++i) { names.push_back(r.String()); r.Take(8); }
            r = {b.first(eo), io};
            for (uint32_t i = 0; i < ic; ++i) { Name(r); Name(r); r.I32(); imports.push_back(Name(r)); }
            r = {b.first(dep), eo};
            for (uint32_t i = 0; i < ec; ++i) {
                Export e; e.cls = r.I32(); r.I32(); e.outer = r.I32(); e.name = Name(r);
                r.I32(); r.Take(8); e.size = r.U32(); e.offset = r.U32(); r.U32();
                r.U32(); r.Take(20);
                Check(e.offset >= header && e.offset <= b.size() && e.size <= b.size() - e.offset);
                Check(e.cls >= 0 || size_t(-int64_t(e.cls)) <= imports.size());
                exports.push_back(std::move(e));
            }
            Check(r.pos == dep);
        }
        Object Read(const Export &e) const {
            Reader r{bytes.first(size_t(e.offset) + e.size), e.offset}; r.I32();
            Object result;
            for (size_t count = 0; count < 4096; ++count) {
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

    inline size_t TiledOffset(uint32_t x, uint32_t y, uint32_t pitch, uint32_t logBytesPerBlock = 4) {
        const uint32_t outer = (((y >> 5) * (pitch >> 5)) + (x >> 5)) << 6;
        const uint32_t inner = (((y >> 1) & 7) << 3) | (x & 7);
        const uint32_t v = (outer | inner) << logBytesPerBlock;
        const uint32_t bank = (y >> 4) & 1, pipe = ((x >> 3) & 3) ^ (((y >> 3) & 1) << 1);
        return ((y & 1) << 4) | (pipe << 6) | (bank << 11) | (v & 15) |
               (((v >> 4) & 1) << 5) | (((v >> 5) & 7) << 8) | ((v >> 8) << 12);
    }

    inline void Bc3(const uint8_t *b, settings::menu_assets::Image &image, uint32_t x, uint32_t y) {
        uint32_t alpha[8] = {b[0], b[1]};
        if (alpha[0] > alpha[1])
            for (uint32_t i = 1; i <= 6; ++i) alpha[i + 1] = ((7 - i) * alpha[0] + i * alpha[1]) / 7;
        else {
            for (uint32_t i = 1; i <= 4; ++i) alpha[i + 1] = ((5 - i) * alpha[0] + i * alpha[1]) / 5;
            alpha[6] = 0; alpha[7] = 255;
        }
        uint64_t abits = 0;
        for (unsigned i = 0; i < 6; ++i) abits |= uint64_t(b[i + 2]) << (8 * i);
        uint32_t colors[4][3]{};
        for (unsigned i = 0; i < 2; ++i) {
            const auto c = uint32_t(b[8 + 2 * i]) | uint32_t(b[9 + 2 * i]) << 8;
            const auto r = (c >> 11) & 31, g = (c >> 5) & 63, bl = c & 31;
            colors[i][0] = (r << 3) | (r >> 2); colors[i][1] = (g << 2) | (g >> 4); colors[i][2] = (bl << 3) | (bl >> 2);
        }
        for (unsigned c = 0; c < 3; ++c) {
            colors[2][c] = (2 * colors[0][c] + colors[1][c]) / 3;
            colors[3][c] = (colors[0][c] + 2 * colors[1][c]) / 3;
        }
        uint32_t cbits = uint32_t(b[12]) | uint32_t(b[13]) << 8 | uint32_t(b[14]) << 16 | uint32_t(b[15]) << 24;
        for (unsigned i = 0; i < 16; ++i) {
            const auto *c = colors[(cbits >> (2 * i)) & 3];
            image.pixels[size_t(y + i / 4) * image.width + x + i % 4] =
                alpha[(abits >> (3 * i)) & 7] << 24 | c[0] << 16 | c[1] << 8 | c[2];
        }
    }

    inline void Bc1(const uint8_t *b, settings::menu_assets::Image &image, uint32_t x, uint32_t y) {
        const auto a = uint32_t(b[0]) | uint32_t(b[1]) << 8;
        const auto z = uint32_t(b[2]) | uint32_t(b[3]) << 8;
        uint32_t colors[4][4]{};
        for (unsigned i = 0; i < 2; ++i) {
            const auto c = i ? z : a;
            colors[i][0] = ((c >> 11) & 31) * 255 / 31;
            colors[i][1] = ((c >> 5) & 63) * 255 / 63;
            colors[i][2] = (c & 31) * 255 / 31;
            colors[i][3] = 255;
        }
        for (unsigned channel = 0; channel < 3; ++channel) {
            colors[2][channel] = a > z ? (2 * colors[0][channel] + colors[1][channel]) / 3
                                          : (colors[0][channel] + colors[1][channel]) / 2;
            colors[3][channel] = a > z ? (colors[0][channel] + 2 * colors[1][channel]) / 3 : 0;
        }
        colors[2][3] = 255;
        colors[3][3] = a > z ? 255 : 0;
        const auto indices = uint32_t(b[4]) | uint32_t(b[5]) << 8 | uint32_t(b[6]) << 16 | uint32_t(b[7]) << 24;
        for (unsigned i = 0; i < 16; ++i) {
            const auto* c = colors[(indices >> (2 * i)) & 3];
            image.pixels[size_t(y + i / 4) * image.width + x + i % 4] =
                c[3] << 24 | c[0] << 16 | c[1] << 8 | c[2];
        }
    }

    settings::menu_assets::Image DecodeTextureAnySize(const Package &package, const Export &e) {
        auto object = package.Read(e); auto r = object.native;
        const auto width = object.integers.at("SizeX"), height = object.integers.at("SizeY");
        const auto format = object.integers.at("Format");
        if (format != 2 && format != 5 && format != 7)
            throw std::runtime_error("unsupported Texture2D format=" + std::to_string(format) + " (RGBA8/2, BC1/5 and BC3/7 only)");
        Check(width >= 4 && height >= 4 && width <= 4096 && height <= 4096 && width % 4 == 0 && height % 4 == 0);
        Check(r.U32() == 0 && r.U32() == 0 && r.U32() == 0);
        Check(r.U32() == r.pos); // sourceOffset
        Check(r.U32() > 0); // mipCount
        const auto flags = r.U32(), rawSize = r.U32(), storedSize = r.U32(), offset = r.U32();
        // Small atlases carry padded tile storage; the visible extent is not
        // the byte count of the allocated top mip.
        const uint32_t bytesPerBlock = format == 2 ? 4 : format == 5 ? 8 : 16;
        const uint32_t minimumBytes = format == 2 ? uint32_t(width * height * 4) :
            uint32_t(width * height / 16 * bytesPerBlock);
        if (flags != 0x10 || rawSize < minimumBytes || rawSize > MaxPackage ||
            storedSize > MaxPackage || offset != r.pos)
            throw std::runtime_error("mip layout flags=" + std::to_string(flags) + " raw=" + std::to_string(rawSize) +
                " stored=" + std::to_string(storedSize) + " offset=" + std::to_string(offset) + " pos=" + std::to_string(r.pos));
        Reader stream{r.Take(storedSize)};
        Check(stream.U32() == 0x9e2a83c1);
        const auto blockSize = stream.U32();
        const auto compressed = stream.U32(), decoded = stream.U32();
        Check(blockSize == 131072 && decoded == rawSize);
        const auto count = (decoded + blockSize - 1) / blockSize;
        std::vector<std::pair<uint32_t, uint32_t>> chunks;
        uint64_t storedTotal = 0;
        for (uint32_t i = 0; i < count; ++i) {
            const auto a = stream.U32(), b = stream.U32();
            Check(a > 0 && b == std::min(blockSize, decoded - i * blockSize));
            chunks.emplace_back(a, b);
            storedTotal += a;
        }
        Check(storedTotal == compressed && stream.pos + storedTotal == stream.bytes.size());
        std::vector<uint8_t> raw(decoded); size_t written = 0;
        for (const auto [a, b] : chunks) {
            const auto input = stream.Take(a); size_t actual = 0;
            Check(lzokay::decompress(input.data(), input.size(), raw.data() + written, b, actual) == lzokay::EResult::Success && actual == b);
            written += actual;
        }
        Check(r.U32() == uint32_t(width) && r.U32() == uint32_t(height));
        settings::menu_assets::Image image{uint32_t(width), uint32_t(height), std::vector<uint32_t>(size_t(width) * height)};
        const uint32_t blockSide = format == 2 ? 1 : 4;
        for (uint32_t by = 0; by < uint32_t(height) / blockSide; ++by)
            for (uint32_t bx = 0; bx < uint32_t(width) / blockSide; ++bx) {
                const auto address = TiledOffset(bx, by, uint32_t(width) / blockSide,
                    format == 2 ? 2 : format == 5 ? 3 : 4);
                Check(address <= raw.size() && bytesPerBlock <= raw.size() - address);
                uint8_t block[16];
                for (unsigned i = 0; i < bytesPerBlock; ++i) block[i] = raw[address + (i ^ 1)];
                if (format == 2) image.pixels[size_t(by) * image.width + bx] =
                    uint32_t(block[0]) << 24 | uint32_t(block[1]) << 16 |
                    uint32_t(block[2]) << 8 | block[3]; // A8R8G8B8 after 8-in-16 byte swap.
                else if (format == 5) Bc1(block, image, bx * 4, by * 4);
                else Bc3(block, image, bx * 4, by * 4);
            }
        return image;
    }
}

static void SaveRgba(const std::filesystem::path& file, const settings::menu_assets::Image& img) {
    std::ofstream out(file, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create RGBA output");
    for (uint32_t pixel : img.pixels) {
        const char rgba[] = {char(pixel >> 16), char(pixel >> 8), char(pixel), char(pixel >> 24)};
        out.write(rgba, 4);
    }
    if (!out) throw std::runtime_error("incomplete RGBA output");
}

int main(int argc, char** argv) {
    if (argc != 5) { std::cerr << "usage: decode <archive.fpd> <byte-offset> <length> <raw-output-directory>\n"; return 2; }
    try {
        const uint64_t offset = std::stoull(argv[2]);
        const size_t length = std::stoull(argv[3]);
        if (!length || length > 128 * 1024 * 1024) throw std::runtime_error("package exceeds 128 MiB limit");
        std::ifstream source(argv[1], std::ios::binary | std::ios::ate);
        if (!source) throw std::runtime_error("source FPD unreadable");
        const auto size = source.tellg();
        if (size < 0 || offset > uint64_t(size) || length > uint64_t(size) - offset)
            throw std::runtime_error("source FPD extent outside archive");
        std::vector<uint8_t> bytes(length);
        source.seekg(std::streamoff(offset));
        if (!source.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(length)))
            throw std::runtime_error("source FPD short read");
        if (bytes.size() >= 3 && bytes[0] == 'c' && bytes[1] == 'p' && bytes[2] == 'x') {
            if (!xenos::resources::cpx::Decode(bytes, bytes)) throw std::runtime_error("invalid CPX stream");
        }
        inspect::Package package(bytes);
        const auto output = std::filesystem::path(argv[4]);
        std::filesystem::create_directories(output);
        for (size_t i = 0; i < package.exports.size(); ++i) {
            const auto& e = package.exports[i];
            const std::string cls = e.cls < 0 ? package.imports[size_t(-int64_t(e.cls) - 1)] : "";
            if (cls != "Texture2D" && cls != "Font") continue;
            std::string owner = e.outer > 0 && size_t(e.outer) <= package.exports.size()
                ? package.exports[size_t(e.outer) - 1].name : "";
            uint32_t width = 0, height = 0;
            int32_t format = -1;
            std::string filename, error, fontRefs;
            try {
                const auto object = package.Read(e);
                if (cls == "Texture2D") {
                    width = uint32_t(object.integers.at("SizeX"));
                    height = uint32_t(object.integers.at("SizeY"));
                    format = object.integers.at("Format");
                    auto img = inspect::DecodeTextureAnySize(package, e);
                    filename = std::to_string(i) + ".rgba";
                    SaveRgba(output / filename, img);
                } else {
                    auto native = object.native;
                    const auto glyphs = native.U32();
                    if (!glyphs || glyphs > 16384) throw std::runtime_error("unsupported UFont glyph table");
                    native.Take(size_t(glyphs) * 17);
                    const auto pages = native.U32();
                    if (!pages || pages > 32) throw std::runtime_error("unsupported UFont page count");
                    for (unsigned j = 0; j < pages; ++j) {
                        const auto ref = native.U32();
                        if (!ref || ref > package.exports.size()) throw std::runtime_error("UFont page reference outside package");
                        const auto& page = package.exports[ref - 1];
                        if (page.outer != int32_t(i + 1) ||
                            package.imports[size_t(-int64_t(page.cls) - 1)] != "Texture2D")
                            throw std::runtime_error("UFont page does not point to an owned Texture2D");
                        if (!fontRefs.empty()) fontRefs += ',';
                        fontRefs += std::to_string(ref - 1);
                    }
                }
            } catch (const std::exception& ex) { error = ex.what(); }
            // Font pages are separate Texture2D exports; class/offset/index disambiguate names.
            std::cout << cls << '\t' << i << '\t' << e.name << '\t' << owner << '\t'
                << e.offset << '\t' << e.size << '\t' << width << '\t' << height << '\t'
                << format << '\t' << filename << '\t' << error << '\t' << fontRefs << '\n';
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "package failure: " << ex.what() << '\n'; return 1;
    }
}
