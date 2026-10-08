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
#include <exception>
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

void OnTerminate()
{
    if (FILE* file = std::fopen(g_crashLogPath, "a"))
    {
        std::fprintf(file, "std::terminate called");
        if (auto exception = std::current_exception())
        {
            try { std::rethrow_exception(exception); }
            catch (const std::exception& error) { std::fprintf(file, ": %s", error.what()); }
            catch (...) { std::fprintf(file, ": unknown exception"); }
        }
        std::fprintf(file, "\n");
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

namespace os::switch_platform
{
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

void SetLoadingBoost(bool enabled)
{
    appletSetCpuBoostMode(enabled ? ApmCpuBoostMode_FastLoad : ApmCpuBoostMode_Normal);
}
}
#endif
