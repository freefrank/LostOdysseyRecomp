#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::tree_box_bounds61 {
using Registers = crt_async_status_transfer::Registers;
// BDDE70 expands the borrowed min/max box at r3 using the box at r4.
// All twelve coordinates are read before any write; equal coordinates select
// the source, preserving signed-zero and original ordered-comparison behavior.
// BD8EE0 reduces selected box IDs r4/count r5 into borrowed output r6.
// Builder r3+72 points to six-float boxes with 24-byte stride. Null selection
// or zero count returns false without writing output. No allocation occurs.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&,
                        float_triplet_transfer::NativeServices&, Registers&);
}
