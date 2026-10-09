#pragma once
#include "lo_semantics/mesh_hull_incremental61.h"
namespace lo::semantic::gpu::mesh_hull_polyhedron61 {
using Registers = mesh_hull_incremental61::Registers;
using Dependencies = mesh_hull_incremental61::Dependencies;
// Compact convex polyhedron: xyz array, half-edge array and plane array.
// Logical/ABI recovery for construction, validity, plane selection and lifetime.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
}
