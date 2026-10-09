#pragma once
#include "lo_semantics/object_sort_dispatch61.h"
namespace lo::semantic::gpu::object_sort_export61 {
using Registers=object_sort_dispatch61::Registers;
using Dependencies=object_sort_dispatch61::Dependencies;
// B9DF18 measures the encoded object referenced by owner r3 +184.
// B9DFA0 exports to borrowed r4->{exact byte count, destination}, returning
// 1 only when the encoded length matches. Both own and release a temporary
// guest writer; all accepted lowers share mutable full register state.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory&,Dependencies,Registers&);
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress entry,GuestMemory&,Dependencies,Registers&);
}
