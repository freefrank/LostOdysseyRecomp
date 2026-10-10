#pragma once
#include "lo_semantics/cloth_storage61.h"
namespace lo::semantic::gpu::cloth_schedule_support61 {
using Registers = cloth_storage61::Registers;
using Dependencies = cloth_storage61::Dependencies;
// Sort signed bucket/local-index keys and build the original-to-packed vertex
// map.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::cloth_schedule_support61
