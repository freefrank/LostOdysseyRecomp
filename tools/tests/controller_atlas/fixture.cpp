#include <gpu/controller_atlas.h>
#include <hid/controller_atlas_glyphs.h>
#include <lzokay.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// The existing UI exporter supplies the real FPD/CPX/UE parser and Xenos
// tiled offsets. We decode *its* compressed payload to the same pitched BC3
// upload layout that production GetTexture consumes after endian conversion.
#define main LoUiAssetsDecoderMain
#include "../../ui_assets/decode.cpp"
#undef main

static void Require(bool yes, const char* reason) {
    if (!yes) throw std::runtime_error(reason);
}

int main(int argc, char** argv) {
    try {
        Require(argc == 5, "usage: fixture <xenon_loc.fpd> <byte-offset> <length> <Icon_Page_0|Texture2D_1>");
        const std::string exportName = argv[4];
        const auto expected = exportName == "Icon_Page_0" ? gpu::controller_atlas::Identity::IconPage0
            : exportName == "Texture2D_1" ? gpu::controller_atlas::Identity::FontIconPage
            : gpu::controller_atlas::Identity::Unknown;
        Require(expected != gpu::controller_atlas::Identity::Unknown, "unknown atlas export");
        const auto offset = std::stoull(argv[2]), length = std::stoull(argv[3]);
        std::ifstream source(argv[1], std::ios::binary | std::ios::ate);
        Require(bool(source) && offset + length <= uint64_t(source.tellg()), "bad FPD extent");
        source.seekg(offset);
        std::vector<uint8_t> packageBytes(length);
        Require(bool(source.read(reinterpret_cast<char*>(packageBytes.data()), length)), "short FPD read");
        if (packageBytes.size() >= 3 && packageBytes[0] == 'c' && packageBytes[1] == 'p' && packageBytes[2] == 'x')
            Require(xenos::resources::cpx::Decode(packageBytes, packageBytes), "bad CPX");
        inspect::Package package(packageBytes);
        const auto it = std::find_if(package.exports.begin(), package.exports.end(),
            [&](const auto& e) { return e.name == exportName; });
        Require(it != package.exports.end(), "missing controller atlas export");
        const auto object = package.Read(*it);
        auto r = object.native;
        const auto width = uint32_t(object.integers.at("SizeX"));
        const auto height = uint32_t(object.integers.at("SizeY"));
        Require(width == 256 && height == 128 && object.integers.at("Format") == 7, "wrong source format");
        for (int i = 0; i < 3; ++i) Require(r.U32() == 0, "unexpected native header");
        Require(r.U32() == r.pos && r.U32() != 0, "unexpected native offset/mips");
        Require(r.U32() == 0x10, "unexpected top-mip flags");
        const auto rawSize = r.U32(), storedSize = r.U32();
        Require(r.U32() == r.pos, "unexpected top-mip offset");
        inspect::Reader blocks{r.Take(storedSize)};
        Require(blocks.U32() == 0x9e2a83c1 && blocks.U32() == 131072, "bad BC3 LZO blocks");
        const auto compressed = blocks.U32(), decoded = blocks.U32();
        Require(rawSize == decoded && rawSize >= width * height, "bad BC3 decoded extent");
        const auto count = (decoded + 131071) / 131072;
        std::vector<std::pair<uint32_t, uint32_t>> sizes;
        for (uint32_t i = 0; i < count; ++i) {
            const auto storedBlock = blocks.U32(), decodedBlock = blocks.U32();
            sizes.emplace_back(storedBlock, decodedBlock);
        }
        std::vector<uint8_t> tiled(decoded);
        size_t written = 0, stored = 0;
        for (auto [a, b] : sizes) {
            const auto input = blocks.Take(a);
            size_t actual = 0;
            Require(lzokay::decompress(input.data(), input.size(), tiled.data() + written, b, actual) ==
                lzokay::EResult::Success && actual == b, "bad LZO data");
            written += actual; stored += a;
        }
        Require(stored == compressed && written == rawSize, "bad LZO lengths");
        // 1024-byte BC3 rows; alter padding in a second copy to demonstrate
        // that the SHA is over tight RGBA, not over raw/upload bytes.
        constexpr size_t rowPitch = 1280;
        std::vector<uint8_t> staging(rowPitch * (height / 4), 0xcd);
        for (uint32_t by = 0; by < height / 4; ++by)
            for (uint32_t bx = 0; bx < width / 4; ++bx) {
                const auto address = inspect::TiledOffset(bx, by, width / 4);
                Require(address + 16 <= tiled.size(), "bad tiled address");
                for (unsigned i = 0; i < 16; ++i)
                    staging[size_t(by) * rowPitch + bx * 16 + i] = tiled[address + (i ^ 1)];
            }
        using namespace gpu::controller_atlas;
        Require(Candidate(1, 20, width, height, width, height, 0), "candidate rejected");
        Require(!Candidate(0, 20, width, height, width, height, 0), "1D accepted");
        Require(!Candidate(2, 20, width, height, width, height, 0), "3D accepted");
        Require(!Candidate(1, 7, width, height, width, height, 0), "UE format mistaken for fetch format");
        Require(!Candidate(1, 20, width, height, width, height, 1), "wrong mip accepted");
        Require(!Candidate(1, 20, width, height, 128, height, 0), "wrong upload dimensions accepted");
        Require(!Candidate(1, 20, width, height, width, 64, 0), "wrong upload height accepted");
        auto decodedRgba = DecodeBc3(staging.data(), staging.size(), rowPitch);
        Require(Identify(decodedRgba) == expected, "real FPD atlas does not match complete RGBA SHA256");
        namespace cells = hid::prompts::atlas;
        const auto within = [](cells::Rect rect, int x, int y) {
            return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
        };
        // Both authenticated assets use the same 36x36 button tiles, not just
        // the same image dimensions. Verify source silhouettes and real patch
        // coverage of every face, shoulder and system button cell.
        std::vector<uint8_t> patched(decodedRgba.size());
        Require(cells::PatchPlayStationAtlasRgba(decodedRgba.data(), width * 4,
            patched.data(), width * 4, width, height), "real atlas patch failed");
        auto verifyCell = [&](cells::Rect tile, cells::Rect artwork, bool system) {
            unsigned sourceOpaque = 0, rgbChanges = 0, alphaChanges = 0;
            for (int y = tile.y; y < tile.y + tile.height; ++y)
                for (int x = tile.x; x < tile.x + tile.width; ++x) {
                    const size_t pixel = (size_t(y) * width + x) * 4;
                    const auto* before = decodedRgba.data() + pixel;
                    const auto* after = patched.data() + pixel;
                    if (before[3]) {
                        ++sourceOpaque;
                        Require(within(artwork, x, y), "real atlas button silhouette differs from measured cell");
                    }
                    rgbChanges += !std::equal(before, before + 3, after);
                    alphaChanges += before[3] != after[3];
                    if (!system) Require(before[3] == after[3], "face/shoulder alpha changed");
                }
            Require(sourceOpaque > 700 && rgbChanges > 200, "missing source or patched button artwork");
            if (system) Require(alphaChanges > 50, "Select/Start alpha was not rebuilt");
        };
        for (auto cell : cells::FaceCells) verifyCell(cell.rect, cell.artwork, false);
        for (auto cell : cells::ShoulderCells) verifyCell(cell.rect, cell.artwork, false);
        for (auto cell : cells::SystemCells) {
            verifyCell(cell.rect, cell.artwork, true);
            const size_t center = (size_t(cell.rect.y + 18) * width + cell.rect.x + 18) * 4;
            Require(patched[center + 3] == 255, "missing opaque Select/Start icon");
        }
        for (uint32_t y = 0; y < height; ++y)
            for (uint32_t x = 0; x < width; ++x) {
                const bool touched = std::any_of(cells::FaceCells.begin(), cells::FaceCells.end(),
                    [&](const auto& cell) { return within(cell.artwork, x, y); }) ||
                    std::any_of(cells::ShoulderCells.begin(), cells::ShoulderCells.end(),
                    [&](const auto& cell) { return within(cell.artwork, x, y); }) ||
                    std::any_of(cells::SystemCells.begin(), cells::SystemCells.end(),
                    [&](const auto& cell) { return within(cell.rect, x, y); });
                const size_t pixel = (size_t(y) * width + x) * 4;
                if (!touched) Require(std::equal(decodedRgba.begin() + pixel, decodedRgba.begin() + pixel + 4,
                    patched.begin() + pixel), "real atlas pixel outside patched cells changed");
            }
        auto bad = decodedRgba; bad[143] ^= 1;
        Require(Identify(bad) == Identity::Unknown, "same-size altered atlas accepted");
        std::fill(bad.begin(), bad.end(), 0);
        Require(Identify(bad) == Identity::Unknown, "same-size zero atlas accepted");
        std::fill_n(staging.begin() + Bc3RowBytes, rowPitch - Bc3RowBytes, 0);
        Require(Identify(DecodeBc3(staging.data(), staging.size(), rowPitch)) == expected, "row padding changes digest");
        Require(Identify(std::vector<uint8_t>(decodedRgba.begin(), decodedRgba.end() - 1)) == Identity::Unknown,
            "short RGBA accepted");
        Require(DecodeBc3(staging.data(), staging.size() - rowPitch, rowPitch).empty(), "short image accepted");
        std::cout << Name(expected) << ": real BC3 SHA256 and cell patch positive; row pitch, changed/zero/mip/dimension negatives passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
