#include "lo_semantics/tree_mesh_callbacks61.h"
#include "lo_semantics/tree_triangle_bounds61.h"
#include "lo_semantics/tree_triangle_split61.h"
#include "lo_semantics/tree_box_bounds61.h"
#include "lo_semantics/tree_box_centroid61.h"
#include "lo_semantics/tree_split_policy61.h"
namespace lo::semantic::gpu::tree_mesh_callbacks61 {
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state) {
    switch (entry) {
    case 0x82bd88e8u: case 0x82bd8ac0u: case 0x82bd8b40u:
        return tree_triangle_bounds61::Apply(entry, memory, deps.fp, state);
    case 0x82bd8bf8u:
        return tree_triangle_split61::Apply(entry, memory, deps.fp, state);
    case 0x82bd8ee0u:
        return tree_box_bounds61::Apply(entry, memory, deps.fp, state);
    case 0x82bd8848u: case 0x82bd8888u:
        return tree_box_centroid61::Apply(entry, memory, deps.fp, state);
    case 0x82bb3b60u: case 0x82bb3b88u:
        return tree_split_policy61::Apply(entry, memory, deps.fp, state);
    case 0x82bdbd90u:
        return tree_flat_strategy61::Apply(entry, memory, deps, state);
    default: return false;
    }
}
}
