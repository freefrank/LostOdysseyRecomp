#pragma once
#include "lo_semantics/crt_async_status_transfer.h"

namespace lo::semantic::gpu::grid_neighbor_update61 {
using Registers = crt_async_status_transfer::Registers;

// 82BB23C0: borrowed object r3 has cubic side length +88, plane stride +92,
// and a borrowed word buffer +108. For each cell, mark bit 30 if bit 31 is
// set in every in-bounds corner of its positive-axis 2x2x2 neighborhood.
// Existing bits remain intact. Returns 1, with no allocation or callbacks.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state);
}
