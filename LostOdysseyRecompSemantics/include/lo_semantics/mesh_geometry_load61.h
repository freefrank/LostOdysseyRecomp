#pragma once
#include "lo_semantics/mesh_geometry_stream61.h"
namespace lo::semantic::gpu::mesh_geometry_load61 {
using Registers=mesh_geometry_stream61::Registers;
using Dependencies=mesh_geometry_stream61::Dependencies;
// Packed normal input with lazy guest spherical lookup/permutation/sign table.
// Logical/ABI recovery; CRT first-use registration stays an explicit boundary.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
