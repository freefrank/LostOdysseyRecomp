#pragma once

#include "lo_semantics/crt_reader_float61.h"

namespace lo::semantic::gpu::reader_buffer_growth61 {
using Registers = crt_reader_float61::Registers;
using Dependencies = crt_reader_float61::Dependencies;

// 82BD2870: r3 borrows a word-array descriptor, r4 is the additional count.
// Fields +0 capacity, +4 used count, +8 owned guest payload, +12 growth factor.
// Grow only above the image threshold, allocate capacity*4 bytes at alignment
// 64, copy used words, then dispose the prior payload and install the new one.
// The allocator owns guest storage; no host pointer or ownership is retained.
// Return r3=1 on allocation success, 0 otherwise. Capacity is updated before
// allocation and remains updated on failure. Callback register changes are live.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies deps, Registers& state);
}
