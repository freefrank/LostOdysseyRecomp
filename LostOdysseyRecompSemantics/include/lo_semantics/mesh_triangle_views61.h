#pragma once
#include "lo_semantics/mesh_triangle_storage61.h"
namespace lo::semantic::gpu::mesh_triangle_views61 {
using Registers = mesh_triangle_storage61::Registers;
// Borrowed triangle channel metadata, optional auxiliary views and mass export.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Registers &);
} // namespace lo::semantic::gpu::mesh_triangle_views61
