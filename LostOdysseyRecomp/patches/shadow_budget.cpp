#include <stdafx.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <vector>
#include <kernel/memory.h>
#include <os/logger.h>
#include "settings/config.h"

// Dynamic shadows Low: per-object shadows only for objects on screen, and for
// at most the LowCount largest of them. Each shadow costs a depth pass, its
// projection passes and their draws on the GPU and the render threads. In
// Uhra's main street the game makes about 60 a frame, 52 of them for people
// and objects that are not on screen; at 10 W they take a third of the GPU
// time.
//
// sub_823BE570 (InitDynamicShadows) runs once per frame and calls sub_823BE9D0
// for each light-primitive interaction (r3 the scene renderer, r4 the
// interaction). sub_823BE9D0 sizes the shadow from the primitive's screen size,
// the largest over the views of
//   max(view+0x24 * view+0x80, view+0x28 * view+0x94) * radius / max(depth, 1)
// (about the object's height in 720p pixels), then creates it unless the
// light's fade is zero; when it creates nothing it changes nothing. Low skips
// the interaction before that.
extern "C" PPC_FUNC(__imp__sub_823BE570);
extern "C" PPC_FUNC(__imp__sub_823BE9D0);

namespace
{
constexpr size_t LowCount = 8;
// A shadow already shown stays down to this fraction of the threshold, so
// objects of about the same size do not trade it every frame.
constexpr float KeepFraction = 0.8f;
// A caster that left the screen keeps its shadow this many frames, so
// visibility that changes from frame to frame does not make it flicker.
constexpr uint32_t OffScreenFrames = 15;

struct Caster
{
    uint32_t lastOnScreen = 0; // frame
    bool shadow = false;       // had a shadow in the frame before
};

// Render thread only.
bool low = false;
uint32_t frame = 1;
float threshold = 0.0f;              // the LowCount-th largest size of the previous frame
std::vector<float> sizes, lastSizes; // sizes of the on-screen casters this frame / the previous frame
// Keyed by primitive scene info. A primitive lit by two lights has two
// interactions; both see the same size, so they decide alike.
std::unordered_map<uint32_t, Caster> casters;

uint32_t LoadU32(uint32_t address)
{
    return __builtin_bswap32(*static_cast<const uint32_t*>(g_memory.Translate(address)));
}
float LoadF32(uint32_t address)
{
    return std::bit_cast<float>(LoadU32(address));
}

// The primitive's screen size as sub_823BE9D0 computes it; its bounds include
// the attached primitives (+0x8C, next +0x90), here as one enclosing sphere.
float ScreenSize(uint32_t renderer, uint32_t primitive)
{
    const float x = LoadF32(primitive + 0x28), y = LoadF32(primitive + 0x2C), z = LoadF32(primitive + 0x30);
    float radius = LoadF32(primitive + 0x40);
    for (uint32_t child = LoadU32(primitive + 0x8C), guard = 0; child && guard < 64; child = LoadU32(child + 0x90), ++guard)
    {
        const float dx = LoadF32(child + 0x28) - x, dy = LoadF32(child + 0x2C) - y, dz = LoadF32(child + 0x30) - z;
        radius = std::max(radius, std::sqrt(dx * dx + dy * dy + dz * dz) + LoadF32(child + 0x40));
    }
    const uint32_t views = LoadU32(renderer + 0x30);
    const uint32_t count = std::min<uint32_t>(LoadU32(renderer + 0x34), 8);
    float size = 0.0f;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t view = views + i * 0x6F0;
        const float scale = std::max(LoadF32(view + 0x24) * LoadF32(view + 0x80), LoadF32(view + 0x28) * LoadF32(view + 0x94)) * 0.5f;
        const float depth = LoadF32(view + 0x11C) * y + LoadF32(view + 0x10C) * x + LoadF32(view + 0x12C) * z + LoadF32(view + 0x13C);
        size = std::max(size, scale * radius / std::max(depth, 1.0f) * 2.0f);
    }
    return std::isfinite(size) ? size : 0.0f;
}

// Whether the primitive or an attached one is visible in a view: the bit of its
// index (+0x20) in the view's visibility map (+0x25C), as sub_823BE9D0 tests
// before it adds the half-size second shadow.
bool OnScreen(uint32_t renderer, uint32_t primitive)
{
    const uint32_t views = LoadU32(renderer + 0x30);
    const uint32_t count = std::min<uint32_t>(LoadU32(renderer + 0x34), 8);
    auto inView = [&](uint32_t p) {
        const int32_t index = int32_t(LoadU32(p + 0x20));
        if (index < 0) return false;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t bits = LoadU32(views + i * 0x6F0 + 0x25C);
            if (bits && (LoadU32(bits + uint32_t(index >> 5) * 4) >> (index & 31) & 1)) return true;
        }
        return false;
    };
    if (inView(primitive)) return true;
    for (uint32_t child = LoadU32(primitive + 0x8C), guard = 0; child && guard < 64; child = LoadU32(child + 0x90), ++guard)
        if (inView(child)) return true;
    return false;
}
}

PPC_FUNC(sub_823BE570)
{
    const bool lowNow = settings::GetConfig().dynamicShadows == 2;
    if (lowNow != low)
        LOG_INFO("graphics effects: dynamic shadows {}", lowNow ? "low" : "full");
    low = lowNow;
    ++frame;
    std::swap(sizes, lastSizes);
    sizes.clear();
    threshold = 0.0f;
    if (low && lastSizes.size() > LowCount)
    {
        std::nth_element(lastSizes.begin(), lastSizes.begin() + (LowCount - 1), lastSizes.end(), std::greater<float>());
        threshold = lastSizes[LowCount - 1];
    }
    if (!low || casters.size() > 4096)
        casters.clear();
    __imp__sub_823BE570(ctx, base);
}

PPC_FUNC(sub_823BE9D0)
{
    const uint32_t interaction = ctx.r4.u32;
    const uint32_t primitive = LoadU32(interaction + 8);
    // The same precondition as the original: only these interactions get a shadow.
    if (!low || (LoadU32(interaction + 0xC) & 0x18000000) != 0x10000000 || LoadU32(primitive + 0x88) != 0)
    {
        __imp__sub_823BE9D0(ctx, base);
        return;
    }
    Caster& caster = casters[primitive];
    if (OnScreen(ctx.r3.u32, primitive))
        caster.lastOnScreen = frame;
    if (frame - caster.lastOnScreen > OffScreenFrames)
    {
        caster.shadow = false;
        return;
    }
    const float size = ScreenSize(ctx.r3.u32, primitive);
    sizes.push_back(size);
    caster.shadow = size >= threshold * (caster.shadow ? KeepFraction : 1.0f);
    if (caster.shadow)
        __imp__sub_823BE9D0(ctx, base);
}
