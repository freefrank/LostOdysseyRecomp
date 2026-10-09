#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::indexed_record_retire61 {
using Registers=crt_async_status_transfer::Registers;
// 822CBA60 borrows object r3: +56 record storage, +60 uint16 index array,
// +120 byte stride, +124 active count, +112 record-relative clear offset.
// Scan active indices backward. If record float+12 exceeds the guest threshold,
// clear five floats at record+clear-offset, swap its index with the last active
// slot, and decrement active count. Removed indices stay in the inactive tail;
// record storage remains allocated/borrowed. Count, pointers and stride reload
// at the original boundaries, including between swaps. Four-at-a-time traversal
// and scalar remainder preserve observable scratch/FP state.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,float_triplet_transfer::NativeServices&,Registers&);
}
