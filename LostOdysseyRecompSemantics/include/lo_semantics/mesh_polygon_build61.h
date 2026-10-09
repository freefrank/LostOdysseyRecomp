#pragma once
#include "lo_semantics/mesh_polygon_collect61.h"
namespace lo::semantic::gpu::mesh_polygon_build61 {
using Registers = mesh_polygon_collect61::Registers;
using Dependencies = mesh_polygon_collect61::Dependencies;
// BB9AA8 replaces mesh polygon records (+36 count,+40 owned 36-byte records,
// +44 owned byte indices), derives outward planes and projection ranges,
// then rebuilds triangle fans. Record fields: u16 degree +0, borrowed index
// slice +4, plane +12..24 and min/max vertex projection +28/+32.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_polygon_build61
