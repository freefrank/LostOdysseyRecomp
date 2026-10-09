#pragma once
#include "lo_semantics/owned_tree_build61.h"
namespace lo::semantic::gpu::owned_tree_expand61 {
using Registers=owned_tree_build61::Registers;
using Dependencies=owned_tree_build61::Dependencies;
// BDAA48 refreshes a node's bounds through context table+4, optionally extends
// one axis to the context plane and inflates all axes by the context margin.
// Call BD9928 to split, link child.parent fields, then recursively process both
// children. Context+60 counts item visits; guest global832DF558 counts nodes.
// Context and word arrays stay borrowed.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
