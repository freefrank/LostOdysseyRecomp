#pragma once
#include "lo_semantics/diagnostic_format_routes61.h"
#include "lo_semantics/mesh_mass_math61.h"
namespace lo::semantic::gpu::mesh_mass_cache61 {
using Registers = mesh_mass_math61::Registers;
struct Dependencies {
    float_triplet_transfer::NativeServices &fp;
    diagnostic_format_routes61::Dependencies diagnostics;
};
// Full-context adapter to the already recovered CRT classifier; no new entry.
void Classify(GuestMemory &, float_triplet_transfer::NativeServices &, Registers &);
// B9F418 lazily caches unit-density mass +292, origin inertia +296 and centroid
// +332. Invalid values fail; negative signed mass logs and flips mass/inertia.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_mass_cache61
