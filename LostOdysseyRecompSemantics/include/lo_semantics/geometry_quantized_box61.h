#pragma once
#include "lo_semantics/geometry_triangle_range61.h"
namespace lo::semantic::gpu::geometry_quantized_box61 {
using Registers=geometry_triangle_range61::Registers;
using Dependencies=geometry_triangle_range61::Dependencies;
// Borrowed query r3 and begin/end r4/r5. Node centers are signed halfwords;
// extents are unsigned halfwords. Query +108..+128 supplies component scales.
// Preserve integer stack spills and double->single->scaled-single stages before
// the six-axis segment/box test. Layouts are 20-byte paired leaves and 24-byte
// nodes with subtree counts. Accepted triangle handling stays a live lower call.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
