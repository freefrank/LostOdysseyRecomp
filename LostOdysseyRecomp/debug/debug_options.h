#pragma once
#include <cstdint>
// System → Debug on the Switch (settings/menu.cpp): A/B switches for benchmark runs (settings::DebugOptions), saved
// at once. Every value is logged as "option: frame=<N> <key>=<value> ...", N the first presented frame
// (g_presentedSwaps) that runs with it, so a log or a benchmark run shows when each setting was on (docs/SWITCH.md,
// Debug page). env.txt stays the way to script runs: its LO_DEKO3D_* win over the saved values at start, and the log
// says so.
namespace debug_options
{
// deko3d's GPU options (plume_deko3d_api.h's Deko3DGpuOptions).
enum class Deko3d : uint32_t
{
    MergeBarriers,
    LeanBarriers,
    Zcull,
    DepthCompression,
    ColorCompression,
    VsOutputs,
    Count
};
// Applied between frames (merged and lean barriers, read per barrier); the others are read when the queue, a render
// target or a pipeline is made, so they apply at the next start.
bool Live(Deko3d option);
// The deko3d back end runs (LO_SWITCH_BACKEND=deko3d); the deko3d rows need it.
bool Deko3dActive();
// What the GPU runs with now (live changes once the render thread took them).
bool Running(Deko3d option);
// The env.txt variable that set the option at start, or null.
const char *ForcedBy(Deko3d option);
// The saved value: what the next start uses unless env.txt sets the option.
bool Saved(Deko3d option);
// The Debug page. Saves the value and logs it; a live option reaches the GPU before the next frame. False when the
// settings could not be saved (nothing changes then).
bool Set(Deko3d option, bool on);
// The debug server (os/debug_server.h) is read at start: env.txt's LO_DEBUG_SERVER, else the saved value. These say
// what the next start does and whether env.txt decides it; SetDebugServer saves the value (the Debug page, the LB+RB
// overlay) and logs it.
bool SetDebugServer(bool on);
bool DebugServerNextStart();
bool DebugServerFromEnvironment();
// Logs a performance overlay mode the Debug page saved (0 Off, 1 FPS, 2 Detailed).
void LogPerformanceOverlay(uint32_t mode);
// gpu/video.cpp, before the deko3d device is made: env.txt over the saved values into plume's options, each logged.
void StartDeko3d();
// The render thread, after a frame is submitted (renderer::Flush): live changes the page asked for.
void ApplyPending();
} // namespace debug_options
