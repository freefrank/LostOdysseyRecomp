// Nintendo Switch process setup, crash log and system dialogs.
// Parts follow UnleashedRecomp-NX (os/switch/runtime_switch.cpp, crash_switch.cpp).
#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include <os/switch_platform.h>
#include <switch.h>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cxxabi.h>
#include <exception>
#include <typeinfo>
#include <cstdlib>
#include <malloc.h>
#include <system_error>

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

namespace
{
char g_crashLogPath[512] = "sdmc:/switch/LostOdysseyRecomp/crash.log";
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
    // Runs on __nx_exception_stack. Best effort: stdio may be unusable if the
    // fault happened inside it.
    FILE* file = std::fopen(g_crashLogPath, "a");
    if (!file)
        ExitWithLog();
    const std::time_t now = std::time(nullptr);
    std::fprintf(file, "\n=== Lost Odyssey Recomp crash, %s", std::ctime(&now));
    std::fprintf(file, "error_desc %#x  esr %#x  far %016" PRIx64 "  pstate %#x\n",
        ctx->error_desc, ctx->esr, ctx->far.x, ctx->pstate);
    std::fprintf(file, "module %016" PRIxPTR "-%016" PRIxPTR "\n", g_moduleBase, g_moduleEnd);
    WriteAddress(file, "pc", ctx->pc.x);
    WriteAddress(file, "lr", ctx->lr.x);
    WriteAddress(file, "sp", ctx->sp.x);
    WriteAddress(file, "fp", ctx->fp.x);
    for (int i = 0; i < 29; ++i)
    {
        char name[8];
        std::snprintf(name, sizeof(name), "x%d", i);
        WriteAddress(file, name, ctx->cpu_gprs[i].x);
    }
    // Frame-pointer chain of the host code (the recompiled game code omits
    // frame pointers, so the chain stops at the first guest function).
    std::fprintf(file, "backtrace:\n");
    uintptr_t frame = ctx->fp.x;
    for (int depth = 0; depth < 32 && frame && (frame & 7) == 0; ++depth)
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
    std::fclose(file);
    ExitWithLog();
}

namespace
{
constexpr u32 kHandheldGpu460Memory1331 = 0x92220008;
Thread g_apmThread;

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
void ApmThreadMain(void*)
{
    if (R_FAILED(apmInitialize()))
    {
        std::fprintf(stderr, "[apm] apm is not available; clocks unchanged.\n");
        return;
    }
    ApmPerformanceMode mode = ApmPerformanceMode_Invalid;
    if (R_FAILED(apmGetPerformanceMode(&mode)) || mode != ApmPerformanceMode_Normal)
    {
        std::fprintf(stderr, "[apm] Console is not in handheld mode; clocks unchanged.\n");
        return;
    }
    u32 original = 0;
    const bool hasOriginal = R_SUCCEEDED(apmGetPerformanceConfiguration(mode, &original));
    u32 gpuBefore = 0, memoryBefore = 0;
    if (!ReadClocks(gpuBefore, memoryBefore))
    {
        std::fprintf(stderr, "[apm] Unable to read clocks through clkrst; clocks unchanged.\n");
        return;
    }
    if (R_FAILED(apmSetPerformanceConfiguration(mode, kHandheldGpu460Memory1331)))
    {
        std::fprintf(stderr, "[apm] Configuration 0x%08X is not available on this firmware.\n", kHandheldGpu460Memory1331);
        return;
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
        std::fprintf(stderr, "[apm] 0x%08X changed the memory clock (%.1f -> %.1f MHz); reverted to 0x%08X.\n",
            kHandheldGpu460Memory1331, memoryBefore / 1e6, memoryAfter / 1e6, original);
        return;
    }
    std::fprintf(stderr, "[apm] GPU %.1f -> %.1f MHz, memory stays at %.1f MHz (configuration 0x%08X).\n",
        gpuBefore / 1e6, gpuAfter / 1e6, memoryAfter / 1e6, kHandheldGpu460Memory1331);
}
}

namespace os::switch_platform
{
void StartHandheldGpuBoost()
{
    // A libnx thread: it ends on its own and is never joined.
    if (R_SUCCEEDED(threadCreate(&g_apmThread, ApmThreadMain, nullptr, nullptr, 0x10000, 0x3B, -2)))
        threadStart(&g_apmThread);
}

void Initialize(const std::filesystem::path& dataRoot)
{
    std::error_code ec;
    std::filesystem::create_directories(dataRoot, ec);
    std::snprintf(g_crashLogPath, sizeof(g_crashLogPath), "%s", (dataRoot / "crash.log").c_str());
    CaptureModuleRange();
    std::set_terminate(OnTerminate);

    // Horizon has no console behind stderr: keep the renderer's and the NVK
    // driver's own diagnostics (UnleashedRecomp-NX does the same).
    std::filesystem::rename(dataRoot / "stderr.log", dataRoot / "stderr.previous.log", ec);
    if (std::freopen((dataRoot / "stderr.log").c_str(), "w", stderr))
        std::setvbuf(stderr, nullptr, _IOLBF, 1024);

    // Keep running (and rendering) while the HOME menu overlay is open, and
    // never let the console sleep during long loads or cutscenes.
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

    appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
    appletSetAutoSleepDisabled(true);
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
    char text[192];
    std::snprintf(text, sizeof(text), "heap in use %llu MiB (arena %llu MiB), process %llu/%llu MiB, threads %llu/%llu",
        static_cast<unsigned long long>(size_t(heap.uordblks) >> 20), static_cast<unsigned long long>(size_t(heap.arena) >> 20),
        static_cast<unsigned long long>(used >> 20), static_cast<unsigned long long>(total >> 20),
        static_cast<unsigned long long>(threadsUsed), static_cast<unsigned long long>(threadsLimit));
    return text;
}

void SetCurrentThreadPriority(int priority)
{
    svcSetThreadPriority(CUR_THREAD_HANDLE, u32(priority));
}

void SetLoadingBoost(bool enabled)
{
    appletSetCpuBoostMode(enabled ? ApmCpuBoostMode_FastLoad : ApmCpuBoostMode_Normal);
}
}
#endif
