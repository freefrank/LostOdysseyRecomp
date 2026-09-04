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
        const uint32_t regs[32] = {
            ppc->r0.u32, ppc->r1.u32, ppc->r2.u32, ppc->r3.u32, ppc->r4.u32, ppc->r5.u32, ppc->r6.u32, ppc->r7.u32,
            ppc->r8.u32, ppc->r9.u32, ppc->r10.u32, ppc->r11.u32, ppc->r12.u32, ppc->r13.u32, ppc->r14.u32, ppc->r15.u32,
            ppc->r16.u32, ppc->r17.u32, ppc->r18.u32, ppc->r19.u32, ppc->r20.u32, ppc->r21.u32, ppc->r22.u32, ppc->r23.u32,
            ppc->r24.u32, ppc->r25.u32, ppc->r26.u32, ppc->r27.u32, ppc->r28.u32, ppc->r29.u32, ppc->r30.u32, ppc->r31.u32 };
        for (int i = 0; i < 32; i += 8)
            fprintf(stderr, "[crash] r%02d-%02d: %08X %08X %08X %08X %08X %08X %08X %08X%c", i, i + 7,
                regs[i], regs[i + 1], regs[i + 2], regs[i + 3], regs[i + 4], regs[i + 5], regs[i + 6], regs[i + 7], 10);
        fprintf(stderr, "[crash] guest r1=%08X r3=%08X r4=%08X r5=%08X r13=%08X lr=%08llX ctr=%08llX\n",
            ppc->r1.u32, ppc->r3.u32, ppc->r4.u32, ppc->r5.u32, ppc->r13.u32, (unsigned long long)ppc->lr, (unsigned long long)ppc->ctr.u64);
    }

    // LO_CRASH_DUMP=<guest address>: print 256 bytes of guest memory (as
    // ASCII and UTF-16) - handy for reading the game's own fatal-error text.
    // LO_CRASH_DUMP accepts a list: "8336D9B0,r27+0x300,r19+0xd0" (hex address
    // or register-relative), 256 bytes each as hex words + ASCII/UTF-16.
    std::string dumpList = getenv("LO_CRASH_DUMP") ? getenv("LO_CRASH_DUMP") : "";
    size_t dumpPos = 0;
    while (dumpPos < dumpList.size())
    {
        size_t comma = dumpList.find(',', dumpPos);
        std::string item = dumpList.substr(dumpPos, comma == std::string::npos ? std::string::npos : comma - dumpPos);
        dumpPos = comma == std::string::npos ? dumpList.size() : comma + 1;
        uint32_t addr = 0;
        if (item.size() > 1 && item[0] == 'r' && GetPPCContext())
        {
            auto* ppc = GetPPCContext();
            const PPCRegister* regs[32] = { &ppc->r0, &ppc->r1, &ppc->r2, &ppc->r3, &ppc->r4, &ppc->r5, &ppc->r6, &ppc->r7, &ppc->r8, &ppc->r9, &ppc->r10, &ppc->r11,
                &ppc->r12, &ppc->r13, &ppc->r14, &ppc->r15, &ppc->r16, &ppc->r17, &ppc->r18, &ppc->r19, &ppc->r20, &ppc->r21, &ppc->r22, &ppc->r23,
                &ppc->r24, &ppc->r25, &ppc->r26, &ppc->r27, &ppc->r28, &ppc->r29, &ppc->r30, &ppc->r31 };
            char* end = nullptr;
            int reg = int(strtol(item.c_str() + 1, &end, 10)) & 31;
            addr = regs[reg]->u32 + uint32_t(end && *end ? strtol(end, nullptr, 0) : 0);
        }
        else
            addr = strtoul(item.c_str(), nullptr, 16);
        // Trailing '*': follow the big-endian pointer stored at the address.
        if (!item.empty() && item.back() == '*' && addr >= 0x1000 && addr < 0xFFFFF000u)
        {
            auto* pp = static_cast<const uint8_t*>(g_memory.Translate(addr));
            uint32_t target = (uint32_t(pp[0]) << 24) | (uint32_t(pp[1]) << 16) | (uint32_t(pp[2]) << 8) | pp[3];
            fprintf(stderr, "[crash] [%08X] -> %08X%c", addr, target, 10);
            addr = target;
        }
        if (addr < 0x1000 || addr >= 0xFFFFF000u)
            continue;
        auto* p = static_cast<const uint8_t*>(g_memory.Translate(addr));
        for (int row = 0; row < 256; row += 32)
        {
            fprintf(stderr, "[crash] %08X:", addr + row);
            for (int i = 0; i < 32; i += 4)
                fprintf(stderr, " %02X%02X%02X%02X", p[row + i], p[row + i + 1], p[row + i + 2], p[row + i + 3]);
            fprintf(stderr, "%c", 10);
        }
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
