#pragma once
#include "lo_semantics/mesh_triangle_links61.h"
namespace lo::semantic::gpu::mesh_polygon_collect61 {
using Registers = mesh_triangle_links61::Registers;
using Dependencies = mesh_triangle_links61::Dependencies;
// BB9318 groups a closed triangle mesh across untagged links, orders each
// component's boundary, appends count-prefixed polygon vertex IDs and optional
// component triangle IDs. r3 is polygon count, r4/r6 are word-array outputs,
// r5 borrows the mesh adapter. B7E504 probes ordinary guest stack pages.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_polygon_collect61
