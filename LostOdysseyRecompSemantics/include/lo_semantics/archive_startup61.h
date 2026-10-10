#pragma once
#include "lo_semantics/archive_loader61.h"
namespace lo::semantic::gpu::archive_startup61 {
using Registers = archive_loader61::Registers;
using Dependencies = archive_loader61::Dependencies;
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::archive_startup61
