#include <stdafx.h>
#include <cpu/ppc_context.h>
#include <kernel/critical_section.h>
#include <kernel/memory.h>
#include <xbox.h>

namespace
{
XRTL_CRITICAL_SECTION& RequestSection(const PPCRegister& manager)
{
    return *reinterpret_cast<XRTL_CRITICAL_SECTION*>(g_memory.base + manager.u32 + 2908);
}
}

// The worker (82DD02B0) consumes requests under manager+2908. Protect only
// publication of the request flag and status +1632: otherwise a fast worker
// completion can be overwritten by the producer's final busy store. Keep
// synchronous content and file operations outside this critical section. Use
// the kernel's lock primitive directly: an import bridge can pause the guest
// before entering the host function, including before an unlock.
void SaveWorkerRequestBegin(PPCRegister& manager)
{
    auto& section = RequestSection(manager);
    GuestCriticalSection::Enter(section.RecursionCount, section.OwningThread, GetPPCContext()->r13.u32);
}

void SaveWorkerRequestEnd(PPCRegister& manager)
{
    auto& section = RequestSection(manager);
    GuestCriticalSection::Leave(section.RecursionCount, section.OwningThread);
}
