#pragma once

#include "lo_semantics/crt_reader_bucket_sort61.h"
#include "lo_semantics/crt_reader_float61.h"

namespace lo::semantic::gpu::object_sort_engine61 {
using Registers = crt_reader_float61::Registers;
struct Dependencies {
    crt_reader_bucket_sort61::Dependencies sort;
    float_triplet_transfer::NativeServices& fp;
};

// 82BAE200: encode a borrowed array of linear 3D grid indices. r3 points to
// {unused word, count, indices}, r4 is a borrowed bit-output writer, r5 is the
// grid side length. Emit count, split coordinates, Morton-sort their indices,
// then emit neighbor/absolute-coordinate codes. Four temporary guest arrays
// and the sort permutation are released through existing guest allocators.
// Previous coordinates live in guest globals 83216184/188/18C and remain
// updated for the next invocation. All lower calls share mutable Registers.
// Ordinary nonzero divisors are the supported arithmetic boundary; original
// divide traps, faults/MMIO, concurrency and runtime integration are untested.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
}
