#pragma once
#include "lo_semantics/mesh_stream_codec61.h"
namespace lo::semantic::gpu::mesh_support_load61 {
using Registers = mesh_stream_codec61::Registers;
using Dependencies = mesh_stream_codec61::Dependencies;
// ICE/SUPM and GAUS support counts and owned dual byte-table loading.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::mesh_support_load61
