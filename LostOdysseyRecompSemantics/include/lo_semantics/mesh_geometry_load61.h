#pragma once
#include "lo_semantics/mesh_geometry_stream61.h"
namespace lo::semantic::gpu::mesh_geometry_load61 {
using Registers = mesh_geometry_stream61::Registers;
using Dependencies = mesh_geometry_stream61::Dependencies;
// ICE/CVHL and CLHL geometry load, aggregate relocation, packed normals and
// VALE. Logical/ABI recovery; CRT first-use registration stays an explicit
// boundary.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_geometry_load61
