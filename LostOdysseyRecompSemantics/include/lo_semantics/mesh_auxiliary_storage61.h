#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::mesh_auxiliary_storage61 {
using Registers = manager_release_context61::Registers;
using Dependencies = manager_release_context61::Dependencies;
// BC6428/BC8588 initialize the base/derived mesh auxiliary storage descriptor;
// D33160 clears its five-word subdescriptor. BC7E90 frees one aggregate buffer
// or its two separately owned arrays. BC67C8 likewise chooses aggregate +72
// versus ten individual buffers, then cleans/frees optional +76 descriptor.
// BC85F0 composes derived then base cleanup. Original live-field reloads,
// allocator order, vtable stores and untouched interior fields are preserved.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_auxiliary_storage61
