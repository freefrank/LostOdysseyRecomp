#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::text_bank61 {
using Registers = manager_release_context61::Registers;
using Dependencies = manager_release_context61::Dependencies;
// Decode offset-based string banks, consume 1/7-column records, and borrow menu rows.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::text_bank61
