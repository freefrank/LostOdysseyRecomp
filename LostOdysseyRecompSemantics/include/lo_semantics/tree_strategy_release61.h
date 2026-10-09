#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
namespace lo::semantic::gpu::tree_strategy_release61 {
using Registers = crt_close_recursive_buffer_context::Registers;
using GuestServices = crt_close_recursive_buffer_context::GuestServices;
// Four strategy destructors release owned count-prefixed storage at +8 and
// restore the common base table. Their deleting wrappers additionally free
// owner r3 when r4 bit0 is set, then return its original pointer. Count/scales
// are left untouched. Allocator and free callbacks retain full mutable state.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, GuestServices &, Registers &);
} // namespace lo::semantic::gpu::tree_strategy_release61
