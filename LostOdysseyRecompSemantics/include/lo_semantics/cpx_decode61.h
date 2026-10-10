#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::cpx_decode61 {
using Registers = manager_release_context61::Registers;
// CPX bit input, block parameters, match lengths and stored/LZ block decoding.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Registers &);
} // namespace lo::semantic::gpu::cpx_decode61
