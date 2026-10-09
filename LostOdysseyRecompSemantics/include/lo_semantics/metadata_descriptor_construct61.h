#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
namespace lo::semantic::gpu::metadata_descriptor_construct61 {
using Registers=crt_async_status_transfer::Registers;
// CC58 initializes a borrowed 140-byte base descriptor: +8 flags are incoming
// r8 OR mandatory0x0400008000004000; r5/r6/r7 populate +80/+44/+40. When the
// global registration gate is0, +32 links the previous pending-list head and
// the new object becomes head. Storage stays caller-owned; no allocation.
// 10A28 extends this to376 bytes. Incoming r6 supplies +184 flags (OR128),
// r7 lowbyte supplies +192, r8/r9 supply base+44/+40, r10 supplies +200.
// Caller stack +80 supplies64-bit base flags; +92/+100/+108 supply +284/+288/
// +292. It intentionally writes home slots +55(byte),+76(word) before calling
// CC58; remaining fields are initialized in their original read/store order.
// Offsets with unresolved meaning remain offsets rather than invented types.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Registers&);
}
