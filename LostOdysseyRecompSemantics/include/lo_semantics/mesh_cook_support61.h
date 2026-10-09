#pragma once
#include "lo_semantics/mesh_cook_tree61.h"
namespace lo::semantic::gpu::mesh_cook_support61 {
using Registers = mesh_cook_tree61::Registers;
using Dependencies = mesh_cook_tree61::Dependencies;
// B9EB58 checks byte-index limits, replaces support owner and samples a 16x16
// cube for meshes above 32 vertices. BC61B0/BC6210/BC63C8 own support storage.
// +12 is one loaded block; otherwise +24/+28 are separate sampled tables.
// +32 borrows the mesh. Original partial-allocation behavior is preserved.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_cook_support61
