#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::tree_box_centroid61 {
using Registers = crt_async_status_transfer::Registers;
// Second-tree vtable 820D6300 slots +12/+16 borrow builder r3. Builder+72
// points to packed six-float boxes (minimum xyz, maximum xyz), stride 24.
// BD8848: r4 box index, r5 axis, centroid coordinate returned in f1.
// BD8888: r4 box index, r5 borrowed output xyz. All six inputs are loaded
// before output writes, including when output overlaps the selected box.
// Binary32 addition and multiplication are separate stages; retain the guest
// half constant and observable scratch registers for eventual Rust migration.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&,
                        float_triplet_transfer::NativeServices&, Registers&);
}
