#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
namespace lo::semantic::gpu::tree_flat_load61 {
using Registers=crt_close_recursive_buffer_context::Registers;
using GuestServices=crt_close_recursive_buffer_context::GuestServices;
// BED8 borrows reader r5: +12 returns node count, +24 reads payload to r4/r5.
// Owner r3 replaces count-prefixed 36-byte node storage; r4 low byte requests
// byte swapping for count and all nine words per node. Count is changed before
// allocation, old storage is freed first, and read callback status is ignored,
// exactly as the original. No new rollback or input-validation contract.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,GuestServices&,Registers&);
}
