#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
namespace lo::semantic::gpu::owned_tree_visit61 {
using Registers=crt_close_recursive_buffer_context::Registers;
using GuestServices=crt_close_recursive_buffer_context::GuestServices;
// Borrowed 40-byte nodes use +24 as the tagged pointer to two contiguous
// children. Callback targets and userdata remain guest-owned; only the low
// return byte selects continuation. Callbacks may mutate the full live state.
// A0A8: r3 node, r4 maximum-depth word, r5 current-depth word, optional r6
// callback, r7 userdata. Visit current node before children; reload its child
// pointer after the left subtree, so a callback may redirect the right branch.
// A1A0: r3 node, r4 callback, r5 userdata. Visit both children first, recurse
// through the left child, then continue the right child using the same frame.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory&,GuestServices&,Registers&);
}
