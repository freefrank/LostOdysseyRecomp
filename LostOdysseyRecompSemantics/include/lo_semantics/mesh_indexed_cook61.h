#pragma once
#include "lo_semantics/mesh_cook_hull61.h"
namespace lo::semantic::gpu::mesh_indexed_cook61 {
using Registers = mesh_cook_hull61::Registers;
using Dependencies = mesh_cook_hull61::Dependencies;
// Indexed mesh cooking: owned geometry, polygon construction/validation,
// adapter metadata, strided input conversion and B9F198 validation/build
// orchestration into tree, bounds and support data. Logical/ABI implementation;
// volatile-register equivalence is not asserted for these high-level entries.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
}
