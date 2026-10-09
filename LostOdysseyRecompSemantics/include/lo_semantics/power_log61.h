#pragma once
#include "lo_semantics/power_fp_support61.h"
namespace lo::semantic::gpu::power_log61 {
using Registers = power_fp_support61::Registers;
// 82301A68: guest-constant natural logarithm with subnormal normalization,
// rational mantissa correction and split exponent contribution. Constants
// remain guest-resident; no host log substitution or owned memory.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, float_triplet_transfer::NativeServices &,
                         Registers &);
} // namespace lo::semantic::gpu::power_log61
