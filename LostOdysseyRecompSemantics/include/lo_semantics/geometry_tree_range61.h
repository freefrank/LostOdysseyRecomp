#pragma once
#include "lo_semantics/geometry_unbounded_range61.h"
namespace lo::semantic::gpu::geometry_tree_range61 {
using Registers=geometry_unbounded_range61::Registers;
using Vector=geometry_unbounded_range61::Vector;
using VectorState=geometry_unbounded_range61::VectorState;
using GuestServices=geometry_unbounded_range61::GuestServices;
using Dependencies=geometry_unbounded_range61::Dependencies;
// BD6C58: r3 borrowed ray query, r4 first 32-byte node, r5 exclusive end.
// Node +0 center,+12 half extent,+24 leaf/type bits,+28 subtree skip count.
// A rejected internal node skips its subtree; accepted bit31 leaves invoke
// BD4448, and bit30 also invokes the next leaf index unless stop-on-hit applies.
// Query+96 counts tested nodes. The shared local VectorState retains raw VMX
// lanes without changing Full72. Save/restore GPR28..31 and vector125..127;
// all other vector/scalar state stays live at the reused scalar lower boundary.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&,VectorState&);
}
