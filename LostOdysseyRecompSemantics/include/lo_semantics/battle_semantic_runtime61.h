#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::battle_semantic_runtime61 {
using Registers = manager_release_context61::Registers;
using Dependencies = manager_release_context61::Dependencies;
// Enter the recovered battle graph. Nested direct and indirect calls to known
// battle entries stay in handwritten semantics; unknown services are forwarded
// to the supplied guest bridge. Unknown top-level entries return false without
// changing state. This is an integration entry point, not gameplay acceptance.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::battle_semantic_runtime61
