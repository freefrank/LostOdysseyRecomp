#pragma once
#include "lo_semantics/mesh_cache_build61.h"
namespace lo::semantic::gpu::mesh_partition_support61 {
using Registers = mesh_cache_build61::Registers;
using Dependencies = mesh_cache_build61::Dependencies;
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_partition_support61
