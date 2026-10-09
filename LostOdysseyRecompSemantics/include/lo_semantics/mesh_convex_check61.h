#pragma once
#include "lo_semantics/geometry_primitives61.h"
namespace lo::semantic::gpu::mesh_convex_check61 {
using Registers = geometry_primitives61::Registers;
// BB86C8 averages the vertices and checks every triangle against that interior
// point, optionally reversing inconsistent winding while reporting it.
// Indexed triangles: reverse indices 1/2, detect repeated indices, and test
// point sidedness with the original scalar triple product. BB8E88 checks that
// vertices outside each polygon's own byte-index list lie behind its plane.
// Geometry is borrowed. Preserve binary32 stages and original tolerance.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_convex_check61
