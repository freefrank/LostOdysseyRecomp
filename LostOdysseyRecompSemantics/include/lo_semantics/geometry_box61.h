#pragma once
#include "lo_semantics/geometry_triangle_range61.h"
namespace lo::semantic::gpu::geometry_box61 {
using Registers=geometry_triangle_range61::Registers;
using Dependencies=geometry_triangle_range61::Dependencies;
// Borrowed query r3, begin/end node addresses r4/r5. Both layouts contain a
// float center and half extents at +0/+12. Six separating-axis tests prune the
// segment box; leaf flags select the accepted triangle-range lower. The 32-byte
// layout can hold paired leaves, while the 36-byte layout has an explicit
// subtree record count. Counters and early-hit flags remain in guest memory.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
