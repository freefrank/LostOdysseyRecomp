#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::archive_metadata61 {
using Registers = manager_release_context61::Registers;
using Dependencies = manager_release_context61::Dependencies;
// Decode packed archive dates, bridge guest time conversion, and rewrite a
// suffix.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::archive_metadata61
