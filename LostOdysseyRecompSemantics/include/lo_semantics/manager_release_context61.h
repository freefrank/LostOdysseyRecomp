#pragma once
#include "lo_semantics/owned_tree_cleanup61.h"
namespace lo::semantic::gpu::manager_release_context61 {
using Registers=owned_tree_cleanup61::Registers;
class GuestServices:public owned_tree_cleanup61::GuestServices {
public:
    // Manager initialization's allocator and concrete constructors remain
    // explicit mutable guest calls, not assumed host allocation or no-ops.
    virtual void CallDirect(GuestAddress,GuestMemory&,Registers&)=0;
};
struct Dependencies {
    GuestServices& guest;
    float_triplet_transfer::NativeServices& fp;
};
// 823F3340 releases r3 through global manager 8330B608, lazily initializing it
// via the accepted 827C5F38 flow, then reloading it and calling vtable+12.
// 82388B58 is a true tail alias. BDB260 first runs BDAC88 owned cleanup, releases
// its optional +8 allocation through that alias, then clears +8/+12 as in PPC.
// All Full Registers stay live across initialization/service calls. The narrow
// existing initializer only edits its represented fields; the bridge retains
// callback changes to every other FPR/CR field in the surrounding full state.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
