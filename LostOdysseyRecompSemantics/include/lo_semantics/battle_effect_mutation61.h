#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::battle_effect_mutation61 {
using Registers = manager_release_context61::Registers;
using Dependencies = manager_release_context61::Dependencies;
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::battle_effect_mutation61
