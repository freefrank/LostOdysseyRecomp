#pragma once
#include "lo_semantics/crt_reader_float61.h"
namespace lo::semantic::gpu::memory_input61 {
using Registers = crt_reader_float61::Registers;
// Reader r3+4 is a borrowed guest cursor, with no stored end bound. E550/E568/
// E580 consume u8/u16/u32; E598/E5B8 consume f32/f64 via guest stack scratch.
// E5D8 copies r5 bytes into r4, then advances the live cursor and retains the
// copy lower's result. Preserve load/store ordering and guest-endian values;
// no bounds checking or host ownership is added.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::memory_input61
