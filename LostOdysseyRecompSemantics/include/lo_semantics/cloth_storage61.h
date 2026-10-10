#pragma once
#include "lo_semantics/mesh_cache_lifetime61.h"
namespace lo::semantic::gpu::cloth_storage61 {
using Registers = mesh_cache_lifetime61::Registers;
using Dependencies = mesh_cache_lifetime61::Dependencies;
// CLTH cooking vectors, nested meshes and 32-bucket workspace lifetime.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::cloth_storage61
