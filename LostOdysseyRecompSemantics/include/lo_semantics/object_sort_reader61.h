#pragma once
#include "lo_semantics/reader_buffer_growth61.h"

namespace lo::semantic::gpu::object_sort_reader61 {
using Registers = reader_buffer_growth61::Registers;
using Dependencies = reader_buffer_growth61::Dependencies;

// 82BAF600: r3 is a borrowed output word-array descriptor, r4 a byte reader,
// r5 the grid dimension. Decode count:u32 followed by count 5-bit opcodes.
// Opcodes 0..25 update a persistent xyz cursor by one neighboring-cell delta;
// 26..31 replace selected coordinates from the bit stream. Flatten each cursor
// as (z*dimension+y)*dimension+x and append it through the guest growth lower.
// Reader +24/+25 hold the remaining-bit mask/current byte. Global cursor and
// opcode histogram are guest state, shared across calls, not host-owned state.
// Return the decoded count in r3. Like the original, allocation failure is not
// checked by this caller before the append; no new fallback policy is added.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies deps, Registers& state);
}
