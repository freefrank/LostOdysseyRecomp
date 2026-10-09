#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::tree_split_policy61 {
using Registers = crt_async_status_transfer::Registers;
// BB3B60: midpoint of bounds r6 along axis r7, returned in f1.
// BB3B88: unsigned item count r5 exceeds borrowed policy r3 threshold +4.
// Neither callback owns storage. Preserve binary32 stages and XER carry.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&,
                        float_triplet_transfer::NativeServices&, Registers&);
}
