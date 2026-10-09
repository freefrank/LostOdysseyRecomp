#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
namespace lo::semantic::gpu::manager_cached_links61 {
using Registers=crt_async_status_transfer::Registers;
// 824002F0 consumes two global dirty flags over a borrowed object-pointer list.
// First pass: forty-byte entries at object+184/+188 clear cached fields +32/+36;
// keep +28 only when its target's 64-bit flags+8 contain bit58.
// Second pass: 108-byte entries at object+196/+200 with nonnull +48 and flag+76
// bit0 detach the target's owner+28/index+32 and clear the reciprocal entry.
// These are reference updates only: no allocation, destruction or host ownership.
// Both dirty flags clear after their pass; list/count/entry addresses are live
// guest reads, including when updates alias other ordinary mapped RAM fields.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Registers&);
}
