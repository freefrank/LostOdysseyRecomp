#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
namespace lo::semantic::gpu::record_snapshot_gather61 {
using Registers=crt_close_recursive_buffer_context::Registers;
using GuestServices=crt_close_recursive_buffer_context::GuestServices;
// BD1900: r3 borrows a record descriptor, r4 the expected nonzero row count,
// r5 uint32 gather indices. Descriptor fields: +0 optional approval callback,
// +4 callback context, +8 row count, +16 borrowed packed 12-byte record storage.
// The callback receives (count, indices, context), retaining complete mutable
// machine state. A false low byte means successfully handled, with no local copy.
// Otherwise allocate an owned snapshot with a four-byte count prefix, gather
// back into the existing storage, then release the snapshot through slot12.
// Caller storage and indices survive the call; no host ownership or rollback.
// Ordinary valid indices/storage and a nonoverlapping 128-byte frame are assumed.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,GuestServices&,Registers&);
}
