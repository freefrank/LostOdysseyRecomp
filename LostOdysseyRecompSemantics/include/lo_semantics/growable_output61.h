#pragma once
#include "lo_semantics/crt_reader_float61.h"
namespace lo::semantic::gpu::growable_output61 {
using Registers = crt_reader_float61::Registers;
using Dependencies = crt_reader_float61::Dependencies;
// Output object r3: +4 used bytes, +8 capacity, +12 owned data. E498 appends
// borrowed source r4/length r5, growing to required+4096 through global allocator
// 832DF58C. It copies used data then frees old storage; failure behavior is
// unchanged. E330/E378/E3C0/E408/E450 stage u8/u16/u32/f32/f64 values and call
// the live virtual block append slot +48. E2C8 frees payload and restores base
// vtable, without clearing the data member or freeing the owner itself.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::growable_output61
