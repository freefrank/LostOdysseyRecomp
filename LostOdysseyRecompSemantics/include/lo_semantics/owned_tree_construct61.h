#pragma once
#include "lo_semantics/owned_tree_expand61.h"
#include "lo_semantics/owned_tree_cleanup61.h"
namespace lo::semantic::gpu::owned_tree_construct61 {
using Registers=owned_tree_expand61::Registers;
using Dependencies=owned_tree_expand61::Dependencies;
// BDAD18 builds an owned tree from borrowed context r4 into owner r3.
// Context+24 gives item count; owner+0 owns the ascending index array and
// owner+4 owns a counted arena of (2*count-1) forty-byte records. Context+28
// borrows that arena during expansion. Existing owner contents are disposed
// before allocation. Context+64 starts at1 and +68 at0; expansion updates
// accounting copied to owner+16/+20. Return r3 is the original success flag.
// No rollback or second-allocation null guard is invented: the original
// continues its root writes/expansion even if the arena allocation is null.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
