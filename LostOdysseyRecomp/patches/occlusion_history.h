#pragma once

#include <cstdint>

struct PPCContext;

// Policy::History guest hooks (patches/occlusion_history.cpp,
// gpu/occlusion_history.h). Guest render thread only.
namespace occlusion_history
{
    // The owner to report for a query the game's pool handed out from call
    // site `caller`, or 0 to keep the default (OwnerKey) owner.
    uint64_t IssueOwner(uint32_t query, uint32_t caller);

    // sub_823CF390 (blocking GetData loop): answers the game's deferred reads.
    // False when the caller is not one of them; the loop then runs as before.
    bool Read(PPCContext& ctx, uint8_t* base);
}
