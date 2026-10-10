#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::archive_names61 {
using Registers = manager_release_context61::Registers;
// Packed base-40 names, ASCII folding, path segments and archive-prefix
// candidates.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Registers &);
} // namespace lo::semantic::gpu::archive_names61
