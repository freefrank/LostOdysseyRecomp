#include <stdafx.h>

extern "C" PPC_FUNC(__imp__sub_82DCBFD8);
extern "C" PPC_FUNC(__imp__sub_82DCC0E8);
extern "C" PPC_FUNC(__imp__sub_82DCC190);
extern "C" PPC_FUNC(__imp__sub_82DCC3C8);

namespace
{
void Submit(PPCContext& ctx, uint8_t* base, PPCFunc* request)
{
    // The worker (82DD02B0) consumes the request under this critical section.
    // These entries publish the request before setting status +1632 to busy;
    // otherwise a fast completion can be overwritten by that final busy store.
    // Match the existing content-create request (82DCBE48).
    const uint32_t criticalSection = ctx.r3.u32 + 2908;
    PPCContext lockContext = ctx;
    lockContext.r3.u32 = criticalSection;
    __imp__RtlEnterCriticalSection(lockContext, base);
    request(ctx, base);
    lockContext.r3.u32 = criticalSection;
    __imp__RtlLeaveCriticalSection(lockContext, base);
}
}

PPC_FUNC(sub_82DCBFD8) { Submit(ctx, base, __imp__sub_82DCBFD8); }
PPC_FUNC(sub_82DCC0E8) { Submit(ctx, base, __imp__sub_82DCC0E8); }
PPC_FUNC(sub_82DCC190) { Submit(ctx, base, __imp__sub_82DCC190); }
PPC_FUNC(sub_82DCC3C8) { Submit(ctx, base, __imp__sub_82DCC3C8); }
