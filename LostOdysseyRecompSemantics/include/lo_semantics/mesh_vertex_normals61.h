#pragma once
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_auxiliary_storage61.h"
namespace lo::semantic::gpu::mesh_vertex_normals61 {
using Registers = mesh_auxiliary_storage61::Registers;
struct Dependencies {
    mesh_auxiliary_storage61::Dependencies lifetime;
    crt_reader_chain61::Dependencies memory;
};
// Descriptor: vertex count/positions +0/+4, triangle count +8, u32/u16 indices
// +12/+16, angle-weight flag +20, optional borrowed face/vertex outputs +24/+28.
// The two-word owner retains only newly allocated outputs. BB9160 replaces
// mesh +20 with negated angle-weighted vertex normals and frees temporary faces.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_vertex_normals61
