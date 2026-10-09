#pragma once
#include "lo_semantics/crt_reader_sort_float61.h"

namespace lo::semantic::gpu::object_sort_support61 {
using Registers = crt_reader_sort_float61::Registers;
using Dependencies = crt_reader_sort_float61::Dependencies;

// Borrowed guest objects used by the 82BAE200 sorting engine:
// 82BD2A08 initializes three words and a float level at +12.
// 82BD2C08 tail-calls the existing float-state cleanup (82BD2A28).
// 82BD2C50 initializes a 21-byte state: high-bit sentinel at +0,
// four zero words at +4..+16 and an enabled byte at +20.
// Initializers allocate nothing and do not dispose previous contents.
// Cleanup retains its guest service ownership and mutable-register boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
}
