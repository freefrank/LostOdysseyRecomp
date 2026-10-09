#pragma once
#include "lo_semantics/mesh_auxiliary_storage61.h"
namespace lo::semantic::gpu::mesh_cook_storage61 {
using Registers = mesh_auxiliary_storage61::Registers;
using Dependencies = mesh_auxiliary_storage61::Dependencies;
// BC58A0/B9E4B0 build the 348-byte mesh processing owner: embedded tree +8,
// six-word state +84, bounds +112 and auxiliary storage +156. BC51E8 releases
// optional +80 arrays, tree payload and virtual +288 object; BC5950/B9E518
// compose complete owner teardown. BBCD80/BBD4E0 free the four-array descriptor.
// BA01C0 frees two temporary mesh output arrays through its original allocator.
// All owned fields, base tables, live callback state and cleanup order follow
// PPC. Constructors intentionally leave unspecified fields untouched.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_cook_storage61
