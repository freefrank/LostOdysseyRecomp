#pragma once
#include "lo_semantics/tree_scalar_write61.h"
namespace lo::semantic::gpu::tree_flat_write61 {
using Registers = tree_scalar_write61::Registers;
using Dependencies = tree_scalar_write61::Dependencies;
// D868 writes 36-byte full records; DB20 writes 32-byte internal-only records.
// Owner r3, endian flag r4 low byte, borrowed writer r5. Count precedes records;
// six floats are loaded/stored through FP as original, followed by topology.
// Optional endian conversion affects each 32-bit word in stack staging only.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::tree_flat_write61
