#pragma once
#include "lo_semantics/cloth_storage61.h"
namespace lo::semantic::gpu::cloth_face_map61 {
using Registers = cloth_storage61::Registers;
using Dependencies = cloth_storage61::Dependencies;
// Canonicalize unordered triangle vertex keys and select the lowest face ID.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies,
                         Registers &);
} // namespace lo::semantic::gpu::cloth_face_map61
