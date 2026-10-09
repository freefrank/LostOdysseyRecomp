#pragma once
#include "lo_semantics/geometry_unbounded_range61.h"
namespace lo::semantic::gpu::geometry_quantized_unbounded61 {
using Registers=geometry_unbounded_range61::Registers;
using VectorState=geometry_unbounded_range61::VectorState;
using Vector=geometry_unbounded_range61::Vector;
using Dependencies=geometry_unbounded_range61::Dependencies;
using GuestServices=geometry_unbounded_range61::GuestServices;
// BD6FD0: r3 query, r4 first packed 24-byte node, r5 exclusive end. Center
// halfwords +0/+2/+4 are signed; extents +6/+8/+10 unsigned. Scale each axis
// using query +108/+112/+116 and +120/+124/+128 respectively. +12 contains
// leaf/type bits; +20 subtree skip count. Expanded center/extents are written
// into guest stack scratch, then the unbounded ray/box test visits or skips.
// Borrowed ownership, saved VMX125..127, live callback vectors and lower
// BD4448 boundaries match the unquantized route; global Full72 is unchanged.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,
    Dependencies deps,Registers& scalar,VectorState& vectors);
}
