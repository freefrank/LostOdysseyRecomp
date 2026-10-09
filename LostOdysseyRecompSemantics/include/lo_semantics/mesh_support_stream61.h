#pragma once
#include "lo_semantics/mesh_stream_write61.h"
namespace lo::semantic::gpu::mesh_support_stream61 {
using Registers = mesh_stream_write61::Registers;
using Dependencies = mesh_stream_write61::Dependencies;
// Borrowed support-map adapter: two aliases to source, ICE/SUPM and ICE/GAUS
// sections, two counts and two byte arrays. Destruction restores the base table;
// it does not own or release the source or destination stream.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_support_stream61
