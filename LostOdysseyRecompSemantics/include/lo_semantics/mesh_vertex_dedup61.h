#pragma once
#include "lo_semantics/object_sort_engine61.h"
namespace lo::semantic::gpu::mesh_vertex_dedup61 {
using Registers = object_sort_engine61::Registers;
using Dependencies = object_sort_engine61::Dependencies;
// Existing BC2D28 initializer bridge (no new mapping credit).
void Initialize(GuestMemory &, Registers &);
// BC2DD0 stable-sorts x/y/z, merges exact xyz bit matches, retains unique xyz
// at +12 and original-to-unique map at +16. +0/+4 borrow count/input; +8 count.
// Optional result r4 aliases {vertices,count,map}; it does not take ownership.
// BC2D48 and tail alias BC38E0 release only retained allocations +16/+12.
// BB8580 checks uniqueness; low-byte r5 enables in-place compaction of r4,
// updating count at r3. Duplicate input returns zero even after compaction.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_vertex_dedup61
