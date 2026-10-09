#include "lo_semantics/tree_mesh_callbacks61.h"
#include "lo_semantics/tree_box_bounds61.h"
#include "lo_semantics/tree_box_centroid61.h"
#include "lo_semantics/tree_compact_strategy61.h"
#include "lo_semantics/tree_flat_load61.h"
#include "lo_semantics/tree_flat_write61.h"
#include "lo_semantics/tree_quantized_load61.h"
#include "lo_semantics/tree_quantized_strategy61.h"
#include "lo_semantics/tree_quantized_write61.h"
#include "lo_semantics/tree_scalar_write61.h"
#include "lo_semantics/tree_split_policy61.h"
#include "lo_semantics/tree_strategy_release61.h"
#include "lo_semantics/tree_triangle_bounds61.h"
#include "lo_semantics/tree_triangle_split61.h"
namespace lo::semantic::gpu::tree_mesh_callbacks61 {
bool Apply(GuestAddress entry, GuestMemory &memory, Dependencies deps, Registers &state) {
    switch (entry) {
    case 0x82bd88e8u:
    case 0x82bd8ac0u:
    case 0x82bd8b40u:
        return tree_triangle_bounds61::Apply(entry, memory, deps.fp, state);
    case 0x82bd8bf8u:
        return tree_triangle_split61::Apply(entry, memory, deps.fp, state);
    case 0x82bd8ee0u:
        return tree_box_bounds61::Apply(entry, memory, deps.fp, state);
    case 0x82bd8848u:
    case 0x82bd8888u:
        return tree_box_centroid61::Apply(entry, memory, deps.fp, state);
    case 0x82bb3b60u:
    case 0x82bb3b88u:
        return tree_split_policy61::Apply(entry, memory, deps.fp, state);
    case 0x82bdbd90u:
        return tree_flat_strategy61::Apply(entry, memory, deps, state);
    case 0x82bdd058u:
        return tree_compact_strategy61::Apply(entry, memory, deps, state);
    case 0x82bdc208u:
    case 0x82bdd1e8u:
        return tree_quantized_strategy61::Apply(entry, memory, deps, state);
    case 0x82bdcfe0u:
    case 0x82bdd170u:
    case 0x82bdd7f0u:
    case 0x82bdda48u:
    case 0x82bddac0u:
    case 0x82bddcd8u:
    case 0x82bddd38u:
    case 0x82bddd98u:
        return tree_strategy_release61::Apply(entry, memory, deps.guest, state);
    case 0x82bdbed8u:
    case 0x82bdb350u:
        return tree_flat_load61::Apply(entry, memory, deps.guest, state);
    case 0x82bdc9f0u:
    case 0x82bdb7f8u:
        return tree_quantized_load61::Apply(entry, memory, deps, state);
    case 0x82bdc838u:
    case 0x82bdb660u:
        return tree_quantized_write61::Apply(entry, memory, {deps.guest, deps.fp}, state);
    case 0x82bdd868u:
    case 0x82bddb20u:
        return tree_flat_write61::Apply(entry, memory, {deps.guest, deps.fp}, state);
    case 0x82bd7d58u:
    case 0x82bd7e18u:
        return tree_scalar_write61::Apply(entry, memory, {deps.guest, deps.fp}, state);
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::tree_mesh_callbacks61
