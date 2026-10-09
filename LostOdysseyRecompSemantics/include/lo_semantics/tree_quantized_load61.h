#pragma once
#include "lo_semantics/tree_flat_strategy61.h"
namespace lo::semantic::gpu::tree_quantized_load61 {
using Registers = tree_flat_strategy61::Registers;
using Dependencies = tree_flat_strategy61::Dependencies;
// C9F0 reads count, replaces 24-byte quantized nodes, and reads six float
// reconstruction scales. Borrowed reader r5 slots +12 (word) and +24 (block),
// owner r3, r4 low byte controls endian conversion. Each node contains six
// 16-bit coordinates followed by three 32-bit topology words. Allocation and
// read-result behavior match the original; no added bounds checks or rollback.
// B7F8 is the 20-byte variant: six halfwords plus two topology words.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::tree_quantized_load61
