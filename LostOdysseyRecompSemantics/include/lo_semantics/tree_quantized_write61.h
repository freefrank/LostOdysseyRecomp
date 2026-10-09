#pragma once
#include "lo_semantics/tree_scalar_write61.h"
namespace lo::semantic::gpu::tree_quantized_write61 {
using Registers = tree_scalar_write61::Registers;
using Dependencies = tree_scalar_write61::Dependencies;
// C838 writes owner r3 count, 24-byte quantized nodes, then six decoding
// scales to borrowed writer r5. r4 low byte controls endian conversion:
// six 16-bit coordinates and three 32-bit topology words per node. Each node
// is staged on the guest stack, leaving owned storage unchanged. Known word,
// float and block append lowers execute directly; allocation stays borrowed.
// B660 writes the 20-byte variant: six halfwords plus two topology words.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::tree_quantized_write61
