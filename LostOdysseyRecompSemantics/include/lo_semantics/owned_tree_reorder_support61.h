#pragma once
#include "lo_semantics/transform_owner_routes61.h"
namespace lo::semantic::gpu::owned_tree_reorder_support61 {
using Registers=transform_owner_routes61::Registers;
using Dependencies=transform_owner_routes61::Dependencies;
// B1C0 computes traversal depth while visiting the owner's root (+4).
// B208 visits that root, then its child pairs, honoring the callback low byte.
// The borrowed callbacks 1B50/2168/1B78 count leaves, export leaf boxes and
// packed index ranges, or append leaf addresses to a growable word array.
// 20F0 releases transform storage and the owner's +32 and (+24)-4 allocations.
// Guest allocator/destructor boundaries remain mutable Full72 calls. No host
// ownership is introduced. The traversal bridges execute known callbacks here
// and forward other guest targets to the caller's service.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
