#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::cpx_context61 {
using Registers = manager_release_context61::Registers;
using Dependencies = manager_release_context61::Dependencies;
// CPX owned header/index, lazy block scratch, lookup and release primitives.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::cpx_context61
