#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::curve_tangent_update61 {
using Registers=crt_async_status_transfer::Registers;
// 8262B498 borrows r3's point pointer/count at+0/+4. Each32-byte point stores
// two coordinates at+4/+8, tangent pairs at+12/+20, and mode byte at+28.
// Mode1 updates automatic tangents: first/last points clear +20/+12;
// a single point clears +20 regardless of its mode.
// interior points blend adjacent coordinate differences using input f1 and
// two guest float constants. Previous mode2 clears both pairs; other modes
// can retain existing pairs. No point storage is allocated or owned here.
// Each pair write retains the original pointer reload and FP rounding stages.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,float_triplet_transfer::NativeServices&,Registers&);
}
