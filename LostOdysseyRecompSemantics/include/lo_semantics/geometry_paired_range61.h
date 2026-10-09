#pragma once
#include "lo_semantics/geometry_unbounded_range61.h"
namespace lo::semantic::gpu::geometry_paired_range61 {
using Registers=geometry_unbounded_range61::Registers;
using Vector=geometry_unbounded_range61::Vector;
using VectorState=geometry_unbounded_range61::VectorState;
using Dependencies=geometry_unbounded_range61::Dependencies;
// BD5910: quantized 20-byte paired nodes; borrowed VMX state and mutable
// scalar/vector guest lower boundary. Saves v119..127 below caller SP-64.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory&,Dependencies,Registers&,VectorState&);
}
