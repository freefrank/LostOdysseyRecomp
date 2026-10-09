#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::indexed_record_accumulate61 {
using Registers=crt_async_status_transfer::Registers;
// 822CD290 borrows descriptor r4: +56 record storage, +60 uint16 indices,
// +120 byte stride, +124 active count. r5 is a record-relative source offset;
// f1 is the scale. Scan active indices backward, skipping record+92 bit 0.
// Accumulate scaled source into triplets +48 and +32, reloading the source
// after the first write (overlap is observable). No allocation/count mutation.
// Full scratch state and distinct unrolled/remainder FP staging are retained.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,float_triplet_transfer::NativeServices&,Registers&);
}
