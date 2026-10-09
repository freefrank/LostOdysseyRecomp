#pragma once
#include "lo_semantics/reader_buffer_growth61.h"
namespace lo::semantic::gpu::geometry_triangle_range61 {
using Registers = reader_buffer_growth61::Registers;
using Dependencies = reader_buffer_growth61::Dependencies;
// r3 borrows a ray query, r4 selects a packed leaf. Query +8 points to a leaf
// table (+24 descriptors, +32 optional triangle-index list); low four descriptor
// bits encode count-1, upper bits encode the list offset or first triangle.
// Query +12 borrows mesh arrays (+16 index triplets, +20 float vertex triplets).
// Ray origin/direction are +16/+28; +100/+104 count tested/accepted triangles.
// +76 is a four-word hit (triangle, distance, barycentric u/v), +132 the current
// distance limit, +136 tolerance, +140 closest-only, +141 backface culling.
// Optional +92 is a borrowed growable word-array descriptor. Its guest-owned
// payload can be reallocated through live callbacks. No host storage is retained.
// Each accepted closer hit updates ray bounds +40..72 and narrows the limit.
// Flags +4 bit0 stop on the first accepted hit; bit2 records a hit. FP stages
// retain the generated single rounding boundaries, not a host vector abstraction.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies deps, Registers& state);
}
