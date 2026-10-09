#pragma once
#include "lo_semantics/diagnostic_format_routes61.h"
#include "lo_semantics/object_sort_engine61.h"
namespace lo::semantic::gpu::mesh_edge_build61 {
using Registers = object_sort_engine61::Registers;
struct Dependencies {
    object_sort_engine61::Dependencies engine;
    diagnostic_format_routes61::Dependencies diagnostics;
};
// Descriptor: +0 unique edge count,+4 endpoint pairs,+8 triangle count,
// +12 triangle-side to edge map,+16/+20 later auxiliary arrays. Input r4 is
// triangle count; r5/r6 select borrowed u32/u16 indices. Normalize endpoints,
// stable-sort twice, deduplicate and retain side mapping. Existing mapping is
// reused. BBD1E0 adds eight-byte edge records (u16 degree at +2, u32 list
// offset at +4) and flat u32 incident-triangle IDs at descriptor +20.
// Original partial-allocation ownership/failure order is unchanged.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_edge_build61
