#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::battle_script_registration61 {
using Registers = manager_release_context61::Registers;
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Registers &);
} // namespace lo::semantic::gpu::battle_script_registration61
