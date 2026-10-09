#pragma once
#include "lo_semantics/mesh_polygon_topology61.h"
namespace lo::semantic::gpu::mesh_geometry_stream61 {
using Registers = mesh_polygon_topology61::Registers;
using Dependencies = mesh_polygon_topology61::Dependencies;
// BBC110 writes ICE/CVHL v5 through a borrowed stream, lazily completing
// polygons, edge incidence, and vertex normals. Polygon pointers become byte
// offsets in the serialized copy. Two incidence scratch arrays are temporary;
// newly built geometry remains owned by the mesh. Keep endian and normal modes.
// BB3220 wraps CVHL in ICE/CLHL v0 and appends the lazily built VALE cache.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_geometry_stream61
