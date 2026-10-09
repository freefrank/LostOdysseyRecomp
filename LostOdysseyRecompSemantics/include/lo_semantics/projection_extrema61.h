#pragma once
#include "lo_semantics/float_triplet_transfer.h"
#include "lo_semantics/crt_async_status_transfer.h"
namespace lo::semantic::gpu::projection_extrema61 {
using Registers=crt_async_status_transfer::Registers;
using NativeServices=float_triplet_transfer::NativeServices;
// BB34F8: r3 borrows an owner, r4 an output-table slot, r5 a direction xyz.
// Owner+8 points to storage: +24/+28 are minimum/maximum byte-index tables,
// +32 points to a point view with count at+12 and packed xyz floats at+16.
// Store first strict minimum/maximum dot-product indices, truncated to bytes;
// count0 stores index0 in both tables. No allocation or ownership transfer.
// Preserve the four-point loop's distinct float accumulation order, its tail,
// mutable FP control, selected scratch and the two saved GPR stack slots.
// Ordinary finite inputs are the focused contract; indices beyond255 retain
// the original byte truncation and no host-size or bounds guard is introduced.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,NativeServices&,Registers&);
}
