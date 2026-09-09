#pragma once
#include "shader/position_evidence.h"
#include "temporal_scene.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gpu::temporal {
// Independent diagnostics for an unknown shader, evaluated BEFORE jitter edits.
// Bits: anchor / exact position camera / full viewport / depth predicate / finite.
inline uint32_t PositionGuards(const position_evidence::Summary& position, bool compatibleViewport,
    const SceneAnchor* anchor, uint64_t depthAllocation, const Viewport& viewport, const uint32_t* constants)
{
    uint32_t guards=anchor?1u:0u;
    if(anchor && compatibleViewport && viewport.x==anchor->viewport.x && viewport.y==anchor->viewport.y &&
       viewport.width==anchor->viewport.width && viewport.height==anchor->viewport.height)guards|=4;
    if(anchor && (!depthAllocation || depthAllocation==anchor->depthAllocation))guards|=8;
    if(position.kind!=1 || position.slot<0 || position.slot>252 || !constants)return guards;
    const auto* vp=constants+position.slot*4;
    if(anchor && std::memcmp(vp,anchor->vpBits.data(),16*sizeof(uint32_t))==0)guards|=2;
    bool finite=true;
    for(unsigned i=0;i<16;++i)finite&=std::isfinite(std::bit_cast<float>(vp[i]));
    if(finite)guards|=16;
    return guards;
}
}
