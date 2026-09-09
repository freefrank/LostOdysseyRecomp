#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace gpu::geometry_prepare
{
    // Preserve the old large-buffer sampling coverage, using exact comparisons
    // instead of serial hash arithmetic. Small buffers include trailing bytes.
    class SampledContent
    {
        size_t sourceSize = 0;
        std::vector<uint8_t> samples;
        template<class Visitor> static bool Visit(size_t bytes, Visitor visitor)
        {
            if (bytes <= 8192) return visitor(0, bytes);
            if (!visitor(0, 512) || !visitor(bytes - 512, 512)) return false;
            const size_t step = (bytes - 1024) / 64;
            for (size_t i = 0; i < 64; ++i)
                if (!visitor(512 + i * step, 64)) return false;
            return true;
        }
    public:
        bool Matches(const uint8_t* data, size_t bytes) const
        {
            if (sourceSize != bytes || samples.size() != (bytes <= 8192 ? bytes : 5120)) return false;
            size_t position = 0;
            return Visit(bytes, [&](size_t offset, size_t count) {
                const bool equal = !count || !std::memcmp(data + offset, samples.data() + position, count);
                position += count;
                return equal;
            });
        }
        void Capture(const uint8_t* data, size_t bytes)
        {
            sourceSize = bytes;
            samples.resize(bytes <= 8192 ? bytes : 5120);
            size_t position = 0;
            Visit(bytes, [&](size_t offset, size_t count) {
                if (count) std::memcpy(samples.data() + position, data + offset, count);
                position += count;
                return true;
            });
        }
    };

    template<bool Wide, unsigned Endian>
    inline void Convert(const uint8_t* src, uint32_t* dst, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t v;
            if constexpr (Wide) std::memcpy(&v, src + size_t(i) * 4, 4);
            else { uint16_t narrow; std::memcpy(&narrow, src + size_t(i) * 2, 2); v = narrow; }
            if constexpr (Endian == 1) v = ((v & 0xFF00FF00u) >> 8) | ((v & 0x00FF00FFu) << 8);
            if constexpr (Endian == 2) v = (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
            if constexpr (Endian == 3) v = (v >> 16) | (v << 16);
            if constexpr (!Wide) v &= 0xFFFF;
            dst[i] = v;
        }
    }
    inline void ConvertIndices(const uint8_t* src, uint32_t* dst, uint32_t count, bool wide, uint32_t endian)
    {
        // Dispatch once, allowing each loop to vectorize without per-index branches.
        switch ((wide ? 4 : 0) | (endian & 3))
        {
        case 0: return Convert<false, 0>(src, dst, count);
        case 1: return Convert<false, 1>(src, dst, count);
        case 2: return Convert<false, 2>(src, dst, count);
        case 3: return Convert<false, 3>(src, dst, count);
        case 4: return Convert<true, 0>(src, dst, count);
        case 5: return Convert<true, 1>(src, dst, count);
        case 6: return Convert<true, 2>(src, dst, count);
        case 7: return Convert<true, 3>(src, dst, count);
        }
    }
}
