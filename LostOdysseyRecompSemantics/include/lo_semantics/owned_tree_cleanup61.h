#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::owned_tree_cleanup61 {
using Registers=crt_close_recursive_buffer_context::Registers;
using GuestServices=crt_close_recursive_buffer_context::GuestServices;
// BD9740: r3 points to one 40-byte node or a counted node array (r4 bit1).
// Array count is the word immediately before the first node. Walk arrays in
// reverse. Node +24 holds a tagged child-array pointer: bit0 marks borrowed
// storage, while untagged nonzero pointers recurse with flags3 (array + free).
// Clear node +32/+36 after child cleanup. r4 bit0 additionally frees this node
// or array header through the guest table callback. Return its allocation base.
// No host ownership/depth guard is introduced; callback state remains live.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,GuestServices&,Registers&);
struct Dependencies {
    GuestServices& guest;
    float_triplet_transfer::NativeServices& fp;
};
// BDAC88 releases an owner's descriptor at +24 (including its float-buffer
// payload), child array at +4, and raw allocation at +0, in that order.
// Clear each owner slot only after disposal, using live callback state.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
