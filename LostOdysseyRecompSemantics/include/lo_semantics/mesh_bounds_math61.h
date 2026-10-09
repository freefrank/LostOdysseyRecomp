#pragma once
#include "lo_semantics/geometry_primitives61.h"
namespace lo::semantic::gpu::mesh_bounds_math61 {
using Registers = geometry_primitives61::Registers;
// BC9580/BC9600/BC9780 construct spheres through two/three/four points,
// preserving the guest radius epsilon and circumcenter arithmetic stages.
// BC9040 uses six axis-extreme points to seed a sphere, then expands it in
// input order to enclose every packed xyz point. Output is center + radius.
// Binary32 arithmetic stages, four-point scan batches, red-zone FPR saves and
// guest scratch remain explicit; inputs and output are borrowed.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_bounds_math61
