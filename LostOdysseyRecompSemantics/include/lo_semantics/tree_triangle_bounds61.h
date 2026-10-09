#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::tree_triangle_bounds61 {
using Registers=crt_async_status_transfer::Registers;
// First-tree callbacks borrow builder r3; builder+72 points to a mesh whose
// +16 is a u32 triangle-index triplet array and +20 a float vertex-triplet array.
// BD88E8: r4 selected triangle IDs, r5 count, r6 output min/max six floats;
// null selection or zero count returns false without touching output.
// BD8AC0: r4 triangle ID, r5 axis, returns one centroid coordinate in f1.
// BD8B40: r4 triangle ID, r5 output centroid triplet. All buffers stay borrowed.
// Finite FP stages and conditional min/max selection preserve original order;
// no host geometry objects, allocation, or external guest callbacks are needed.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,float_triplet_transfer::NativeServices&,Registers&);
}
