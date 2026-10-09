#pragma once
#include "lo_semantics/geometry_query_dispatch61.h"
#include "lo_semantics/crt_random_thread61.h"
#include "lo_semantics/geometry_support61.h"
namespace lo::semantic::gpu::object_grid_probe61 {
using Registers=geometry_query_dispatch61::Registers;
using VectorState=geometry_query_dispatch61::VectorState;
struct Dependencies {
    geometry_query_dispatch61::Dependencies query;
    crt_random_thread61::Dependencies random;
};
// BB03B0 probes object r3 from borrowed point r4 using mode r5. Modes0/1/2
// select coordinate axes, mode3 one random unit direction, all other values
// three random unit directions. The object's +48 query source remains borrowed.
// A stack query is initialized/dispatched/torn down per direction. Odd hit
// counts vote inside; r3 reports a strict majority. Thread RNG callbacks,
// host FP control and the caller's local vector state remain live dependencies.
// Float-to-single stages intentionally differ between the one/three-ray paths.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&,VectorState&);
}
