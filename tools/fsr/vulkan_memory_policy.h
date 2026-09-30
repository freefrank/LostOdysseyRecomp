#pragma once

#include <cstdint>

namespace lo::fsr {

struct MemoryPropertyBits {
    uint32_t deviceLocal, hostVisible, hostCoherent, disabled;
};

// The caller supplies Vulkan's flag values and physical memory properties.
// Keep this policy independent of a loader/device so UMA, BAR and discrete
// layouts can be tested without manufacturing a Vulkan allocation.
template<class PropertiesAt>
inline uint32_t SelectMemoryType(uint32_t allowedTypes, uint32_t count,
    uint32_t required, MemoryPropertyBits bits, PropertiesAt propertiesAt) {
    uint32_t selected = UINT32_MAX;
    int selectedRank = -1;
    for (uint32_t i = 0; i < count && i < 32; ++i) {
        if (!(allowedTypes & (uint32_t(1) << i))) continue;
        const uint32_t properties = propertiesAt(i);
        if ((properties & required) != required || (properties & bits.disabled)) continue;
        int rank = 0;
        // Prefer invisible local memory on discrete GPUs, but never require
        // it: UMA or fully host-visible local heaps are legitimate fallbacks.
        if (required == bits.deviceLocal && !(properties & bits.hostVisible)) ++rank;
        if ((required & bits.hostVisible) && (properties & bits.hostCoherent)) ++rank;
        if (selected == UINT32_MAX || rank > selectedRank) {
            selected = i;
            selectedRank = rank;
        }
    }
    return selected;
}

} // namespace lo::fsr
