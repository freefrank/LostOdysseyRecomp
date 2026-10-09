#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
namespace lo::semantic::gpu::grid_transform_routes61 {
using Registers=crt_close_recursive_buffer_context::Registers;
// BD7950 initializes a borrowed 20-byte transform reader: the accepted base
// initializer zeros four pointer/state fields, then the derived vtable replaces
// the base vtable. Returns the same guest object in r3; no allocation/ownership
// transfer. BD12D0 exposes that already mapped base initializer for composition.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Registers&);
}
