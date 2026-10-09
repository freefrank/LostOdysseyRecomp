#pragma once
#include "lo_semantics/transform_owner_routes61.h"
namespace lo::semantic::gpu::owned_tree_mesh_build61 {
using Registers=transform_owner_routes61::Registers;
using Dependencies=transform_owner_routes61::Dependencies;
// BD22A8 builds a derived mesh owner r3 from borrowed settings r4. Owner+4
// borrows the source descriptor; +12 owns the first tree, +16 the strategy,
// +24 owns a count-prefixed packed leaf-range map, +32 an optional preserved
// index array. Each range packs an index-array byte offset shifted left two
// above the low four bits of (leaf item count - 1).
// First tree leaves are exported as bounds, rebuilt into a second tree, and
// remapped into the final leaf order. Settings+27 permits in-place source
// gathering; otherwise the original index permutation is retained at+32.
// Settings+26 retains the first tree. Temporary bounds/list/second-tree
// lifetimes follow each original branch; failures do not imply rollback.
// Complete Registers, guest allocation/callback service and native FP service
// remain borrowed across all lowers. Ordinary buffers/settings do not alias
// this function's 384-byte frame. No new runtime replacement is installed.
// Concrete guest targets remain unresolved, rather than silently emulated:
// 820D6C54: BD88E8/BD8BF8/BD8AC0/BD8B40/BB3B88;
// 820D6300: BD8EE0/BB3B60/BD8848/BD8888/BB3B88;
// strategy 820D6EBC+4: BDBD90. Full-target validation is outside this unit.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
