#pragma once
#include "lo_semantics/growable_output61.h"
namespace lo::semantic::gpu::mesh_stream_write61 {
using Registers = growable_output61::Registers;
using Dependencies = growable_output61::Dependencies;
// Borrowed virtual stream output: scalar f1, float span (r3/r4), word scalar, and NXS/ICE
// section prefix plus four-byte tag and version. Low-byte endian flags retain
// original float staging and byte order. No bounds/ownership policy added.
// BD7D00 adds halfword output; BD7E70/BD7FF0 share float scalar/span logic
// with in-place byte swapping and their distinct original scratch contracts.
// B9E528..B9E668 forward six writer slots through borrowed adapter +4 and
// return the adapter itself; no ownership transfer or runtime table install.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory &, Dependencies, Registers &);
} // namespace lo::semantic::gpu::mesh_stream_write61
