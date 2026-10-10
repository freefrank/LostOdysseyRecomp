#pragma once
#include "lo_semantics/mesh_hull_preprocess61.h"
namespace lo::semantic::gpu::mesh_hull_incremental61 {
using Registers=mesh_hull_preprocess61::Registers;
using Dependencies=mesh_hull_preprocess61::Dependencies;
// Shared guest-table trig evaluation; caller establishes the guest FP mode.
[[nodiscard]] double EvaluateGuestTrig(GuestMemory&,double,bool cosine);
// Plain incremental hull, perturbed support/simplex selection and guest trig
// polynomial, plus vector/face topology primitives. Logical/ABI implementation.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
