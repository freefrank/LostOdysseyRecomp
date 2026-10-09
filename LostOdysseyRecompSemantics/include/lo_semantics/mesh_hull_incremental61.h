#pragma once
#include "lo_semantics/mesh_hull_preprocess61.h"
namespace lo::semantic::gpu::mesh_hull_incremental61 {
using Registers=mesh_hull_preprocess61::Registers;
using Dependencies=mesh_hull_preprocess61::Dependencies;
// Incremental hull vector/face primitives, eligibility-filtered support search,
// global face selection and registration. Logical/ABI implementation.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
