#pragma once
#include "lo_semantics/crt_reader_float61.h"

namespace lo::semantic::gpu::grid_transform_support61 {
using Registers = crt_reader_float61::Registers;
using NativeServices = float_triplet_transfer::NativeServices;

// Closed leaves needed by the grid-transform chain. Memory is borrowed;
// these entries allocate/free nothing and do not call guest function tables.
// 822C5128: ceil-like double conversion using guest constants, preserving
// signed zero and explicit FPR scratch stages (finite inputs validated).
// 82F2B308: clear six words. 82BD1278: initialize 28-byte format settings.
// 82BD78C0: visit-record vtable + sentinel initialization.
// 82BD78D8: reset only that vtable, without releasing any payload.
// 82BD78E8: if r5 is nonnull, copy its +4 word to destination r3+4;
// return zero in r3 on both branches. No ownership transfer is inferred.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
}
