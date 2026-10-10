#pragma once
#include "lo_semantics/mesh_stream_write61.h"
namespace lo::semantic::gpu::mesh_valence_stream61 {
using Registers = mesh_stream_write61::Registers;
using Dependencies = mesh_stream_write61::Dependencies;
// BC7F98 reads ICE/VALE into owned degree/prefix and adjacency storage;
// Read completion mirrors existing BC7F48 halfword prefixes. Logical/ABI.
// Halfword maximum and compact byte/halfword emission feed the ICE/VALE
// adjacency stream. The temporary degree array is released before raw edges.
// Preserve original allocation/count preconditions, without added fallback.
// BADFA0/BD8668 add word maximum and 8/16/32-bit adaptive output. The full
// word branch deliberately passes through the original float-span staging.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_valence_stream61
