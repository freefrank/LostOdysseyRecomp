#include <stdafx.h>
#include <os/logger.h>
#include <cpu/ppc_context.h>
#include <kernel/memory.h>

// Debug facility: LO_WATCH_PHYS=<physical address> logs every host access to
// the guest dwords around that address (via a PAGE_GUARD page plus single
// stepping) together with the host symbol and a guest back trace. Used to find
// who overwrites GPU command memory.

#ifdef _WIN32
#include <dbghelp.h>

static uint8_t* s_watchPage = nullptr;
static uint8_t* s_watchLo = nullptr;
static uint8_t* s_watchHi = nullptr;
static thread_local bool t_rearm = false;
static thread_local uint64_t t_pendingFault = 0;
static thread_local bool t_pendingWrite = false;
namespace gpu { extern std::atomic<uint32_t> g_swapCount; }
static std::atomic<int> s_hits = 0;

static void ArmWatchPage()
{
    DWORD old = 0;
    VirtualProtect(s_watchPage, 0x1000, PAGE_READWRITE | PAGE_GUARD, &old);
}

static void LogHit(EXCEPTION_POINTERS* info, uint64_t fault, bool write)
{
    HANDLE process = GetCurrentProcess();
    char symbolBuffer[sizeof(SYMBOL_INFO) + 512]{};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 511;
    DWORD64 displacement = 0;
    const char* name = "?";
    if (SymFromAddr(process, (DWORD64)info->ExceptionRecord->ExceptionAddress, &displacement, symbol))
        name = symbol->Name;

    std::string bt;
    if (auto* ppc = GetPPCContext())
    {
        bt += fmt::format(" lr={:#x} r1={:#x} bt:", uint32_t(ppc->lr), ppc->r1.u32);
        uint32_t frame = ppc->r1.u32;
        for (int i = 0; i < 12 && frame > 0x1000 && frame < 0x7C000000; i++)
        {
            uint32_t caller = *reinterpret_cast<be<uint32_t>*>(g_memory.Translate(frame));
            if (caller <= frame || caller >= 0x7C000000)
                break;
            uint32_t ret = *reinterpret_cast<be<uint32_t>*>(g_memory.Translate(caller - 8));
            bt += fmt::format(" {:#x}", ret);
            frame = caller;
        }
    }

    fprintf(stderr, "[watch] %s guest 0x%08llX from %s+0x%llx tid=%lu%s\n",
        write ? "WRITE" : "read", (unsigned long long)(fault - (uint64_t)g_memory.base),
        name, (unsigned long long)displacement, GetCurrentThreadId(), bt.c_str());
    fflush(stderr);
}

static LONG WINAPI WatchHandler(EXCEPTION_POINTERS* info)
{
    auto* rec = info->ExceptionRecord;
    if (rec->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION)
    {
        uint64_t fault = rec->ExceptionInformation[1];
        if (fault < (uint64_t)s_watchPage || fault >= (uint64_t)s_watchPage + 0x1000)
            return EXCEPTION_CONTINUE_SEARCH;
        t_pendingFault = 0;
        if (fault >= (uint64_t)s_watchLo && fault < (uint64_t)s_watchHi && s_hits < 600)
        {
            s_hits++;
            t_pendingWrite = rec->ExceptionInformation[0] == 1;
            if (t_pendingWrite)
                t_pendingFault = fault;
            LogHit(info, fault, t_pendingWrite);
        }
        // Guard is now cleared; single-step so the access completes, then re-arm.
        t_rearm = true;
        info->ContextRecord->EFlags |= 0x100;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (rec->ExceptionCode == STATUS_SINGLE_STEP && t_rearm)
    {
        t_rearm = false;
        if (t_pendingFault)
        {
            uint32_t v = *reinterpret_cast<be<uint32_t>*>(t_pendingFault & ~uint64_t(3));
            fprintf(stderr, "[watch]    -> [0x%08llX] = 0x%08X\n", (unsigned long long)((t_pendingFault & ~uint64_t(3)) - (uint64_t)g_memory.base), v);
            t_pendingFault = 0;
        }
        ArmWatchPage();
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void InstallPhysicalWatchpoint()
{
    const char* env = getenv("LO_WATCH_PHYS");
    if (!env)
        return;
    uint32_t phys = strtoul(env, nullptr, 16) & 0x1FFFFFFF;
    uint32_t len = 0x20;
    if (const char* l = getenv("LO_WATCH_LEN"))
        len = strtoul(l, nullptr, 16);

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);

    s_watchLo = static_cast<uint8_t*>(g_memory.Translate(0xA0000000u + phys));
    s_watchHi = s_watchLo + len;
    s_watchPage = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(s_watchLo) & ~uintptr_t(0xFFF));
    AddVectoredExceptionHandler(1, WatchHandler);
    ArmWatchPage();
    LOG_WARNING("watching physical {:#x}..{:#x}", phys, phys + len);
}
#else
void InstallPhysicalWatchpoint() {}
#endif
