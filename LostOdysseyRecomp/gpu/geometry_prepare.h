#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#if defined(__SSE2__)
#include <emmintrin.h>
#endif
#if defined(__SSSE3__)
#include <tmmintrin.h>
#endif

namespace gpu::geometry_prepare
{
    // Shared QuadList expansion used by the guest draw and its motion replay.
    inline void ExpandQuadList(std::vector<uint32_t>& indices, std::vector<uint32_t>& primitiveScratch,
        bool& useIndices, uint32_t indexCount)
    {
        auto& out = primitiveScratch;
        out.clear();
        uint32_t quads = (useIndices ? uint32_t(indices.size()) : indexCount) / 4;
        out.reserve(quads * 6);
        for (uint32_t q = 0; q < quads; q++)
        {
            uint32_t v[4];
            for (int k = 0; k < 4; k++) v[k] = useIndices ? indices[q * 4 + k] : q * 4 + k;
            out.insert(out.end(), { v[0], v[1], v[2], v[0], v[2], v[3] });
        }
        indices.swap(out);
        useIndices = true;
    }

    template<unsigned Endian>
    inline void CopyDwordsSwappedImpl(uint8_t* dst, const uint8_t* src, size_t dwords)
    {
#if defined(__SSSE3__)
        // The runtime targets Sandy Bridge. Unaligned loads/stores also support
        // guest buffers and upload offsets without introducing destination reads.
        const __m128i order = Endian == 1
            ? _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14)
            : Endian == 2
            ? _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12)
            : _mm_setr_epi8(2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13);
        while (dwords >= 4)
        {
            const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), _mm_shuffle_epi8(value, order));
            src += 16;
            dst += 16;
            dwords -= 4;
        }
#endif
        while (dwords--)
        {
            uint32_t value;
            std::memcpy(&value, src, 4);
            if constexpr (Endian == 1) value = ((value & 0xFF00FF00u) >> 8) | ((value & 0x00FF00FFu) << 8);
            if constexpr (Endian == 2) value = (value >> 24) | ((value >> 8) & 0xFF00u) | ((value << 8) & 0xFF0000u) | (value << 24);
            if constexpr (Endian == 3) value = (value >> 16) | (value << 16);
            std::memcpy(dst, &value, 4);
            src += 4;
            dst += 4;
        }
    }

    // Source and destination must not overlap. Upload heaps are write-combined:
    // swap on the way in, never read the destination, and never touch tail bytes.
    inline void CopyDwordsSwapped(void* dst, const void* src, size_t dwords, uint32_t endian)
    {
        if (!dwords) return;
        switch (endian & 3)
        {
        case 0: std::memcpy(dst, src, dwords * 4); return;
        case 1: return CopyDwordsSwappedImpl<1>(static_cast<uint8_t*>(dst), static_cast<const uint8_t*>(src), dwords);
        case 2: return CopyDwordsSwappedImpl<2>(static_cast<uint8_t*>(dst), static_cast<const uint8_t*>(src), dwords);
        case 3: return CopyDwordsSwappedImpl<3>(static_cast<uint8_t*>(dst), static_cast<const uint8_t*>(src), dwords);
        }
    }

    // Read exactly one sample block with unaligned SIMD loads. An explicit
    // comparison avoids compiler-dependent expansion of constant-size memcmp.
    inline bool EqualSampleBlock64(const uint8_t* left, const uint8_t* right)
    {
#if defined(__SSE2__)
        __m128i equal = _mm_cmpeq_epi8(
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(left)),
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(right)));
        equal = _mm_and_si128(equal, _mm_cmpeq_epi8(
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(left + 16)),
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(right + 16))));
        equal = _mm_and_si128(equal, _mm_cmpeq_epi8(
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(left + 32)),
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(right + 32))));
        equal = _mm_and_si128(equal, _mm_cmpeq_epi8(
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(left + 48)),
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(right + 48))));
        return _mm_movemask_epi8(equal) == 0xFFFF;
#else
        return std::memcmp(left, right, 64) == 0;
#endif
    }

    // Guest PPC stores are not all instrumented with a write generation yet.
    // A cache hit must therefore compare every source byte against the owned
    // CPU snapshot. Never read the write-combined GPU upload heap.
    class ExactContent
    {
    protected:
        std::vector<uint8_t> snapshot;
        bool captured = false;
    public:
        size_t Size() const { return snapshot.size(); }
        size_t AllocatedBytes() const { return snapshot.capacity(); }
        bool Matches(const uint8_t* data, size_t bytes) const
        {
            if (!captured || snapshot.size() != bytes) return false;
            if (!bytes) return true;
            return std::memcmp(data, snapshot.data(), bytes) == 0;
        }
        void Capture(const uint8_t* data, size_t bytes)
        {
            snapshot.resize(bytes);
            if (bytes) std::memcpy(snapshot.data(), data, bytes);
            captured = true;
        }
    };
    // Performance-first vertex policy: large buffers retain the v0.5.8 sample
    // coverage. Unsampled writes may be missed; tracked in the roadmap (#57).
    // Index caches must keep ExactContent, including motion-index hash reuse.
    class VertexSampledContent : public ExactContent
    {
    public:
        bool Matches(const uint8_t* data, size_t bytes) const
        {
            if (!captured || snapshot.size() != bytes) return false;
            if (bytes <= 8192) return ExactContent::Matches(data, bytes);
            if (std::memcmp(data, snapshot.data(), 512) != 0 ||
                std::memcmp(data + bytes - 512, snapshot.data() + bytes - 512, 512) != 0)
                return false;
            const size_t step = (bytes - 1024) / 64;
            for (size_t i = 0; i < 64; ++i)
            {
                const size_t offset = 512 + i * step;
                if (!EqualSampleBlock64(data + offset, snapshot.data() + offset)) return false;
            }
            return true;
        }
    };
    // Source compatibility for diagnostics that used the old helper name.
    using SampledContent = ExactContent;

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
