#pragma once
#include <cstdlib>
#include <cstring>
#include "settings/config.h"

// The game's depth of field and bloom are two quarter-size blur chains that only
// the tone-map pass reads: the depth of field blur in fetch slot 2, where a
// pixel's depth weight is above c255.x, and the bloom in slot 3, scaled by c7.x.
// Depth of field at 0% scales the weight to 0 and Bloom off zeroes c7.x
// (renderer.cpp), so the guest skips the chain that would feed nothing
// (patches/graphics_effects.cpp) and the tone map reads the black dummy texture
// in that slot: the image is the same as with the chain running.
// LO_POST_BLUR_SKIP=0 keeps the chains running.
namespace gpu::post_blur
{
inline bool SkipEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_POST_BLUR_SKIP");
        return !value || std::strcmp(value, "0") != 0;
    }();
    return enabled;
}
inline bool SkipDepthOfField(const settings::Config& config) { return SkipEnabled() && config.depthOfFieldPercent == 0; }
inline bool SkipBloom(const settings::Config& config) { return SkipEnabled() && !config.bloom; }
}
