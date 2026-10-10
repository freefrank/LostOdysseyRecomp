#pragma once
#include "lo_semantics/cloth_storage61.h"
namespace lo::semantic::gpu::cloth_import61 {
using Registers = cloth_storage61::Registers;
using Dependencies = cloth_storage61::Dependencies;
// Append strided triangle/tetrahedral input into owned cooking vectors.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::cloth_import61
