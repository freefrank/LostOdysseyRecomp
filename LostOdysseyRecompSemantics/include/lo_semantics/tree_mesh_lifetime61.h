#pragma once
#include "lo_semantics/owned_tree_reorder_support61.h"
namespace lo::semantic::gpu::tree_mesh_lifetime61 {
using Registers = owned_tree_reorder_support61::Registers;
using Dependencies = owned_tree_reorder_support61::Dependencies;
// BD2200 validates borrowed mesh r4 before releasing old owned tree storage,
// installs mesh at owner r3+4, then calls live vtable +28 with options r5.
// BD2268 destroys fields then invokes base cleanup without freeing the owner.
// BD27F8 additionally frees owner when incoming r4 bit0 is set, returning the
// original owner pointer even after free. External callbacks remain borrowed.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, Dependencies, Registers&);
}
