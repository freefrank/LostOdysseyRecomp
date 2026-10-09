#pragma once
#include "lo_semantics/mesh_cache_build61.h"
namespace lo::semantic::gpu::mesh_triangle_links61 {
using Registers = mesh_edge_build61::Registers;
using Dependencies = mesh_cache_build61::Dependencies;
// Three packed links per triangle: low29 neighbor ID, top2 neighbor edge slot;
// low29 all-ones denotes an open boundary. Side order is (0,1),(0,2),(1,2).
// BC3CB0 groups stable-sorted edges and links manifold pairs; BC3F20 builds
// the owned array and optionally propagates geometric boundary/angle flags.
// Owned link payload follows a four-byte allocation prefix; release uses p-4.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_triangle_links61
