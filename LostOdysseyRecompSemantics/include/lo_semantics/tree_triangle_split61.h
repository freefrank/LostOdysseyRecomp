#pragma once
#include "lo_semantics/tree_triangle_bounds61.h"
namespace lo::semantic::gpu::tree_triangle_split61 {
using Registers=tree_triangle_bounds61::Registers;
// BD8BF8 borrows builder r3, selected triangle IDs r4/count r5, bounds r6,
// axis r7. Builder+8 bit 0x20 chooses the mean of all selected vertex axis
// coordinates; otherwise return the bounds midpoint. Builder+72 mesh uses
// +16 u32 triangle triplets and +20 float vertex triplets. Result is f1.
// Four-triangle batches and remainder retain single-rounded addition order.
// Input buffers do not alias this leaf's register-save/scratch area below SP.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,float_triplet_transfer::NativeServices&,Registers&);
}
