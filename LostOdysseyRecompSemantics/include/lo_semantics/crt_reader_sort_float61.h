#pragma once
#include "lo_semantics/crt_reader_float61.h"

namespace lo::semantic::gpu::crt_reader_sort_float61 {
using Registers = crt_reader_float61::Registers;
using Dependencies = crt_reader_float61::Dependencies;

// 82BD2A28: reset two words at reader+0/+4. reader+8 is a guest payload
// handle; reader+12 is a float compared with the guest constant 82000E50.
// Below threshold, retain the payload. Otherwise, if nonzero, resolve the
// shared table via 82BD0798, invoke table+12 with receiver r3/payload r4,
// then clear the payload. Field meanings beyond these uses remain unknown.
// Memory and services are borrowed; disposal belongs to the guest callback.
// That callback may mutate all Registers and memory. Subsequent stores use
// live r30/r31, then restore the caller's saved r30/r31 and low-32-bit LR.
// FP control follows the existing native boundary; no implicit host restore.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
}
