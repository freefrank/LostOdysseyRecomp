#pragma once
#include "lo_semantics/tree_mesh_lifetime61.h"
namespace lo::semantic::gpu::tree_envelope_load61 {
using Registers = tree_mesh_lifetime61::Registers;
using Dependencies = tree_mesh_lifetime61::Dependencies;
// OPC strategy plus HBM mapping input. Live strategy callbacks retain
// ownership.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::tree_envelope_load61
