#pragma once
#include "lo_semantics/cloth_storage61.h"
namespace lo::semantic::gpu::cloth_triangle_edges61 {
using Registers = cloth_storage61::Registers;
using Dependencies = cloth_storage61::Dependencies;
// Group canonical triangle edges and emit the original 68-byte constraints.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::cloth_triangle_edges61
