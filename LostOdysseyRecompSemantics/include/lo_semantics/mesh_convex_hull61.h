#pragma once
#include "lo_semantics/mesh_polygon_build61.h"
namespace lo::semantic::gpu::mesh_convex_hull61 {
using Registers = mesh_polygon_build61::Registers;
using Dependencies = mesh_polygon_build61::Dependencies;
// BBA028 constructs the convex hull from a borrowed point descriptor at r4.
// r3 is the mesh adapter; its +4 mesh retains compacted points and triangles.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_convex_hull61
