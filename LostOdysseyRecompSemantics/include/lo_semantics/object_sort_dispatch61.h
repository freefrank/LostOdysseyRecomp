#pragma once
#include "lo_semantics/object_sort_engine61.h"
#include "lo_semantics/crt_close_block_output_context.h"

namespace lo::semantic::gpu::object_sort_dispatch61 {
using Registers = object_sort_engine61::Registers;
struct Dependencies {
    object_sort_engine61::Dependencies engine;
    crt_close_block_output_context::ErrorOutputServices& output;
};

// 82BAFEC0: encode object r3 into the borrowed writer r5 (or a temporary writer
// when r5 is zero). Object +88 is grid side, +104 word count, +108 cell words.
// Emit PMAP/version 4, group cell indices by their low 30-bit value, invoke the
// shared coordinate engine for each group, and append occupancy bits. Optional
// r4 selects the existing file-output path. Temporary arrays/writer are guest
// owned and released through accepted lowers. All calls retain mutable state.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
}
