#include <stdafx.h>
#include "perf_stats.h"
#include <debug/map_info.h>
#include <gpu/gpu_busy.h>
#include <gpu/video.h>
#include <os/detach_thread.h>
#include <os/logger.h>
#include <os/thread_name.h>
#include <settings/quit_action_hook.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#if LO_PLATFORM_SWITCH
#include <os/switch_cpu_profiler.h>
#include <os/switch_platform.h>
#endif
#if defined(LO_SWITCH_DEKO3D)
#include <plume_deko3d_api.h>
#endif

#ifdef _WIN32
#define LO_PERF_ENVIRON _environ
#else
extern char** environ;
#define LO_PERF_ENVIRON environ
#endif

namespace perf_stats
{
namespace
{
using Clock = std::chrono::steady_clock;

int EnvInt(const char* name, int fallback)
{
    const char* value = std::getenv(name);
    return value && *value ? std::atoi(value) : fallback;
}

double Ms(uint64_t ns) { return double(ns) / 1e6; }

struct Config
{
    int intervalSeconds = 0;  // LO_PERF_INTERVAL; 0: no perf lines and no hitch lines
    bool gpu = true;          // LO_PERF_GPU=0: no GPU timestamps for these lines
    double hitchMs = 0;       // LO_PERF_HITCH_MS; 0: twice the target frame time
    uint32_t hitchLimit = 300; // LO_PERF_HITCHES: hitch lines per session
};

const Config& GetConfig()
{
    static const Config config = [] {
        Config c;
#if LO_PLATFORM_SWITCH
        constexpr int defaultInterval = 10;
#else
        constexpr int defaultInterval = 0;
#endif
        c.intervalSeconds = std::max(0, EnvInt("LO_PERF_INTERVAL", defaultInterval));
        c.gpu = EnvInt("LO_PERF_GPU", 1) != 0;
        c.hitchMs = double(std::max(0, EnvInt("LO_PERF_HITCH_MS", 0)));
        c.hitchLimit = uint32_t(std::max(0, EnvInt("LO_PERF_HITCHES", 300)));
        return c;
    }();
    return config;
}

// ---------------------------------------------------------------- counters

std::atomic<uint64_t> g_cpIdleNs{0};
std::atomic<uint64_t> g_draws{0};
std::atomic<uint32_t> g_lastDraws{0};
std::atomic<uint64_t> g_sceneSize{0}; // width << 32 | height
std::atomic<uint32_t> g_gpuMhz{0};

// Running totals: a window's figures are the difference of two reads.
struct Counters
{
    uint64_t gpuBusyNs = 0, cpIdleNs = 0, draws = 0;
    bool backend = false; // the back end's counters below were read (deko3d)
    uint64_t gpuDraws = 0, pipelineBinds = 0, stateBinds = 0, descriptorWrites = 0;
    uint64_t barriersFull = 0, barriersOther = 0, uploads = 0, copies = 0;
    uint64_t compiles = 0, compileNs = 0, translations = 0, translateNs = 0;
};

Counters ReadCounters()
{
    Counters c;
    c.gpuBusyNs = gpu::gpu_busy::Total();
    c.cpIdleNs = g_cpIdleNs.load(std::memory_order_relaxed);
    c.draws = g_draws.load(std::memory_order_relaxed);
#if defined(LO_SWITCH_DEKO3D)
    if (gpu::video::IsDeko3d())
    {
        const plume::Deko3DCounters d = plume::Deko3DReadCounters();
        c.backend = true;
        c.gpuDraws = d.draws;
        c.pipelineBinds = d.pipelineBinds;
        c.stateBinds = d.stateBinds;
        c.descriptorWrites = d.descriptorWrites;
        c.barriersFull = d.barriersFull;
        c.barriersOther = d.barriersPrimitives + d.barriersFragments;
        c.uploads = d.copyUpload;
        c.copies = d.copyBuffer + d.copyReadback + d.copyImage + d.copyResolve + d.copyBlit;
        c.compiles = d.shaderCompiles;
        c.compileNs = d.shaderCompileNs;
        c.translations = d.translations;
        c.translateNs = d.translateNs;
    }
#endif
    return c;
}

// " key=value" per frame for the back end's work between two reads.
std::string BackendWork(const Counters& a, const Counters& b, double frames)
{
    if (!b.backend || frames <= 0)
        return {};
    const auto per = [frames](uint64_t x, uint64_t y) { return double(y - x) / frames; };
    return fmt::format(" gpu_draws={:.0f} pipeline_binds={:.1f} state_binds={:.1f} descriptor_writes={:.1f}"
        " barriers_full={:.1f} barriers_other={:.1f} uploads={:.2f} copies={:.1f} compiles={} compile_ms={:.1f}"
        " translations={} translate_ms={:.1f}",
        per(a.gpuDraws, b.gpuDraws), per(a.pipelineBinds, b.pipelineBinds), per(a.stateBinds, b.stateBinds),
        per(a.descriptorWrites, b.descriptorWrites), per(a.barriersFull, b.barriersFull),
        per(a.barriersOther, b.barriersOther), per(a.uploads, b.uploads), per(a.copies, b.copies),
        b.compiles - a.compiles, Ms(b.compileNs - a.compileNs), b.translations - a.translations,
        Ms(b.translateNs - a.translateNs));
}

std::string SceneText()
{
    const uint64_t size = g_sceneSize.load(std::memory_order_relaxed);
    return size ? fmt::format("{}x{}", uint32_t(size >> 32), uint32_t(size)) : std::string("-");
}

// The map's name as one word ("-" when there is none, as on the title screen).
std::string MapText()
{
    debug_menu::MapInfo info;
    if (!debug_menu::TryGetMapInfo(info) || !info.available || info.name.empty())
        return "-";
    std::string text;
    for (const wchar_t c : info.name)
        text += c > L' ' && c < 127 ? char(c) : '_';
    return text;
}

// ---------------------------------------------------------------- the console

#if LO_PLATFORM_SWITCH
struct ThreadSample
{
    uint64_t tick = 0;
    size_t count = 0;
    os::switch_cpu_profiler::ThreadTime threads[256];
};

bool SampleThreads(ThreadSample& sample, bool wait)
{
    const size_t count = os::switch_cpu_profiler::SampleThreadTicks(sample.threads, std::size(sample.threads), sample.tick, wait);
    if (!count)
        return false;
    sample.count = count;
    return true;
}

// " cpu_total_pct=... threads=Name:pct,..." for the busiest threads between two samples. A share is of one core:
// a thread that never waits reads 100; cpu_total_pct adds every registered thread (300 is all three cores).
std::string ThreadShares(const ThreadSample& a, const ThreadSample& b, size_t top)
{
    if (!a.count || !b.count || b.tick <= a.tick)
        return " threads=-";
    const double window = double(b.tick - a.tick);
    std::vector<std::pair<double, const char*>> shares;
    double total = 0;
    for (size_t i = 0; i < b.count; ++i)
        for (size_t j = 0; j < a.count; ++j)
            if (a.threads[j].handle == b.threads[i].handle && std::strcmp(a.threads[j].name, b.threads[i].name) == 0)
            {
                if (b.threads[i].ticks >= a.threads[j].ticks)
                {
                    const double percent = 100.0 * double(b.threads[i].ticks - a.threads[j].ticks) / window;
                    total += percent;
                    shares.emplace_back(percent, b.threads[i].name);
                }
                break;
            }
    std::sort(shares.begin(), shares.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
    std::string list;
    for (size_t i = 0; i < shares.size() && i < top && shares[i].first >= 1.0; ++i)
    {
        if (!list.empty())
            list += ',';
        for (const char* c = shares[i].second; *c; ++c)
            list += *c == ' ' || *c == ',' || *c == '=' ? '_' : *c;
        list += fmt::format(":{:.1f}", shares[i].first);
    }
    return fmt::format(" cpu_total_pct={:.0f} threads={}", total, list.empty() ? "-" : list);
}

// " mode=... cpu_mhz=... gpu_mhz=... mem_mhz=... heap_mib=used/limit"
std::string ConsoleText()
{
    uint32_t cpu = 0, gpu = 0, memory = 0;
    os::switch_platform::ReadClockRates(cpu, gpu, memory);
    g_gpuMhz.store(uint32_t(std::lround(gpu / 1e6)), std::memory_order_relaxed);
    uint64_t heapUsed = 0, heapLimit = 0;
    os::switch_platform::HeapUsage(heapUsed, heapLimit);
    return fmt::format(" mode={} cpu_mhz={:.0f} gpu_mhz={:.0f} mem_mhz={:.0f} heap_mib={}/{}",
        os::switch_platform::IsDocked() ? "docked" : "handheld", cpu / 1e6, gpu / 1e6, memory / 1e6,
        heapUsed >> 20, heapLimit >> 20);
}
#endif

// ---------------------------------------------------------------- frame times

struct FrameSummary
{
    size_t count = 0;
    double mean = 0, p50 = 0, p90 = 0, p95 = 0, p99 = 0, max = 0, stdev = 0;
    size_t late = 0; // frames over 1.25x the target frame time
};

FrameSummary Summarize(std::vector<float> intervals, double targetMs)
{
    FrameSummary s;
    s.count = intervals.size();
    if (intervals.empty())
        return s;
    std::sort(intervals.begin(), intervals.end());
    const auto at = [&](double p) { return double(intervals[std::min(intervals.size() - 1, size_t(p * double(intervals.size() - 1) + 0.5))]); };
    double sum = 0, squares = 0;
    for (const float ms : intervals)
    {
        sum += ms;
        squares += double(ms) * ms;
        s.late += targetMs > 0 && ms > targetMs * 1.25;
    }
    s.mean = sum / double(s.count);
    s.stdev = std::sqrt(std::max(0.0, squares / double(s.count) - s.mean * s.mean));
    s.p50 = at(0.50);
    s.p90 = at(0.90);
    s.p95 = at(0.95);
    s.p99 = at(0.99);
    s.max = intervals.back();
    return s;
}

double TargetMs(uint32_t fps) { return 1000.0 / double(fps ? fps : 30); }

// ---------------------------------------------------------------- perf lines

// Filled by Present on the command processor thread, taken by the monitor thread.
struct Window
{
    std::vector<float> intervals; // ms, frames outside loading screens
    uint32_t frames = 0, loadingFrames = 0, hitches = 0, targetFps = 0;
    uint64_t sleepNs = 0, presentNs = 0;
};
std::mutex g_windowMutex;
Window g_window;
std::mutex g_lastLineMutex;
std::string g_lastLine; // the last perf: line, for the debug server's info

void MonitorMain()
{
    os::SetCurrentThreadName("Perf Monitor");
#if LO_PLATFORM_SWITCH
    // Above the game's threads (0x2C), as the network log: Horizon runs strictly by priority, and a default pthread
    // (0x3B) waits for as long as the game keeps the three cores busy, which is when these lines matter. It wakes
    // for about a millisecond every interval.
    os::switch_platform::SetCurrentThreadPriority(0x2B);
#endif
    const Config& config = GetConfig();
    Counters before = ReadCounters();
    auto start = Clock::now();
#if LO_PLATFORM_SWITCH
    static ThreadSample threadsBefore, threadsAfter;
    SampleThreads(threadsBefore, true);
#endif
    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::seconds(config.intervalSeconds));
        Window window;
        window.intervals.reserve(4096);
        {
            std::lock_guard lock(g_windowMutex);
            std::swap(window, g_window);
        }
        const auto now = Clock::now();
        const Counters after = ReadCounters();
        const double seconds = std::chrono::duration<double>(now - start).count();
        start = now;
        const double frames = double(window.frames);
        const FrameSummary s = Summarize(std::move(window.intervals), TargetMs(window.targetFps));
        std::string times = s.count
            ? fmt::format("ms_p50={:.1f} ms_p95={:.1f} ms_p99={:.1f} ms_max={:.1f}", s.p50, s.p95, s.p99, s.max)
            : std::string("ms_p50=- ms_p95=- ms_p99=- ms_max=-");
        const uint64_t gpuNs = after.gpuBusyNs - before.gpuBusyNs;
        std::string gpu = gpuNs && frames
            ? fmt::format("gpu_ms={:.1f} gpu_pct={:.0f}", Ms(gpuNs) / frames, 100.0 * double(gpuNs) / (seconds * 1e9))
            : std::string("gpu_ms=- gpu_pct=-");
        std::string console;
#if LO_PLATFORM_SWITCH
        if (SampleThreads(threadsAfter, false))
        {
            console += ThreadShares(threadsBefore, threadsAfter, 5);
            std::swap(threadsBefore, threadsAfter);
        }
        console += ConsoleText();
#endif
        const double perFrame = std::max(1.0, frames);
        std::string line = fmt::format("perf: fps={:.1f} frames={} {} target_fps={} loading={} hitches={} pace_ms={:.1f} present_ms={:.1f}"
            " cp_idle_ms={:.1f} {} draws={:.0f}{} scene={}{} map={}",
            frames / seconds, window.frames, times, window.targetFps, window.loadingFrames, window.hitches,
            Ms(window.sleepNs) / perFrame, Ms(window.presentNs) / perFrame,
            Ms(after.cpIdleNs - before.cpIdleNs) / perFrame, gpu, double(after.draws - before.draws) / perFrame,
            BackendWork(before, after, frames), SceneText(), console, MapText());
        LOG_NOTICE("{}", line);
        {
            std::lock_guard lock(g_lastLineMutex);
            g_lastLine = std::move(line);
        }
        before = after;
    }
}

// ---------------------------------------------------------------- benchmark runs

struct BenchConfig
{
    std::string name;
    uint32_t startSwap = 0, startDraws = 0, settle = 150, frames = 900;
    double timeoutSeconds = 900;
    bool stick = false, shot = false, exit = false;
    int16_t stickX = 0, stickY = 0;
};

const BenchConfig& GetBench()
{
    static const BenchConfig bench = [] {
        BenchConfig b;
        const char* name = std::getenv("LO_BENCH");
        if (!name || !*name || std::strcmp(name, "0") == 0)
            return b;
        b.name = name;
        for (char& c : b.name)
            if (c == ' ' || c == '=')
                c = '_';
        b.startSwap = uint32_t(std::max(0, EnvInt("LO_BENCH_START_SWAP", 0)));
        b.startDraws = uint32_t(std::max(0, EnvInt("LO_BENCH_START_DRAWS", 0)));
        b.settle = uint32_t(std::max(0, EnvInt("LO_BENCH_SETTLE", 150)));
        b.frames = uint32_t(std::max(30, EnvInt("LO_BENCH_FRAMES", 900)));
        b.timeoutSeconds = double(std::max(30, EnvInt("LO_BENCH_TIMEOUT", 900)));
        if (const char* stick = std::getenv("LO_BENCH_STICK"))
        {
            int x = 0, y = 0;
            if (std::sscanf(stick, "%d,%d", &x, &y) == 2)
            {
                b.stick = true;
                b.stickX = int16_t(std::clamp(x, -32768, 32767));
                b.stickY = int16_t(std::clamp(y, -32768, 32767));
            }
        }
        b.shot = EnvInt("LO_BENCH_SHOT", 0) != 0;
        b.exit = EnvInt("LO_BENCH_EXIT", 0) != 0;
        return b;
    }();
    return bench;
}

std::atomic<bool> g_benchStick{false};
std::atomic<bool> g_benchShot{false};

enum class Phase { Waiting, Settling, Measuring, Finishing, Done };

struct BenchRun
{
    Phase phase = Phase::Waiting;
    uint32_t settleLeft = 0, finishLeft = 0, armedSwap = 0, startSwap = 0;
    uint32_t loadingFrames = 0, hitches = 0, targetFps = 0;
    uint64_t sleepNs = 0, presentNs = 0;
    std::vector<float> intervals;
    Counters begin;
    Clock::time_point beginTime{};
#if LO_PLATFORM_SWITCH
    ThreadSample threadsBegin, threadsEnd;
#endif
};

std::string EnvironmentText()
{
    std::vector<std::string> entries;
    for (char** entry = LO_PERF_ENVIRON; entry && *entry; ++entry)
        if (std::strncmp(*entry, "LO_", 3) == 0)
            entries.emplace_back(*entry);
    std::sort(entries.begin(), entries.end());
    std::string text;
    for (const auto& entry : entries)
    {
        text += ' ';
        for (const char c : entry)
            text += c == ' ' ? '_' : c;
    }
    return text;
}

void FinishBench(BenchRun& run, const BenchConfig& bench, uint32_t swap, const char* status)
{
    const auto end = Clock::now();
    const Counters after = ReadCounters();
    std::string console;
#if LO_PLATFORM_SWITCH
    if (std::strcmp(status, "ok") == 0 && SampleThreads(run.threadsEnd, true))
        console += ThreadShares(run.threadsBegin, run.threadsEnd, 8);
    console += ConsoleText();
#endif
    const double frames = double(run.intervals.size());
    const double seconds = run.beginTime != Clock::time_point{} ? std::chrono::duration<double>(end - run.beginTime).count() : 0.0;
    const double targetMs = TargetMs(run.targetFps);
    const FrameSummary s = Summarize(run.intervals, targetMs);
    const uint64_t gpuNs = after.gpuBusyNs - run.begin.gpuBusyNs;
    const double perFrame = std::max(1.0, frames);
    std::string result = fmt::format("name={} status={} frames={} seconds={:.2f} start_swap={} end_swap={}",
        bench.name, status, run.intervals.size(), seconds, run.startSwap, swap);
    if (s.count && seconds > 0)
    {
        result += fmt::format(" fps={:.2f} ms_mean={:.2f} ms_p50={:.2f} ms_p90={:.2f} ms_p95={:.2f} ms_p99={:.2f}"
            " ms_max={:.2f} ms_stdev={:.2f} low1_fps={:.2f} late_pct={:.1f} target_fps={} hitches={} loading={}"
            " pace_ms={:.2f} present_ms={:.2f} cp_idle_ms={:.2f} {} draws={:.0f}{} scene={}",
            frames / seconds, s.mean, s.p50, s.p90, s.p95, s.p99, s.max, s.stdev, 1000.0 / s.p99,
            100.0 * double(s.late) / double(s.count), run.targetFps, run.hitches, run.loadingFrames,
            Ms(run.sleepNs) / perFrame, Ms(run.presentNs) / perFrame, Ms(after.cpIdleNs - run.begin.cpIdleNs) / perFrame,
            gpuNs ? fmt::format("gpu_ms={:.2f} gpu_pct={:.1f}", Ms(gpuNs) / perFrame, 100.0 * double(gpuNs) / (seconds * 1e9))
                  : std::string("gpu_ms=- gpu_pct=-"),
            double(after.draws - run.begin.draws) / perFrame, BackendWork(run.begin, after, frames), SceneText());
    }
    LOG_NOTICE("bench result: {}{} map={}", result, console, MapText());
    LOG_NOTICE("bench env:{}", EnvironmentText());
    LOG_NOTICE("bench done: name={} status={}", bench.name, status);
    g_benchStick.store(false, std::memory_order_relaxed);
    if (bench.shot && std::strcmp(status, "ok") == 0)
        g_benchShot.store(true, std::memory_order_relaxed);
    run.phase = Phase::Finishing;
    run.finishLeft = 30; // the screenshot, and a second for the log to reach the card
}

void StepBench(BenchRun& run, const PresentSample& sample, float intervalMs, bool hitch)
{
    const BenchConfig& bench = GetBench();
    switch (run.phase)
    {
    case Phase::Waiting:
        if (os::logger::ElapsedSeconds() > bench.timeoutSeconds)
        {
            LOG_NOTICE("bench: {} not reached after {:.0f} s (start_swap={} start_draws={}, last frame {} draws)",
                bench.name, bench.timeoutSeconds, bench.startSwap, bench.startDraws, g_lastDraws.load(std::memory_order_relaxed));
            FinishBench(run, bench, sample.swap, "timeout");
            break;
        }
        if (sample.loading || sample.swap < bench.startSwap || g_lastDraws.load(std::memory_order_relaxed) < bench.startDraws)
            break;
        run.phase = Phase::Settling;
        run.settleLeft = bench.settle;
        run.armedSwap = sample.swap;
        g_benchStick.store(bench.stick, std::memory_order_relaxed);
        LOG_NOTICE("bench: {} armed at swap {} ({} draws, map {}); measuring {} frames after {} more", bench.name,
            sample.swap, g_lastDraws.load(std::memory_order_relaxed), MapText(), bench.frames, bench.settle);
        break;
    case Phase::Settling:
        if (sample.loading)
        {
            run.settleLeft = bench.settle; // a load started: settle again once it is over
            break;
        }
        if (run.settleLeft && --run.settleLeft)
            break;
        run.phase = Phase::Measuring;
        run.startSwap = sample.swap;
        run.intervals.clear();
        run.intervals.reserve(bench.frames);
        run.loadingFrames = run.hitches = 0;
        run.sleepNs = run.presentNs = 0;
        run.targetFps = sample.targetFps;
#if LO_PLATFORM_SWITCH
        SampleThreads(run.threadsBegin, true);
#endif
        run.begin = ReadCounters();
        run.beginTime = Clock::now();
        LOG_NOTICE("bench: {} measuring from swap {} (target {} fps, scene {})", bench.name, sample.swap,
            sample.targetFps, SceneText());
        break;
    case Phase::Measuring:
        run.intervals.push_back(intervalMs);
        run.loadingFrames += sample.loading;
        run.hitches += hitch;
        run.sleepNs += sample.sleepNs;
        run.presentNs += sample.presentNs;
        if (run.intervals.size() >= bench.frames)
            FinishBench(run, bench, sample.swap, run.loadingFrames ? "loading" : "ok");
        break;
    case Phase::Finishing:
        if (run.finishLeft && --run.finishLeft)
            break;
        run.phase = Phase::Done;
        if (bench.exit)
        {
            LOG_NOTICE("bench: {} quitting (LO_BENCH_EXIT)", bench.name);
            settings::quit_action::PushQuit();
        }
        break;
    case Phase::Done:
        break;
    }
}
}

void Present(const PresentSample& sample)
{
    const Config& config = GetConfig();
    const bool bench = !GetBench().name.empty();
    const bool perf = config.intervalSeconds > 0;
    if (!perf && !bench)
        return;

    static Clock::time_point last{};
    static Counters previous{};
    static uint32_t hitchesLogged = 0;
    static BenchRun run;
    static std::once_flag started;
    std::call_once(started, [&] {
        if (config.gpu)
            gpu::gpu_busy::g_perf.store(true, std::memory_order_relaxed);
        if (perf)
            os::DetachThread(std::thread(MonitorMain));
        LOG_NOTICE("perf stats: {}, GPU timing {}{}",
            perf ? fmt::format("perf lines every {} s, hitch lines over {}", config.intervalSeconds,
                       config.hitchMs > 0 ? fmt::format("{:.0f} ms", config.hitchMs)
                                         : std::string("twice the target and twice the recent median frame time"))
                 : std::string("perf and hitch lines off"),
            config.gpu ? "on" : "off", bench ? fmt::format("; benchmark run '{}'", GetBench().name) : std::string());
    });

    const auto now = Clock::now();
    const Counters current = ReadCounters();
    if (last == Clock::time_point{})
    {
        last = now;
        previous = current;
        return;
    }
    const float intervalMs = std::chrono::duration<float, std::milli>(now - last).count();
    last = now;
    // A hitch stands out from the frames around it: over twice the target and over twice the median of the last
    // 64 frames outside loading screens. A scene that runs at 15 fps throughout has slow frames, not hitches.
    static float recent[64] = {};
    static uint32_t recentCount = 0, recentNext = 0;
    static float recentMedian = 0;
    if (!sample.loading)
    {
        recent[recentNext] = intervalMs;
        recentNext = (recentNext + 1) % 64;
        recentCount = std::min<uint32_t>(recentCount + 1, 64);
        if (recentNext % 16 == 0)
        {
            float sorted[64];
            std::copy(recent, recent + recentCount, sorted);
            std::nth_element(sorted, sorted + recentCount / 2, sorted + recentCount);
            recentMedian = sorted[recentCount / 2];
        }
    }
    const double threshold = config.hitchMs > 0 ? config.hitchMs
        : std::max(2.0 * TargetMs(sample.targetFps), 2.0 * double(recentMedian));
    // The first frames (logos, the first shaders) are not worth a line each.
    const bool hitch = !sample.loading && sample.swap > 60 && intervalMs > threshold;
    if (perf)
    {
        std::lock_guard lock(g_windowMutex);
        ++g_window.frames;
        g_window.targetFps = sample.targetFps;
        g_window.sleepNs += sample.sleepNs;
        g_window.presentNs += sample.presentNs;
        if (sample.loading)
            ++g_window.loadingFrames;
        else
            g_window.intervals.push_back(intervalMs);
        g_window.hitches += hitch;
    }
    if (hitch && perf && hitchesLogged < config.hitchLimit)
    {
        ++hitchesLogged;
        const uint64_t gpuNs = current.gpuBusyNs - previous.gpuBusyNs;
        LOG_NOTICE("hitch: swap={} frame_ms={:.1f} target_ms={:.1f} {} cp_idle_ms={:.1f} pace_ms={:.1f} present_ms={:.1f}"
            " draws={}{} median_ms={:.1f} ({} of {})",
            sample.swap, intervalMs, TargetMs(sample.targetFps),
            gpuNs ? fmt::format("gpu_ms={:.1f}", Ms(gpuNs)) : std::string("gpu_ms=-"),
            Ms(current.cpIdleNs - previous.cpIdleNs), Ms(sample.sleepNs), Ms(sample.presentNs),
            g_lastDraws.load(std::memory_order_relaxed), BackendWork(previous, current, 1.0), recentMedian, hitchesLogged,
            config.hitchLimit);
    }
    previous = current;
    if (bench)
        StepBench(run, sample, intervalMs, hitch);
}

void CpIdle(uint64_t ns)
{
    g_cpIdleNs.fetch_add(ns, std::memory_order_relaxed);
}

void FrameDraws(uint32_t draws)
{
    g_lastDraws.store(draws, std::memory_order_relaxed);
    g_draws.fetch_add(draws, std::memory_order_relaxed);
}

void SceneSize(uint32_t width, uint32_t height)
{
    g_sceneSize.store(uint64_t(width) << 32 | height, std::memory_order_relaxed);
}

std::string MapName() { return MapText(); }

std::string LastPerfLine()
{
    std::lock_guard lock(g_lastLineMutex);
    return g_lastLine;
}

uint64_t TotalDraws()
{
    return g_draws.load(std::memory_order_relaxed);
}

uint32_t GpuClockMhz()
{
    return g_gpuMhz.load(std::memory_order_relaxed);
}

bool BenchStick(int16_t& x, int16_t& y)
{
    if (!g_benchStick.load(std::memory_order_relaxed))
        return false;
    x = GetBench().stickX;
    y = GetBench().stickY;
    return true;
}

bool TakeBenchScreenshot()
{
    return g_benchShot.load(std::memory_order_relaxed) && g_benchShot.exchange(false, std::memory_order_relaxed);
}
}
