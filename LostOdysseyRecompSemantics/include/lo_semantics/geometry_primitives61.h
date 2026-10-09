#pragma once
#include "lo_semantics/tree_box_centroid61.h"
namespace lo::semantic::gpu::geometry_primitives61 {
using Registers = tree_box_centroid61::Registers;
// DF08 expands borrowed min/max box r3 to a centered cube at r4, returning
// largest half extent in f1. DFC8 tests whether box r4 contains box r3.
// E040 exports eight xyz corners to r4 (null output returns false).
// E108 swaps triangle vertices1/2 in place; E140 returns triangle area in f1.
// E1B8 moves each vertex away from the original centroid by f1 times its
// centroid-relative vector, normalized first when r5 low byte is nonzero.
// Boxes use six floats, triangles nine floats. Memory remains borrowed and
// FP rounding/comparison stages and observed scratch state remain explicit.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::geometry_primitives61
