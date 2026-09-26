#include <hid/controller_atlas_glyphs.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

namespace
{
using namespace hid::prompts::atlas;
constexpr size_t RowBytes = size_t(Width) * 4;

void Require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

bool Contains(Rect rect, int x, int y)
{
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}

bool Patched(int x, int y)
{
    for (auto cell : FaceCells) if (Contains(cell.artwork, x, y)) return true;
    for (auto cell : ShoulderCells) if (Contains(cell.artwork, x, y)) return true;
    for (auto cell : SystemCells) if (Contains(cell.rect, x, y)) return true;
    return false;
}

bool SystemTile(int x, int y)
{
    for (auto cell : SystemCells) if (Contains(cell.rect, x, y)) return true;
    return false;
}
} // namespace

int main(int argc, char** argv)
{
    using hid::prompts::Face;
    Require(FaceCells[0].face == Face::A && FaceCells[1].face == Face::B &&
            FaceCells[2].face == Face::X && FaceCells[3].face == Face::Y,
            "face cells must keep physical A/B/X/Y mapping");
    for (size_t i = 0; i < 4; ++i)
    {
        Require(FaceCells[i].rect.x == int(i) * 36 && FaceCells[i].rect.y == 72 &&
                FaceCells[i].rect.width == 36 && FaceCells[i].rect.height == 36,
                "face UV tile is not the original 36x36 cell");
        Require(ShoulderCells[i].rect.x == int(i) * 36 && ShoulderCells[i].rect.y == 0 &&
                ShoulderCells[i].rect.width == 36 && ShoulderCells[i].rect.height == 36,
                "shoulder UV tile is not the original 36x36 cell");
    }
    Require(ShoulderCells[0].side == 'L' && ShoulderCells[0].number == '1' &&
            ShoulderCells[1].side == 'R' && ShoulderCells[1].number == '1' &&
            ShoulderCells[2].side == 'L' && ShoulderCells[2].number == '2' &&
            ShoulderCells[3].side == 'R' && ShoulderCells[3].number == '2',
            "physical LB/RB/LT/RT must map to L1/R1/L2/R2");
    Require(SystemCells[0].button == SystemButton::Back &&
            SystemCells[0].rect.x == 108 && SystemCells[0].rect.y == 36 &&
            SystemCells[0].rect.width == 36 && SystemCells[0].rect.height == 36 &&
            SystemCells[0].artwork.x == 111 && SystemCells[0].artwork.y == 38 &&
            SystemCells[0].artwork.width == 30 && SystemCells[0].artwork.height == 32,
            "BACK/Select artwork or tile boundary changed");
    Require(SystemCells[1].button == SystemButton::Start &&
            SystemCells[1].rect.x == 144 && SystemCells[1].rect.y == 36 &&
            SystemCells[1].rect.width == 36 && SystemCells[1].rect.height == 36 &&
            SystemCells[1].artwork.x == 146 && SystemCells[1].artwork.y == 38 &&
            SystemCells[1].artwork.width == 32 && SystemCells[1].artwork.height == 32,
            "START/Options artwork or tile boundary changed");
    constexpr std::array<std::array<uint8_t, 3>, 4> expectedInk{{
        {133, 207, 255}, {255, 153, 164}, {230, 174, 245}, {174, 244, 174}
    }};
    const size_t sourceStride = RowBytes + 19, destinationStride = RowBytes + 11;
    std::vector<uint8_t> source(sourceStride * Height, 0x3b);
    std::vector<uint8_t> destination(destinationStride * Height, 0xba);
    for (int y = 0; y < Height; ++y)
        for (int x = 0; x < Width; ++x)
        {
            uint8_t* pixel = source.data() + size_t(y) * sourceStride + size_t(x) * 4;
            pixel[0] = uint8_t((x * 37 + y * 11) % 256);
            pixel[1] = uint8_t((x * 3 + y * 29) % 256);
            pixel[2] = uint8_t((x * 17 + y * 19) % 256);
            pixel[3] = (x % 17 == 0 || y % 9 == 0) ? 0 : 255;
        }
    const auto original = source;
    Require(PatchPlayStationAtlasRgba(source.data(), sourceStride, destination.data(), destinationStride, Width, Height),
            "strided atlas patch");
    Require(source == original, "input modified");
    std::array<int, 4> faceChanges{};
    std::array<int, 4> faceInk{};
    std::array<int, 4> shoulderChanges{};
    std::array<int, 2> systemChanges{}, systemAlphaChanges{};
    for (int y = 0; y < Height; ++y)
    {
        for (int x = 0; x < Width; ++x)
        {
            const uint8_t* before = source.data() + size_t(y) * sourceStride + size_t(x) * 4;
            const uint8_t* after = destination.data() + size_t(y) * destinationStride + size_t(x) * 4;
            if (!SystemTile(x, y)) Require(before[3] == after[3], "alpha changed outside BACK/START tiles");
            if (!Patched(x, y) || (before[3] == 0 && !SystemTile(x, y)))
                Require(std::equal(before, before + 4, after), "pixel outside changed rect or transparent RGB changed");
            for (size_t i = 0; i < FaceCells.size(); ++i)
                if (Contains(FaceCells[i].rect, x, y))
                {
                    faceChanges[i] += !std::equal(before, before + 3, after);
                    const auto ink = expectedInk[i];
                    faceInk[i] += std::abs(int(after[0]) - ink[0]) < 8 &&
                                  std::abs(int(after[1]) - ink[1]) < 8 &&
                                  std::abs(int(after[2]) - ink[2]) < 8;
                }
            for (size_t i = 0; i < ShoulderCells.size(); ++i)
                if (Contains(ShoulderCells[i].rect, x, y))
                    shoulderChanges[i] += !std::equal(before, before + 3, after);
            for (size_t i = 0; i < SystemCells.size(); ++i)
                if (Contains(SystemCells[i].rect, x, y))
                {
                    systemChanges[i] += !std::equal(before, before + 4, after);
                    systemAlphaChanges[i] += before[3] != after[3];
                }
        }
        const uint8_t* padding = destination.data() + size_t(y) * destinationStride + RowBytes;
        Require(std::all_of(padding, padding + destinationStride - RowBytes,
                            [](uint8_t v) { return v == 0xba; }), "output row padding modified");
    }
    for (size_t i = 0; i < FaceCells.size(); ++i)
        Require(faceChanges[i] > 600 && faceInk[i] > 5, "face icon missing or mapping wrong");
    for (size_t i = 0; i < FaceCells.size(); ++i)
    {
        const auto rect = FaceCells[i].artwork;
        const uint8_t* center = destination.data() + size_t(rect.y + 17) * destinationStride + size_t(rect.x + 16) * 4;
        const auto ink = expectedInk[i];
        const bool lit = std::abs(int(center[0]) - ink[0]) < 16 &&
                         std::abs(int(center[1]) - ink[1]) < 16 &&
                         std::abs(int(center[2]) - ink[2]) < 16;
        Require(lit == (i == 0), "cross center must be lit, circle/square/triangle centers must be open");
    }
    for (int count : shoulderChanges) Require(count > 300, "shoulder icon missing");
    for (size_t i = 0; i < SystemCells.size(); ++i)
        Require(systemChanges[i] > 500 && systemAlphaChanges[i] > 50, "BACK/START tile did not replace old letter alpha");
    const auto systemInk = [](const uint8_t* pixel) {
        return pixel[0] > 190 && pixel[1] > 195 && pixel[2] > 200 && pixel[3] > 200;
    };
    const uint8_t* shareCenter = destination.data() + 54 * destinationStride + 126 * 4;
    const uint8_t* menuCenter = destination.data() + 54 * destinationStride + 162 * 4;
    Require(!systemInk(shareCenter) && systemInk(menuCenter), "Share nodes or Options three-line mark absent");

    std::vector<uint8_t> inplace(RowBytes * Height);
    for (int y = 0; y < Height; ++y)
        std::copy_n(source.data() + size_t(y) * sourceStride, RowBytes,
                    inplace.data() + size_t(y) * RowBytes);
    Require(PatchPlayStationAtlasRgba(inplace.data(), RowBytes, inplace.data(), RowBytes, Width, Height),
            "in-place patch");
    for (int y = 0; y < Height; ++y)
        Require(std::equal(inplace.data() + size_t(y) * RowBytes,
                           inplace.data() + size_t(y + 1) * RowBytes,
                           destination.data() + size_t(y) * destinationStride), "in-place differs from separated buffers");

    const auto unchanged = destination;
    Require(!PatchPlayStationAtlasRgba(source.data(), sourceStride, destination.data(), destinationStride, Width - 1, Height),
            "wrong atlas size accepted");
    Require(!PatchPlayStationAtlasRgba(source.data(), RowBytes - 1, destination.data(), destinationStride, Width, Height),
            "short source stride accepted");
    Require(destination == unchanged, "failed patch wrote to destination");

    if (argc == 3)
    {
        std::ifstream input(argv[1], std::ios::binary);
        Require(bool(input), "cannot open input raw RGBA8 atlas");
        const std::vector<uint8_t> pixels(std::istreambuf_iterator<char>{input}, {});
        Require(pixels.size() == RowBytes * Height, "raw input is not 256x128 RGBA8");
        std::vector<uint8_t> output(pixels.size());
        Require(PatchPlayStationAtlasRgba(pixels.data(), RowBytes, output.data(), RowBytes, Width, Height),
                "real atlas patch");
        std::ofstream result(argv[2], std::ios::binary);
        Require(bool(result.write(reinterpret_cast<const char*>(output.data()),
                                  std::streamsize(output.size()))), "cannot write patched raw RGBA8 atlas");
    }
    else Require(argc == 1, "usage: controller_atlas_glyph_test [input.rgba output.rgba]");
    std::puts("PASS: face/shoulder/Share/Options mapping, alpha scope, untouched pixels, strides and in-place patch");
    return 0;
}
