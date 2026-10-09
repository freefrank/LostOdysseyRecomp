#pragma once
#include "lo_semantics/crt_close_reader_callers_context.h"
#include "lo_semantics/object_sort_reader61.h"
namespace lo::semantic::gpu::object_grid_read61 {
using Registers=object_sort_reader61::Registers;
struct Dependencies {
    crt_close_reader_callers_context::Dependencies reader;
    float_triplet_transfer::NativeServices& fp;
};
// BB2098: r3 borrows a grid, r4 is an optional pathname, r5 an optional reader.
// Reset cells to -1; decode color groups (implicit successive ID or explicit
// u32), scatter decoded coordinates, then apply one occupancy bit per cell.
// Explicit ID -1 ends groups. A missing reader is allocated/loaded temporarily
// and disposed after reading; a supplied reader remains borrowed. The original
// pathname check and allocator failure behavior are preserved without guards.
// BD0A30 is the connected bounded seek: r3 reader, r4 new byte offset; update
// node used/current pointer only when offset is strictly below node capacity.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
