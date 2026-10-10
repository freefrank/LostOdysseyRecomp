#pragma once
#include "lo_semantics/cloth_storage61.h"
namespace lo::semantic::gpu::cloth_topology_support61 {
using Registers = cloth_storage61::Registers;
// Lexicographic record sorts, unique pair lookup and borrowed mesh descriptor.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Registers &);
} // namespace lo::semantic::gpu::cloth_topology_support61
