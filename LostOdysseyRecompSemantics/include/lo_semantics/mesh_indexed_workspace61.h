#pragma once
#include "lo_semantics/object_sort_engine61.h"
namespace lo::semantic::gpu::mesh_indexed_workspace61 {
using Registers = object_sort_engine61::Registers;
using Dependencies = object_sort_engine61::Dependencies;
// Indexed-mesh workspace: thirteen 16-byte buffers, copied xyz channels,
// face-capacity arrays, and option bytes. Reset/destroy preserve the original
// allocation ownership and partial-failure behavior; inputs remain borrowed.
// BBE310 inserts a face and three per-corner channel tuples; optional exact
// degeneracy filtering skips without consuming capacity. Missing IDs stay -1;
// supplied out-of-range IDs are clamped to zero, preserving guest semantics.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_indexed_workspace61
