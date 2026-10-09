#pragma once
#include "lo_semantics/crt_reader_float61.h"
namespace lo::semantic::gpu::cube_projection_table61 {
using Registers=crt_reader_float61::Registers;
using Dependencies=crt_reader_float61::Dependencies;
// BB38A0 writes resolution and 6*resolution^2 into owner+4's descriptor,
// prepares the destination through slot+4, samples normalized directions on
// six cube faces through slot+8, then calls slot+12. Slots retain live Full
// state. BB3430 prepares the projection consumer's two owned byte-index tables
// at (owner+8)->+24/+28, rejecting point counts above255 before allocation.
// Allocation ownership is attached to that storage; prior table disposal is
// not added. Resolution0 skips samples; resolution1/nonfinite behavior is not
// part of focused validation. Float stages, caller scratch and frames remain
// explicit for a future Rust implementation; no host allocation is introduced.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
