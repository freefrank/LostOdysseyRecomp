#pragma once
#include "lo_semantics/mesh_indexed_channels61.h"
namespace lo::semantic::gpu::mesh_indexed_normals61 {
using Registers = mesh_indexed_channels61::Registers;
using Dependencies = mesh_indexed_channels61::Dependencies;
// BBED20 computes face normals (+32..40), optionally appends normal output,
// and builds per-position face counts/prefix/adjacency at +264/+268/+272.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_indexed_normals61
