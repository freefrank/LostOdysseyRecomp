#pragma once
#include "lo_semantics/mesh_convex_hull61.h"
namespace lo::semantic::gpu::mesh_cook_hull61 {
using Registers = mesh_convex_hull61::Registers;
using Dependencies = mesh_convex_hull61::Dependencies;
// BB3350 builds hull then valence. B9E3F8 owns a temporary mesh adapter/cache.
// B9E7B0 copies strided input points to guest stack, cooks owner+156 geometry,
// and sets owner+108 bit0 only after success. Inputs stay borrowed.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_cook_hull61
