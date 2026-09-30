#pragma once

#include <cstdint>

namespace gpu::scene_copy_promotion {

// Called after a guest draw has already uploaded its constants and indices.
// A failed reservation must bypass promotion, never rotate the GPU slot.
inline constexpr bool CanAppendConstants(uint64_t offset, uint64_t capacity,
    uint64_t bytes, uint32_t count = 2, uint64_t alignment = 256) {
    if (!alignment || (alignment & (alignment - 1)) || !bytes || offset > capacity) return false;
    for (uint32_t i = 0; i < count; ++i) {
        const uint64_t padding = (alignment - (offset & (alignment - 1))) & (alignment - 1);
        if (padding > capacity - offset) return false;
        offset += padding;
        if (bytes > capacity - offset) return false;
        offset += bytes;
    }
    return true;
}

// Check before borrowing either framebuffer attachment. A promoted color
// attachment cannot be paired with the parked low-resolution depth grid.
inline constexpr bool MustRestore(bool sameStorage, bool sameFrame, bool sameEpoch,
    bool needsDepthStencil, uint32_t requestedGuestHeight, uint32_t promotedGuestHeight) {
    return !sameStorage || !sameFrame || !sameEpoch || needsDepthStencil ||
        requestedGuestHeight > promotedGuestHeight;
}

} // namespace gpu::scene_copy_promotion
