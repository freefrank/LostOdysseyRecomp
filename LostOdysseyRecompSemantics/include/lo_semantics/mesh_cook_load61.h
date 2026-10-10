#pragma once
#include "lo_semantics/mesh_cook_stream61.h"
namespace lo::semantic::gpu::mesh_cook_load61 {
using Registers = mesh_cook_stream61::Registers;
using Dependencies = mesh_cook_stream61::Dependencies;
// Cooked geometry/tree load and load-scale-export composition. Logical/ABI
// scope.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_cook_load61
