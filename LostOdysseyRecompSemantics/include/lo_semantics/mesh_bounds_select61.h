#pragma once
#include "lo_semantics/mesh_stream_write61.h"
namespace lo::semantic::gpu::mesh_bounds_select61 {
using Registers = mesh_stream_write61::Registers;
using Dependencies = mesh_stream_write61::Dependencies;
// B9EA90 publishes owner min/max +112/+124, sphere +136 and tolerance +152
// using the actual guest power path for the fixed 2^-22 multiplier.
// BC9AF0 allocates a temporary point-pointer array through the cooking context,
// recursively improves its sphere and frees the array. BC9C68 compares it with
// the axis-extreme sphere, choosing the valid nonnegative smaller candidate.
// Context allocation slots +8/+20 are live calls; no new ownership of points.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_bounds_select61
