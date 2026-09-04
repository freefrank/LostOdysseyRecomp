#include <stdafx.h>
#include <os/logger.h>
#include <cpu/ppc_context.h>
#include <kernel/memory.h>

#ifdef _WIN32
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

// Unhandled exception filter: prints the faulting address, the guest register
// file and a symbolised host stack so crashes in recompiled code are readable
// without a debugger attached.

static const char* ExceptionName(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
    case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
    case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
    default: return "EXCEPTION";
    }
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* info)
{
    auto* rec = info->ExceptionRecord;
    auto* ctx = info->ContextRecord;

    fprintf(stderr, "\n[crash] %s (0x%08lX) at host %p\n", ExceptionName(rec->ExceptionCode), rec->ExceptionCode, rec->ExceptionAddress);
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
    {
        uint64_t addr = rec->ExceptionInformation[1];
        const char* kind = rec->ExceptionInformation[0] == 0 ? "read" : rec->ExceptionInformation[0] == 1 ? "write" : "execute";
        if (g_memory.base && addr >= (uint64_t)g_memory.base && addr < (uint64_t)g_memory.base + PPC_MEMORY_SIZE)
            fprintf(stderr, "[crash] %s of guest address 0x%08llX\n", kind, (unsigned long long)(addr - (uint64_t)g_memory.base));
        else
            fprintf(stderr, "[crash] %s of host address 0x%016llX\n", kind, (unsigned long long)addr);
    }

    if (auto* ppc = GetPPCContext())
    {
        fprintf(stderr, "[crash] guest r1=%08X r3=%08X r4=%08X r5=%08X r13=%08X lr=%08llX ctr=%08llX\n",
            ppc->r1.u32, ppc->r3.u32, ppc->r4.u32, ppc->r5.u32, ppc->r13.u32, (unsigned long long)ppc->lr, (unsigned long long)ppc->ctr.u64);
    }

    // LO_CRASH_DUMP=<guest address>: print 256 bytes of guest memory (as
    // ASCII and UTF-16) - handy for reading the game's own fatal-error text.
    if (const char* dumpEnv = getenv("LO_CRASH_DUMP"))
    {
        uint32_t addr = strtoul(dumpEnv, nullptr, 16);
        auto* p = static_cast<const uint8_t*>(g_memory.Translate(addr));
        std::string ascii, wide;
        for (int i = 0; i < 256; i++)
            ascii += (p[i] >= 0x20 && p[i] < 0x7F) ? char(p[i]) : '.';
        for (int i = 0; i < 256; i += 2)
        {
            uint16_t c = (uint16_t(p[i]) << 8) | p[i + 1];
            wide += (c >= 0x20 && c < 0x7F) ? char(c) : '.';
        }
        fprintf(stderr, "[crash] guest %08X ascii: %s\n", addr, ascii.c_str());
        fprintf(stderr, "[crash] guest %08X utf16: %s\n", addr, wide.c_str());
    }

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(process, nullptr, TRUE);

    CONTEXT c = *ctx;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = c.Rip; frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = c.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = c.Rsp; frame.AddrStack.Mode = AddrModeFlat;

    char symbolBuffer[sizeof(SYMBOL_INFO) + 512]{};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 511;

    for (int i = 0; i < 48; i++)
    {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &c, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        if (frame.AddrPC.Offset == 0)
            break;

        DWORD64 displacement = 0;
        const char* name = "?";
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
            name = symbol->Name;

        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &line))
            fprintf(stderr, "[crash]   #%02d %s+0x%llx  (%s:%lu)\n", i, name, (unsigned long long)displacement, line.FileName, line.LineNumber);
        else
            fprintf(stderr, "[crash]   #%02d %s+0x%llx\n", i, name, (unsigned long long)displacement);
    }
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

void InstallCrashHandler()
{
    SetUnhandledExceptionFilter(CrashFilter);
}
#else
void InstallCrashHandler() {}
#endif
