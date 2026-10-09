#pragma once
#include "lo_semantics/tree_flat_strategy61.h"
namespace lo::semantic::gpu::tree_mesh_callbacks61 {
using Registers = tree_flat_strategy61::Registers;
using Dependencies = tree_flat_strategy61::Dependencies;
// Address-based adapter for concrete tree geometry, four strategy binders and their cleanup callbacks.
// The caller supplies borrowed guest allocation services and FP control.
// Unknown targets return false without touching state; the caller can forward
// them to its external service boundary. No vtable or runtime hook is installed.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, Dependencies, Registers&);
}
