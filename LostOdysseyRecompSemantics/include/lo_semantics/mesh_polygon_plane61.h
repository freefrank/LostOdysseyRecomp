#pragma once
#include "lo_semantics/mesh_geometry_math61.h"
namespace lo::semantic::gpu::mesh_polygon_plane61 {
using Registers = mesh_geometry_math61::Registers;
// BD9390: Newell polygon plane from byte indices r5, count r4, xyz positions r6,
// output r3. Plane offset uses arithmetic mean of vertices. Retains four-edge
// accumulation order and scalar tail. BC3880 reverses borrowed byte indices.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_polygon_plane61
