#pragma once
#include "lo_semantics/mesh_geometry_math61.h"
namespace lo::semantic::gpu::mesh_mass_math61 {
using Registers = mesh_geometry_math61::Registers;
// BCD300 constructs a binary64 plane from strided binary32 mesh positions.
// BCCCA8 scales integrated moments by density and applies the parallel-axis
// correction to a symmetric 3x3 inertia tensor. Preserve binary32 centroid
// stages and borrowed descriptor/output storage for the later Rust boundary.
// BCCE48/BCD0F8 integrate projected triangle monomials and lift them to face
// moments. BCD400 accumulates signed volume, centroid and origin/center inertia;
// BCD8A8 adapts the seven-word mesh descriptor and density to that pipeline.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_mass_math61
