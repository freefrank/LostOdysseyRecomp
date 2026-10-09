#pragma once

#include "lo_semantics/crt_reader_float61.h"

namespace lo::semantic::gpu::object_sort_lifecycle61 {
using Registers = crt_reader_float61::Registers;
using Dependencies = crt_reader_float61::Dependencies;

// Borrowed guest object in r3. Initialization installs its vtable, two float
// triplets (+4/+16), float fields +96/+100, and zero fields +88/+92/+104/+108/+112.
// Cleanup reinstalls that vtable and disposes the nonzero resource at +108
// through a mutable global guest service (table slot +20). The service owns
// disposal; no host memory is allocated or freed here. Its returned Registers
// remain live, including r31 used to clear the resource after the call.
// Field meanings beyond these observed uses remain unresolved.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
}
