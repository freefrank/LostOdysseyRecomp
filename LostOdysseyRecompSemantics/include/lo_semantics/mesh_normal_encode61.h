#pragma once
#include "lo_semantics/mesh_geometry_math61.h"
namespace lo::semantic::gpu::mesh_normal_encode61 {
using Registers = mesh_geometry_math61::Registers;
// Guest-table asin supports original normal encoding: sign/order masks at
// r7/r6, two quantized angles at r8/r9, precision from low byte of r10.
// Preserve the original absolute-component ordering and binary32 stages.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_normal_encode61
