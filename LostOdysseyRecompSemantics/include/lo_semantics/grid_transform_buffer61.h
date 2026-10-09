#pragma once
#include "lo_semantics/crt_reader_float61.h"

namespace lo::semantic::gpu::grid_transform_buffer61 {
using Registers = crt_reader_float61::Registers;

// Borrowed guest descriptors; these leaves allocate/free nothing.
// BD17F0 returns whether all four words at +8/+12/+16/+20 are nonzero.
// BD1830 reads the triplet count at +8 and triplet pointer at +16;
// vertex storage is based at +20.
// Count triplets whose three 12-byte vertex addresses contain an equal pair.
// Equality uses wrapped 32-bit addresses, not coordinate values or index
// equality. Vertex storage is never dereferenced or copied by this leaf.
// BDAC60 clears seven words at +0..+24, retaining the descriptor in r3.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state);
}
