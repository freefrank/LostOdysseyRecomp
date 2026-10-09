#pragma once
#include "lo_semantics/power_fp_support61.h"
namespace lo::semantic::gpu::power_math61 {
using Registers = power_fp_support61::Registers;
// B7E860 binary64 power: integer fast path, tabulated log/exp reduction,
// sign/parity and nonfinite routes. Guest coefficients and staged arithmetic
// remain explicit; this is not a host pow substitution.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::power_math61
