#pragma once
#include "lo_semantics/mesh_indexed_cook61.h"
namespace lo::semantic::gpu::mesh_hull_preprocess61 {
using Registers=mesh_indexed_cook61::Registers;
using Dependencies=mesh_indexed_cook61::Dependencies;
// BA0230 normalizes/deduplicates point input and repairs degenerate bounds.
// BA0998 compacts referenced points and remaps indices in first-use order.
// BA5CF8 owns preparation/output buffers; BA5A70 selects plain/inflated hull
// and packs triangulated output. Dynamic word/triangle arrays are concrete.
// BA4BF8/BA5480 remain live guest algorithm boundaries; plain hull is concrete. Logical/ABI only.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
