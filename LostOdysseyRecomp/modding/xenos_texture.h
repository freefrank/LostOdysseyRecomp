#pragma once
#include <cstdint>

// CPU helpers for Xenos texture data read from game packages, shared by the
// native menu loader and the asset exporter. Header-only: no GPU dependency.
namespace modding::xenos_texture
{
// Byte offset of block (x, y) in a tiled 2D surface whose pitch is given in
// blocks (a multiple of 32). Same addressing as gpu::video::TiledOffset2D.
constexpr uint32_t TiledOffset2D(uint32_t x, uint32_t y, uint32_t pitchBlocks, uint32_t bytesPerBlockLog2)
{
    const uint32_t outer = (((y >> 5) * (pitchBlocks >> 5)) + (x >> 5)) << 6;
    const uint32_t inner = (((y >> 1) & 7) << 3) | (x & 7);
    const uint32_t v = (outer | inner) << bytesPerBlockLog2;
    const uint32_t bank = (y >> 4) & 1, pipe = ((x >> 3) & 3) ^ (((y >> 3) & 1) << 1);
    return ((y & 1) << 4) | (pipe << 6) | (bank << 11) | (v & 15) |
           (((v >> 4) & 1) << 5) | (((v >> 5) & 7) << 8) | ((v >> 8) << 12);
}

// BC1/BC2/BC3 (DXT1, DXT2/3, DXT4/5) blocks, little endian (after the 8in16
// swap). Writes 16 row-major texels as 0xAARRGGBB. Interpolants truncate,
// matching the menu loader's original BC3 decoder bit for bit.
inline void DecodeBcBlock(const uint8_t *b, unsigned bc, uint32_t out[16])
{
    const uint8_t *color = bc == 1 ? b : b + 8;
    uint32_t colors[4][4]{};
    const uint32_t c0 = uint32_t(color[0]) | uint32_t(color[1]) << 8, c1 = uint32_t(color[2]) | uint32_t(color[3]) << 8;
    for (unsigned i = 0; i < 2; ++i)
    {
        const auto c = i ? c1 : c0;
        const auto r = (c >> 11) & 31, g = (c >> 5) & 63, bl = c & 31;
        colors[i][0] = (r << 3) | (r >> 2); colors[i][1] = (g << 2) | (g >> 4); colors[i][2] = (bl << 3) | (bl >> 2);
        colors[i][3] = 255;
    }
    if (bc != 1 || c0 > c1)
        for (unsigned c = 0; c < 3; ++c)
        {
            colors[2][c] = (2 * colors[0][c] + colors[1][c]) / 3;
            colors[3][c] = (colors[0][c] + 2 * colors[1][c]) / 3;
            colors[2][3] = colors[3][3] = 255;
        }
    else
    {
        // DXT1 three-colour mode: index 3 is transparent black.
        for (unsigned c = 0; c < 3; ++c) colors[2][c] = (colors[0][c] + colors[1][c]) / 2;
        colors[2][3] = 255;
    }
    uint32_t alpha[16]{};
    if (bc == 3)
    {
        uint32_t palette[8] = {b[0], b[1]};
        if (palette[0] > palette[1])
            for (uint32_t i = 1; i <= 6; ++i) palette[i + 1] = ((7 - i) * palette[0] + i * palette[1]) / 7;
        else
        {
            for (uint32_t i = 1; i <= 4; ++i) palette[i + 1] = ((5 - i) * palette[0] + i * palette[1]) / 5;
            palette[6] = 0; palette[7] = 255;
        }
        uint64_t bits = 0;
        for (unsigned i = 0; i < 6; ++i) bits |= uint64_t(b[i + 2]) << (8 * i);
        for (unsigned i = 0; i < 16; ++i) alpha[i] = palette[(bits >> (3 * i)) & 7];
    }
    else if (bc == 2)
        for (unsigned i = 0; i < 16; ++i) alpha[i] = ((b[i / 2] >> ((i & 1) * 4)) & 15) * 17;
    const uint32_t indices = uint32_t(color[4]) | uint32_t(color[5]) << 8 | uint32_t(color[6]) << 16 | uint32_t(color[7]) << 24;
    for (unsigned i = 0; i < 16; ++i)
    {
        const auto *c = colors[(indices >> (2 * i)) & 3];
        out[i] = (bc == 1 ? c[3] : alpha[i]) << 24 | c[0] << 16 | c[1] << 8 | c[2];
    }
}
}
