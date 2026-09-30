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

    void SetCpuNumber(uint32_t cpuNumber);
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
    struct Control {
        GuestThreadParams params;
        std::atomic<bool> suspended;
        kernel::wait::Event completion{true, false};
        explicit Control(const GuestThreadParams& p) : params(p), suspended((p.flags & 1) != 0) {}
    };
    std::shared_ptr<Control> control;
    std::thread thread;

    GuestThreadHandle(const GuestThreadParams& params);
    ~GuestThreadHandle() override;

    uint32_t GetThreadId() const;

    kernel::wait::Target* WaitTarget() override { return &control->completion; }
};

struct GuestThread
{
    // Cooperative pause point for code executing on behalf of a guest thread.
    // Call only at boundaries where no host subsystem lock is held.
    static void WaitIfPaused();

    static uint32_t Start(const GuestThreadParams& params);
    static std::shared_ptr<GuestThreadHandle> Start(const GuestThreadParams& params, uint32_t* threadId);

    static uint32_t GetCurrentThreadId();
    static void SetLastError(uint32_t error);
    static uint32_t GetLastError();

#ifdef _WIN32
    static void SetThreadName(uint32_t threadId, const char* name);
#endif
};
