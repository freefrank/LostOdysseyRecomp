#pragma once
#include "lo_semantics/mesh_polygon_build61.h"
namespace lo::semantic::gpu::mesh_polygon_topology61 {
using Registers = mesh_polygon_build61::Registers;
using Dependencies = mesh_polygon_build61::Dependencies;
// BBB728 finalizes polygon topology: unique byte endpoint pairs, polygon u16
// edge IDs, edge-to-polygon incidence records/bytes, and normalized sums of
// adjacent face normals. Existing partial-allocation behavior is retained.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_polygon_topology61
