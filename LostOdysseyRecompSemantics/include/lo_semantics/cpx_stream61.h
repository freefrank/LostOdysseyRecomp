#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::cpx_stream61 {
using Registers = manager_release_context61::Registers;
class GuestServices : public manager_release_context61::GuestServices {
public:
  virtual std::uint64_t ReadTimeBase() = 0;
  // Model the guest reservation-loop status exchange with runtime atomicity.
  virtual void ExchangeStatus(GuestMemory &, GuestAddress, std::uint32_t) = 0;
};
struct Dependencies {
  GuestServices &guest;
  float_triplet_transfer::NativeServices &fp;
};
// Time-budgeted read/decompress state machine; I/O, clock and allocator
// services are explicit boundaries, while CPX context/block decoding is
// composed locally.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::cpx_stream61
