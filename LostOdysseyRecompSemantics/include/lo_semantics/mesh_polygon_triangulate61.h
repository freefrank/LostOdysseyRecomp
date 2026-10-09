#pragma once
#include "lo_semantics/mesh_auxiliary_storage61.h"
namespace lo::semantic::gpu::mesh_polygon_triangulate61 {
using Registers = mesh_auxiliary_storage61::Registers;
using Dependencies = mesh_auxiliary_storage61::Dependencies;
// Adapter +4 borrows a mesh with 36-byte polygon records at +40 (count +36).
// Replace owned u32 triangles at mesh +8 with polygon fans, then orient each
// away from the surface-area centroid. Byte polygon indices remain borrowed.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_polygon_triangulate61
