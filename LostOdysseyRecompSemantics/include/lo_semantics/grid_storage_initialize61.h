#pragma once
#include "lo_semantics/reader_buffer_growth61.h"

namespace lo::semantic::gpu::grid_storage_initialize61 {
using Registers = reader_buffer_growth61::Registers;
using Dependencies = reader_buffer_growth61::Dependencies;

// 82BB1E58: r3 borrows the grid object, r4 is its dimension, r5 points to
// six floats (min xyz, max xyz). Set bounds, center/half extent, full extent,
// coordinate scales, dimension/square/cube counts, and allocate owned cells.
// Object +108 receives the guest allocation; cells start at sentinel -1.
// The external allocator (global service table +8) owns storage and can mutate
// live state. Return r3=1 iff the returned pointer is nonzero; failure retains
// the initialized metadata and stores a null payload. No host ownership.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies deps, Registers& state);
}
