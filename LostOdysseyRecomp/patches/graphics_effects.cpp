#include <stdafx.h>
#include <atomic>
#include <os/logger.h>
#include "gpu/post_blur.h"
#include "settings/config.h"

// Player switches for two of the game's own effects, the recompiled
// counterparts of the Xenia Canary community patches "Disable Motion Blur" and
// "Disable Dynamic Shadows" (author boma). Both read the live settings, so a
// saved change applies from the next frame.

extern "C" PPC_FUNC(__imp__sub_826E1860);
extern "C" PPC_FUNC(__imp__sub_823D5E50);

namespace
{
// Logs each on/off transition once, so run logs show which state a scene used.
bool Logged(std::atomic<int>& last, bool enabled, const char* effect)
{
    if (last.exchange(enabled ? 1 : 0, std::memory_order_relaxed) != (enabled ? 1 : 0))
        LOG_INFO("graphics effects: {} {}", effect, enabled ? "on" : "off");
    return enabled;
}

std::atomic<int> motionBlurState{-1};
std::atomic<int> shadowState{-1};
}

// Render method of the motion blur post-process proxy (vtable entry 8220356C).
// Its own early exit at 826E1EDC returns 0 ("nothing rendered") and changes no
// guest state, which is where Xenia's inverted beq at 826E1884 sends it.
PPC_FUNC(sub_826E1860)
{
    if (!Logged(motionBlurState, settings::GetConfig().motionBlur, "motion blur"))
    {
        ctx.r3.u64 = 0;
        return;
    }
    __imp__sub_826E1860(ctx, base);
}

// Renders the shadow depths for one light's shadow casters. Its only callers
// are 823D5AA8 (light pass; the result is unused) and 823DE090 (modulated
// shadows; the result is ORed into the "scene colour changed" flag). Xenia
// removes both calls: a branch over 823D5AA8 and a nop at 823DE090, after
// which r3 still holds the renderer that the caller passed in r3. Returning
// without touching r3 reproduces both. Its third edit, be16 0x4800 at 823D5FCC,
// writes the bytes that are already there (the bl's link bit is in the low
// half), so it changes nothing.
PPC_FUNC(sub_823D5E50)
{
    if (!Logged(shadowState, settings::GetConfig().dynamicShadows != 0, "dynamic shadows"))
        return;
    __imp__sub_823D5E50(ctx, base);
}

// The depth of field and bloom post-process step (sub_823E2DE8) downsamples
// the scene to a quarter-size surface (sub_823E3900), blurs it twice into the
// depth of field texture (sub_823E4E70) and twice into the bloom texture
// (sub_823E6F48), then draws the tone map, which reads them in fetch slots 2
// and 3. At Depth of field 0% and with Bloom off those reads contribute
// nothing (gpu/post_blur.h), so the blurs that feed them are skipped, and the
// downsample when neither needs it. Each function only renders and resolves
// its own surfaces and returns nothing the caller reads. Only the calls from
// 823E2DE8 (return addresses 823E2E7C, 823E2F34, 823E2F5C) are skipped: the
// downsample has other callers.
extern "C" PPC_FUNC(__imp__sub_823E3900);
extern "C" PPC_FUNC(__imp__sub_823E4E70);
extern "C" PPC_FUNC(__imp__sub_823E6F48);

namespace
{
std::atomic<int> depthOfFieldBlurState{-1};
std::atomic<int> bloomBlurState{-1};
}

PPC_FUNC(sub_823E3900)
{
    if (ctx.lr == 0x823E2E7C)
    {
        const auto config = settings::GetConfig();
        if (gpu::post_blur::SkipDepthOfField(config) && gpu::post_blur::SkipBloom(config))
            return;
    }
    __imp__sub_823E3900(ctx, base);
}

PPC_FUNC(sub_823E4E70)
{
    if (ctx.lr == 0x823E2F34 &&
        !Logged(depthOfFieldBlurState, !gpu::post_blur::SkipDepthOfField(settings::GetConfig()), "depth of field blur"))
        return;
    __imp__sub_823E4E70(ctx, base);
}

PPC_FUNC(sub_823E6F48)
{
    if (ctx.lr == 0x823E2F5C && !Logged(bloomBlurState, !gpu::post_blur::SkipBloom(settings::GetConfig()), "bloom blur"))
        return;
    __imp__sub_823E6F48(ctx, base);
}
