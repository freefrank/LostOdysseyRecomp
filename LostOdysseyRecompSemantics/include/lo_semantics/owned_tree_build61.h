#pragma once
#include "lo_semantics/owned_tree_storage61.h"
namespace lo::semantic::gpu::owned_tree_build61 {
using Registers=owned_tree_storage61::Registers;
using Dependencies=owned_tree_storage61::Dependencies;
// BD9928: split a borrowed 40-byte node (r3) using context r4. Node min/max
// bounds occupy +0..20; +24 is tagged child storage, +32/+36 the word list/count.
// Context flags +8 choose extent, variance, balance, axis retry, or half split.
// Context table+20 approves splitting; table+16 supplies word coordinates;
// accepted BD9858 uses table+8/+12 to partition. Children borrow portions of
// the parent's word array. Child records are borrowed from arena+28 or owned
// via an 84-byte counted allocation; the pointer tag records that ownership.
// Return1 also covers a retained leaf; return0 means no supported split/result.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
