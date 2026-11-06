#pragma once
#include <cstdint>
#include <string>

// Performance lines every player's log carries, and fixed-window benchmark runs (docs/SWITCH.md, "Performance log").
//
// perf:   every LO_PERF_INTERVAL seconds (Switch: 10 by default, elsewhere off): frame time percentiles, where the
//         frame went (pacing sleep, present, command processor idle), GPU busy time, draws and back-end work per
//         frame, per-thread CPU, scene size, docked/handheld, clocks and the map. Written by a thread of its own.
// hitch:  a frame over twice the target frame time and twice the recent median outside loading screens (the first LO_PERF_HITCHES, 300), with
//         what that frame did: GPU time, shader compiles and translations, texture uploads, idle and pacing.
// bench:  LO_BENCH=<name> measures a fixed number of frames once a scene is reached and logs one "bench result"
//         line (tools/switch/bench.py writes the env.txt and reads the result).
//
// Everything is counted on the command processor thread from timers and counters the renderer keeps anyway; the
// per-frame cost is a clock read, a few relaxed atomic loads and a short uncontended lock.
namespace perf_stats
{
// One presented guest frame, on the GPU command processor thread after its present.
struct PresentSample
{
    uint32_t swap = 0;
    uint32_t targetFps = 0;  // the frame pacing target (0: uncapped)
    uint64_t sleepNs = 0;    // the frame pacing sleep
    uint64_t presentNs = 0;  // the present and the event pump
    bool loading = false;    // a loading screen (loading pacing or load boost)
};
void Present(const PresentSample& sample);
// The command processor had nothing to execute (it waited for the game) for this long.
void CpIdle(uint64_t ns);
// The renderer's draws in the frame it just submitted.
void FrameDraws(uint32_t draws);
// The scene's render size for the frame being planned.
void SceneSize(uint32_t width, uint32_t height);

// For the performance overlay: draws since the start, and the GPU clock the perf line last read (MHz; 0 unknown).
uint64_t TotalDraws();
uint32_t GpuClockMhz();

// For the debug server's info: the map's name as one word ("-" when there is none) and the last perf: line (empty
// before the first). Any thread.
std::string MapName();
std::string LastPerfLine();

// Benchmark run hooks. BenchStick: the left stick position while a run holds it (LO_BENCH_STICK), from the input
// thread. TakeBenchScreenshot: true once, on the frame a run asks for its screenshot (LO_BENCH_SHOT).
bool BenchStick(int16_t& x, int16_t& y);
bool TakeBenchScreenshot();
}
