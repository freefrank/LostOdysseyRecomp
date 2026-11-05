#include "debug_options.h"
#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <gpu/video.h>
#include <os/logger.h>
#include <settings/config.h>
#if defined(LO_SWITCH_DEKO3D)
#include <plume_deko3d_api.h>
#endif

extern std::atomic<uint32_t> g_presentedSwaps;

namespace debug_options
{
namespace
{
struct Info
{
    const char *key;  // in the option lines
    const char *env;  // env.txt
    bool live;
    bool settings::DebugOptions::*saved;
};
constexpr Info kInfo[] = {
    {"deko3d_merge_barriers", "LO_DEKO3D_MERGE_BARRIERS", true, &settings::DebugOptions::deko3dMergeBarriers},
    {"deko3d_lean_barriers", "LO_DEKO3D_LEAN_BARRIERS", true, &settings::DebugOptions::deko3dLeanBarriers},
    {"deko3d_zcull", "LO_DEKO3D_ZCULL", false, &settings::DebugOptions::deko3dZcull},
    {"deko3d_depth_compression", "LO_DEKO3D_DEPTH_COMPRESSION", false, &settings::DebugOptions::deko3dDepthCompression},
    {"deko3d_color_compression", "LO_DEKO3D_COLOR_COMPRESSION", false, &settings::DebugOptions::deko3dColorCompression},
    {"deko3d_vs_outputs", "LO_DEKO3D_VS_OUTPUTS", false, &settings::DebugOptions::deko3dVsOutputs},
};
static_assert(std::size(kInfo) == size_t(Deko3d::Count));
const Info &Of(Deko3d option) { return kInfo[size_t(option)]; }
uint32_t Bit(Deko3d option) { return 1u << uint32_t(option); }

// Bits by Deko3d. running: what the GPU uses (the render thread updates the live ones); requested: what the page
// asked for, taken by ApplyPending when pending is set. forcedBy is written once, before the menu can open.
std::atomic<uint32_t> running{0}, requested{0};
std::atomic<bool> pending{false};
const char *forcedBy[size_t(Deko3d::Count)] = {};

// The first presented frame a change made now runs in: the one after the frame being recorded or presented.
uint32_t NextFrame() { return g_presentedSwaps.load(std::memory_order_relaxed) + 1; }
} // namespace

bool Live(Deko3d option) { return Of(option).live; }
bool Deko3dActive() { return gpu::video::IsDeko3d(); }
bool Running(Deko3d option) { return (running.load(std::memory_order_relaxed) & Bit(option)) != 0; }
const char *ForcedBy(Deko3d option) { return forcedBy[size_t(option)]; }
bool Saved(Deko3d option) { return settings::GetConfig().debug.*Of(option).saved; }

bool Set(Deko3d option, bool on)
{
    const Info &info = Of(option);
    auto options = settings::GetConfig().debug;
    options.*info.saved = on;
    if (!settings::SaveDebugOptions(options)) return false;
    const char *env = ForcedBy(option);
    if (info.live)
    {
        // Logged by ApplyPending, with the frame the GPU takes it at.
        const uint32_t bit = Bit(option);
        uint32_t want = requested.load(std::memory_order_relaxed);
        while (!requested.compare_exchange_weak(want, on ? want | bit : want & ~bit, std::memory_order_relaxed)) {}
        pending.store(true, std::memory_order_release);
    }
    else
        LOG_NOTICE("option: frame={} {}={} source=menu applies=next_start{}{}", NextFrame(), info.key, int(on),
                   env ? " env_wins=" : "", env ? env : "");
    return true;
}

bool DebugServerFromEnvironment()
{
    const char *value = std::getenv("LO_DEBUG_SERVER");
    return value && *value;
}

bool DebugServerNextStart()
{
    return DebugServerFromEnvironment() ? std::strcmp(std::getenv("LO_DEBUG_SERVER"), "0") != 0
                                        : settings::GetConfig().debug.debugServer;
}

bool SetDebugServer(bool on)
{
    auto options = settings::GetConfig().debug;
    options.debugServer = on;
    if (!settings::SaveDebugOptions(options)) return false;
    LOG_NOTICE("option: frame={} debug_server={} source=menu applies=next_start{}", NextFrame(), int(on),
               DebugServerFromEnvironment() ? " env_wins=LO_DEBUG_SERVER" : "");
    return true;
}

void LogPerformanceOverlay(uint32_t mode)
{
    LOG_NOTICE("option: frame={} performance_overlay={} source=menu", NextFrame(), mode);
}

void StartDeko3d()
{
#if defined(LO_SWITCH_DEKO3D)
    // env.txt first: LO_DEKO3D_<NAME>=0/1, then LO_DEKO3D_GPU_FAST=0/1 for all but colour compression. Without
    // either the saved value (Debug page), whose defaults are plume's: all off but colour compression.
    const auto saved = settings::GetConfig();
    const auto set = [](const char *name) {
        const char *value = std::getenv(name);
        return value && *value ? value : nullptr;
    };
    const char *fast = set("LO_DEKO3D_GPU_FAST");
    plume::Deko3DGpuOptions gpu;
    bool *field[] = {&gpu.mergeBarriers, &gpu.leanBarriers, &gpu.zcull, &gpu.depthCompression, &gpu.colorCompression,
                     &gpu.vsOutputs};
    static_assert(std::size(field) == size_t(Deko3d::Count));
    uint32_t bits = 0;
    const uint32_t frame = NextFrame();
    for (uint32_t i = 0; i < uint32_t(Deko3d::Count); ++i)
    {
        const auto option = Deko3d(i);
        const Info &info = Of(option);
        const bool savedValue = saved.debug.*info.saved;
        const char *env = set(info.env);
        if (!env && option != Deko3d::ColorCompression && fast) env = "LO_DEKO3D_GPU_FAST";
        const char *value = env ? std::getenv(env) : nullptr;
        *field[i] = value ? std::strcmp(value, "0") != 0 : savedValue;
        forcedBy[i] = env;
        if (*field[i]) bits |= Bit(option);
        if (!env)
            LOG_NOTICE("option: frame={} {}={} source=settings", frame, info.key, int(*field[i]));
        else if (*field[i] == savedValue)
            LOG_NOTICE("option: frame={} {}={} source=env.txt env={}", frame, info.key, int(*field[i]), env);
        else
            LOG_NOTICE("option: frame={} {}={} source=env.txt env={} saved={} (env.txt wins over the Debug page)",
                       frame, info.key, int(*field[i]), env, int(savedValue));
    }
    running.store(bits, std::memory_order_relaxed);
    requested.store(bits, std::memory_order_relaxed);
    plume::Deko3DSetGpuOptions(gpu);
    LOG_NOTICE("video: deko3d GPU options: color_compression={} depth_compression={} zcull={} merge_barriers={} "
               "lean_barriers={} vs_outputs={}", gpu.colorCompression, gpu.depthCompression, gpu.zcull,
               gpu.mergeBarriers, gpu.leanBarriers, gpu.vsOutputs);
    LOG_NOTICE("option: frame={} performance_overlay={} source=settings", frame, saved.performanceOverlay);
    LOG_NOTICE("option: frame={} debug_server={} source={}", frame, int(DebugServerNextStart()),
               DebugServerFromEnvironment() ? "env.txt env=LO_DEBUG_SERVER" : "settings");
#endif
}

void ApplyPending()
{
#if defined(LO_SWITCH_DEKO3D)
    if (!pending.exchange(false, std::memory_order_acquire)) return;
    const uint32_t live = Bit(Deko3d::MergeBarriers) | Bit(Deko3d::LeanBarriers);
    const uint32_t before = running.load(std::memory_order_relaxed);
    const uint32_t after = (before & ~live) | (requested.load(std::memory_order_relaxed) & live);
    if (after == before) return;
    const auto on = [after](Deko3d option) { return (after & Bit(option)) != 0; };
    plume::Deko3DSetBarrierOptions(on(Deko3d::MergeBarriers), on(Deko3d::LeanBarriers));
    running.store(after, std::memory_order_relaxed);
    const uint32_t frame = NextFrame();
    for (const auto option : {Deko3d::MergeBarriers, Deko3d::LeanBarriers})
        if ((after ^ before) & Bit(option))
        {
            const char *env = ForcedBy(option);
            LOG_NOTICE("option: frame={} {}={} source=menu{}{}", frame, Of(option).key, int(on(option)),
                       env ? " overrides_env=" : "", env ? env : "");
        }
#endif
}
} // namespace debug_options
#endif
