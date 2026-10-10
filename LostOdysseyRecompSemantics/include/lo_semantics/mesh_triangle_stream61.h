#pragma once
#include "lo_semantics/mesh_cache_build61.h"
namespace lo::semantic::gpu::mesh_triangle_stream61 {
using Registers = mesh_cache_build61::Registers;
using Dependencies = mesh_cache_build61::Dependencies;
// NXS/MESH owner serialization and scalar-callback adaptive index input.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_triangle_stream61
