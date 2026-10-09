#pragma once
#include "lo_semantics/tree_flat_strategy61.h"
namespace lo::semantic::gpu::tree_compact_strategy61 {
using Registers = tree_flat_strategy61::Registers;
using Dependencies = tree_flat_strategy61::Dependencies;
// D058 binds an internal-only 32-byte representation. Borrowed tree r4 must
// satisfy full-node-count=2*leaf-count-1; owned output count is leaf-count-1.
// Matching count reuses storage, changed count frees and allocates a count-
// prefixed buffer (tag31), then CE38 folds leaves into internal-node records.
// Original ordering and valid-internal-tree preconditions are retained.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::tree_compact_strategy61
