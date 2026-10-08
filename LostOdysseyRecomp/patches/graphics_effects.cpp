#include <stdafx.h>
#include <atomic>
#include <os/logger.h>
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
    if (!Logged(shadowState, settings::GetConfig().dynamicShadows, "dynamic shadows"))
        return;
    __imp__sub_823D5E50(ctx, base);
}
