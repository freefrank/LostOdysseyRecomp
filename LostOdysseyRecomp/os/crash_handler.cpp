#include <stdafx.h>
#include <os/crash_handler.h>
#include <os/log_file.h>
#include <cpu/ppc_context.h>
#include <kernel/memory.h>
#include <version.h>

#ifdef _WIN32
#include <csignal>
#include <exception>
#include <dbghelp.h>
#include <psapi.h>
#pragma comment(lib, "dbghelp.lib")

namespace
{
    // Cached before game threads start. No CRT environment access during a crash.
    char g_dumpList[1024]{};
    volatile LONG g_crashActive = 0;

    struct CrashText
    {
        char data[2048];
        size_t size = 0;
        CrashText& Text(const char* text) noexcept
        {
            if (text)
                while (*text && size < sizeof(data)) data[size++] = *text++;
            return *this;
        }
        CrashText& Hex(uint64_t value, unsigned width = 8) noexcept
        {
            const char digits[] = "0123456789ABCDEF";
            for (unsigned i = width; i > 0; --i)
                if (size < sizeof(data)) data[size++] = digits[(value >> ((i - 1) * 4)) & 15];
            return *this;
        }
        CrashText& Decimal(unsigned value) noexcept
        {
            char digits[10];
            unsigned count = 0;
            do { digits[count++] = char('0' + value % 10); value /= 10; } while (value);
            while (count && size < sizeof(data)) data[size++] = digits[--count];
            return *this;
        }
        void Write() noexcept
        {
            os::logger::EmergencyWrite(data, size);
            size = 0;
        }
    };

    const char* ExceptionName(DWORD code) noexcept
    {
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
        case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
        case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
        case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
        case 0xE06D7363u: return "CPP_EXCEPTION";
        default: return "EXCEPTION";
        }
    }

    void EnterCrash() noexcept
    {
        if (InterlockedCompareExchange(&g_crashActive, 1, 0) != 0)
        {
            constexpr char message[] = "\n[crash] secondary failure while reporting; terminating\n";
            os::logger::EmergencyWrite(message, sizeof(message) - 1);
            TerminateProcess(GetCurrentProcess(), EXCEPTION_NONCONTINUABLE_EXCEPTION);
        }
    }

    bool ReadLocal(const void* address, void* result, size_t size) noexcept
    {
        SIZE_T read = 0;
        return address && ReadProcessMemory(GetCurrentProcess(), address, result, size, &read) && read == size;
    }

    struct GuestRegisters
    {
        uint32_t r[32]{};
        uint64_t lr = 0, ctr = 0;
        bool valid = false;
    };

    GuestRegisters ReadGuestRegisters() noexcept
    {
        GuestRegisters result;
        PPCContext copy;
        if (!ReadLocal(GetPPCContext(), &copy, sizeof(copy))) return result;
        const uint32_t regs[32] = {
            copy.r0.u32, copy.r1.u32, copy.r2.u32, copy.r3.u32, copy.r4.u32, copy.r5.u32, copy.r6.u32, copy.r7.u32,
            copy.r8.u32, copy.r9.u32, copy.r10.u32, copy.r11.u32, copy.r12.u32, copy.r13.u32, copy.r14.u32, copy.r15.u32,
            copy.r16.u32, copy.r17.u32, copy.r18.u32, copy.r19.u32, copy.r20.u32, copy.r21.u32, copy.r22.u32, copy.r23.u32,
            copy.r24.u32, copy.r25.u32, copy.r26.u32, copy.r27.u32, copy.r28.u32, copy.r29.u32, copy.r30.u32, copy.r31.u32};
        for (unsigned i = 0; i < 32; ++i) result.r[i] = regs[i];
        result.lr = copy.lr;
        result.ctr = copy.ctr.u64;
        result.valid = true;
        return result;
    }

    // Write the fault identity before guest reads, module-loader or symbol work.
    // VirtualQuery supplies an image RVA without symbols or the loader lock.
    void WriteEssential(const char* reason, DWORD code, const void* address,
        const CONTEXT& host, const EXCEPTION_RECORD* rec = nullptr) noexcept
    {
        CrashText text;
        text.Text("\n[crash] ").Text(reason).Text(" code=0x").Hex(code)
            .Text(" host=0x").Hex(reinterpret_cast<uintptr_t>(address), 16)
            .Text(" thread=").Decimal(GetCurrentThreadId()).Text(" version=").Text(lo_version::Source).Text("\n");
        text.Text("[crash] host RIP=0x").Hex(host.Rip, 16).Text(" RSP=0x").Hex(host.Rsp, 16)
            .Text(" RBP=0x").Hex(host.Rbp, 16).Text("\n");
        if (rec && (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) && rec->NumberParameters >= 2)
        {
            const auto accessed = rec->ExceptionInformation[1];
            const auto base = reinterpret_cast<uintptr_t>(g_memory.base);
            text.Text("[crash] access=").Text(rec->ExceptionInformation[0] == 0 ? "read" : rec->ExceptionInformation[0] == 1 ? "write" : "execute")
                .Text(" address=0x").Hex(accessed, 16);
            if (base && accessed >= base && accessed - base < PPC_MEMORY_SIZE)
                text.Text(" guest=0x").Hex(accessed - base);
            if (code == EXCEPTION_IN_PAGE_ERROR && rec->NumberParameters >= 3)
                text.Text(" io_status=0x").Hex(rec->ExceptionInformation[2]);
            text.Text("\n");
        }
        text.Write();
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(address, &memory, sizeof(memory)) && memory.Type == MEM_IMAGE)
        {
            text.Text("[crash] module_base=0x").Hex(reinterpret_cast<uintptr_t>(memory.AllocationBase), 16)
                .Text(" module_rva=0x").Hex(reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(memory.AllocationBase), 16).Text("\n");
            text.Write();
            wchar_t path[512]{};
            const DWORD count = K32GetMappedFileNameW(GetCurrentProcess(), memory.AllocationBase, path, DWORD(std::size(path)));
            if (count && count < std::size(path))
            {
                const wchar_t* name = path;
                for (const wchar_t* p = path; *p; ++p)
                    if (*p == '\\' || *p == '/') name = p + 1;
                char utf8[1536]{};
                if (WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8, int(sizeof(utf8)), nullptr, nullptr))
                    text.Text("[crash] module_name=").Text(utf8).Text("\n");
            }
        }
        else
            text.Text("[crash] module=unavailable\n");
        text.Write();
    }

    void WriteGuestRegisters(const GuestRegisters& guest) noexcept
    {
        CrashText text;
        if (!guest.valid)
            text.Text("[crash] guest context unavailable on this thread\n");
        else
        {
            for (unsigned i = 0; i < 32; i += 8)
            {
                text.Text("[crash] r").Decimal(i).Text("-").Decimal(i + 7).Text(":");
                for (unsigned j = 0; j < 8; ++j) text.Text(" ").Hex(guest.r[i + j]);
                text.Text("\n");
            }
            text.Text("[crash] guest r1=").Hex(guest.r[1]).Text(" r3=").Hex(guest.r[3])
                .Text(" r4=").Hex(guest.r[4]).Text(" r5=").Hex(guest.r[5]).Text(" r13=").Hex(guest.r[13])
                .Text(" lr=").Hex(guest.lr, 16).Text(" ctr=").Hex(guest.ctr, 16).Text("\n");
        }
        text.Text("[crash] essential report complete\n").Write();
    }

    bool ParseNumber(const char*& p, const char* end, unsigned base, uint32_t& value) noexcept
    {
        if (end - p >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
        const char* begin = p;
        value = 0;
        while (p != end)
        {
            const unsigned digit = *p >= '0' && *p <= '9' ? *p - '0' :
                *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : *p >= 'A' && *p <= 'F' ? *p - 'A' + 10 : 99;
            if (digit >= base) break;
            if (value > (UINT32_MAX - digit) / base) return false;
            value = value * base + digit;
            ++p;
        }
        return p != begin;
    }

    bool ParseDump(const char* begin, const char* end, const GuestRegisters& guest,
        uint32_t& address, bool& indirect) noexcept
    {
        while (begin != end && (*begin == ' ' || *begin == '\t')) ++begin;
        while (begin != end && (end[-1] == ' ' || end[-1] == '\t')) --end;
        indirect = begin != end && end[-1] == '*';
        if (indirect) --end;
        if (begin == end) return false;
        if (*begin == 'r')
        {
            ++begin;
            uint32_t reg = 0;
            if (!guest.valid || !ParseNumber(begin, end, 10, reg) || reg > 31) return false;
            address = guest.r[reg];
            if (begin != end && (*begin == '+' || *begin == '-'))
            {
                const bool negative = *begin++ == '-';
                uint32_t offset = 0;
                if (!ParseNumber(begin, end, 10, offset)) return false;
                if (negative ? offset > address : offset > UINT32_MAX - address) return false;
                address = negative ? address - offset : address + offset;
            }
        }
        else if (!ParseNumber(begin, end, 16, address)) return false;
        return begin == end;
    }

    bool ReadGuest(uint32_t address, void* data, size_t size) noexcept
    {
        return g_memory.base && address >= 0x1000 && uint64_t(address) + size <= PPC_MEMORY_SIZE &&
            ReadLocal(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(g_memory.base) + address), data, size);
    }

    void WriteGuestDumps(const GuestRegisters& guest) noexcept
    {
        const char* item = g_dumpList;
        // Bounded cached list and reads. Invalid guest pages cannot recursively
        // fault this filter, including pointer indirection and partial reads.
        while (*item)
        {
            const char* end = item;
            while (*end && *end != ',') ++end;
            uint32_t address = 0;
            bool indirect = false;
            CrashText text;
            const bool valid = ParseDump(item, end, guest, address, indirect);
            item = *end ? end + 1 : end;
            if (!valid)
            {
                text.Text("[crash] guest dump skipped: malformed address or unavailable register\n").Write();
                continue;
            }
            uint8_t bytes[256];
            if (indirect)
            {
                if (!ReadGuest(address, bytes, 4))
                {
                    text.Text("[crash] guest pointer unreadable: ").Hex(address).Text("\n").Write();
                    continue;
                }
                const uint32_t target = (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) | (uint32_t(bytes[2]) << 8) | bytes[3];
                text.Text("[crash] [").Hex(address).Text("] -> ").Hex(target).Text("\n").Write();
                address = target;
            }
            if (!ReadGuest(address, bytes, sizeof(bytes)))
            {
                text.Text("[crash] guest dump unreadable: ").Hex(address).Text("\n").Write();
                continue;
            }
            for (unsigned row = 0; row < sizeof(bytes); row += 32)
            {
                text.Text("[crash] ").Hex(address + row).Text(":");
                for (unsigned i = 0; i < 32; ++i)
                {
                    if (i % 4 == 0) text.Text(" ");
                    text.Hex(bytes[row + i], 2);
                }
                text.Text("\n").Write();
            }
            char ascii[257], wide[129];
            for (unsigned i = 0; i < 256; ++i) ascii[i] = bytes[i] >= 0x20 && bytes[i] < 0x7F ? char(bytes[i]) : '.';
            for (unsigned i = 0; i < 128; ++i)
            {
                const uint16_t c = (uint16_t(bytes[2 * i]) << 8) | bytes[2 * i + 1];
                wide[i] = c >= 0x20 && c < 0x7F ? char(c) : '.';
            }
            ascii[256] = wide[128] = 0;
            text.Text("[crash] guest ").Hex(address).Text(" ascii: ").Text(ascii).Text("\n").Write();
            text.Text("[crash] guest ").Hex(address).Text(" utf16: ").Text(wide).Text("\n").Write();
        }
    }

    void WriteHostStack(const CONTEXT& context) noexcept
    {
        CrashText text;
        text.Text("[crash] optional host symbols begin\n").Write();
        HANDLE process = GetCurrentProcess();
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
        // Do not inherit symbol-server environment paths during a fatal error.
        if (!SymInitialize(process, "", TRUE))
        {
            text.Text("[crash] host symbols unavailable\n").Write();
            return;
        }
        CONTEXT c = context;
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = c.Rip; frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = c.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = c.Rsp; frame.AddrStack.Mode = AddrModeFlat;
        alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO) + 512]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 511;
        for (unsigned i = 0; i < 48; ++i)
        {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &c,
                nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr) || !frame.AddrPC.Offset) break;
            DWORD64 displacement = 0;
            text.Text("[crash]   #").Decimal(i).Text(" host=0x").Hex(frame.AddrPC.Offset, 16).Text(" ");
            if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
                text.Text(symbol->Name).Text("+0x").Hex(displacement, 16);
            else text.Text("?");
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisp = 0;
            if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &line))
                text.Text(" (").Text(line.FileName).Text(":").Decimal(line.LineNumber).Text(")");
            text.Text("\n").Write();
        }
        text.Text("[crash] optional host symbols complete\n").Write();
    }

    LONG WINAPI CrashFilter(EXCEPTION_POINTERS* info)
    {
        EnterCrash();
        const auto& rec = *info->ExceptionRecord;
        WriteEssential(ExceptionName(rec.ExceptionCode), rec.ExceptionCode, rec.ExceptionAddress, *info->ContextRecord, &rec);
        const auto guest = ReadGuestRegisters();
        WriteGuestRegisters(guest);
        if (rec.ExceptionCode != EXCEPTION_STACK_OVERFLOW)
        {
            __try
            {
                WriteGuestDumps(guest);
                WriteHostStack(*info->ContextRecord);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                CrashText text;
                text.Text("[crash] optional diagnostics failed code=0x").Hex(GetExceptionCode()).Text("; essential report retained\n").Write();
            }
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }

    [[noreturn]] void ReportRuntimeFailure(const char* reason, DWORD code) noexcept
    {
        EnterCrash();
        CONTEXT context{};
        RtlCaptureContext(&context);
        WriteEssential(reason, code, reinterpret_cast<const void*>(context.Rip), context);
        WriteGuestRegisters(ReadGuestRegisters());
        // Avoid re-entering the CRT through abort/exit/previous handlers or
        // attempting optional allocation/symbol work in a terminate/signal path.
        TerminateProcess(GetCurrentProcess(), code);
        for (;;) {} // Self-termination does not return on success.
    }
    void TerminateHandler() noexcept { ReportRuntimeFailure("std::terminate", 0xE0000001u); }
    void AbortHandler(int) { ReportRuntimeFailure("SIGABRT", 0xE0000002u); }
}

void InstallCrashHandler()
{
    if (GetEnvironmentVariableA("LO_CRASH_DUMP", g_dumpList, DWORD(sizeof(g_dumpList))) >= sizeof(g_dumpList))
        g_dumpList[0] = 0; // Do not truncate an oversized optional list into another address.
    SetUnhandledExceptionFilter(CrashFilter);
    std::set_terminate(TerminateHandler);
    std::signal(SIGABRT, AbortHandler);
}
#else
void InstallCrashHandler() {}
#endif
