// Issue #12: drain the rendering thread before the garbage collector purges UObjects.
//
// The original Lost Odyssey engine (2007-era Unreal Engine 3) frees unreachable UObjects in
// UObject::CollectGarbage (guest 0x8249A568) / IncrementalPurgeGarbage (guest 0x822FD0A8)
// without first waiting for the rendering thread. A live scene proxy can still hold raw
// UMaterialInterface pointers to materials the game thread just replaced (the pawn's
// SetSkeletalMesh path at the funeral hand-in creates new MaterialInstanceConstants and drops
// the old ones without the old proxy having been removed yet). On the console the render
// thread has usually finished the previous frame by the time the purge runs; in the recompiled
// runtime it is still executing that frame's FDrawSceneCommand, reads the freed material and
// jumps to address 0 (Issue #12). Later UE3 versions fixed exactly this by calling
// FlushRenderingCommands() at the start of CollectGarbage; this hook does the same from the
// host side, only while threaded rendering is active, and only when a purge can actually free
// objects. All registers the original entry point consumes are preserved.

#include <stdafx.h>
#include <os/logger.h>

extern "C" PPC_FUNC(__imp__sub_8249A568); // UObject::CollectGarbage(KeepFlags, bPerformFullPurge)
extern "C" PPC_FUNC(__imp__sub_822FD0A8); // UObject::IncrementalPurgeGarbage(bUseTimeLimit, TimeLimit)

#ifdef ISSUE12_EXTERNAL_GC_HOOKS
// Diagnostic build: chain into the probe's event-logging wrappers instead of the guest body.
void issue12_chain_sub_8249A568(PPCContext& __restrict ctx, uint8_t* base);
void issue12_chain_sub_822FD0A8(PPCContext& __restrict ctx, uint8_t* base);
#define GC_FLUSH_NEXT_COLLECT issue12_chain_sub_8249A568
#define GC_FLUSH_NEXT_PURGE issue12_chain_sub_822FD0A8
#else
#define GC_FLUSH_NEXT_COLLECT __imp__sub_8249A568
#define GC_FLUSH_NEXT_PURGE __imp__sub_822FD0A8
#endif

namespace
{
    constexpr uint32_t kGIsThreadedRendering = 0x83318040; // written only by Start/StopRenderingThread
    constexpr uint32_t kGObjPurgeIsRequired = 0x83315F40;  // set after BeginDestroy, cleared when the purge completes

    bool Enabled()
    {
        static const bool enabled = [] {
            const char* value = getenv("LO_GC_RENDER_FLUSH");
            return !value || strcmp(value, "0") != 0; // opt-out only
        }();
        return enabled;
    }

    void FlushRenderingThread(PPCContext& ctx, uint8_t* base, const char* reason)
    {
        // FlushRenderingCommands (guest 0x82485C18) takes no arguments; it clobbers the volatile
        // registers and LR like any guest call, so keep the caller-visible state intact.
        const PPCRegister r3 = ctx.r3, r4 = ctx.r4, r5 = ctx.r5, r6 = ctx.r6, r7 = ctx.r7;
        const PPCRegister f1 = ctx.f1;
        const uint64_t lr = ctx.lr; const PPCRegister ctr = ctx.ctr;
        static thread_local uint32_t reports = 0;
        if (reports++ < 8)
            LOG_INFO("gc render flush before {} (caller={:#x})", reason, uint32_t(lr));
        sub_82485C18(ctx, base);
        ctx.r3 = r3; ctx.r4 = r4; ctx.r5 = r5; ctx.r6 = r6; ctx.r7 = r7;
        ctx.f1 = f1;
        ctx.lr = lr; ctx.ctr = ctr;
    }
}

PPC_FUNC(sub_8249A568)
{
    if (Enabled() && PPC_LOAD_U32(kGIsThreadedRendering) != 0)
        FlushRenderingThread(ctx, base, "CollectGarbage");
    GC_FLUSH_NEXT_COLLECT(ctx, base);
}

PPC_FUNC(sub_822FD0A8)
{
    // Only when a purge is pending: this entry point is also called every tick with nothing to do.
    if (Enabled() && PPC_LOAD_U32(kGIsThreadedRendering) != 0 && PPC_LOAD_U32(kGObjPurgeIsRequired) != 0)
        FlushRenderingThread(ctx, base, "IncrementalPurgeGarbage");
    GC_FLUSH_NEXT_PURGE(ctx, base);
}
