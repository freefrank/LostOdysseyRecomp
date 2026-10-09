#pragma once
#include "lo_semantics/object_grid_probe61.h"
#include "lo_semantics/object_grid_read61.h"
#include "lo_semantics/object_sort_dispatch61.h"
#include "lo_semantics/transform_owner_build61.h"
namespace lo::semantic::gpu::grid_transform_pipeline61 {
using Registers=object_grid_probe61::Registers;
using VectorState=object_grid_probe61::VectorState;
using MachineState=diagnostic_lock61::MachineState;
class GuestServices {
public:
    virtual ~GuestServices()=default;
    virtual void CallIndirect(GuestAddress,GuestMemory&,Registers&,VectorState&,MachineState&)=0;
};
struct Dependencies {
    object_grid_read61::Dependencies read;
    object_sort_dispatch61::Dependencies write;
    transform_owner_build61::Dependencies spatial;
    object_grid_probe61::Dependencies probe;
    GuestServices& guest;
};
// BB2638: r3 destination grid, r4 borrowed source mesh, r5 dimension,
// r6 optional pathname, r7 optional reader/writer, r8 load-existing flag,
// r9 optional diagnostic sink. Load a PMAP grid, or build a temporary spatial
// index, classify grid points with a borrowed thread RNG, propagate nearby
// classifications, transform and encode cells, and release guest temporaries.
// Full scalar state, caller-owned vector state and the diagnostic dependency's
// MachineState remain live through calls. Guest objects/payloads do not overlap
// this function's 640-byte frame and saved-register area. Finite FP inputs are
// the selected comparison contract; no host object ownership is introduced.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&,VectorState&);
}
