#pragma once
#include "lo_semantics/crt_reader_float61.h"
namespace lo::semantic::gpu::mesh_boundary_walk61 {
using Registers = crt_reader_float61::Registers;
using Dependencies = crt_reader_float61::Dependencies;
// BB8498 collects a component through untagged triangle links, using borrowed
// visited bytes. BC2A18 cancels duplicate undirected pairs then orders remaining
// edges into a chain, preserving partial output on disconnected input.
// BD2988 replaces word-array capacity; BD2B90 copies an array into owned storage.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_boundary_walk61
