#pragma once
#include "lo_semantics/mesh_indexed_workspace61.h"
namespace lo::semantic::gpu::mesh_indexed_vertex_output61 {
using Registers = mesh_indexed_workspace61::Registers;
using Dependencies = mesh_indexed_workspace61::Dependencies;
// BC0108 emits one face batch: indexed or expanded channels, smoothing-mask
// normals with optional angle weighting and incidence records, final indices
// and batch metadata. Guest constants/atan2 and source ownership are retained.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_indexed_vertex_output61
