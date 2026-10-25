#include <stdafx.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <kernel/memory.h>
#include <os/logger.h>
#include "settings/config.h"

// Low rejects a light-primitive interaction before the guest allocates its
// shadow, avoiding the downstream depth/projection work. On and Off retain
// their original initialization; graphics_effects.cpp controls depth rendering.
//
// InitDynamicShadows (823BE570) enumerates interactions on the render thread.
// Its views, visibility and primitive bounds are inputs to that enumeration;
// 823BE9D0 sizes/creates shadows, not those inputs. Cache measurements only
// inside that synchronous call, never across it. LO_SHADOW_CACHE_VERIFY=1
// checks this immutability assumption on repeated interactions in device runs.
extern "C" PPC_FUNC(__imp__sub_823BE570);
extern "C" PPC_FUNC(__imp__sub_823BE9D0);

namespace
{
constexpr size_t LowCount = 8, HistoryLimit = 4096;
constexpr float KeepFraction = 0.8f;
constexpr uint64_t OffScreenFrames = 15;

struct Caster
{
    uint64_t lastSeen = 0, lastOnScreen = 0, measured = 0;
    uint32_t index = 0;
    float size = 0;
    bool everOnScreen = false, visible = false, eligible = false, shadow = false;
};
struct Candidate
{
    uint32_t primitive, index;
    float size;
    bool shadow;
};

// Previous-pass rank, with the existing 20% preference for an incumbent. Ties
// keep incumbents, then use a deterministic primitive key, not light order.
float Priority(const Candidate& c) { return c.size * (c.shadow ? 1.0f : KeepFraction); }
bool Larger(const Candidate& a, const Candidate& b)
{
    if (Priority(a) != Priority(b)) return Priority(a) > Priority(b);
    if (a.shadow != b.shadow) return a.shadow;
    return a.primitive < b.primitive;
}

bool Option(const char* name)
{
    const char* value = std::getenv(name);
    return value && *value && std::strcmp(value, "0") != 0;
}
const bool statistics = Option("LO_SHADOW_STATS");
const bool verifyCache = Option("LO_SHADOW_CACHE_VERIFY");
using Clock = std::chrono::steady_clock;
double Ms(Clock::duration value) { return std::chrono::duration<double, std::milli>(value).count(); }
struct Statistics
{
    uint64_t passes = 0, interactions = 0, measurements = 0, hits = 0;
    uint64_t offscreen = 0, budget = 0, forwarded = 0, fallback = 0, invalid = 0;
    double gateMs = 0, initMs = 0;
} stats;
struct GateTimer
{
    Clock::time_point started{};
    GateTimer() { if (statistics) started = Clock::now(); }
    ~GateTimer() { if (statistics) stats.gateMs += Ms(Clock::now() - started); }
};

// Render thread only. No guest pointers are retained, only integer identities.
// History is retained only for casters encountered in consecutive passes;
// measurements are stamped with the current pass and are never reused later.
std::unordered_map<uint32_t, Caster> casters;
std::vector<Candidate> candidates, previous;
std::array<Candidate, LowCount> selected{};
size_t selectedCount = 0, newcomers = 0;
bool low = false, initializing = false, invalidCache = false;
uint32_t activeRenderer = 0;
uint32_t activeViews = 0, activeViewCount = 0;
const uint8_t* activeBase = nullptr;
uint64_t frame = 0;

uint32_t LoadU32(uint32_t address)
{
    return __builtin_bswap32(*static_cast<const uint32_t*>(g_memory.Translate(address)));
}
float LoadF32(uint32_t address) { return std::bit_cast<float>(LoadU32(address)); }

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

void ResetHistory()
{
    casters.clear(); candidates.clear(); previous.clear();
    selectedCount = newcomers = 0;
}

void BeginPass(bool lowNow, uint32_t renderer, const uint8_t* base)
{
    const uint32_t views = lowNow ? LoadU32(renderer + 0x30) : 0;
    const uint32_t viewCount = lowNow ? LoadU32(renderer + 0x34) : 0;
    // A nested/unexpected initialization invalidates the outer cache too.
    // Missing passes, mode changes and renderer/address-space changes cannot
    // transfer visibility grace or previous-frame decisions to another scene.
    if (!lowNow || !low || initializing || renderer != activeRenderer || views != activeViews ||
        viewCount != activeViewCount || base != activeBase || frame == UINT64_MAX)
        ResetHistory();
    if (frame == UINT64_MAX) frame = 0;
    ++frame;
    low = lowNow; activeRenderer = renderer; activeBase = base;
    activeViews = views; activeViewCount = viewCount;
    initializing = low; invalidCache = false;
    if (!low) return;

    for (auto it = casters.begin(); it != casters.end();)
        if (it->second.lastSeen != frame - 1) it = casters.erase(it);
        else ++it;
    std::swap(candidates, previous);
    candidates.clear();
    selectedCount = std::min(LowCount, previous.size());
    std::partial_sort(previous.begin(), previous.begin() + selectedCount, previous.end(), Larger);
    std::copy_n(previous.begin(), selectedCount, selected.begin());
    newcomers = 0;
}

bool Selected(uint32_t primitive, uint32_t index)
{
    for (size_t i = 0; i < selectedCount; ++i)
        if (selected[i].primitive == primitive && selected[i].index == index) return true;
    // An unfilled previous budget admits newcomers immediately. With a full
    // budget, a new candidate participates in the next pass's ranking, just
    // as the original policy used the previous pass's sizes. Initial traversal
    // order can affect only the first pass, before any ranking exists.
    if (selectedCount + newcomers == LowCount) return false;
    ++newcomers;
    return true;
}

bool Keep(uint32_t renderer, uint32_t primitive)
{
    GateTimer timer;
    if (statistics) ++stats.interactions;
    auto found = casters.find(primitive);
    if (found == casters.end())
    {
        // Fail open on pathological scenes instead of growing history without
        // bound or removing unmeasured shadows. The eight-object guarantee is
        // for tracked eligible casters; diagnostics report this fallback.
        if (casters.size() == HistoryLimit) { if (statistics) ++stats.fallback; return true; }
        found = casters.try_emplace(primitive).first;
    }
    Caster& caster = found->second;
    const uint32_t index = LoadU32(primitive + 0x20);
    if (caster.index != index)
    {
        if (caster.measured == frame)
        {
            // Identity changed inside an allegedly immutable enumeration. Do
            // not append duplicate identities or grow the candidate vector.
            invalidCache = true;
            if (statistics) ++stats.invalid;
            return true;
        }
        caster = Caster{};
    }
    caster.index = index;
    caster.lastSeen = frame;
    if (caster.measured == frame)
    {
        if (statistics) ++stats.hits;
        if (verifyCache)
        {
            const bool visible = OnScreen(renderer, primitive);
            const bool eligible = visible || (caster.everOnScreen && frame - caster.lastOnScreen <= OffScreenFrames);
            const float size = eligible ? ScreenSize(renderer, primitive) : 0;
            if (visible != caster.visible || size != caster.size)
            {
                // Do not silently use stale inputs if the guest's immutable
                // enumeration contract changes. Delegate the rest of this pass
                // and discard its history at exit. This is a diagnostic path.
                invalidCache = true;
                if (statistics) ++stats.invalid;
                return true;
            }
        }
        if (statistics) ++(caster.shadow ? stats.forwarded : caster.eligible ? stats.budget : stats.offscreen);
        return caster.shadow;
    }
    if (statistics) ++stats.measurements;
    caster.measured = frame;
    caster.visible = OnScreen(renderer, primitive);
    if (caster.visible) { caster.everOnScreen = true; caster.lastOnScreen = frame; }
    caster.eligible = caster.everOnScreen && frame - caster.lastOnScreen <= OffScreenFrames;
    if (!caster.eligible)
    {
        caster.size = 0; caster.shadow = false;
        if (statistics) ++stats.offscreen;
        return false;
    }
    caster.size = ScreenSize(renderer, primitive);
    caster.shadow = Selected(primitive, index);
    candidates.push_back({primitive, index, caster.size, caster.shadow});
    if (statistics) ++(caster.shadow ? stats.forwarded : stats.budget);
    return caster.shadow;
}
}

PPC_FUNC(sub_823BE570)
{
    const bool lowNow = settings::GetConfig().dynamicShadows == 2;
    if (lowNow != low)
        LOG_INFO("graphics effects: dynamic shadows {}", lowNow ? "low" : "full");
    const auto started = statistics && lowNow ? Clock::now() : Clock::time_point{};
    BeginPass(lowNow, ctx.r3.u32, base);
    __imp__sub_823BE570(ctx, base);
    initializing = false;
    if (invalidCache) ResetHistory();
    if (statistics && lowNow)
    {
        stats.initMs += Ms(Clock::now() - started);
        if (++stats.passes == 120)
        {
            LOG_INFO("shadow budget passes={} interactions={} measurements={} cache_hits={} offscreen={} budget={} forwarded={} "
                "fallback={} invalid={} history={} gate_wall_ms={:.6f} init_wall_ms={:.6f} scope=cpu_guest_initialization_not_gpu_passes",
                stats.passes, stats.interactions, stats.measurements, stats.hits, stats.offscreen, stats.budget, stats.forwarded,
                stats.fallback, stats.invalid, casters.size(), stats.gateMs / stats.passes, stats.initMs / stats.passes);
            stats = {};
        }
    }
}

PPC_FUNC(sub_823BE9D0)
{
    // On/Off need no guest reads. Unscoped calls and a different renderer must
    // not reuse another initialization's measurements or modify its history.
    if (!initializing || !low || invalidCache || ctx.r3.u32 != activeRenderer || base != activeBase)
    {
        __imp__sub_823BE9D0(ctx, base);
        return;
    }
    const uint32_t interaction = ctx.r4.u32;
    const uint32_t primitive = LoadU32(interaction + 8);
    // Match the original function's shadow-creation preconditions.
    if ((LoadU32(interaction + 0xC) & 0x18000000) != 0x10000000 || LoadU32(primitive + 0x88) != 0 || Keep(ctx.r3.u32, primitive))
        __imp__sub_823BE9D0(ctx, base);
}
