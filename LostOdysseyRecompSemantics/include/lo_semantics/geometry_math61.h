#pragma once
#include "lo_semantics/crt_reader_float61.h"
namespace lo::semantic::gpu::geometry_math61 {
using Registers = crt_reader_float61::Registers;
using Dependencies = crt_reader_float61::Dependencies;
// BD4448 visits a packed triangle leaf of the borrowed query object r3 (leaf
// index r4). Mesh indices may be indirect or consecutive. Intersection scratch
// is stored at +76..+88; counters and hit flags update in place. Optional +92
// result storage selects nearest replacement or append through accepted growth
// and copy lowers. Input/output memory and services stay borrowed.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, Dependencies, Registers&);
}
