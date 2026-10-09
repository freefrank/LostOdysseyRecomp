#pragma once
#include "lo_semantics/grid_transform_pipeline61.h"
namespace lo::semantic::gpu::grid_blob_routes61 {
using Registers=grid_transform_pipeline61::Registers;
using VectorState=grid_transform_pipeline61::VectorState;
using Dependencies=grid_transform_pipeline61::Dependencies;
// B9DD90 loads borrowed {byte count, byte pointer} r4 into source-owner r3's
// owned grid at +184. A prior grid is deleted through its live virtual slot;
// failure deletes the newly attached grid and emits the existing diagnostic.
// BA60F8 builds a temporary grid from source-wrapper r4+8, dimension r5 and
// optional diagnostic sink r6, then returns owned {byte count, byte pointer}
// at r3. Ownership of the guest-allocated copy passes to the caller; stack grid/writer are
// disposed before return. No host ownership or allocation-failure guard added.
// All lower calls share Full scalar, local vector and diagnostic machine state.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&,VectorState&);
}
