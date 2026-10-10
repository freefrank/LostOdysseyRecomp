#pragma once
#include "lo_semantics/cloth_storage61.h"
namespace lo::semantic::gpu::cloth_load61 {
using Registers = cloth_storage61::Registers;
using Dependencies = cloth_storage61::Dependencies;
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::cloth_load61
