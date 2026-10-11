#include <stdafx.h>
#include <os/logger.h>
#include <cpu/poll_wait.h>
#include <gpu/renderer.h>
#include <gpu/occlusion_queries.h>
#include <patches/occlusion_history.h>
#include <unordered_map>

extern "C" PPC_FUNC(__imp__sub_827B7408);
extern "C" PPC_FUNC(__imp__sub_823CDCA8);
extern "C" PPC_FUNC(__imp__sub_823CF3F0);
extern "C" PPC_FUNC(__imp__sub_823CCCE8);
void ArmGuestWriteWatchpoint(uint32_t address, uint32_t length);

namespace
{
bool QueryLifetimeTraceEnabled()
{
    // This startup-only diagnostic must not scan the environment on every query.
    static const bool enabled = getenv("LO_QUERY_TRACE") != nullptr;
    return enabled;
}

// Physical address of the slot holding a query's first record (query+28,
// a 0xA0000000/0xE0000000-based guest address).
uint32_t QueryRecordSlot(uint8_t* base, uint32_t query)
{
    const uint32_t record = PPC_LOAD_U32(query + 28);
    return (record & 0x1FFFFFFF) + (record >= 0xE0000000 ? 0x1000 : 0);
}

// D3D GetData returns 1 for a type-9 (occlusion) query while the GPU fence has
// not passed it or its END record still holds the sentinel. With host queries
// the renderer completes that record when the command processor is asked to,
// so tell it a guest is waiting, and on which record.
void NoteQueryResult(bool occlusion, int32_t result, uint32_t slot)
{
    if (occlusion && result == 1) gpu::renderer::NoteOcclusionWait(slot);
    poll_wait::QueryResult(result);
}
}

// The renderer's occlusion query pool (sub_823CCCE8) hands out query objects
// in allocation order and restarts at index 0 every frame, so a query's
// records belong to whichever object asks first. Report each allocation's call
// site and per-frame ordinal at that site as the owner of the query's record
// slot, so Fast answers follow the object (gpu/occlusion_queries.h).
PPC_FUNC(sub_823CCCE8)
{
    const uint32_t pool = PPC_LOAD_U32(0x83235AB8);
    const uint32_t index = pool ? PPC_LOAD_U32(pool + 0x20) : 0;
    const uint32_t caller = uint32_t(ctx.lr);
    __imp__sub_823CCCE8(ctx, base);
    const uint32_t query = ctx.r3.u32;
    if (!query) return;
    // Allocation is single-threaded (the render thread of the game).
    static std::unordered_map<uint32_t, uint32_t> ordinals;
    if (index == 0) ordinals.clear();
    const uint32_t ordinal = ordinals[caller]++;
    // query+28: guest address of the query's first record slot (D3D GetData).
    const uint32_t record = PPC_LOAD_U32(query + 28);
    const uint32_t physical = QueryRecordSlot(base, query);
    static uint32_t logged = 0;
    if (logged < 8 || (caller == 0x823D5688 && logged < 16)) {
        ++logged;
        LOG_INFO("occlusion query owner: query={:#x} pool_index={} caller={:#x} ordinal={} record={:#x} slot={:#x}",
            query, index, caller, ordinal, record, physical);
    }
    // Policy::History: one owner per issue instead (patches/occlusion_history.cpp).
    const uint64_t history = occlusion_history::IssueOwner(query, caller);
    gpu::renderer::NoteOcclusionQueryOwner(physical, history ? history : gpu::occlusion::OwnerKey(caller, ordinal));
}

PPC_FUNC(sub_823CF3F0)
{
    static const bool enabled = getenv("LO_QUERY_CALL_TRACE") != nullptr;
    const bool occlusion = ctx.r3.u32 && PPC_LOAD_U32(ctx.r3.u32 + 4) == 9;
    const uint32_t slot = occlusion ? QueryRecordSlot(base, ctx.r3.u32) : 0;
    if (!enabled)
    {
        __imp__sub_823CF3F0(ctx, base);
        NoteQueryResult(occlusion, ctx.r3.s32, slot);
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
    NoteQueryResult(occlusion, ctx.r3.s32, slot);
}

// Opt-in lifetime evidence for D3D type-9 queries. Never change query results.
PPC_FUNC(sub_827B7408)
{
    const uint32_t device = ctx.r3.u32;
    const uint32_t caller = uint32_t(ctx.lr);
    __imp__sub_827B7408(ctx, base);
    if (!QueryLifetimeTraceEnabled()) return;
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
    if (QueryLifetimeTraceEnabled())
    {
        const uint32_t query = ctx.r3.u32;
        if (PPC_LOAD_U32(query + 12) == 1)
            LOG_INFO("query final release: query={:#x} device={:#x} type={} caller={:#x}",
                query, PPC_LOAD_U32(query), PPC_LOAD_U32(query + 4), uint32_t(ctx.lr));
    }
    __imp__sub_823CDCA8(ctx, base);
}
