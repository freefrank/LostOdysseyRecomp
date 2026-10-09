#pragma once
#include "lo_semantics/crt_reader_float61.h"
namespace lo::semantic::gpu::owned_tree_refit61 {
using Registers=crt_reader_float61::Registers;
using NativeServices=float_triplet_transfer::NativeServices;
// Refit borrowed 40-byte nodes: bounds +0..20, tagged paired children +24,
// item-index pointer +32 and count +36. Input item bounds are six floats
// (min xyz, max xyz) in 24-byte records. No allocation or ownership transfer.
// BDA248: r3 owns {+4 nodes, +16 count}; r4+72 holds the item-bounds array.
// Visit all nodes in reverse order; null owner returns0, otherwise return1.
// BDA5F0: r3 holds {+4 nodes, +8 dirty words, +12 word count}, r5 the bounds
// array. Consume marked nodes in descending order without clipping the last
// word; return the original r3. Complete scalar state remains explicit.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory&,NativeServices&,Registers&);
}
