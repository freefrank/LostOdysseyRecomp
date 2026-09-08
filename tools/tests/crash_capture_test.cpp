// Child-process fixture. The Python runner checks actual fatal exits and files;
// no fake invocation of the exception filter or guest/game library is used.
#include <stdafx.h>
#include <os/crash_handler.h>
#include <os/log_file.h>
#include <os/logger.h>
#include <cpu/ppc_context.h>
#include <kernel/memory.h>
#include <exception>
#include <stdexcept>

Memory::Memory() {}
Memory g_memory;

static void SetFixtureContext(PPCContext& context)
{
    context.r1.u32 = 0x11112222;
    context.r3.u32 = 0x33334444;
    context.r13.u32 = 0x13131313;
    context.lr = 0xABCDEF01;
    context.ctr.u64 = 0xABCD1234;
    SetPPCContext(context);
}

[[noreturn]] static void AccessViolation()
{
    auto* page = static_cast<volatile DWORD*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY));
    if (!page) ExitProcess(20);
    // A real CPU write to a valid, read-only page; not a synthetic RaiseException.
    *page = 0x12345678;
    ExitProcess(21);
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3) return 10;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    const std::wstring mode = argv[1];
    if (std::wstring_view(argv[2]) != L"-" && !os::logger::OpenFile(argv[2])) return 11;
    InstallCrashHandler();
    LOG_INFO("crash-fixture ready main-thread={}", GetCurrentThreadId());
    PPCContext context{};
    SetFixtureContext(context);

    if (mode == L"av-bad-context") g_ppcContext = reinterpret_cast<PPCContext*>(uintptr_t(1));
    if (mode == L"av-dump")
    {
        g_memory.base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (!g_memory.base) return 12;
        g_memory.base[0x1002] = 0x20; // Big-endian pointer 00002000.
        memcpy(g_memory.base + 0x2000, "fixture-readable-guest", 22);
    }
    if (mode == L"av-no-stderr" || mode == L"av-no-sinks")
        SetStdHandle(STD_ERROR_HANDLE, nullptr);
    if (mode == L"av-full-stderr-pipe")
    {
        HANDLE readPipe = nullptr, writePipe = nullptr;
        DWORD capacity = 0;
        if (!CreatePipe(&readPipe, &writePipe, nullptr, 4096) ||
            !GetNamedPipeInfo(writePipe, nullptr, &capacity, nullptr, nullptr) || !capacity)
            return 14;
        char fill[1024]{};
        for (DWORD remaining = capacity; remaining;)
        {
            DWORD written = 0;
            const DWORD count = (std::min)(remaining, DWORD(sizeof(fill)));
            if (!WriteFile(writePipe, fill, count, &written, nullptr) || written != count) return 15;
            remaining -= written;
        }
        // Retain the read end without consuming it. Any synchronous stderr
        // write now blocks; only the independent runtime file can report.
        if (!SetStdHandle(STD_ERROR_HANDLE, writePipe)) return 16;
    }
    if (mode == L"av-worker-locked" || mode == L"terminate-worker-locked" || mode == L"abort-worker-locked")
    {
        // The faulting thread cannot acquire these locks: the main thread
        // keeps them until process death. Includes both CRT output FILE locks.
        os::logger::g_mutex.lock();
        if (os::logger::g_file) _lock_file(os::logger::g_file);
        _lock_file(stderr);
        std::thread worker([&]
        {
            PPCContext workerContext{};
            SetFixtureContext(workerContext);
            if (mode == L"terminate-worker-locked") std::terminate();
            if (mode == L"abort-worker-locked") std::abort();
            AccessViolation();
        });
        worker.join();
        return 13;
    }
    if (mode == L"terminate") std::terminate();
    if (mode == L"uncaught") throw std::runtime_error("fixture uncaught exception");
    if (mode == L"abort") std::abort();
    if (mode == L"av-main-locked")
    {
        os::logger::g_mutex.lock();
        if (os::logger::g_file) _lock_file(os::logger::g_file);
        _lock_file(stderr);
    }
    AccessViolation();
}
