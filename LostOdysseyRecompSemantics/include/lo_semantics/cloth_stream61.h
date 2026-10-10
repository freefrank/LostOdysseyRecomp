#pragma once
#include "lo_semantics/mesh_stream_codec61.h"
namespace lo::semantic::gpu::cloth_stream61 {
using Registers = mesh_stream_codec61::Registers;
using Dependencies = mesh_stream_codec61::Dependencies;
// NXS/CLTH version 3 output for both source topology types.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::cloth_stream61
