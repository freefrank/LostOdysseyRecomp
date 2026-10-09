#pragma once
#include "lo_semantics/transform_owner_routes61.h"
namespace lo::semantic::gpu::transform_owner_initialize61 {
using Registers=transform_owner_routes61::Registers;
using Dependencies=transform_owner_routes61::Dependencies;
// BD12F8 replaces the owned strategy at owner+16. Incoming r4/r5 low bytes
// select owner+8 bits 1/0, a 12/36-byte allocation and one of four tables.
// Mode bits 00/01/10/11 choose BDBD58/BDC1D0/BDB310/BDB610 respectively.
// Existing strategy is deleted first with r4=1; failure leaves +16 null.
// Owner, allocator and callback state are borrowed. Allocation ownership passes
// to the owner only at the final +16 store. Guest callbacks remain live across
// cleanup/allocation; caller-saved and nonvolatile scratch are not cached.
// Returns r3=1 on allocation success, otherwise 0. No host allocations.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
