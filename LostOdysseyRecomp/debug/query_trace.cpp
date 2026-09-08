#include <stdafx.h>
#include <os/logger.h>
#include <cpu/poll_wait.h>

extern "C" PPC_FUNC(__imp__sub_827B7408);
extern "C" PPC_FUNC(__imp__sub_823CDCA8);
extern "C" PPC_FUNC(__imp__sub_823CF3F0);
void ArmGuestWriteWatchpoint(uint32_t address, uint32_t length);

PPC_FUNC(sub_823CF3F0)
{
    static const bool enabled = getenv("LO_QUERY_CALL_TRACE") != nullptr;
    if (!enabled)
    {
        __imp__sub_823CF3F0(ctx, base);
        poll_wait::QueryResult(ctx.r3.s32);
        return;
    }
    const uint32_t query = ctx.r3.u32, output = ctx.r4.u32, sp = ctx.r1.u32;
    const uint64_t r27 = ctx.r27.u64, r28 = ctx.r28.u64, r29 = ctx.r29.u64;
    const uint64_t r30 = ctx.r30.u64, r31 = ctx.r31.u64;
    static thread_local unsigned reports = 0;
    if (reports++ < 4 || !output)
        LOG_INFO("query call: query={:#x} output={:#x} sp={:#x} device={:#x} caller={:#x}",
            query, output, sp, query ? PPC_LOAD_U32(query) : 0, uint32_t(ctx.lr));
    __imp__sub_823CF3F0(ctx, base);
    // Snapshot before comparison so the diagnostic prints the exact values
    // tested, even if another callback unexpectedly touches this context.
    const uint32_t afterSp = ctx.r1.u32;
    const std::array<uint64_t, 5> after{ctx.r27.u64, ctx.r28.u64, ctx.r29.u64, ctx.r30.u64, ctx.r31.u64};
    if (afterSp != sp || after[0] != r27 || after[1] != r28 ||
        after[2] != r29 || after[3] != r30 || after[4] != r31)
        LOG_ERROR("query callee changed saved registers: query={:#x} output={:#x} sp={:#x}->{:#x} r27={:#x}->{:#x} r28={:#x}->{:#x} r29={:#x}->{:#x} r30={:#x}->{:#x} r31={:#x}->{:#x}",
            query, output, sp, afterSp, r27, after[0], r28, after[1],
            r29, after[2], r30, after[3], r31, after[4]);
    poll_wait::QueryResult(ctx.r3.s32);
}

// Opt-in lifetime evidence for D3D type-9 queries. Never change query results.
PPC_FUNC(sub_827B7408)
{
    const uint32_t device = ctx.r3.u32;
    const uint32_t caller = uint32_t(ctx.lr);
    __imp__sub_827B7408(ctx, base);
    if (!getenv("LO_QUERY_TRACE")) return;
    static std::atomic<uint32_t> sequence{0};
    const uint32_t index = ++sequence;
    const uint32_t query = ctx.r3.u32;
    LOG_INFO("query create: index={} query={:#x} device={:#x} stored={:#x} caller={:#x}",
        index, query, device, query ? PPC_LOAD_U32(query) : 0, caller);
    const char* selected = getenv("LO_QUERY_WATCH_INDEX");
    if (query && selected && index == strtoul(selected, nullptr, 10))
        ArmGuestWriteWatchpoint(query, 4);
}

PPC_FUNC(sub_823CDCA8)
{
    if (getenv("LO_QUERY_TRACE"))
    {
        const uint32_t query = ctx.r3.u32;
        if (PPC_LOAD_U32(query + 12) == 1)
            LOG_INFO("query final release: query={:#x} device={:#x} type={} caller={:#x}",
                query, PPC_LOAD_U32(query), PPC_LOAD_U32(query + 4), uint32_t(ctx.lr));
    }
    __imp__sub_823CDCA8(ctx, base);
}
