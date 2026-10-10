#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::cpx_lifecycle61 {
using Registers = manager_release_context61::Registers;
using Dependencies = manager_release_context61::Dependencies;
// Unlink a CPX context under guest locking and release its owned buffers.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::cpx_lifecycle61
