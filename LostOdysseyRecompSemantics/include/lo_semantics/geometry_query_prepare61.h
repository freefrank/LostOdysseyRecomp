#pragma once
#include "lo_semantics/reader_buffer_growth61.h"
namespace lo::semantic::gpu::geometry_query_prepare61 {
using Registers=reader_buffer_growth61::Registers;
using Dependencies=reader_buffer_growth61::Dependencies;
// r3 borrows a query, r4 a six-float origin/direction ray, r5 an optional matrix,
// r6 an optional cached triangle index. The matrix uses three four-float axes
// and translation at +48/+52/+56; transform arithmetic/order is preserved.
// Query +8 tree metadata (+8 flags), +12 mesh (+16 indices,+20 vertices),
// +92 optional word-array output descriptor; all objects are borrowed. Reset
// counters +96/+100/+104 and output used count, clear hit flags2/3, copy/transform
// the ray into +16/+28. A single-triangle tree or eligible cached hit can resolve
// the query (r3=1); otherwise build ray bounds +40..72 and return0 for traversal.
// +132 is distance limit (0x7f7fffff means unbounded), +136 tolerance, +140
// closest-only, +141 backface culling. Result +76 contains triangle,t,u,v.
// Buffer growth owns guest payload allocation through borrowed mutable services.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,
    Dependencies deps,Registers& state);
}
