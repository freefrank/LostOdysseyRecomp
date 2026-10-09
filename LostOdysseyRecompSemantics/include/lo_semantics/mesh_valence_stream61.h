#pragma once
#include "lo_semantics/mesh_stream_write61.h"
namespace lo::semantic::gpu::mesh_valence_stream61 {
using Registers = mesh_stream_write61::Registers;
using Dependencies = mesh_stream_write61::Dependencies;
// Halfword maximum and compact byte/halfword emission feed the ICE/VALE
// adjacency stream. The temporary degree array is released before raw edges.
// Preserve original allocation/count preconditions, without added fallback.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_valence_stream61
