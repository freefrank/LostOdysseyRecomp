#pragma once
#include "lo_semantics/mesh_cache_build61.h"
namespace lo::semantic::gpu::mesh_triangle_tree61 {
using Registers = mesh_cache_build61::Registers;
using Dependencies = mesh_cache_build61::Dependencies;
// Triangle tree build/load and coupled triangle/material/remap reordering.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_triangle_tree61
