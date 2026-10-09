#pragma once
#include "lo_semantics/mesh_edge_build61.h"
namespace lo::semantic::gpu::mesh_edge_flags61 {
using Registers = mesh_edge_build61::Registers;
using Dependencies = mesh_edge_build61::Dependencies;
// Classify boundary/angle-selected edges using existing incidence records.
// Bit31 marks selected triangle sides; bit30 marks vertices touched by those
// sides; edge-record bit0 mirrors edge selection. Positions and index arrays
// are borrowed. Two temporary byte masks are allocated/freed in original order.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_edge_flags61
