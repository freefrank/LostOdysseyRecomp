#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::archive_loader61 {
using Registers = manager_release_context61::Registers;
class GuestServices : public manager_release_context61::GuestServices {
public:
  virtual void ExchangeStatus(GuestMemory &, GuestAddress, std::uint32_t) = 0;
};
struct Dependencies {
  GuestServices &guest;
  float_triplet_transfer::NativeServices &fp;
};
// FPI header conversion, locked read, and resident index load/relocation.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::archive_loader61
