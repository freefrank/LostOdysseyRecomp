#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
namespace lo::semantic::gpu::scratch_arena61 {
using Registers = crt_close_recursive_buffer_context::Registers;
using GuestServices = crt_close_recursive_buffer_context::GuestServices;
// E628 reserves reusable aligned global scratch storage for r3 bytes, refusing
// growth while the current cursor is beyond the start. E738 toggles counting
// mode (r3 low byte), resets counters on entry and updates high-water on exit.
// E810 enters/exits scoped scratch use: r4 region size, r5 reusable capacity,
// r6 temporary alignment. It preserves the original global field/allocator
// ordering and diagnostics; exit restores cursor/alignment without inventing a
// free, guard or rollback. Allocator and diagnostic calls stay borrowed.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, GuestServices &, Registers &);
} // namespace lo::semantic::gpu::scratch_arena61
