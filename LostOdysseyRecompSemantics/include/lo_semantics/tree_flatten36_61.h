#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::tree_flatten36_61 {
using Registers=crt_async_status_transfer::Registers;
// BDBC18 borrows output records r3, destination index r4, mutable next-index
// counter r5 and a 40-byte source node r6. It converts min/max to center/half
// extents and flattens paired children depth-first into 36-byte records.
// Leaf +24 stores bit31 | first item ID; branch +24/+28 store child indices
// and +32 stores descendant count. Leaf +28/+32 remain untouched.
// Inputs must describe a valid acyclic tree; no allocator, guards or host
// ownership transfer are introduced. Source fields are reloaded after writes.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,
                        float_triplet_transfer::NativeServices&,Registers&);
}
