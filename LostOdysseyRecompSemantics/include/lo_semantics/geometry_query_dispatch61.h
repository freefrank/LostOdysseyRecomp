#pragma once
#include "lo_semantics/geometry_unbounded_range61.h"
namespace lo::semantic::gpu::geometry_query_dispatch61 {
using Registers=geometry_unbounded_range61::Registers;
using VectorState=geometry_unbounded_range61::VectorState;
using Dependencies=geometry_unbounded_range61::Dependencies;
// BD7258: borrowed query r3, ray r4, tree r5, optional transform r6 and cached
// triangle r7. Install tree/mesh links and flag16, then prepare the query. A null
// tree/mesh returns0; all completed queries return1, independently of hit flags.
// Tree flag4 scans every mesh triangle. Otherwise bits1/0 select paired/quantized
// node layouts; distance-limit bits exactly 0x7f7fffff select unbounded VMX
// routes, other limits select scalar segment routes. All eight lowers are real
// semantic implementations, with full borrowed VMX state at dynamic callbacks.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,
    Dependencies deps,Registers& scalar,VectorState& vectors);
}
