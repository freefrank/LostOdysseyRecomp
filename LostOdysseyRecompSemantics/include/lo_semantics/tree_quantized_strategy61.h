#pragma once
#include "lo_semantics/tree_flat_strategy61.h"
namespace lo::semantic::gpu::tree_quantized_strategy61 {
using Registers = tree_flat_strategy61::Registers;
using Dependencies = tree_flat_strategy61::Dependencies;
// C208 flattens borrowed tree r4, finds six global absolute maxima, and packs
// 36-byte float nodes into owned 24-byte nodes: signed 16-bit center xyz,
// unsigned 16-bit half extents and three unchanged topology words. Owner r3
// +12..+32 stores six decoding scales. Global 83216670 enables conservative
// extent enlargement and reduces extent quantization to 15 bits. Temporary
// and final buffers have a four-byte count prefix. Preserve original failure
// ordering, including temporary allocation lifetime on a later failure.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::tree_quantized_strategy61
