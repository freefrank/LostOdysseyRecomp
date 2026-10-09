#pragma once
#include "lo_semantics/mesh_geometry_stream61.h"
namespace lo::semantic::gpu::mesh_cook_stream61 {
using Registers = mesh_geometry_stream61::Registers;
using Dependencies = mesh_geometry_stream61::Dependencies;
// B9F6F0 emits NXS/CVXM: CLHL geometry/valence, length-prefixed OPC/HBM tree,
// bounds/scalars, cached mass properties and optional support-map sections.
// The temporary adapter owns only its valence cache; mesh/tree remain owned
// by the caller. Virtual stream, tree-strategy and allocation targets stay live.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_cook_stream61
