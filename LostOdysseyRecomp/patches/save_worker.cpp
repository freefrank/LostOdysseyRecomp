#include <stdafx.h>

#include <kernel/memory.h>

// The worker (82DD02B0) consumes requests under manager+2908. Protect only
// publication of the request flag and status +1632: otherwise a fast worker
// completion can be overwritten by the producer's final busy store. Keep
// synchronous content and file operations outside this critical section.
void SaveWorkerRequestBegin(PPCRegister& manager)
{
    PPCContext ctx{};
    ctx.r3.u32 = manager.u32 + 2908;
    __imp__RtlEnterCriticalSection(ctx, g_memory.base);
}

void SaveWorkerRequestEnd(PPCRegister& manager)
{
    PPCContext ctx{};
    ctx.r3.u32 = manager.u32 + 2908;
    __imp__RtlLeaveCriticalSection(ctx, g_memory.base);
}
