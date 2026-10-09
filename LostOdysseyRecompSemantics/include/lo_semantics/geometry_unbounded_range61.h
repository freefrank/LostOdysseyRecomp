#pragma once
#include "lo_semantics/geometry_math61.h"
#include <array>
namespace lo::semantic::gpu::geometry_unbounded_range61 {
using Registers=geometry_math61::Registers;
using Vector=std::array<std::uint32_t,4>;
// Raw lanes match generated ctx.vN.u32[0..3]. Lane0 is the low host lane;
// a guest big-endian aligned quadword maps guest words +12,+8,+4,+0 here.
// This state is borrowed independently of Full72; no global ABI is extended.
struct VectorState { std::array<Vector,128> v{}; };
class GuestServices {
public:
    virtual ~GuestServices()=default;
    virtual void CallIndirect(GuestAddress target,GuestMemory& memory,
        Registers& scalar,VectorState& vectors)=0;
};
struct Dependencies { GuestServices& guest; float_triplet_transfer::NativeServices& fp; };
// BD6E28: r3 query, r4 first 36-byte node, r5 exclusive end. Node +0 center,
// +12 half extent, +24 leaf/type bits, +32 subtree skip count. Query origin,
// direction, absolute direction are at +16/+28/+40. Separating-axis ray/box
// rejection skips internal subtrees; accepted leaves invoke BD4448. Query +96
// counts visited nodes. Vector125..127 and GPR26..31 are stack-saved; volatile
// vectors and scalar state remain observable, including across guest callbacks.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,
    Dependencies deps,Registers& scalar,VectorState& vectors);
}
