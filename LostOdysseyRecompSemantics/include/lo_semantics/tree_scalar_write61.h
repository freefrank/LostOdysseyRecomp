#pragma once
#include "lo_semantics/crt_reader_float61.h"
namespace lo::semantic::gpu::tree_scalar_write61 {
using Registers = crt_reader_float61::Registers;
using Dependencies = crt_reader_float61::Dependencies;
// 7D58 writes integer r3, 7E18 writes float f1, to borrowed writer r5.
// Low byte of r4 requests byte reversal. The float route reinterprets the
// swapped word as float and uses the actual float append lower; its FP and
// mutable allocation behavior must not be replaced by a raw-byte shortcut.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::tree_scalar_write61
