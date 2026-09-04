#pragma once

#include <kernel/xdm.h>

#define CURRENT_THREAD_HANDLE uint32_t(-2)

// Per-thread guest state: PCR (r13 points here), TLS slots, TEB and the guest
// stack, all carved from the host heap inside guest memory.
struct GuestThreadContext
{
    PPCContext ppcContext{};
    uint8_t* thread = nullptr;

    GuestThreadContext(uint32_t cpuNumber);
    ~GuestThreadContext();
};

struct GuestThreadParams
{
    uint32_t function;
    uint32_t value;   // r3
    uint32_t flags;
    uint32_t value2;  // r4 (used when a thread starts through the XAPI startup shim)
};

struct GuestThreadHandle : KernelObject
{
    GuestThreadParams params;
    std::atomic<bool> suspended;
    std::atomic<bool> finished{ false };
    std::thread thread;

    GuestThreadHandle(const GuestThreadParams& params);
    ~GuestThreadHandle() override;

    uint32_t GetThreadId() const;

    uint32_t Wait(uint32_t timeout) override;
};

struct GuestThread
{
    static uint32_t Start(const GuestThreadParams& params);
    static GuestThreadHandle* Start(const GuestThreadParams& params, uint32_t* threadId);

    static uint32_t GetCurrentThreadId();
    static void SetLastError(uint32_t error);
    static uint32_t GetLastError();

#ifdef _WIN32
    static void SetThreadName(uint32_t threadId, const char* name);
#endif
};
