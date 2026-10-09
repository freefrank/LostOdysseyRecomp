#pragma once
#include "lo_semantics/mesh_indexed_workspace61.h"
namespace lo::semantic::gpu::mesh_indexed_channels61 {
using Registers = mesh_indexed_workspace61::Registers;
using Dependencies = mesh_indexed_workspace61::Dependencies;
// BB3C00 appends xyz to a word buffer. BBE948 splits zero-smoothing faces into
// private position IDs when enabled. BBEBE0 deduplicates corner channel tuples,
// rewrites face corner IDs and replaces the owned tuple array.
// BBF208 appends selected output channels; the first attribute uses packed
// two or three components according to option +281.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_indexed_channels61
