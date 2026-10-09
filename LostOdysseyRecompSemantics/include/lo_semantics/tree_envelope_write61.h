#pragma once
#include "lo_semantics/tree_scalar_write61.h"
namespace lo::semantic::gpu::tree_envelope_write61 {
using Registers = tree_scalar_write61::Registers;
using Dependencies = tree_scalar_write61::Dependencies;
// OPC base envelope and HBM mesh mapping envelope write to a borrowed linked
// stream. Strategy payload remains a mutable virtual callback. 8550 emits
// u8/u16/u32 adaptive indices; 7CB8 writes a three-byte tag plus endian flag.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::tree_envelope_write61
