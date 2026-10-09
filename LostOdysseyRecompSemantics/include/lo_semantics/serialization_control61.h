#pragma once
#include "lo_semantics/growable_output61.h"
namespace lo::semantic::gpu::serialization_control61 {
using Registers = growable_output61::Registers;
using Dependencies = growable_output61::Dependencies;
// E948 writes scalar r3, optionally byte-reversed by r4 low byte, via borrowed
// writer r5 vtable slot +36. E9B8 installs a nonnull context at 832DF588 only
// when empty; an existing context emits the original diagnostic and returns0.
// B9CB70 reads the global endian mode: 0 ->1, 1/2 ->0, other values return the
// original caller scratch byte at SP-16. Do not replace that fallback with a
// host-endian default or add a policy to accept duplicate registration.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::serialization_control61
