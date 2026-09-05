#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gpu
{
    struct TextureBlockOffset
    {
        uint32_t x = 0;
        uint32_t y = 0;
    };

    // Follows Xenia texture_util.cc GetPackedMipOffset (Copyright 2022 Ben
    // Vanik, BSD 3-Clause; see thirdparty/xenia-LICENSE.txt). Dimensions are
    // those of the original texture, before selecting the mip level.
    constexpr TextureBlockOffset PackedMipOffset2D(uint32_t width, uint32_t height,
        uint32_t mip, uint32_t blockWidth, uint32_t blockHeight)
    {
        const uint32_t logWidth = std::bit_width(width - 1);
        const uint32_t logHeight = std::bit_width(height - 1);
        const uint32_t logMin = std::min(logWidth, logHeight);
        if (logMin > 4 + mip) return {};
        const uint32_t firstPackedMip = logMin > 4 ? logMin - 4 : 0;
        const uint32_t packedMip = mip - firstPackedMip;
        TextureBlockOffset offset;
        if (packedMip < 3)
        {
            if (logWidth > logHeight) offset.y = 16u >> packedMip;
            else offset.x = 16u >> packedMip;
        }
        else
        {
            if (logWidth > logHeight)
                offset.x = (1u << (logWidth - firstPackedMip)) >> (packedMip - 2);
            else
                offset.y = (1u << (logHeight - firstPackedMip)) >> (packedMip - 2);
        }
        offset.x /= blockWidth;
        offset.y /= blockHeight;
        return offset;
    }
}
