#pragma once
#include "lo_semantics/mesh_indexed_workspace61.h"
namespace lo::semantic::gpu::mesh_indexed_compact61 {
using Registers = mesh_indexed_workspace61::Registers;
using Dependencies = mesh_indexed_workspace61::Dependencies;
// BBF7F0 removes unused channel values, merges exact xyz values, remaps corner
// IDs and removes newly degenerate faces for the position channel. BC0058
// sequences position/attribute channels, honoring position-preservation +289.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_indexed_compact61
