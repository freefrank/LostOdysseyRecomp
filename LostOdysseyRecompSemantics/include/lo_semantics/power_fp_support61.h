#pragma once
#include "lo_semantics/mesh_bounds_math61.h"
namespace lo::semantic::gpu::power_fp_support61 {
using Registers = mesh_bounds_math61::Registers;
// B7E668 classifies integer parity (0 noninteger,1 odd,2 even). B822F0 reads
// the binary64 exponent; B822C8 replaces it; B823C8 decomposes into a signed
// [0.5,1) mantissa and writes exponent through r4, including subnormal shifts.
// Guest spills, flag state and native rounding mode remain observable.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::power_fp_support61
