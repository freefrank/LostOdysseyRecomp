#pragma once
#include "movie_decoder.h"
#include <cstring>
#include <span>

namespace movie {
struct Yuv420Plane {
    std::span<const uint8_t> bytes;
    uint32_t rowStride = 0, pixelStride = 0;
};

// Android Image planes are always Y, Cb, Cr, including interleaved NV12/NV21
// storage. Only bytes through the last pixel of the last row are guaranteed.
inline bool CopyYuv420Frame(const Yuv420Plane (&planes)[3], uint32_t imageWidth, uint32_t imageHeight,
                           uint32_t left, uint32_t top, uint32_t width, uint32_t height,
                           Nv12Frame& frame, std::string& error) {
    if (!width || !height || left > imageWidth || width > imageWidth - left ||
        top > imageHeight || height > imageHeight - top || (left & 1) || (top & 1)) {
        error = "unsupported YUV420 crop";
        return false;
    }
    // Reject odd crop origins: simply rounding them down changes chroma phase.
    const uint32_t cw = width / 2 + width % 2, ch = height / 2 + height % 2;
    size_t starts[3];
    for (unsigned p = 0; p < 3; ++p) {
        const auto& src = planes[p];
        const uint32_t x = p ? left / 2 : left, y = p ? top / 2 : top;
        const uint32_t w = p ? cw : width, h = p ? ch : height;
        const uint64_t rowEnd = uint64_t(x + w - 1) * src.pixelStride + 1;
        const uint64_t last = uint64_t(y + h - 1) * src.rowStride + rowEnd;
        if (!src.pixelStride || !src.rowStride || rowEnd > src.rowStride || last > src.bytes.size()) {
            error = "invalid YUV420 plane layout";
            return false;
        }
        starts[p] = size_t(y) * src.rowStride + size_t(x) * src.pixelStride;
    }
    frame.luma.resize(size_t(width) * height);
    frame.chroma.resize(size_t(cw) * 2 * ch);
    frame.width = width; frame.height = height;
    frame.lumaPitch = width; frame.chromaPitch = cw * 2;
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* row = planes[0].bytes.data() + starts[0] + size_t(y) * planes[0].rowStride;
        uint8_t* out = frame.luma.data() + size_t(y) * width;
        if (planes[0].pixelStride == 1) std::memcpy(out, row, width);
        else for (uint32_t x = 0; x < width; ++x) out[x] = row[size_t(x) * planes[0].pixelStride];
    }
    for (uint32_t y = 0; y < ch; ++y) {
        uint8_t* out = frame.chroma.data() + size_t(y) * frame.chromaPitch;
        for (unsigned p = 1; p < 3; ++p) {
            const auto& src = planes[p];
            const uint8_t* row = src.bytes.data() + starts[p] + size_t(y) * src.rowStride;
            for (uint32_t x = 0; x < cw; ++x) out[x * 2 + p - 1] = row[size_t(x) * src.pixelStride];
        }
    }
    return true;
}
}
