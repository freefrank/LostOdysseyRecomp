#pragma once
#include <cstdint>

namespace gpu::fsr_alpha {

enum class Coverage : uint8_t { Unavailable, PartialCoverage };

// Only the six PS/VS combinations whose output alpha was audited in f5446.
// This is deliberately independent of the much broader jitter VS whitelist.
inline constexpr bool AuditedPair(uint64_t vs, uint64_t ps) {
    if (vs == 0x22557143e0f243ddull)
        return ps == 0x549c25501ca08530ull || ps == 0x62674247668b2cfbull ||
               ps == 0x69865680ed1eff11ull;
    if (vs == 0x4c87bb5b986defc8ull)
        return ps == 0x819898a6ae24db7bull || ps == 0x8ba62e23bbf0febeull ||
               ps == 0x670b45e31df82273ull;
    return false;
}

// First collection slice: require the audited transparent raster state. A
// source that changes its blend/depth semantics needs a new capture and audit.
inline constexpr bool AuditedState(uint32_t blend, uint32_t depthControl,
    uint32_t depthInfo, uint32_t colorMask, uint32_t primitive) {
    // The low twelve bits are an EDRAM address, not depth semantics. Renderer
    // separately ties the actual allocation to the scene depth/jitter anchor.
    return blend == 0x01000106u && depthControl == 0x00700762u &&
        (depthInfo & ~0xFFFu) == 0x00010000u && colorMask == 0xFu && primitive == 13u;
}

static_assert(AuditedPair(0x22557143e0f243ddull, 0x549c25501ca08530ull));
static_assert(AuditedPair(0x22557143e0f243ddull, 0x62674247668b2cfbull));
static_assert(AuditedPair(0x22557143e0f243ddull, 0x69865680ed1eff11ull));
static_assert(AuditedPair(0x4c87bb5b986defc8ull, 0x819898a6ae24db7bull));
static_assert(AuditedPair(0x4c87bb5b986defc8ull, 0x8ba62e23bbf0febeull));
static_assert(AuditedPair(0x4c87bb5b986defc8ull, 0x670b45e31df82273ull));
static_assert(!AuditedPair(0x0eb223d33f8e8e0cull, 0x463f83252b104180ull));
static_assert(!AuditedState(0x01000106u, 0x00700766u, 0x00010000u, 0xFu, 13u));

} // namespace gpu::fsr_alpha
