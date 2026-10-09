#pragma once
#include "lo_semantics/mesh_auxiliary_storage61.h"
namespace lo::semantic::gpu::mesh_cache_lifetime61 {
using Registers = mesh_auxiliary_storage61::Registers;
using Dependencies = mesh_auxiliary_storage61::Dependencies;
// Borrowed mesh adapter at +4/+12 owns only optional valence cache +16.
// Cache cleanup releases its arrays before its descriptor, resets +16, then
// restores the base adapter table. BC7F48 computes wrapping u16 degree offsets;
// it retains the original nonempty-array precondition.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_cache_lifetime61
