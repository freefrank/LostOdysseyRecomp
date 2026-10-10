#pragma once
#include "lo_semantics/mesh_partition_support61.h"
namespace lo::semantic::gpu::mesh_partition61 {
using Registers = mesh_partition_support61::Registers;
using Dependencies = mesh_partition_support61::Dependencies;
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_partition61
