#pragma once
#include "lo_semantics/owned_tree_storage61.h"
namespace lo::semantic::gpu::owned_tree_plane_query61 {
using Registers=owned_tree_storage61::Registers;
using Dependencies=owned_tree_storage61::Dependencies;
// BDA8B8: r3 borrowed 40-byte node, r4 borrowed 16-byte plane array,
// r5 active-plane bitmask, r6 visitor target, r7 userdata. Bounds are min/max
// xyz at +0..20; +24 tags a contiguous child pair; +32/+36 are indices/count.
// Outside nodes are rejected. Planes containing the full box leave the mask;
// mask0 emits the whole node with callback(count,indices,0,user), otherwise
// intersecting leaves emit flag1. Callback state is mutable and guest-owned.
// Frame+80 is read from existing guest stack scratch, not a supplied argument.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory&,Dependencies,Registers&);
}
