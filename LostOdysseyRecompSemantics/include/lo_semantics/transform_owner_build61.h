#pragma once
#include "lo_semantics/transform_owner_initialize61.h"
#include "lo_semantics/owned_tree_construct61.h"
#include "lo_semantics/grid_transform_buffer61.h"
#include "lo_semantics/diagnostic_format_routes61.h"
namespace lo::semantic::gpu::transform_owner_build61 {
using Registers=transform_owner_initialize61::Registers;
struct Dependencies {
    transform_owner_initialize61::Dependencies owner;
    diagnostic_format_routes61::Dependencies diagnostics;
};
// BD7A60 consumes borrowed descriptor r4: +0 vertex source, +4..20 tree
// settings, +24/+25 strategy modes, +26 retain-tree flag. Owner r3 borrows
// vertex source at+4 and owns tree/strategy at+12/+16. Single-item input sets
// owner flag4 without allocating. General input creates tree then strategy,
// invokes strategy table+4 with tree, and optionally disposes the tree.
// Failure retains whatever allocations were already attached, for later
// owner cleanup. Kind !=1 uses the complete diagnostic chain, including
// caller-owned machine reservation/MSR state; no replacement lock state.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
