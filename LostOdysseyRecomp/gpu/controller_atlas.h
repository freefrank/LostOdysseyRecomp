#pragma once

#include "shader/resource_cpx_index_sha256.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace gpu::controller_atlas {

inline constexpr uint32_t Width = 256, Height = 128, Bc3RowBytes = Width / 4 * 16;
inline constexpr std::array<uint8_t, 32> IconPage0Sha256{
    0x8e, 0x18, 0x11, 0x95, 0xd7, 0x5d, 0x0d, 0xda,
    0xed, 0x8e, 0xd8, 0x5c, 0xa9, 0xa3, 0x6f, 0xa6,
    0x41, 0xcf, 0x9c, 0xd8, 0x68, 0x7d, 0xf2, 0xb4,
    0x6b, 0x9d, 0x90, 0xf2, 0x98, 0x54, 0xa7, 0xa1};
// rpfontscommon_int.xxx Font Icon #00644, font_page_exports[28] Texture2D_1.
inline constexpr std::array<uint8_t, 32> FontIconPageSha256{
    0xcf, 0xd3, 0x0b, 0x83, 0x0a, 0x2f, 0xbf, 0x5d,
    0x12, 0x52, 0x0e, 0xa5, 0xc6, 0xcc, 0x2a, 0x6a,
    0x3a, 0x37, 0xd5, 0x34, 0xf8, 0xf4, 0xc7, 0xf8,
    0x46, 0xfd, 0xc5, 0x05, 0x20, 0xba, 0x82, 0x29};

enum class Identity { Unknown, IconPage0, FontIconPage };

inline const char* Name(Identity identity) {
    switch (identity) {
    case Identity::IconPage0: return "Icon_Page_0";
    case Identity::FontIconPage: return "Font_Icon_Texture2D_1";
    default: return "unknown";
    }
}

// Xenos format 20 is BC3; the UE package's Format 7 is not a fetch format.
inline bool Candidate(uint32_t dimension, uint32_t format, uint32_t originalWidth,
    uint32_t originalHeight, uint32_t uploadWidth, uint32_t uploadHeight, uint32_t sourceMip) {
    return dimension == 1 && format == 20 && originalWidth == Width && originalHeight == Height &&
        uploadWidth == Width && uploadHeight == Height && sourceMip == 0;
}

// Input is already untiled and endian-corrected by Renderer::GetTexture. Read
// compressed rows at their GPU upload pitch, but hash a tight RGBA byte stream.
// No second i^1 swap is valid here. Match only the complete 256x128 image.
inline std::vector<uint8_t> DecodeBc3(const uint8_t* bytes, size_t size, size_t rowPitch) {
    if (!bytes || rowPitch < Bc3RowBytes || size < rowPitch * (Height / 4)) return {};
    std::vector<uint8_t> rgba(size_t(Width) * Height * 4);
    for (uint32_t by = 0; by < Height / 4; ++by)
        for (uint32_t bx = 0; bx < Width / 4; ++bx) {
            const uint8_t* b = bytes + size_t(by) * rowPitch + bx * 16;
            uint8_t alphas[8] = {b[0], b[1]};
            if (alphas[0] > alphas[1])
                for (uint32_t i = 1; i <= 6; ++i)
                    alphas[i + 1] = uint8_t(((7 - i) * alphas[0] + i * alphas[1]) / 7);
            else {
                for (uint32_t i = 1; i <= 4; ++i)
                    alphas[i + 1] = uint8_t(((5 - i) * alphas[0] + i * alphas[1]) / 5);
                alphas[6] = 0; alphas[7] = 255;
            }
            uint64_t alphaBits = 0;
            for (unsigned i = 0; i < 6; ++i) alphaBits |= uint64_t(b[i + 2]) << (i * 8);
            uint8_t colors[4][3]{};
            for (unsigned i = 0; i < 2; ++i) {
                const uint32_t c = uint32_t(b[8 + 2 * i]) | uint32_t(b[9 + 2 * i]) << 8;
                const uint8_t r = uint8_t((c >> 11) & 31), g = uint8_t((c >> 5) & 63), blue = uint8_t(c & 31);
                colors[i][0] = uint8_t((r << 3) | (r >> 2));
                colors[i][1] = uint8_t((g << 2) | (g >> 4));
                colors[i][2] = uint8_t((blue << 3) | (blue >> 2));
            }
            for (unsigned c = 0; c < 3; ++c) {
                colors[2][c] = uint8_t((2 * colors[0][c] + colors[1][c]) / 3);
                colors[3][c] = uint8_t((colors[0][c] + 2 * colors[1][c]) / 3);
            }
            const uint32_t colorBits = uint32_t(b[12]) | uint32_t(b[13]) << 8 |
                uint32_t(b[14]) << 16 | uint32_t(b[15]) << 24;
            for (unsigned i = 0; i < 16; ++i) {
                uint8_t* dst = rgba.data() + (size_t(by * 4 + i / 4) * Width + bx * 4 + i % 4) * 4;
                const auto* color = colors[(colorBits >> (2 * i)) & 3];
                dst[0] = color[0]; dst[1] = color[1]; dst[2] = color[2];
                dst[3] = alphas[(alphaBits >> (3 * i)) & 7];
            }
        }
    return rgba;
}

inline Identity Identify(const std::vector<uint8_t>& rgba) {
    if (rgba.size() != size_t(Width) * Height * 4) return Identity::Unknown;
    const auto hash = xenos::resources::Sha256(std::span<const uint8_t>(rgba));
    if (hash == IconPage0Sha256) return Identity::IconPage0;
    if (hash == FontIconPageSha256) return Identity::FontIconPage;
    return Identity::Unknown;
}

} // namespace gpu::controller_atlas
