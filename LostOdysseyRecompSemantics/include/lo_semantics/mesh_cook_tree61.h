#pragma once
#include "lo_semantics/diagnostic_format_routes61.h"
#include "lo_semantics/owned_tree_reorder_support61.h"
namespace lo::semantic::gpu::mesh_cook_tree61 {
using Registers = owned_tree_reorder_support61::Registers;
struct Dependencies {
    owned_tree_reorder_support61::Dependencies tree;
    diagnostic_format_routes61::Dependencies diagnostics;
};
// Rebuild owner+8 tree from borrowed triangle/position arrays +164/+172.
// Owner virtual getters supply vertex/triangle counts. Settings select compact
// layout and the global quantization choice. Preserve diagnostic failure order.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_cook_tree61
