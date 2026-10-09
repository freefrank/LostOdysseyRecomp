#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/float_triplet_transfer.h"

namespace lo::semantic::gpu::geometry_support61 {
using Registers = crt_async_status_transfer::Registers;

// Borrowed object r3; no storage ownership transfer or allocation. The base
// initializer installs a vtable and clears +4/+8/+12. The extended initializer
// also sets +92/+96/+100/+104, guest-constant floats +132/+136 and flags +140/+141.
// Teardown only transitions the vtable back to the base type; it does not free
// object storage or resources. Unknown field meanings are deliberately unnamed.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    float_triplet_transfer::NativeServices& fp, Registers& state);
}
