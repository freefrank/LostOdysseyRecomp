#pragma once
#include "lo_semantics/geometry_primitives61.h"
namespace lo::semantic::gpu::mesh_geometry_math61 {
using Registers = geometry_primitives61::Registers;
// BD92C0 derives a normalized triangle plane at r3 from points r4/r5/r6.
// 2DA388 evaluates the original guest-table rational atan2 approximation for
// f1/f2, preserving signed-zero handling and caller scratch stores. Constants
// remain in guest memory; do not substitute host atan2 or new degeneracy policy.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_geometry_math61
