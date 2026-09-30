#include <stdafx.h>
#include "language_trace.h"

extern "C" PPC_FUNC(__imp__sub_82481E78);
extern "C" PPC_FUNC(__imp__sub_82481F40);

PPC_FUNC(sub_82481E78)
{
    const auto caller = uint32_t(ctx.lr);
    __imp__sub_82481E78(ctx, base);
    settings::language::TraceEffective(base, "voice-applied", caller, 0);
}

PPC_FUNC(sub_82481F40)
{
    const auto caller = uint32_t(ctx.lr);
    __imp__sub_82481F40(ctx, base);
    settings::language::TraceEffective(base, "voice-id", caller, ctx.r3.u32);
}
