#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <xxhash.h>

namespace gpu::texture_cache
{
    // Phys aliases addresses into the 512 MiB guest physical mapping. Validate
    // the entire extent before decoding or hashing; a span may not wrap it.
    constexpr bool PhysicalRangeValid(uint32_t address, uint64_t bytes)
    {
        constexpr uint64_t physicalBytes = 0x20000000ull;
        return bytes <= physicalBytes - (address & 0x1FFFFFFFu);
    }

    inline uint64_t ContentHash(const void* data, size_t bytes)
    {
        // XXH3 reads every byte, including short tails. Its vectorized scan
        // avoids an extra texture-sized snapshot allocation on every upload.
        return XXH3_64bits(data, bytes);
    }

    // Cheap per-frame check: every byte of small extents, otherwise the head,
    // the tail and 64 evenly spread 64-byte windows. The inline multiply-xor
    // loop is deliberate: ~20k short XXH3 calls per frame cost as much as a
    // full scan of all active textures. Four independent lanes hide the
    // multiply latency; each step is invertible, so any changed byte changes
    // its lane and the result.
    inline uint64_t SampledContentHash(const void* data, size_t bytes)
    {
        const auto* p = static_cast<const uint8_t*>(data);
        uint64_t lane[4] = {0x9E3779B97F4A7C15ull ^ bytes, 0xC2B2AE3D27D4EB4Full ^ bytes,
            0x165667B19E3779F9ull ^ bytes, 0x27D4EB2F165667C5ull ^ bytes};
        auto step = [](uint64_t& h, uint64_t v)
        {
            h = (h ^ v) * 0x100000001B3ull;
            h ^= h >> 29;
        };
        auto mix = [&](const uint8_t* q, size_t n)
        {
            size_t i = 0;
            for (; i + 32 <= n; i += 32)
                for (size_t k = 0; k < 4; ++k)
                {
                    uint64_t v;
                    memcpy(&v, q + i + k * 8, 8);
                    step(lane[k], v);
                }
            for (size_t k = 0; i + 8 <= n; i += 8, ++k)
            {
                uint64_t v;
                memcpy(&v, q + i, 8);
                step(lane[k], v);
            }
            if (i < n)
            {
                uint64_t v = 0;
                memcpy(&v, q + i, n - i);
                step(lane[3], v);
            }
        };
        if (bytes <= 8192)
            mix(p, bytes);
        else
        {
            mix(p, 512);
            mix(p + bytes - 512, 512);
            const size_t stride = (bytes - 1024) / 64;
            for (size_t i = 0; i < 64; i++)
                mix(p + 512 + i * stride, 64);
        }
        uint64_t h = lane[0];
        for (size_t k = 1; k < 4; ++k) step(h, lane[k]);
        return h;
    }

    inline uint64_t GuestContentHash(const void* base, size_t baseBytes,
        const void* mips, size_t mipBytes)
    {
        const uint64_t hash = ContentHash(base, baseBytes);
        return mipBytes ? hash ^ ContentHash(mips, mipBytes) * 0x9E3779B97F4A7C15ull : hash;
    }

    inline uint64_t SampledGuestContentHash(const void* base, size_t baseBytes,
        const void* mips, size_t mipBytes)
    {
        const uint64_t hash = SampledContentHash(base, baseBytes);
        return mipBytes ? hash ^ SampledContentHash(mips, mipBytes) * 0x9E3779B97F4A7C15ull : hash;
    }

    // Fully scanning every active texture each frame (173 textures, 24 MB in
    // the 4K Uhra benchmark) added about 0.4-0.6 ms per frame. Sampled windows
    // are checked every frame; each texture's full extent once per interval,
    // so a write outside the windows is picked up within kFullScanInterval
    // frames. Textures uploaded together start at address-derived phases
    // instead of all scanning on one frame.
    constexpr uint64_t kFullScanInterval = 16;

    constexpr uint64_t FirstFullScanFrame(uint64_t uploadFrame, uint32_t address)
    {
        return uploadFrame + 1 + (uint32_t((address >> 12) * 0x9E3779B1u) >> 16) % kFullScanInterval;
    }
}
