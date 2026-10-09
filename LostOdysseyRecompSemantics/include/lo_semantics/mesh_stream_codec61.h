#pragma once
#include "lo_semantics/growable_output61.h"
namespace lo::semantic::gpu::mesh_stream_codec61 {
using Registers=growable_output61::Registers;
using Dependencies=growable_output61::Dependencies;
// NXS headers, endian-aware scalar/array reads and writes on borrowed streams.
// Logical/ABI recovery; stream vtables remain live, with no added bounds policy.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
