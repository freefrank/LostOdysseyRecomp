"""Extend issue12_probe.cpp (r1 -> r2): engine event log (GC, BeginDestroy, fence execute,
reattach, proxy create/destroy, flush) and an optional render-thread delay."""
from pathlib import Path

p = Path(__file__).resolve().parent / 'issue12_probe.cpp'
s = p.read_text(encoding='utf-8')
assert 'sub_82702098' not in s, 'already patched'

s = s.replace('extern "C" PPC_FUNC(__imp__sub_823CB350);\n', '''extern "C" PPC_FUNC(__imp__sub_823CB350);
extern "C" PPC_FUNC(__imp__sub_82702098); // UMaterialInstance::BeginDestroy (vtable 0x82005A30 +32)
extern "C" PPC_FUNC(__imp__sub_8249A568); // UObject::CollectGarbage(KeepFlags, bPerformFullPurge)
extern "C" PPC_FUNC(__imp__sub_822FD0A8); // UObject::IncrementalPurgeGarbage(bUseTimeLimit, TimeLimit)
extern "C" PPC_FUNC(__imp__sub_823B6650); // FSetRenderCommandFence::Execute (render thread)
extern "C" PPC_FUNC(__imp__sub_8258D648); // SetSkeletalMesh-like (W6)
extern "C" PPC_FUNC(__imp__sub_822D68C0); // FComponentReattachContext ctor (detach)
extern "C" PPC_FUNC(__imp__sub_822D6CE0); // FComponentReattachContext dtor (attach)
extern "C" PPC_FUNC(__imp__sub_822D6810); // BeginDeferredReattach
extern "C" PPC_FUNC(__imp__sub_8255B620); // CreateSceneProxy (skeletal)
extern "C" PPC_FUNC(__imp__sub_82559E80); // skeletal proxy vtable slot 0 (destructor)
extern "C" PPC_FUNC(__imp__sub_82485C18); // FlushRenderingCommands
''')

helpers = r'''
    // ---- event log (rare engine events) ----
    constexpr uint32_t kNames = 0x833690D0u;
    std::string FNameStr(uint8_t* base, uint32_t idx, uint32_t num)
    {
        uint32_t data, n, entry; char out[160];
        if (!SafeLD32(base, kNames, data) || !SafeLD32(base, kNames + 4, n) || idx >= n || !SafeLD32(base, data + idx * 4, entry) || !entry) { std::snprintf(out, sizeof(out), "<%u_%u>", idx, num); return out; }
        char raw[128]{}; SafeRead(base, entry + 0x10, raw, sizeof(raw) - 2); std::string s;
        for (int i = 0; i + 1 < int(sizeof(raw)); i += 2) { if (!raw[i] && !raw[i + 1]) break; s.push_back(raw[i] ? '?' : raw[i + 1]); }
        if (num) { std::snprintf(out, sizeof(out), "_%u", num - 1); s += out; }
        return s;
    }
    std::string ObjName(uint8_t* base, uint32_t obj)
    {
        uint32_t idx, num, cls, ci = 0, cn = 0;
        if (!obj || !SafeLD32(base, obj + 0x2C, idx) || !SafeLD32(base, obj + 0x30, num) || !SafeLD32(base, obj + 0x34, cls)) return "<bad>";
        std::string s = FNameStr(base, idx, num);
        if (cls && SafeLD32(base, cls + 0x2C, ci) && SafeLD32(base, cls + 0x30, cn)) s = FNameStr(base, ci, cn) + " " + s;
        return s;
    }
    void Event(uint8_t* base, PPCContext& ctx, const char* what, const char* detail)
    {
        if (!g_out) return;
        uint32_t bt[kDepth]; Backtrace(base, ctx.r1.u32, uint32_t(ctx.lr), bt);
        std::lock_guard lock(g_mutex);
        std::fprintf(g_out, "EV t=%.4f swap=%u tid=%x %s %s bt=", Sec(Qpc()), g_presentedSwaps.load(std::memory_order_relaxed), unsigned(GetCurrentThreadId()), what, detail);
        PrintBt(bt); std::fprintf(g_out, "\n"); std::fflush(g_out);
    }
    uint32_t g_watchFence[512]; uint32_t g_watchOwner[512]; std::atomic<uint32_t> g_watchN{0};
    void WatchFence(uint32_t fence, uint32_t owner) { uint32_t i = g_watchN.fetch_add(1); if (i < 512) { g_watchFence[i] = fence; g_watchOwner[i] = owner; } }
    uint32_t WatchedOwner(uint32_t fence) { uint32_t n = std::min<uint32_t>(g_watchN.load(), 512); for (uint32_t i = 0; i < n; i++) if (g_watchFence[i] == fence) return g_watchOwner[i]; return 0; }
    uint32_t DelayUs() { static const uint32_t v = [] { const char* s = getenv("LO_ISSUE12_RT_DELAY_US"); return s ? uint32_t(strtoul(s, nullptr, 10)) : 0u; }(); return v; }
    void SpinUs(uint32_t us) { if (!us) return; const uint64_t until = Qpc() + uint64_t(us) * uint64_t(g_qpf.QuadPart) / 1000000ull; while (Qpc() < until) _mm_pause(); }
'''
anchor = '    // Returns false when the draw should be skipped'
assert s.count(anchor) == 1
s = s.replace(anchor, helpers + '\n' + anchor)

old_draw = '''PPC_FUNC(sub_823CB350)
{
    if (issue12::Enabled() && !issue12::Validate(ctx, base)) return;
    __imp__sub_823CB350(ctx, base);
}'''
assert s.count(old_draw) == 1
new_hooks = r'''PPC_FUNC(sub_823CB350)
{
    if (issue12::Enabled() && !issue12::Validate(ctx, base)) return;
    issue12::SpinUs(issue12::DelayUs());
    __imp__sub_823CB350(ctx, base);
}

PPC_FUNC(sub_82702098) // UMaterialInstance::BeginDestroy
{
    const uint32_t obj = ctx.r3.u32; char d[512];
    if (issue12::Enabled())
    {
        uint32_t r0 = 0, r1 = 0, f0 = 0, f1 = 0; issue12::SafeLD32(base, obj + 200, r0); issue12::SafeLD32(base, obj + 204, r1);
        if (r0) { issue12::SafeLD32(base, r0 + 148, f0); issue12::WatchFence(r0 + 148, obj); }
        if (r1) { issue12::SafeLD32(base, r1 + 148, f1); issue12::WatchFence(r1 + 148, obj); }
        uint32_t threaded = 0; issue12::SafeLD32(base, 0x83318040u, threaded);
        std::snprintf(d, sizeof(d), "obj=%08x %s res0=%08x fence0=%u res1=%08x fence1=%u GIsThreadedRendering=%u", obj, issue12::ObjName(base, obj).c_str(), r0, f0, r1, f1, threaded);
        issue12::Event(base, ctx, "MIC_BeginDestroy", d);
    }
    __imp__sub_82702098(ctx, base);
}

PPC_FUNC(sub_8249A568) // CollectGarbage
{
    char d[160]; std::snprintf(d, sizeof(d), "keep=%08llx fullpurge=%u", (unsigned long long)ctx.r3.u64, ctx.r4.u32);
    if (issue12::Enabled()) issue12::Event(base, ctx, "CollectGarbage_enter", d);
    __imp__sub_8249A568(ctx, base);
    if (issue12::Enabled()) issue12::Event(base, ctx, "CollectGarbage_exit", d);
}

PPC_FUNC(sub_822FD0A8) // IncrementalPurgeGarbage
{
    char d[160]; std::snprintf(d, sizeof(d), "useTimeLimit=%u timeLimit=%.4f", ctx.r3.u32, ctx.f1.f64);
    if (issue12::Enabled()) issue12::Event(base, ctx, "IncrementalPurge_enter", d);
    __imp__sub_822FD0A8(ctx, base);
    if (issue12::Enabled()) issue12::Event(base, ctx, "IncrementalPurge_exit", d);
}

PPC_FUNC(sub_823B6650) // FSetRenderCommandFence::Execute
{
    if (issue12::Enabled())
    {
        uint32_t fence = 0, before = 0; issue12::SafeLD32(base, ctx.r3.u32 + 4, fence); issue12::SafeLD32(base, fence, before);
        const uint32_t owner = issue12::WatchedOwner(fence);
        if (owner) { char d[160]; std::snprintf(d, sizeof(d), "fence=%08x before=%u owner=%08x", fence, before, owner); issue12::Event(base, ctx, "FenceExecute(watched)", d); }
    }
    __imp__sub_823B6650(ctx, base);
}

PPC_FUNC(sub_8258D648) // SetSkeletalMesh-like
{
    if (issue12::Enabled()) { char d[320]; std::snprintf(d, sizeof(d), "comp=%08x %s newmesh=%08x %s flag=%u", ctx.r3.u32, issue12::ObjName(base, ctx.r3.u32).c_str(), ctx.r4.u32, issue12::ObjName(base, ctx.r4.u32).c_str(), ctx.r5.u32); issue12::Event(base, ctx, "SetSkeletalMesh", d); }
    __imp__sub_8258D648(ctx, base);
}

PPC_FUNC(sub_822D68C0) // FComponentReattachContext ctor (detach)
{
    if (issue12::Enabled()) { char d[256]; std::snprintf(d, sizeof(d), "ctx=%08x comp=%08x %s", ctx.r3.u32, ctx.r4.u32, issue12::ObjName(base, ctx.r4.u32).c_str()); issue12::Event(base, ctx, "ReattachCtx_detach", d); }
    __imp__sub_822D68C0(ctx, base);
}

PPC_FUNC(sub_822D6CE0) // FComponentReattachContext dtor (attach)
{
    if (issue12::Enabled()) { uint32_t comp = 0; issue12::SafeLD32(base, ctx.r3.u32, comp); char d[256]; std::snprintf(d, sizeof(d), "ctx=%08x comp=%08x %s", ctx.r3.u32, comp, issue12::ObjName(base, comp).c_str()); issue12::Event(base, ctx, "ReattachCtx_attach", d); }
    __imp__sub_822D6CE0(ctx, base);
}

PPC_FUNC(sub_822D6810) // BeginDeferredReattach
{
    if (issue12::Enabled()) { char d[256]; std::snprintf(d, sizeof(d), "comp=%08x %s", ctx.r3.u32, issue12::ObjName(base, ctx.r3.u32).c_str()); issue12::Event(base, ctx, "BeginDeferredReattach", d); }
    __imp__sub_822D6810(ctx, base);
}

PPC_FUNC(sub_8255B620) // CreateSceneProxy (skeletal)
{
    const uint32_t comp = ctx.r3.u32;
    __imp__sub_8255B620(ctx, base);
    if (issue12::Enabled()) { char d[256]; std::snprintf(d, sizeof(d), "comp=%08x %s -> proxy=%08x", comp, issue12::ObjName(base, comp).c_str(), ctx.r3.u32); issue12::Event(base, ctx, "CreateSceneProxy", d); }
}

PPC_FUNC(sub_82559E80) // skeletal proxy destructor
{
    if (issue12::Enabled()) { char d[96]; std::snprintf(d, sizeof(d), "proxy=%08x flags=%u", ctx.r3.u32, ctx.r4.u32); issue12::Event(base, ctx, "ProxyDestructor", d); }
    __imp__sub_82559E80(ctx, base);
}

PPC_FUNC(sub_82485C18) // FlushRenderingCommands
{
    if (issue12::Enabled()) issue12::Event(base, ctx, "FlushRenderingCommands_enter", "");
    __imp__sub_82485C18(ctx, base);
    if (issue12::Enabled()) issue12::Event(base, ctx, "FlushRenderingCommands_exit", "");
}'''
s = s.replace(old_draw, new_hooks)
s = s.replace('#include <vector>\n', '#include <vector>\n#include <algorithm>\n#include <intrin.h>\n')
p.write_text(s, encoding='utf-8')
print('probe source updated (r2)')
