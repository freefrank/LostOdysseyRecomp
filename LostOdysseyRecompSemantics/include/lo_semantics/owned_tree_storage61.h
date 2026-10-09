#pragma once
#include "lo_semantics/crt_reader_float61.h"

namespace lo::semantic::gpu::owned_tree_storage61 {
using Registers = crt_reader_float61::Registers;
using Dependencies = crt_reader_float61::Dependencies;

// BD9858 partitions a borrowed node's word array (+32 pointer, +36 count).
// r4 supplies opaque context; r5 is a borrowed guest service. Its table+8
// selects an initial double threshold from the array/node/context arguments.
// Table+12 scores each word; words scoring strictly above the live f31
// threshold move to the front. Return that prefix length. No allocation,
// disposal or ownership transfer occurs here. Calls may mutate all state;
// array/count are reloaded and swaps preserve their original access order.
// Saved r25..r31/f31 and caller frame/LR restore at the return boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
}
