#pragma once
#include "lo_semantics/mesh_indexed_workspace61.h"
namespace lo::semantic::gpu::mesh_indexed_remap61 {
using Registers = mesh_indexed_workspace61::Registers;
using Dependencies = mesh_indexed_workspace61::Dependencies;
// BBF3F8 maps a corner tuple once and appends {position,attribute1,attribute2,
// smoothing}. BBFEA8 owns a temporary sentinel map for a face batch, rewrites
// final face indices and appends {face count,new vertex count} batch metadata.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_indexed_remap61
