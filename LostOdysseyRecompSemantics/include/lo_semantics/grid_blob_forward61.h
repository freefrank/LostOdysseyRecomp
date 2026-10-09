#pragma once
#include "lo_semantics/grid_blob_routes61.h"
namespace lo::semantic::gpu::grid_blob_forward61 {
using Registers=grid_blob_routes61::Registers;
using VectorState=grid_blob_routes61::VectorState;
using Dependencies=grid_blob_routes61::Dependencies;
// B9CBC0 forwards r4 output, r5 source-wrapper, r6 dimension and r7 optional
// diagnostic sink to BA60F8, discarding incoming r3. This is a true tail call:
// no frame/LR change is introduced before the shared serializer. Output owns
// {byte count, guest-allocated byte pointer}; ownership passes to its caller.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&,VectorState&);
}
