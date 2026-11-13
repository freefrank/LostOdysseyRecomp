#include "host_scheduling.h"
#include <os/platform.h>

#if LO_PLATFORM_MACOS
#include <cstdint>
#include <objc/message.h>
#include <objc/runtime.h>
#endif
#if defined(__ANDROID__)
#include <os/logger.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <ctime>
#include <cmath>
#include <dlfcn.h>
#include <android/thermal.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>
#endif

namespace os::scheduling
{
#if LO_PLATFORM_MACOS
void BeginLatencyCriticalActivity()
{
    // [[NSProcessInfo processInfo] beginActivityWithOptions:reason:] through the
    // Objective-C runtime, so the runtime needs no Objective-C++ source. The
    // returned token is intentionally retained for the process lifetime.
    // NSActivityUserInitiated (0x00FFFFFF) | NSActivityLatencyCritical (0xFF00000000)
    constexpr uint64_t kOptions = 0x00FFFFFFull | 0xFF00000000ull;
    using ProcessInfoFn = id (*)(Class, SEL);
    using StringFn = id (*)(Class, SEL, const char*);
    using BeginFn = id (*)(id, SEL, uint64_t, id);
    using RetainFn = id (*)(id, SEL);

    Class processInfoClass = objc_getClass("NSProcessInfo");
    Class stringClass = objc_getClass("NSString");
    if (!processInfoClass || !stringClass)
        return;
    id processInfo = reinterpret_cast<ProcessInfoFn>(objc_msgSend)(processInfoClass, sel_registerName("processInfo"));
    id reason = reinterpret_cast<StringFn>(objc_msgSend)(stringClass, sel_registerName("stringWithUTF8String:"),
        "Real-time game rendering and audio");
    if (!processInfo || !reason)
        return;
    id activity = reinterpret_cast<BeginFn>(objc_msgSend)(processInfo,
        sel_registerName("beginActivityWithOptions:reason:"), kOptions, reason);
    if (activity)
        reinterpret_cast<RetainFn>(objc_msgSend)(activity, sel_registerName("retain"));
}
#else
void BeginLatencyCriticalActivity()
{
}
#endif

#if defined(__ANDROID__)
namespace
{
// CPUs whose capacity is at most half the largest one are efficiency cores.
// Empty when the device has none or does not report capacities.
cpu_set_t PerformanceCores(int* count)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    *count = 0;
    const char* env = getenv("LO_PERFORMANCE_CORES");
    if (env && env[0] == '0')
        return set;
    const long cpus = std::min<long>(sysconf(_SC_NPROCESSORS_CONF), CPU_SETSIZE);
    std::vector<int> capacity(size_t(std::max<long>(cpus, 0)), 0);
    int largest = 0;
    for (long cpu = 0; cpu < cpus; ++cpu) {
        char path[64];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/cpu_capacity", cpu);
        if (FILE* file = fopen(path, "re")) {
            if (fscanf(file, "%d", &capacity[cpu]) != 1) capacity[cpu] = 0;
            fclose(file);
        }
        largest = std::max(largest, capacity[cpu]);
    }
    bool hasEfficiency = false;
    for (long cpu = 0; cpu < cpus; ++cpu) {
        if (capacity[cpu] > 0 && capacity[cpu] * 2 <= largest) hasEfficiency = true;
        else if (capacity[cpu] > 0) { CPU_SET(cpu, &set); ++*count; }
    }
    if (!hasEfficiency) {
        CPU_ZERO(&set);
        *count = 0;
    }
    return set;
}

int64_t ThreadCpuNs(pid_t tid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/task/%d/schedstat", int(tid));
    unsigned long long ns = 0;
    if (FILE* file = fopen(path, "re")) {
        if (fscanf(file, "%llu", &ns) != 1) ns = 0;
        fclose(file);
    }
    return int64_t(ns);
}

std::atomic<pid_t> g_guestMainTid{0};

// Read-only diagnostics. Resolve API 30/31 entry points at runtime so the
// minimum Android API stays unchanged. No affinity, quality or clock policy.
struct ThermalStats
{
    void* library = nullptr;
    AThermalManager* manager = nullptr;
    using Release = void (*)(AThermalManager*);
    using Status = AThermalStatus (*)(AThermalManager*);
    using Headroom = float (*)(AThermalManager*, int);
    Release release = nullptr;
    Status status = nullptr;
    Headroom headroom = nullptr;
    std::chrono::steady_clock::time_point next{};

    ThermalStats()
    {
        const char* env = getenv("LO_THERMAL_STATS");
        if (!env || std::strcmp(env, "1") != 0) return;
        library = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
        if (library) {
            using Acquire = AThermalManager* (*)();
            auto acquire = reinterpret_cast<Acquire>(dlsym(library, "AThermal_acquireManager"));
            release = reinterpret_cast<Release>(dlsym(library, "AThermal_releaseManager"));
            status = reinterpret_cast<Status>(dlsym(library, "AThermal_getCurrentThermalStatus"));
            headroom = reinterpret_cast<Headroom>(dlsym(library, "AThermal_getThermalHeadroom"));
            if (acquire && release && status) manager = acquire();
        }
        if (!manager) LOG_WARNING("thermal stats: Android thermal API unavailable");
    }
    ~ThermalStats()
    {
        if (manager) release(manager);
        if (library) dlclose(library);
    }
    void Frame()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now < next) return;
        next = now + std::chrono::seconds(10);
        const auto current = status(manager);
        const float value = headroom ? headroom(manager, 0) : NAN;
        // Status is an OS assessment, not a measured CPU/GPU frequency cap.
        // NaN (common on unsupported OEMs) is reported, never used as policy.
        if (std::isfinite(value))
            LOG_INFO("thermal stats: status={} headroom={:.3f} forecast_seconds=0", int(current), value);
        else
            LOG_INFO("thermal stats: status={} headroom=unavailable", int(current));
    }
};

// LO_FRAME_STATS=<seconds> (1 means 10): frame interval percentiles and the
// render and guest main threads' CPU time per frame, without LO_RENDER_TIMING.
struct FrameStats
{
    int seconds = 0;
    std::chrono::steady_clock::time_point last{}, windowStart{};
    int64_t lastRenderCpu = 0, lastMainCpu = 0;
    std::vector<float> intervals;
    double renderCpuMs = 0, mainCpuMs = 0;

    FrameStats()
    {
        if (const char* env = getenv("LO_FRAME_STATS")) seconds = env[0] == '1' && !env[1] ? 10 : atoi(env);
    }

    void Frame()
    {
        const auto now = std::chrono::steady_clock::now();
        timespec ts{};
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
        const int64_t renderCpu = int64_t(ts.tv_sec) * 1000000000 + ts.tv_nsec;
        const pid_t mainTid = g_guestMainTid.load(std::memory_order_relaxed);
        const int64_t mainCpu = mainTid ? ThreadCpuNs(mainTid) : 0;
        if (last == std::chrono::steady_clock::time_point{}) {
            windowStart = now;
        } else {
            intervals.push_back(std::chrono::duration<float, std::milli>(now - last).count());
            renderCpuMs += (renderCpu - lastRenderCpu) / 1e6;
            mainCpuMs += lastMainCpu && mainCpu ? (mainCpu - lastMainCpu) / 1e6 : 0;
        }
        last = now;
        lastRenderCpu = renderCpu;
        lastMainCpu = mainCpu;
        if (now - windowStart < std::chrono::seconds(seconds) || intervals.empty())
            return;
        std::vector<float> sorted = intervals;
        std::sort(sorted.begin(), sorted.end());
        const auto at = [&](double p) { return sorted[std::min(sorted.size() - 1, size_t(p * (sorted.size() - 1) + 0.5))]; };
        const double frames = double(intervals.size());
        LOG_INFO("frame stats: frames={} fps={:.1f} interval_p50={:.2f} p95={:.2f} p99={:.2f} max={:.2f} render_cpu={:.2f} main_cpu={:.2f}",
            intervals.size(), frames / std::chrono::duration<double>(now - windowStart).count(), at(0.50), at(0.95), at(0.99),
            sorted.back(), renderCpuMs / frames, mainCpuMs / frames);
        intervals.clear();
        renderCpuMs = mainCpuMs = 0;
        windowStart = now;
    }
};
}

void PreferPerformanceCores(bool guestMain)
{
    static int count = 0;
    static const cpu_set_t cores = PerformanceCores(&count);
    if (guestMain)
        g_guestMainTid.store(gettid(), std::memory_order_relaxed);
    if (count > 0 && sched_setaffinity(0, sizeof(cores), &cores) != 0)
        LOG_WARNING("thread placement: sched_setaffinity failed, errno {}", errno);
    static std::once_flag logged;
    std::call_once(logged, [] { LOG_INFO("thread placement: game threads on {} performance cores", count); });
}

void OnRenderFrame()
{
    static FrameStats stats;
    if (stats.seconds > 0)
        stats.Frame();
    static ThermalStats thermal;
    if (thermal.manager)
        thermal.Frame();
}

void RaiseAudioThreadPriority()
{
    // ANDROID_PRIORITY_AUDIO; an app may raise its own threads this far.
    if (setpriority(PRIO_PROCESS, gettid(), -16) != 0)
        LOG_WARNING("audio thread priority: setpriority failed, errno {}", errno);
}
#else
void PreferPerformanceCores(bool)
{
}

void OnRenderFrame()
{
}

void RaiseAudioThreadPriority()
{
}
#endif
}
