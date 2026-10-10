#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::archive_member61 {
using Registers = manager_release_context61::Registers;
using Dependencies = manager_release_context61::Dependencies;
// Match recursive archive entries and compose candidate lookup/file metadata.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::archive_member61
