#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::tree_flat_strategy61 {
using Registers=crt_close_recursive_buffer_context::Registers;
struct Dependencies{crt_close_recursive_buffer_context::GuestServices& guest;float_triplet_transfer::NativeServices& fp;};
// BDBD90 binds borrowed tree r4 into strategy r3. Tree node count must equal
// 2*root-item-count-1. Strategy+4 is stored count; +8 owns 36-byte flat nodes,
// addressed four bytes after their count-prefixed allocation. A count change
// frees/replaces storage; matching counts reuse it. Failure does not roll back
// the new count. Guest alloc/free callbacks stay mutable borrowed boundaries.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
