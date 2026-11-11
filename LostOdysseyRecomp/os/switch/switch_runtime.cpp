// Nintendo Switch process setup, crash log and system dialogs.
// Parts follow UnleashedRecomp-NX (os/switch/runtime_switch.cpp, crash_switch.cpp).
#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include <os/debug_server.h>
#include <os/netlog.h>
#include <os/switch_platform.h>
#include <os/switch_cpu_profiler.h>
#include <switch.h>
#include <sys/iosupport.h>
#include <atomic>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cxxabi.h>
#include <exception>
#include <typeinfo>
#include <cstdlib>
#include <malloc.h>
#include <system_error>

#ifdef LO_SWITCH_PGO_GEN
namespace os::pgo { void StartAutoSave(); void SaveOnExit(); } // os/switch/pgo_switch.cpp: training builds only
#endif

extern "C"
{
    // A full application (title takeover) gets all the memory and every
    // syscall the 4 GiB guest window needs. __nx_heap_size 0: libnx gives the
    // newlib heap everything that is left; guest memory is carved out of it.
    u32 __nx_applet_type = AppletType_Application;
    size_t __nx_heap_size = 0;

    // libnx calls __libnx_exception_handler on this stack after a CPU fault.
    alignas(16) u8 __nx_exception_stack[0x8000];
    u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
}

// libnx's bounds of newlib's heap (sbrk hands out memory between them).
extern "C" char* fake_heap_start;
extern "C" char* fake_heap_end;

// os/log_file.h (not included: this file sees libnx headers, not the PCH).
namespace os::logger { void DrainForExit() noexcept; }

namespace
{
// stderr (fd 2) goes through this device: the file plus the network log.
FILE* g_stderrFile = nullptr;
devoptab_t g_stderrDevice{};
AppletHookCookie g_operationModeHook{};
std::atomic<bool> g_operationModeChanged{false};
void StartBoostThread(); // below

void OnAppletEvent(AppletHookType hook, void*)
{
    if (hook == AppletHookType_OnOperationMode)
        g_operationModeChanged.store(true, std::memory_order_release);
    // A game started docked gets the handheld GPU configuration when it is first undocked.
    if (hook == AppletHookType_OnPerformanceMode && appletGetPerformanceMode() == ApmPerformanceMode_Normal)
        StartBoostThread();
}

ssize_t StderrWrite(struct _reent*, void*, const char* data, size_t size)
{
    if (g_stderrFile)
        std::fwrite(data, 1, size, g_stderrFile);
    os::netlog::Write(data, size);
    return ssize_t(size);
}

char g_crashLogPath[512] = "sdmc:/switch/LostOdysseyRecomp/crash.log";

// RawAppend's own file system session (see switch_platform.h). The requests are libnx's fs.c ones,
// sent directly on this session.
Service g_rawFs{};
Service g_rawSd{};
bool g_rawReady = false;
char g_rawRoot[256] = "/switch/LostOdysseyRecomp";
Mutex g_rawMutex;

Result RawInitialize()
{
    Result rc = smGetService(&g_rawFs, "fsp-srv");
    if (R_FAILED(rc))
        return rc;
    const uint64_t pidPlaceholder = 0;
    rc = serviceDispatchIn(&g_rawFs, 1, pidPlaceholder, .in_send_pid = true); // SetCurrentProcess
    if (R_SUCCEEDED(rc))
        rc = serviceDispatch(&g_rawFs, 18, .out_num_objects = 1, .out_objects = &g_rawSd); // OpenSdCardFileSystem
    if (R_FAILED(rc))
    {
        serviceClose(&g_rawFs);
        return rc;
    }
    g_rawReady = true;
    return 0;
}

// The crash report is built here: the handler must not allocate.
char g_crashText[8192];
size_t g_crashLength = 0;
std::atomic<int> g_crashDepth{0};

__attribute__((format(printf, 1, 2))) void CrashAppend(const char* format, ...)
{
    if (g_crashLength + 1 >= sizeof(g_crashText))
        return;
    va_list args;
    va_start(args, format);
    const int length = std::vsnprintf(g_crashText + g_crashLength, sizeof(g_crashText) - g_crashLength, format, args);
    va_end(args);
    if (length > 0)
        g_crashLength = std::min(sizeof(g_crashText) - 1, g_crashLength + size_t(length));
}
uintptr_t g_moduleBase = 0;
uintptr_t g_moduleEnd = 0;

// The main NRO's text segment, so crash addresses can be resolved against
// LostOdysseyRecomp.elf with addr2line (offset = pc - base).
void CaptureModuleRange()
{
    MemoryInfo info{};
    u32 page = 0;
    const auto self = reinterpret_cast<uintptr_t>(&CaptureModuleRange);
    if (R_SUCCEEDED(svcQueryMemory(&info, &page, self)))
    {
        g_moduleBase = info.addr;
        g_moduleEnd = info.addr + info.size;
    }
}

void WriteAddress(FILE* file, const char* name, uint64_t value)
{
    if (value >= g_moduleBase && value < g_moduleEnd)
        std::fprintf(file, "%-4s %016" PRIx64 "  (elf+%#" PRIx64 ")\n", name, value, value - g_moduleBase);
    else
        std::fprintf(file, "%-4s %016" PRIx64 "\n", name, value);
}

[[noreturn]] void ExitWithLog()
{
    os::logger::DrainForExit(); // lines still queued for the log writer
    std::fflush(nullptr);
    svcExitProcess();
    __builtin_unreachable();
}

void WriteAddress(FILE* file, const char* name, uint64_t value);

// Frame-pointer chain from the caller of this function (host code keeps frame
// pointers; the recompiled game code does not, so the chain may stop there).
void WriteBacktrace(FILE* file, uintptr_t frame)
{
    std::fprintf(file, "backtrace:\n");
    for (int depth = 0; depth < 48 && frame && (frame & 7) == 0; ++depth)
    {
        MemoryInfo info{};
        u32 page = 0;
        if (R_FAILED(svcQueryMemory(&info, &page, frame)) || !(info.perm & Perm_R))
            break;
        const auto* record = reinterpret_cast<const uintptr_t*>(frame);
        WriteAddress(file, "  at", record[1]);
        if (record[0] <= frame)
            break;
        frame = record[0];
    }
}

void OnTerminate()
{
    if (FILE* file = std::fopen(g_crashLogPath, "a"))
    {
        const std::time_t now = std::time(nullptr);
        std::fprintf(file, "\n=== std::terminate, %s", std::ctime(&now));
        if (const std::type_info* type = abi::__cxa_current_exception_type())
        {
            int status = 0;
            char* name = abi::__cxa_demangle(type->name(), nullptr, nullptr, &status);
            std::fprintf(file, "exception type: %s\n", status == 0 && name ? name : type->name());
            std::free(name);
            if (auto exception = std::current_exception())
            {
                try { std::rethrow_exception(exception); }
                catch (const std::exception& error) { std::fprintf(file, "what(): %s\n", error.what()); }
                catch (...) {}
            }
        }
        else
        {
            std::fprintf(file, "no active exception (thread/noexcept/abort path)\n");
        }
        std::fprintf(file, "module %016" PRIxPTR "-%016" PRIxPTR "\n", g_moduleBase, g_moduleEnd);
        std::fprintf(file, "%s\n", os::switch_platform::MemorySummary().c_str());
        WriteBacktrace(file, reinterpret_cast<uintptr_t>(__builtin_frame_address(0)));
        std::fclose(file);
    }
    ExitWithLog();
}
}

extern "C" void __libnx_exception_handler(ThreadExceptionDump* ctx)
{
    // Runs on __nx_exception_stack. Nothing here allocates or uses stdio until the report is on the
    // card: a crash with an empty crash.log (Uhra, round 6) means the old fopen/fprintf path got stuck.
    if (g_crashDepth.fetch_add(1) != 0)
    {
        // A second fault: in this handler, or another thread while the first is being reported
        // (they share the exception stack). Note it and leave.
        char line[192];
        const int length = std::snprintf(line, sizeof(line), "=== second fault while reporting a crash: pc %016" PRIx64
            " (elf+%#" PRIx64 ") far %016" PRIx64 "\n", ctx->pc.x, ctx->pc.x - g_moduleBase, ctx->far.x);
        if (length > 0)
            os::switch_platform::RawAppend("crash.log", line, std::min(size_t(length), sizeof(line) - 1));
        svcExitProcess();
    }

    g_crashLength = 0;
    char thread[48] = "(unnamed)";
    os::switch_cpu_profiler::TryGetCurrentThreadName(thread, sizeof(thread));
    u64 seconds = 0;
    timeGetCurrentTime(TimeType_Default, &seconds);
    CrashAppend("\n=== Lost Odyssey Recomp crash, unix time %llu, thread %s\n", (unsigned long long)seconds, thread);
    CrashAppend("error_desc %#x  esr %#x  far %016" PRIx64 "  pstate %#x\n",
        ctx->error_desc, ctx->esr, ctx->far.x, ctx->pstate);
    CrashAppend("module %016" PRIxPTR "-%016" PRIxPTR "\n", g_moduleBase, g_moduleEnd);
    auto address = [](const char* name, uint64_t value) {
        if (value >= g_moduleBase && value < g_moduleEnd)
            CrashAppend("%-4s %016" PRIx64 "  (elf+%#" PRIx64 ")\n", name, value, value - g_moduleBase);
        else
            CrashAppend("%-4s %016" PRIx64 "\n", name, value);
    };
    address("pc", ctx->pc.x);
    address("lr", ctx->lr.x);
    address("sp", ctx->sp.x);
    address("fp", ctx->fp.x);
    for (int i = 0; i < 29; ++i)
    {
        char name[8];
        std::snprintf(name, sizeof(name), "x%d", i);
        address(name, ctx->cpu_gprs[i].x);
    }
    // Frame-pointer chain of the host code (the recompiled game code omits
    // frame pointers, so the chain stops at the first guest function).
    CrashAppend("backtrace:\n");
    uintptr_t frame = ctx->fp.x;
    for (int depth = 0; depth < 32 && frame && (frame & 7) == 0; ++depth)
    {
        MemoryInfo info{};
        u32 page = 0;
        if (R_FAILED(svcQueryMemory(&info, &page, frame)) || !(info.perm & Perm_R))
            break;
        const auto* record = reinterpret_cast<const uintptr_t*>(frame);
        address("  at", record[1]);
        if (record[0] <= frame)
            break;
        frame = record[0];
    }

    if (os::switch_platform::RawAppend("crash.log", g_crashText, g_crashLength))
    {
        // Where every other thread was: the crash may be a symptom of one of them.
        size_t size = 0;
        const char* threads = os::switch_cpu_profiler::DumpThreads("other threads at the crash:\n", size, "crash.log");
        os::switch_platform::RawAppend("crash.log", threads, size);
        static const char done[] = "=== end of crash report\n";
        os::switch_platform::RawAppend("crash.log", done, sizeof(done) - 1);
    }
    else if (FILE* file = std::fopen(g_crashLogPath, "a"))
    {
        // No session of our own: the old path, best effort.
        std::fwrite(g_crashText, 1, g_crashLength, file);
        std::fclose(file);
    }
    ExitWithLog();
}

namespace
{
constexpr u32 kHandheldGpu460Memory1331 = 0x92220008;
Thread g_apmThread;
bool g_apmThreadCreated = false;          // g_apmThread holds a thread to close before the next one
std::atomic<bool> g_apmWanted{false};     // StartHandheldGpuBoost was called
std::atomic<bool> g_apmRunning{false};    // ApmThreadMain is running
std::atomic<bool> g_apmDone{false};       // it ran in handheld mode: applied, reverted or not available
std::atomic<bool> g_apmApplied{false};    // the configuration is ours: ExitNow puts g_apmOriginal back
u32 g_apmOriginal = 0;                    // apm's handheld configuration before ours

bool ReadClocks(u32& gpuHz, u32& memoryHz)
{
    gpuHz = memoryHz = 0;
    if (R_FAILED(clkrstInitialize()))
        return false;
    bool ok = true;
    ClkrstSession session{};
    if (R_SUCCEEDED(clkrstOpenSession(&session, PcvModuleId_GPU, 3)))
    {
        ok &= R_SUCCEEDED(clkrstGetClockRate(&session, &gpuHz));
        clkrstCloseSession(&session);
    }
    else
        ok = false;
    if (R_SUCCEEDED(clkrstOpenSession(&session, PcvModuleId_EMC, 3)))
    {
        ok &= R_SUCCEEDED(clkrstGetClockRate(&session, &memoryHz));
        clkrstCloseSession(&session);
    }
    else
        ok = false;
    clkrstExit();
    return ok;
}

// apmSetPerformanceConfiguration returns before pcv applies the clocks, so the
// result is checked by polling. The apm session stays open for the process.
// apm keeps a configuration per performance mode, so the handheld one set here
// also applies after the console is docked and undocked again.
void ApmThreadMain(void*)
{
    struct Done { ~Done() { g_apmRunning.store(false, std::memory_order_release); } } done;
    if (R_FAILED(apmInitialize()))
    {
        std::fprintf(stderr, "[apm] apm is not available; clocks unchanged.\n");
        g_apmDone = true;
        return;
    }
    ApmPerformanceMode mode = ApmPerformanceMode_Invalid;
    if (R_FAILED(apmGetPerformanceMode(&mode)) || mode != ApmPerformanceMode_Normal)
    {
        std::fprintf(stderr, "[apm] Console is not in handheld mode; clocks unchanged until it is undocked.\n");
        return;
    }
    g_apmDone = true;
    u32 original = 0;
    const bool hasOriginal = R_SUCCEEDED(apmGetPerformanceConfiguration(mode, &original));
    u32 gpuBefore = 0, memoryBefore = 0;
    if (!ReadClocks(gpuBefore, memoryBefore))
    {
        std::fprintf(stderr, "[apm] Unable to read clocks through clkrst; clocks unchanged.\n");
        return;
    }
    const u32 configuration = kHandheldGpu460Memory1331;
    if (R_FAILED(apmSetPerformanceConfiguration(mode, configuration)))
    {
        std::fprintf(stderr, "[apm] Configuration 0x%08X is not available on this firmware.\n", configuration);
        return;
    }
    if (hasOriginal)
    {
        g_apmOriginal = original;
        g_apmApplied.store(true, std::memory_order_release);
    }
    u32 gpuAfter = 0, memoryAfter = 0;
    bool memoryStable = true;
    for (int i = 0; i < 10; ++i)
    {
        svcSleepThread(150000000);
        if (!ReadClocks(gpuAfter, memoryAfter))
        {
            memoryStable = false;
            break;
        }
        const u32 delta = memoryAfter > memoryBefore ? memoryAfter - memoryBefore : memoryBefore - memoryAfter;
        if (delta > 20000000)
        {
            memoryStable = false;
            break;
        }
    }
    if (!memoryStable)
    {
        if (hasOriginal)
            apmSetPerformanceConfiguration(mode, original);
        g_apmApplied = false;
        std::fprintf(stderr, "[apm] 0x%08X changed the memory clock (%.1f -> %.1f MHz); reverted to 0x%08X.\n",
            configuration, memoryBefore / 1e6, memoryAfter / 1e6, original);
        return;
    }
    std::fprintf(stderr, "[apm] GPU %.1f -> %.1f MHz, memory %.1f -> %.1f MHz (configuration 0x%08X, was 0x%08X).\n",
        gpuBefore / 1e6, gpuAfter / 1e6, memoryBefore / 1e6, memoryAfter / 1e6, configuration, original);
}

// Once per process in handheld mode; a start in docked mode tries again at the next undock.
void StartBoostThread()
{
    if (!g_apmWanted.load(std::memory_order_acquire) || g_apmDone.load(std::memory_order_acquire) ||
        g_apmRunning.exchange(true, std::memory_order_acq_rel))
        return;
    if (g_apmThreadCreated) // ended: g_apmRunning was false
    {
        threadWaitForExit(&g_apmThread);
        threadClose(&g_apmThread);
        g_apmThreadCreated = false;
    }
    // A libnx thread: it ends on its own; the next start or nothing closes it.
    if (R_SUCCEEDED(threadCreate(&g_apmThread, ApmThreadMain, nullptr, nullptr, 0x10000, 0x3B, -2)))
    {
        g_apmThreadCreated = true;
        if (R_SUCCEEDED(threadStart(&g_apmThread)))
            return;
    }
    g_apmRunning = false;
}

// ExitNow: apm's handheld configuration as the game found it. apm also resets it
// when the process ends (every start reads GPU 307.2 MHz before the boost), so
// this is for the HOME menu between the quit and the end of the process.
void RestoreApmForExit(void (*mark)(const char*))
{
    if (!g_apmApplied.exchange(false, std::memory_order_acq_rel))
        return;
    const Result rc = apmSetPerformanceConfiguration(ApmPerformanceMode_Normal, g_apmOriginal);
    char line[160];
    u32 gpuHz = 0, memoryHz = 0;
    if (R_SUCCEEDED(rc) && appletGetOperationMode() == AppletOperationMode_Handheld)
        for (int i = 0; i < 10 && ReadClocks(gpuHz, memoryHz) && gpuHz > 461000000; ++i)
            svcSleepThread(50'000'000);
    std::snprintf(line, sizeof(line), "exit: apm handheld configuration 0x%08X restored (result 0x%x), GPU %.1f MHz, memory %.1f MHz\n",
        g_apmOriginal, rc, gpuHz / 1e6, memoryHz / 1e6);
    mark(line);
}

// Idle dimming and auto-sleep (issue #7). The system dims the screen and then
// sleeps after the player's own settings without input. The game keeps the
// screen on only while nobody needs to touch the controls; SetScreenAwake
// passes the reasons (start-up, a load, the preparation screen, a movie, an
// in-engine cutscene, a debug-server client). While any holds, auto-sleep is
// off and AwakeThreadMain reports the player active every 15 s; a movie or a
// cutscene also sets the media playback state. When one ends, the idle time
// starts again. With LO_SWITCH_AUTO_SLEEP=0 auto-sleep stays off for the whole
// session and nothing else is done.
bool g_autoSleep = true;
Thread g_awakeThread;
bool g_idleTest = false;      // LO_SWITCH_IDLE_SECONDS: debug-server clients do not count
Mutex g_awakeMutex;
uint32_t g_awakeReasons = 0;  // under g_awakeMutex
bool g_mediaPlayback = false; // what the applet has now
bool g_sleepDisabled = false;

const char* AwakeReasonName(uint32_t bit)
{
    using namespace os::switch_platform;
    switch (bit)
    {
    case AwakeStartup: return "start-up";
    case AwakeLoading: return "load";
    case AwakePreparing: return "preparation";
    case AwakeMovie: return "movie";
    case AwakeCutscene: return "cutscene";
    case AwakeDebugClient: return "debug client";
    case AwakeTestWatching: return "test (watching)";
    case AwakeTestBusy: return "test (busy)";
    default: return "?";
    }
}

// idle:sys ReportUserIsActive, as an input does: the screen lights up again and the idle time restarts.
Result ReportUserActive()
{
    Result rc = idlesysInitialize();
    if (R_SUCCEEDED(rc))
    {
        rc = idlesysReportUserIsActive();
        idlesysExit();
    }
    return rc;
}

// While a reason holds, the player counts as active every 15 s. Neither the media playback state nor auto-sleep
// off stops the system dimming the screen (measured on a console set to sleep after 10 min and to sleep while
// playing media: dimmed 31 and 35 s after an idle restart with either one held), and media playback does not stop
// auto-sleep there either.
void AwakeThreadMain(void*)
{
    for (unsigned seconds = 0;; ++seconds)
    {
        svcSleepThread(1'000'000'000);
        mutexLock(&g_awakeMutex);
        const bool held = g_awakeReasons != 0;
        mutexUnlock(&g_awakeMutex);
        if (held && seconds % 15 == 0)
            ReportUserActive();
    }
}

void ApplyAwakeLocked(uint32_t previous, uint32_t reasons)
{
    using namespace os::switch_platform;
    if (!g_autoSleep)
        return;
    // A reason that ends starts the idle time again (idle:sys ReportUserIsActive): the system counts it from the
    // last input, so after a cutscene, movie or load longer than the player's sleep setting the console would dim
    // and sleep at once. Measured with a 20 s dim / 60 s sleep test override, 10 minutes after the last input:
    // dimmed 21 s and asleep about 90 s after a hold ended.
    const bool restart = (previous & ~reasons) != 0;
    const Result idleResult = restart ? ReportUserActive() : 0;
    // Every reason turns auto-sleep off: on a console set to sleep while playing media, the media playback state
    // alone let it sleep during a movie.
    constexpr uint32_t watching = AwakeMovie | AwakeCutscene | AwakeTestWatching;
    const bool media = (reasons & watching) != 0;
    const bool disabled = reasons != 0;
    Result mediaResult = 0, sleepResult = 0;
    if (media != g_mediaPlayback)
    {
        mediaResult = appletSetMediaPlaybackState(media);
        g_mediaPlayback = media;
    }
    if (disabled != g_sleepDisabled)
    {
        sleepResult = appletSetAutoSleepDisabled(disabled);
        g_sleepDisabled = disabled;
    }
    char names[128] = "none";
    size_t length = 0;
    for (uint32_t bit = 1; bit <= AwakeTestBusy; bit <<= 1)
        if (reasons & bit)
            length += std::snprintf(names + length, sizeof(names) - length, "%s%s", length ? ", " : "", AwakeReasonName(bit));
    char idle[48] = "";
    if (restart)
        std::snprintf(idle, sizeof(idle), ", idle time restarted (0x%x)", idleResult);
    std::fprintf(stderr, "[applet] screen kept on: %s (media playback %d, auto-sleep off %d%s%s)\n", names, int(media), int(disabled),
                 idle, R_FAILED(mediaResult) || R_FAILED(sleepResult) ? ", a request failed" : "");
}
}

namespace os::switch_platform
{
bool RawAppend(const char* fileName, const char* data, size_t size)
{
    if (!g_rawReady || size == 0)
        return false;
    // The watchdog and the crash handler may both write. Never wait long: a stuck writer must
    // not stop the other one.
    bool locked = false;
    for (int attempt = 0; attempt < 100 && !(locked = mutexTryLock(&g_rawMutex)); ++attempt)
        svcSleepThread(1'000'000);
    char path[0x301]{};
    std::snprintf(path, sizeof(path), "%s/%s", g_rawRoot, fileName);
    const struct { u32 option; u32 pad; s64 size; } create = { 0, 0, 0 };
    serviceDispatchIn(&g_rawSd, 0, create, // CreateFile; fails harmlessly if it exists
        .buffer_attrs = { SfBufferAttr_HipcPointer | SfBufferAttr_In }, .buffers = { { path, sizeof(path) } });
    Service file{};
    const u32 mode = FsOpenMode_Write | FsOpenMode_Append;
    Result rc = serviceDispatchIn(&g_rawSd, 8, mode, // OpenFile
        .buffer_attrs = { SfBufferAttr_HipcPointer | SfBufferAttr_In }, .buffers = { { path, sizeof(path) } },
        .out_num_objects = 1, .out_objects = &file);
    if (R_SUCCEEDED(rc))
    {
        s64 offset = 0;
        rc = serviceDispatchOut(&file, 4, offset); // GetSize
        if (R_SUCCEEDED(rc))
        {
            const struct { u32 option; u32 pad; s64 offset; s64 size; } in = { FsWriteOption_Flush, 0, offset, s64(size) };
            rc = serviceDispatchIn(&file, 1, in, // Write
                .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In | SfBufferAttr_HipcMapTransferAllowsNonSecure },
                .buffers = { { data, size } });
        }
        serviceClose(&file);
    }
    if (locked)
        mutexUnlock(&g_rawMutex);
    return R_SUCCEEDED(rc);
}

void StartHandheldGpuBoost()
{
    g_apmWanted.store(true, std::memory_order_release);
    StartBoostThread();
}

std::string ScreenSummary()
{
    mutexLock(&g_awakeMutex);
    const uint32_t reasons = g_awakeReasons;
    const bool media = g_mediaPlayback, disabled = g_sleepDisabled;
    mutexUnlock(&g_awakeMutex);
    char names[128] = "none";
    size_t length = 0;
    for (uint32_t bit = 1; bit <= AwakeTestBusy; bit <<= 1)
        if (reasons & bit)
            length += std::snprintf(names + length, sizeof(names) - length, "%s%s", length ? ", " : "", AwakeReasonName(bit));
    // lbl's dimming flag is set while the system has the screen dimmed.
    bool dimmed = false;
    const bool lbl = R_SUCCEEDED(lblInitialize());
    if (lbl)
    {
        if (R_FAILED(lblIsDimmingEnabled(&dimmed)))
            dimmed = false;
        lblExit();
    }
    char text[256];
    if (!g_autoSleep)
        std::snprintf(text, sizeof(text), "auto-sleep off for the session (LO_SWITCH_AUTO_SLEEP=0)");
    else
        std::snprintf(text, sizeof(text), "kept on: %s (media playback %d, auto-sleep off %d)", names, int(media), int(disabled));
    std::string out = text;
    if (lbl)
        out += dimmed ? "; dimmed now" : "; not dimmed";
    return out;
}

void SetScreenAwake(uint32_t mask, uint32_t reasons)
{
    if (g_idleTest)
        reasons &= ~uint32_t(AwakeDebugClient);
    mutexLock(&g_awakeMutex);
    const uint32_t next = (g_awakeReasons & ~mask) | (reasons & mask);
    if (next != g_awakeReasons)
    {
        const uint32_t previous = g_awakeReasons;
        g_awakeReasons = next;
        ApplyAwakeLocked(previous, next);
    }
    mutexUnlock(&g_awakeMutex);
}

void Initialize(const std::filesystem::path& dataRoot)
{
    std::error_code ec;
    std::filesystem::create_directories(dataRoot, ec);
    std::snprintf(g_crashLogPath, sizeof(g_crashLogPath), "%s", (dataRoot / "crash.log").c_str());
    {
        const std::string root = dataRoot.string();
        std::snprintf(g_rawRoot, sizeof(g_rawRoot), "%s", root.compare(0, 5, "sdmc:") == 0 ? root.c_str() + 5 : root.c_str());
        mutexInit(&g_rawMutex);
        if (R_FAILED(RawInitialize()))
            g_rawReady = false;
    }
    CaptureModuleRange();
    std::set_terminate(OnTerminate);

    // Horizon has no console behind stderr: keep the renderer's and the NVK
    // driver's own diagnostics (UnleashedRecomp-NX does the same), and copy
    // them to the network log.
    std::filesystem::rename(dataRoot / "stderr.log", dataRoot / "stderr.previous.log", ec);
    g_stderrFile = std::fopen((dataRoot / "stderr.log").c_str(), "w");
    if (g_stderrFile)
        std::setvbuf(g_stderrFile, nullptr, _IONBF, 0);
    g_stderrDevice.name = "stderr";
    g_stderrDevice.write_r = StderrWrite;
    devoptab_list[STD_ERR] = &g_stderrDevice;
    std::setvbuf(stderr, nullptr, _IOLBF, 1024);

    // The console has no environment: <dataRoot>/env.txt supplies the LO_*
    // switches the PC build reads from it (one KEY=VALUE per line, # comments).
    if (FILE* env = std::fopen((dataRoot / "env.txt").c_str(), "r"))
    {
        char line[512];
        while (std::fgets(line, sizeof(line), env))
        {
            char* text = line;
            while (*text == ' ' || *text == '\t') ++text;
            size_t length = std::strlen(text);
            while (length && (text[length - 1] == '\n' || text[length - 1] == '\r' || text[length - 1] == ' '))
                text[--length] = 0;
            char* equals = std::strchr(text, '=');
            if (!*text || *text == '#' || !equals || equals == text)
                continue;
            *equals = 0;
            setenv(text, equals + 1, 1);
            std::fprintf(stderr, "env.txt: %s=%s\n", text, equals + 1);
        }
        std::fclose(env);
    }
    // The network log (LO_NETLOG) and the debug server (LO_DEBUG_SERVER, settings.ini debug_server) share one
    // socket service setup; both stay off, and cost nothing, unless turned on.
    os::debug_server::StartNetwork(dataRoot.string().c_str());
#ifdef LO_SWITCH_PGO_GEN
    // Training build: the profile is saved to the SD card on its own (os/switch/pgo_switch.cpp).
    os::pgo::StartAutoSave();
#endif

    // The HOME menu and sleep suspend the game, as the system does for every game
    // (libnx's SuspendHomeSleep). LO_SWITCH_HOME_SUSPEND=0 keeps it running and
    // rendering behind the HOME menu instead (benchmarks, debug server).
    const char* homeSuspend = getenv("LO_SWITCH_HOME_SUSPEND");
    const bool suspend = !(homeSuspend && std::strcmp(homeSuspend, "0") == 0);
    appletSetFocusHandlingMode(suspend ? AppletFocusHandlingMode_SuspendHomeSleep : AppletFocusHandlingMode_NoSuspend);
    // Idle dimming and auto-sleep follow the system's settings; SetScreenAwake keeps the screen on while nobody
    // needs the controls. LO_SWITCH_AUTO_SLEEP=0 turns auto-sleep off for the session (long unattended runs).
    const char* autoSleep = getenv("LO_SWITCH_AUTO_SLEEP");
    g_autoSleep = !(autoSleep && std::strcmp(autoSleep, "0") == 0);
    mutexInit(&g_awakeMutex);
    if (!g_autoSleep)
        appletSetAutoSleepDisabled(true);
    else if (R_SUCCEEDED(threadCreate(&g_awakeThread, AwakeThreadMain, nullptr, nullptr, 0x4000, 0x3B, -2)))
        threadStart(&g_awakeThread);
    std::fprintf(stderr, "[applet] HOME menu: %s; auto-sleep: %s\n", suspend ? "suspends the game" : "game keeps running (LO_SWITCH_HOME_SUSPEND=0)",
                 g_autoSleep ? "system settings, off during start-up, loads, preparation, movies and cutscenes" : "off (LO_SWITCH_AUTO_SLEEP=0)");
    if (R_SUCCEEDED(setsysInitialize()))
    {
        // The player's System Settings → Sleep Mode, for reports (flags bit 0: sleeps while playing media).
        static const char* const handheld[] = { "1 min", "3 min", "5 min", "10 min", "30 min", "never" };
        SetSysSleepSettings sleep{};
        if (R_SUCCEEDED(setsysGetSleepSettings(&sleep)))
            std::fprintf(stderr, "[applet] sleep settings: handheld plan %d (%s), docked plan %d, %s while playing media (flags 0x%x)\n",
                         sleep.handheld_sleep_plan,
                         sleep.handheld_sleep_plan >= 0 && sleep.handheld_sleep_plan <= 5 ? handheld[sleep.handheld_sleep_plan] : "?",
                         sleep.console_sleep_plan, (sleep.flags & 1) ? "sleeps" : "does not sleep", sleep.flags);
        setsysExit();
    }
    SetScreenAwake(AwakeStartup, AwakeStartup);
    // Test only: LO_SWITCH_IDLE_SECONDS=<dim>,<sleep> overrides the system's idle times for this session
    // (ISelfController OverrideAutoSleepTimeAndDimmingTime: sleep handheld, sleep docked, dim handheld, dim docked);
    // 0,0 keeps the system's. Either way debug-server clients then do not keep the screen on.
    if (const char* idle = getenv("LO_SWITCH_IDLE_SECONDS"))
    {
        int dim = 0, asleep = 0;
        if (std::sscanf(idle, "%d,%d", &dim, &asleep) == 2 && dim >= 0 && asleep >= 0)
        {
            const Result rc = dim > 0 && asleep > 0 ? appletOverrideAutoSleepTimeAndDimmingTime(asleep, asleep, dim, dim) : 0;
            std::fprintf(stderr, "[applet] idle test: dim after %d s, sleep after %d s (0: the system's; result 0x%x); "
                         "debug-server clients do not keep the screen on\n", dim, asleep, rc);
            g_idleTest = true;
        }
    }
    appletLockExit();
    appletHook(&g_operationModeHook, OnAppletEvent, nullptr);
}

void AllowExit()
{
    appletUnlockExit();
}

extern "C" void __nx_win_exit(void) __attribute__((weak));


void ExitNow()
{
    // An application must tell the applet manager it is leaving
    // (ISelfController::Exit, as libnx's own application exit does) and then
    // wait to be terminated. Ending the process without it left the screen
    // frozen on the last frame (title Quit and Quit to Desktop, 2026-10-22).
    // The libnx exit teardown is still skipped: it unmounted the SD card under
    // game threads that were reading it.
    // Each step goes to exit.log through the raw file session, so a quit that
    // freezes shows how far it got (the runtime log stops before this).
    auto mark = [](const char* text) { RawAppend("exit.log", text, std::strlen(text)); };
    char line[96];
    std::snprintf(line, sizeof(line), "exit: started (system tick %llu)\n", static_cast<unsigned long long>(armGetSystemTick()));
    mark(line);
#ifdef LO_SWITCH_PGO_GEN
    os::pgo::SaveOnExit(); // training build: the counters since the last save
    mark("exit: profile saved\n");
#endif
    RestoreApmForExit(mark);
    os::logger::DrainForExit();
    // Not fflush(nullptr): it locks every open file, game files included.
    std::fflush(stdout);
    std::fflush(stderr);
    mark("exit: logs flushed\n");
    // libnx closes the default window (vi layer and display) before it sends Exit. Without it the Exit request
    // never returned (exit.log of 2026-10-27 ends at "logs flushed"): close it as libnx does. The renderer has
    // already destroyed its swap chain (video: shutdown stage=window-stop).
    if (&__nx_win_exit)
    {
        __nx_win_exit();
        mark("exit: window closed\n");
    }
    appletUnlockExit();
    // The request on its own thread, so a request that never returns cannot keep the process alive.
    static std::atomic<int> requestState{0}; // 0 sent, 1 returned
    static Result requestResult = 0;
    static Thread requestThread;
    const bool threaded = R_SUCCEEDED(threadCreate(&requestThread, [](void*) {
        requestResult = serviceDispatch(appletGetServiceSession_SelfController(), 0); // Exit
        requestState.store(1, std::memory_order_release);
        for (;;)
            svcSleepThread(UINT64_MAX);
    }, nullptr, nullptr, 0x4000, 0x2C, -2)) && R_SUCCEEDED(threadStart(&requestThread));
    if (!threaded)
    {
        requestResult = serviceDispatch(appletGetServiceSession_SelfController(), 0);
        requestState.store(1, std::memory_order_release);
    }
    bool reported = false;
    for (unsigned tenths = 1; tenths <= 100; ++tenths)
    {
        svcSleepThread(100'000'000ull);
        if (!reported && requestState.load(std::memory_order_acquire) == 1)
        {
            std::snprintf(line, sizeof(line), "exit: applet Exit result 0x%x\n", requestResult);
            mark(line);
            reported = true;
            if (R_FAILED(requestResult))
                break;
        }
        if (tenths == 10 || tenths == 30 || tenths == 100)
        {
            std::snprintf(line, sizeof(line), "exit: still running %u s after the Exit request (%s)\n", tenths / 10,
                          reported ? "answered" : "not answered");
            mark(line);
        }
    }
    mark("exit: ending the process directly (svcExitProcess)\n");
    svcExitProcess();
    __builtin_unreachable();
}

void ShowError(const std::string& summary, const std::string& details)
{
    std::fprintf(stderr, "fatal: %s\n%s\n", summary.c_str(), details.c_str());
    ErrorApplicationConfig config{};
    if (R_SUCCEEDED(errorApplicationCreate(&config, summary.c_str(), details.c_str())))
        errorApplicationShow(&config);
}

bool IsDocked()
{
    return appletGetOperationMode() == AppletOperationMode_Console;
}

bool SystemLanguageIsSpanish()
{
    if (R_FAILED(setInitialize()))
        return false;
    u64 code = 0;
    SetLanguage language = SetLanguage_ENUS;
    const bool spanish = R_SUCCEEDED(setGetSystemLanguage(&code)) && R_SUCCEEDED(setMakeLanguage(code, &language)) &&
                         (language == SetLanguage_ES || language == SetLanguage_ES419);
    setExit();
    return spanish;
}

bool TakeOperationModeChanged()
{
    return g_operationModeChanged.exchange(false, std::memory_order_acq_rel);
}

static std::atomic<std::string (*)()> g_memorySummaryExtra{nullptr};

void SetMemorySummaryExtra(std::string (*extra)())
{
    g_memorySummaryExtra.store(extra, std::memory_order_release);
}

std::string MemorySummary()
{
    u64 used = 0, total = 0, threadsUsed = 0, threadsLimit = 0;
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    Handle limit = INVALID_HANDLE;
    if (R_SUCCEEDED(svcGetInfo(reinterpret_cast<u64*>(&limit), InfoType_ResourceLimit, INVALID_HANDLE, 0)) && limit != INVALID_HANDLE)
    {
        s64 current = 0, maximum = 0;
        if (R_SUCCEEDED(svcGetResourceLimitCurrentValue(&current, limit, LimitableResource_Threads)))
            threadsUsed = u64(current);
        if (R_SUCCEEDED(svcGetResourceLimitLimitValue(&maximum, limit, LimitableResource_Threads)))
            threadsLimit = u64(maximum);
        svcCloseHandle(limit);
    }
    // The heap is where nearly everything lives (guest memory chunks, NVK
    // buffers, thread stacks); UsedMemorySize does not count it for us.
    const struct mallinfo heap = mallinfo();
    // libnx gives newlib's heap [fake_heap_start, fake_heap_end); the arena can grow up to that.
    const size_t heapLimit = size_t(fake_heap_end - fake_heap_start);
    char text[224];
    std::snprintf(text, sizeof(text), "heap in use %llu MiB (arena %llu of %llu MiB), process %llu/%llu MiB, threads %llu/%llu",
        static_cast<unsigned long long>(size_t(heap.uordblks) >> 20), static_cast<unsigned long long>(size_t(heap.arena) >> 20),
        static_cast<unsigned long long>(heapLimit >> 20),
        static_cast<unsigned long long>(used >> 20), static_cast<unsigned long long>(total >> 20),
        static_cast<unsigned long long>(threadsUsed), static_cast<unsigned long long>(threadsLimit));
    if (const auto extra = g_memorySummaryExtra.load(std::memory_order_acquire))
        return text + extra();
    return text;
}

void HeapUsage(uint64_t& used, uint64_t& limit)
{
    const struct mallinfo heap = mallinfo();
    used = uint64_t(heap.uordblks);
    limit = uint64_t(fake_heap_end - fake_heap_start);
}

bool ReadClockRates(uint32_t& cpuHz, uint32_t& gpuHz, uint32_t& memoryHz)
{
    cpuHz = gpuHz = memoryHz = 0;
    if (R_FAILED(clkrstInitialize()))
        return false;
    bool ok = true;
    const auto read = [&ok](PcvModuleId module, uint32_t& hz) {
        ClkrstSession session{};
        u32 rate = 0;
        if (R_SUCCEEDED(clkrstOpenSession(&session, module, 3)))
        {
            ok &= R_SUCCEEDED(clkrstGetClockRate(&session, &rate));
            clkrstCloseSession(&session);
        }
        else
            ok = false;
        hz = rate;
    };
    read(PcvModuleId_CpuBus, cpuHz);
    read(PcvModuleId_GPU, gpuHz);
    read(PcvModuleId_EMC, memoryHz);
    clkrstExit();
    return ok;
}

uint32_t CurrentThreadHandle()
{
    return threadGetCurHandle();
}

static std::atomic<uint32_t> g_pinnedThread{0}; // the game's own affinity calls leave this one alone

uint32_t SetThreadIdealCore(uint32_t handle, int core)
{
    if ((handle ? handle : threadGetCurHandle()) == g_pinnedThread.load(std::memory_order_relaxed))
        return 0;
    return svcSetThreadCoreMask(handle ? Handle(handle) : CUR_THREAD_HANDLE, core, 0x7);
}

void SleepNanoseconds(int64_t nanoseconds)
{
    svcSleepThread(nanoseconds);
}

void PinCurrentThread(int core)
{
    if (R_SUCCEEDED(svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, 1u << core)))
        g_pinnedThread.store(threadGetCurHandle(), std::memory_order_relaxed);
}

void SetCurrentThreadPriority(int priority)
{
    SetThreadPriority(0, priority);
}

int CurrentThreadPriority()
{
    s32 priority = -1;
    return R_SUCCEEDED(svcGetThreadPriority(&priority, CUR_THREAD_HANDLE)) ? priority : -1;
}

uint32_t SetThreadPriority(uint32_t handle, int priority)
{
    return svcSetThreadPriority(handle ? Handle(handle) : CUR_THREAD_HANDLE, u32(priority));
}

void SetLoadingBoost(bool enabled)
{
    appletSetCpuBoostMode(enabled ? ApmCpuBoostMode_FastLoad : ApmCpuBoostMode_Normal);
}
}
#endif
