#pragma once
#include "lo_semantics/mesh_cache_lifetime61.h"
#include "lo_semantics/mesh_edge_build61.h"
namespace lo::semantic::gpu::mesh_cache_build61 {
using Registers = mesh_edge_build61::Registers;
struct Dependencies {
    mesh_edge_build61::Dependencies edge;
    mesh_cache_lifetime61::Dependencies lifetime;
};
// Both dependency groups borrow the same guest allocator and FP state.
// BBDDF0 composes requested topology/angle work then drops unretained arrays.
// BBC9F0 derives vertex degrees and optional byte adjacency from unique edges.
// BB3130 lazily owns the cache descriptor and publishes a borrowed +4 view into
// the source at +84. Partial failure ownership follows the original sequence.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_cache_build61
