#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::transform_owner_routes61 {
using Registers=manager_release_context61::Registers;
using Dependencies=manager_release_context61::Dependencies;
// BD1558 disposes the owned +12 transform storage (BDB260 then guest free)
// and invokes +16 object's virtual deleting destructor with r4=1. Each slot
// clears after its callback using live r29/r31. BD1770 installs the base table
// then tail-calls this cleanup; BD7A20 installs the derived table, cleans up,
// then invokes that base route. No host ownership or C++ destructor machinery.
// BDB310/BDB610/BDBD58/BDC1D0 expose already mapped 12-byte constructors through
// the same full-state interface; they add no new mapping credit.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
