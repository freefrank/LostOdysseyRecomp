#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::archive_index_fields61 {
using Registers = manager_release_context61::Registers;
// Swap archive descriptor fields, relocate recursive entry trees, and read CPX
// reserve size.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Registers &);
} // namespace lo::semantic::gpu::archive_index_fields61
