#pragma once

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace gpu::texture_budget
{
    // Above the soft limit, only textures no draw used for about 30 seconds at
    // 60 FPS leave, which removes what earlier maps left behind. Above the hard
    // limit, any texture idle for about a second leaves. A texture in use always
    // stays: a scene that needs more than the limit goes over it and does not
    // reload its own textures every frame.
    constexpr uint64_t kSoftIdleFrames = 1800;
    constexpr uint64_t kHardIdleFrames = 60;

    // hardBytes 0 turns the budget off.
    struct Limits
    {
        uint64_t softBytes = 0, hardBytes = 0;
    };

    constexpr Limits LimitsForHardBytes(uint64_t hardBytes)
    {
        return {hardBytes / 4 * 3, hardBytes};
    }

    // Half of the video memory; render targets and buffers need the rest.
    // limitMib (LO_TEXTURE_CACHE_LIMIT_MIB) replaces the hard limit, and 0 turns
    // the budget off. A device that reports no video memory has no budget.
    inline Limits LimitsFor(uint64_t videoMemoryBytes, const char* limitMib)
    {
        if (limitMib && *limitMib) {
            uint64_t mib = 0;
            const char* end = limitMib + std::strlen(limitMib);
            const auto parsed = std::from_chars(limitMib, end, mib);
            if (parsed.ec == std::errc{} && parsed.ptr == end && mib <= (uint64_t(1) << 40))
                return LimitsForHardBytes(mib << 20);
        }
        return LimitsForHardBytes(videoMemoryBytes / 2);
    }

    template <class Handle>
    struct Candidate
    {
        Handle handle;
        uint64_t lastUsedFrame = 0, bytes = 0;
    };

    // Sorts the candidates oldest first and returns how many of them, from the
    // front, leave so the cache of totalBytes moves toward the soft limit.
    template <class Handle>
    size_t SelectEvictions(std::vector<Candidate<Handle>>& candidates, uint64_t totalBytes, const Limits& limits, uint64_t frame)
    {
        if (!limits.hardBytes || totalBytes <= limits.softBytes) return 0;
        std::sort(candidates.begin(), candidates.end(),
            [](const Candidate<Handle>& a, const Candidate<Handle>& b) { return a.lastUsedFrame < b.lastUsedFrame; });
        size_t count = 0;
        for (const auto& candidate : candidates) {
            const uint64_t idle = frame > candidate.lastUsedFrame ? frame - candidate.lastUsedFrame : 0;
            if (totalBytes <= limits.softBytes ||
                idle < (totalBytes > limits.hardBytes ? kHardIdleFrames : kSoftIdleFrames)) break;
            totalBytes -= std::min(totalBytes, candidate.bytes);
            ++count;
        }
        return count;
    }
}
