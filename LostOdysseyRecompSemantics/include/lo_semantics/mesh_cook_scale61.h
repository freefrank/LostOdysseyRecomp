#pragma once
#include "lo_semantics/mesh_cook_tree61.h"
namespace lo::semantic::gpu::mesh_cook_scale61 {
using Registers=mesh_cook_tree61::Registers;
using Dependencies=mesh_cook_tree61::Dependencies;
// Cooked geometry uniform scaling, derived bounds/inertia and tree refresh.
// Logical/ABI implementation; virtual tree callback remains explicit.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
