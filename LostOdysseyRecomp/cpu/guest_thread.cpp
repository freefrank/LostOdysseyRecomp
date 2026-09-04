#include <stdafx.h>
#include "guest_thread.h"
#include <kernel/memory.h>
#include <kernel/heap.h>
#include <kernel/function.h>
#include <os/logger.h>
#include "ppc_context.h"

// Layout mirrors the real kernel's per-thread block closely enough for the
// game's inline accesses (r13 -> PCR, PCR+0 -> TLS, PCR+0x100 -> TEB).
constexpr size_t PCR_SIZE = 0xAB0;
constexpr size_t TLS_SIZE = 0x100;
constexpr size_t TEB_SIZE = 0x2E0;
constexpr size_t STACK_SIZE = 0x100000; // 1 MiB guest stack; UE3 recursion is deep
constexpr size_t TOTAL_SIZE = PCR_SIZE + TLS_SIZE + TEB_SIZE + STACK_SIZE;

constexpr size_t TEB_OFFSET = PCR_SIZE + TLS_SIZE;

GuestThreadContext::GuestThreadContext(uint32_t cpuNumber)
{
    assert(thread == nullptr);

    // Thread blocks are large (1 MiB stack each) so they come from the guest
    // virtual page allocator rather than the host object heap.
    uint32_t guestBlock = g_pageAllocator.Alloc(g_pageAllocator.virtualRegion, TOTAL_SIZE, 0x1000);
    assert(guestBlock != 0 && "out of guest memory for thread block");
    thread = (uint8_t*)g_memory.Translate(guestBlock);
    memset(thread, 0, TOTAL_SIZE);

    *(uint32_t*)thread = ByteSwap(g_memory.MapVirtual(thread + PCR_SIZE)); // tls pointer
    *(uint32_t*)(thread + 0x100) = ByteSwap(g_memory.MapVirtual(thread + PCR_SIZE + TLS_SIZE)); // teb pointer
    *(thread + 0x10C) = cpuNumber;

    *(uint32_t*)(thread + PCR_SIZE + 0x10) = 0xFFFFFFFF;
    *(uint32_t*)(thread + PCR_SIZE + TLS_SIZE + 0x14C) = ByteSwap(GuestThread::GetCurrentThreadId()); // thread id

    ppcContext.r1.u64 = g_memory.MapVirtual(thread + PCR_SIZE + TLS_SIZE + TEB_SIZE + STACK_SIZE - 0x100); // stack pointer with headroom
    ppcContext.r13.u64 = g_memory.MapVirtual(thread);
    ppcContext.fpscr.loadFromHost();

    assert(GetPPCContext() == nullptr);
    SetPPCContext(ppcContext);
}

GuestThreadContext::~GuestThreadContext()
{
    g_pageAllocator.Free(g_pageAllocator.virtualRegion, g_memory.MapVirtual(thread));
}

static void GuestThreadFunc(GuestThreadHandle* hThread)
{
    hThread->suspended.wait(true);
    GuestThread::Start(hThread->params);
    hThread->finished = true;
    hThread->finished.notify_all();
}

GuestThreadHandle::GuestThreadHandle(const GuestThreadParams& params)
    : params(params), suspended((params.flags & 0x1) != 0), thread(GuestThreadFunc, this)
{
}

GuestThreadHandle::~GuestThreadHandle()
{
    if (thread.joinable())
        thread.join();
}

template <typename ThreadType>
static uint32_t CalcThreadId(const ThreadType& id)
{
    if constexpr (sizeof(id) == 4)
        return *reinterpret_cast<const uint32_t*>(&id);
    else
        return XXH32(&id, sizeof(id), 0);
}

uint32_t GuestThreadHandle::GetThreadId() const
{
    return CalcThreadId(thread.get_id());
}

uint32_t GuestThreadHandle::Wait(uint32_t timeout)
{
    if (timeout == INFINITE)
    {
        if (thread.joinable())
            thread.join();
        return STATUS_WAIT_0;
    }

    if (timeout == 0)
        return finished ? STATUS_WAIT_0 : STATUS_TIMEOUT;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (!finished)
    {
        if (std::chrono::steady_clock::now() >= deadline)
            return STATUS_TIMEOUT;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return STATUS_WAIT_0;
}

uint32_t GuestThread::Start(const GuestThreadParams& params)
{
    const auto procMask = (uint8_t)(params.flags >> 24);
    const auto cpuNumber = procMask == 0 ? 0 : 7 - std::countl_zero(procMask);

    GuestThreadContext ctx(cpuNumber);
    ctx.ppcContext.r3.u64 = params.value;
    ctx.ppcContext.r4.u64 = params.value2;

    // ExTerminateThread longjmps back here (see kernel/imports.cpp).
    extern void GuestThreadRunWithTerminateHook(void (*run)(void*), void* arg);
    struct Launch { PPCContext* ctx; uint32_t function; } launch{ &ctx.ppcContext, params.function };
    GuestThreadRunWithTerminateHook([](void* p)
    {
        auto* l = static_cast<Launch*>(p);
        g_memory.FindFunction(l->function)(*l->ctx, g_memory.base);
    }, &launch);

    return ctx.ppcContext.r3.u32;
}

GuestThreadHandle* GuestThread::Start(const GuestThreadParams& params, uint32_t* threadId)
{
    auto hThread = CreateKernelObject<GuestThreadHandle>(params);

    if (threadId != nullptr)
        *threadId = hThread->GetThreadId();

    return hThread;
}

uint32_t GuestThread::GetCurrentThreadId()
{
    return CalcThreadId(std::this_thread::get_id());
}

void GuestThread::SetLastError(uint32_t error)
{
    auto* thread = (char*)g_memory.Translate(GetPPCContext()->r13.u32);
    if (*(uint32_t*)(thread + 0x150))
        return; // program doesn't want errors

    *(uint32_t*)(thread + TEB_OFFSET + 0x160) = ByteSwap(error);
}

uint32_t GuestThread::GetLastError()
{
    auto* thread = (char*)g_memory.Translate(GetPPCContext()->r13.u32);
    return ByteSwap(*(uint32_t*)(thread + TEB_OFFSET + 0x160));
}

#ifdef _WIN32
void GuestThread::SetThreadName(uint32_t threadId, const char* name)
{
    LOG_KERNEL("thread {:#x} name '{}'", threadId, name);
}
#endif
