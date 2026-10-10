#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::battle_script_core61 {
using Registers = manager_release_context61::Registers;
using Dependencies = manager_release_context61::Dependencies;
// Writable operand classes and basic control, arithmetic and bit operations.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::battle_script_core61
