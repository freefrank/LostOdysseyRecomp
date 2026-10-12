#include "host_scheduling.h"
#include <os/platform.h>

#if LO_PLATFORM_MACOS
#include <cstdint>
#include <objc/message.h>
#include <objc/runtime.h>
#endif
#if defined(__ANDROID__)
#include <os/logger.h>
#include <os/thread_name.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <dlfcn.h>
#include <sched.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
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
struct RoleMasks
{
    uint64_t render = 0, main = 0, guest = 0, other = 0;
    bool Any() const { return render || main || guest || other; }
    uint64_t For(const char* comm) const
    {
        if (strcmp(comm, "GPU CmdProc") == 0) return render;
        if (strcmp(comm, "Guest Main") == 0) return main;
        if (strncmp(comm, "Guest ", 6) == 0) return guest;
        return other;
    }
};

RoleMasks ReadRoleMasks()
{
    RoleMasks masks;
    const char* spec = getenv("LO_THREAD_AFFINITY");
    if (!spec)
        return masks;
    const std::string text(spec);
    for (size_t pos = 0; pos < text.size();) {
        size_t end = text.find(',', pos);
        if (end == std::string::npos) end = text.size();
        const std::string item = text.substr(pos, end - pos);
        const size_t eq = item.find('=');
        if (eq != std::string::npos) {
            const std::string role = item.substr(0, eq);
            const uint64_t mask = strtoull(item.c_str() + eq + 1, nullptr, 0);
            if (role == "render") masks.render = mask;
            else if (role == "main") masks.main = mask;
            else if (role == "guest") masks.guest = mask;
            else if (role == "other") masks.other = mask;
        }
        pos = end + 1;
    }
    return masks;
}

std::atomic<pid_t> g_guestMainTid{0};

template <typename F>
void ForEachThread(F&& visit)
{
    DIR* dir = opendir("/proc/self/task");
    if (!dir)
        return;
    while (dirent* entry = readdir(dir)) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
            continue;
        const pid_t tid = pid_t(atoi(entry->d_name));
        char path[64];
        snprintf(path, sizeof(path), "/proc/self/task/%d/comm", int(tid));
        char comm[32]{};
        if (FILE* file = fopen(path, "re")) {
            if (fgets(comm, sizeof(comm), file)) {
                comm[strcspn(comm, "\n")] = 0;
                visit(tid, comm);
            }
            fclose(file);
        }
    }
    closedir(dir);
}

void FindGuestMain()
{
    ForEachThread([](pid_t tid, const char* comm) {
        if (strcmp(comm, "Guest Main") == 0) g_guestMainTid.store(tid, std::memory_order_relaxed);
    });
}

// Threads are named after they start, so the scan repeats and re-applies a
// mask when a thread's role (name) changes.
void PlacementLoop(RoleMasks masks)
{
    os::SetCurrentThreadName("Thread Placement");
    std::unordered_map<pid_t, uint64_t> applied;
    for (;;) {
        size_t changed = 0, failed = 0;
        ForEachThread([&](pid_t tid, const char* comm) {
            if (strcmp(comm, "Guest Main") == 0) g_guestMainTid.store(tid, std::memory_order_relaxed);
            const uint64_t mask = masks.For(comm);
            auto found = applied.find(tid);
            if (!mask || (found != applied.end() && found->second == mask))
                return;
            cpu_set_t set;
            CPU_ZERO(&set);
            for (int cpu = 0; cpu < 64; ++cpu)
                if (mask & (1ull << cpu)) CPU_SET(cpu, &set);
            (sched_setaffinity(tid, sizeof(set), &set) == 0 ? changed : failed)++;
            applied[tid] = mask;
        });
        if (changed || failed)
            LOG_INFO("thread placement: affinity set on {} threads, {} failed", changed, failed);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
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

int64_t SelfCpuNs()
{
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return int64_t(ts.tv_sec) * 1000000000 + ts.tv_nsec;
}

struct RenderFrameState
{
    using GetManagerFn = void* (*)();
    using CreateSessionFn = void* (*)(void*, const int32_t*, size_t, int64_t);
    using ReportFn = int (*)(void*, int64_t);

    bool adpf = false;
    int statsSeconds = 0;
    int64_t targetNs = 16666667;
    void* session = nullptr;
    ReportFn report = nullptr;
    uint32_t sessionAttempts = 0;
    std::chrono::steady_clock::time_point last{}, windowStart{};
    int64_t lastRenderCpu = 0, lastMainCpu = 0;
    pid_t mainTid = 0;
    std::vector<float> intervals;
    double renderCpuMs = 0, mainCpuMs = 0;

    RenderFrameState()
    {
        const char* adpfEnv = getenv("LO_ADPF");
        adpf = adpfEnv && adpfEnv[0] == '1';
        if (const char* target = getenv("LO_ADPF_TARGET_US")) targetNs = strtoll(target, nullptr, 10) * 1000;
        if (const char* stats = getenv("LO_FRAME_STATS")) statsSeconds = stats[0] == '1' && !stats[1] ? 10 : atoi(stats);
    }

    bool Enabled() const { return adpf || statsSeconds > 0; }

    // The session covers the render thread and, once it exists, the guest main thread.
    void TryCreateSession()
    {
        if (session || !adpf || sessionAttempts > 600 || (!mainTid && sessionAttempts++ < 600))
            return;
        sessionAttempts = 601;
        void* android = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
        auto getManager = android ? reinterpret_cast<GetManagerFn>(dlsym(android, "APerformanceHint_getManager")) : nullptr;
        auto create = android ? reinterpret_cast<CreateSessionFn>(dlsym(android, "APerformanceHint_createSession")) : nullptr;
        report = android ? reinterpret_cast<ReportFn>(dlsym(android, "APerformanceHint_reportActualWorkDuration")) : nullptr;
        void* manager = getManager ? getManager() : nullptr;
        int32_t tids[2] = { int32_t(gettid()), int32_t(mainTid) };
        session = manager && create && report ? create(manager, tids, mainTid ? 2 : 1, targetNs) : nullptr;
        LOG_INFO("thread placement: ADPF session {} (render tid {}, guest main tid {}, target {} us)",
            session ? "created" : "unavailable", tids[0], mainTid, targetNs / 1000);
    }

    void Frame()
    {
        const auto now = std::chrono::steady_clock::now();
        if (!mainTid) {
            mainTid = g_guestMainTid.load(std::memory_order_relaxed);
            if (!mainTid && (intervals.size() % 60) == 0) {
                FindGuestMain();
                mainTid = g_guestMainTid.load(std::memory_order_relaxed);
            }
        }
        const int64_t renderCpu = SelfCpuNs();
        const int64_t mainCpu = mainTid ? ThreadCpuNs(mainTid) : 0;
        if (last != std::chrono::steady_clock::time_point{}) {
            const int64_t renderDelta = renderCpu - lastRenderCpu;
            const int64_t mainDelta = lastMainCpu && mainCpu ? mainCpu - lastMainCpu : 0;
            TryCreateSession();
            if (session) report(session, std::max<int64_t>(std::max(renderDelta, mainDelta), 1));
            intervals.push_back(std::chrono::duration<float, std::milli>(now - last).count());
            renderCpuMs += renderDelta / 1e6;
            mainCpuMs += mainDelta / 1e6;
        } else {
            windowStart = now;
        }
        last = now;
        lastRenderCpu = renderCpu;
        lastMainCpu = mainCpu;
        if (statsSeconds > 0 && now - windowStart >= std::chrono::seconds(statsSeconds) && !intervals.empty()) {
            std::vector<float> sorted = intervals;
            std::sort(sorted.begin(), sorted.end());
            const auto at = [&](double p) { return sorted[std::min(sorted.size() - 1, size_t(p * (sorted.size() - 1) + 0.5))]; };
            const double seconds = std::chrono::duration<double>(now - windowStart).count();
            const double frames = double(intervals.size());
            LOG_INFO("frame stats: frames={} fps={:.1f} interval_p50={:.2f} p95={:.2f} p99={:.2f} max={:.2f} render_cpu={:.2f} main_cpu={:.2f} adpf={}",
                intervals.size(), frames / seconds, at(0.50), at(0.95), at(0.99), sorted.back(),
                renderCpuMs / frames, mainCpuMs / frames, session ? 1 : 0);
            intervals.clear();
            renderCpuMs = mainCpuMs = 0;
            windowStart = now;
        }
    }
};
}

void StartThreadPlacement()
{
    const RoleMasks masks = ReadRoleMasks();
    if (!masks.Any())
        return;
    LOG_INFO("thread placement: affinity render={:#x} main={:#x} guest={:#x} other={:#x}",
        masks.render, masks.main, masks.guest, masks.other);
    std::thread(PlacementLoop, masks).detach();
}

void OnRenderFrame()
{
    static RenderFrameState state;
    if (state.Enabled())
        state.Frame();
}
#else
void StartThreadPlacement()
{
}

void OnRenderFrame()
{
}
#endif
}
