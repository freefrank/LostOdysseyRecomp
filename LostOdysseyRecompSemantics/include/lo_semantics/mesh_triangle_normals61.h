#pragma once
#include "lo_semantics/mesh_cache_build61.h"
namespace lo::semantic::gpu::mesh_triangle_normals61 {
using Registers = mesh_cache_build61::Registers;
using Dependencies = mesh_cache_build61::Dependencies;
// Guest atan2 corner angles and angle-weighted normals with original fallback.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_triangle_normals61
