#pragma once
#include "lo_semantics/tree_flatten36_61.h"
namespace lo::semantic::gpu::tree_compact_flatten61 {
using Registers = tree_flatten36_61::Registers;
// CE38 emits only internal nodes of a valid borrowed full binary tree.
// r3 output32-byte records, r4 output index, r5 next-index counter, r6 source
// 40-byte internal node. Six floats are center/half extents. +24 folds the
// first child's leaf ID with 0x80000000 and the second-leaf flag0x40000000;
// an internal first child uses 0xDEAD. +28 is descendant count when the
// second child is internal, otherwise0. A sole leaf child is ordered first.
// Input must be internal; preserve the original lack of a leaf-input guard.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::tree_compact_flatten61
