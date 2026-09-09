// Issue #12 diagnostic overlay (host side). Diagnostic only; never a production fix.
//
// Hooks three guest functions of the original game:
//   sub_82295B08  FMalloc::Malloc(this=r3, size=r4)            -> records every 0xE0-byte allocation
//   sub_82298990  FMalloc::Free(this=r3, block=r4)              -> records 0xE0 frees and UObject purge frees
//   sub_823CB350  FSkeletalMeshSceneProxy-like DrawDynamicElements(this=r3, PDI=r4, View=r5, DPG=r6)
//                 -> validates every (Material, UseMaterialIndex) record of every LOD before the
//                    original runs; on a stale material it dumps the alloc/free history and can
//                    optionally skip the draw (LO_ISSUE12_SKIP_STALE=1) so the game keeps running.
// Nothing here modifies guest memory, registers or return values. All guest calls go to the
// unchanged generated implementation (__imp__*).

#include <stdafx.h>
#include <os/logger.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>
#include <intrin.h>

extern "C" PPC_FUNC(__imp__sub_82298990);
extern "C" PPC_FUNC(__imp__sub_82295B08);
extern "C" PPC_FUNC(__imp__sub_823CB350);
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
extern std::atomic<uint32_t> g_presentedSwaps;

namespace issue12
{
    constexpr uint32_t kCodeLo = 0x82000000u, kCodeHi = 0x833C0000u;
    constexpr int kDepth = 14;

    struct AllocRec { uint64_t seq, qpc; uint32_t block, size, lr, tid, swap; uint32_t bt[kDepth]; };
    struct FreeRec
    {
        uint64_t seq, qpc, flags; uint32_t block, size, lr, tid, swap;
        uint32_t w10, w14, outer, nameIdx, nameNum, cls, w3c, w4c;
        uint64_t outerFlags; uint32_t outerNameIdx, outerNameNum, outerCls;
        uint32_t bt[kDepth];
    };

    constexpr size_t kAllocRing = 1 << 16, kFreeRing = 1 << 16;
    AllocRec g_alloc[kAllocRing]; size_t g_allocN = 0;
    FreeRec g_free[kFreeRing]; size_t g_freeN = 0;
    std::atomic<uint64_t> g_seq{0};
    std::mutex g_mutex;
    FILE* g_out = nullptr;
    LARGE_INTEGER g_qpf{};
    uint64_t g_qpc0 = 0;
    std::atomic<uint32_t> g_purgeFrees{0};
    uint32_t g_staleProxy[16]{}; uint32_t g_staleCount[16]{}; int g_staleN = 0;
    uint32_t g_lastStaleSwap = 0; int g_detections = 0;

    bool Enabled()
    {
        static const bool enabled = [] {
            const char* path = getenv("LO_ISSUE12_PROBE_FILE");
            if (!path) return false;
            g_out = std::fopen(path, "ab");
            QueryPerformanceFrequency(&g_qpf);
            LARGE_INTEGER now; QueryPerformanceCounter(&now); g_qpc0 = uint64_t(now.QuadPart);
            if (g_out) { std::fprintf(g_out, "\n=== issue12 probe session start tid=%x qpf=%lld ===\n", unsigned(GetCurrentThreadId()), g_qpf.QuadPart); std::fflush(g_out); }
            LOG_INFO("issue12 probe active: file={} skip_stale={}", path, getenv("LO_ISSUE12_SKIP_STALE") ? 1 : 0);
            return g_out != nullptr;
        }();
        return enabled;
    }

    inline uint64_t Qpc() { LARGE_INTEGER now; QueryPerformanceCounter(&now); return uint64_t(now.QuadPart); }
    inline double Sec(uint64_t qpc) { return double(int64_t(qpc - g_qpc0)) / double(g_qpf.QuadPart); }
    inline uint32_t LD32(uint8_t* base, uint32_t a) { return __builtin_bswap32(*(volatile uint32_t*)(base + a)); }
    inline uint64_t LD64(uint8_t* base, uint32_t a) { return __builtin_bswap64(*(volatile uint64_t*)(base + a)); }

    bool SafeRead(uint8_t* base, uint32_t a, void* out, size_t n)
    {
        if (a < 0x10000u || uint64_t(a) + n > 0xFFFFFFFFull) return false;
        SIZE_T got = 0;
        return ReadProcessMemory(GetCurrentProcess(), base + a, out, n, &got) && got == n;
    }
    bool SafeLD32(uint8_t* base, uint32_t a, uint32_t& v) { uint32_t raw; if (!SafeRead(base, a, &raw, 4)) return false; v = __builtin_bswap32(raw); return true; }
    bool SafeLD64(uint8_t* base, uint32_t a, uint64_t& v) { uint64_t raw; if (!SafeRead(base, a, &raw, 8)) return false; v = __builtin_bswap64(raw); return true; }

    // Walk the Xbox 360 back chain: [sp] = caller sp, saved LR at [caller_sp - 8].
    void Backtrace(uint8_t* base, uint32_t sp, uint32_t lr0, uint32_t* out)
    {
        int i = 0; out[i++] = lr0;
        for (int guard = 0; i < kDepth && guard < 64; guard++)
        {
            uint32_t prev, lr;
            if (!SafeLD32(base, sp, prev)) break;
            if (prev <= sp || prev - sp > 0x200000u || (prev & 7)) break;
            if (!SafeLD32(base, prev - 8, lr)) break;
            if (lr < kCodeLo || lr >= kCodeHi) { sp = prev; continue; }
            out[i++] = lr; sp = prev;
        }
        for (; i < kDepth; i++) out[i] = 0;
    }

    // Block size class exactly as sub_82298990 derives it (page table -> pool descriptor -> +0x10).
    uint32_t BlockSize(uint8_t* base, uint32_t alloc, uint32_t block)
    {
        const uint32_t r11 = ((block >> 27) & 0x1Fu) + 846u;
        uint32_t tbl; if (!SafeLD32(base, alloc + r11 * 4, tbl) || !tbl) return 0;
        const uint32_t entry = tbl + ((block >> 11) & 0xFFE0u);
        uint32_t pool; if (!SafeLD32(base, entry + 12, pool)) return 0;
        if (pool == alloc + 3364u) { uint32_t big; return SafeLD32(base, entry, big) ? big : 0; }
        uint32_t size; return SafeLD32(base, pool + 16, size) ? size : 0;
    }

    void OnAlloc(uint8_t* base, uint32_t block, uint32_t size, uint32_t lr, uint32_t sp)
    {
        AllocRec r{}; r.seq = ++g_seq; r.qpc = Qpc(); r.block = block; r.size = size; r.lr = lr;
        r.tid = GetCurrentThreadId(); r.swap = g_presentedSwaps.load(std::memory_order_relaxed);
        Backtrace(base, sp, lr, r.bt);
        std::lock_guard lock(g_mutex);
        g_alloc[g_allocN++ % kAllocRing] = r;
    }

    void OnFree(uint8_t* base, uint32_t alloc, uint32_t block, uint32_t lr, uint32_t sp)
    {
        const uint32_t size = BlockSize(base, alloc, block);
        uint64_t flags = 0; SafeLD64(base, block + 8, flags);
        const bool purge = size >= 0x3C && (uint32_t(flags) & 0x18000u) == 0x18000u; // BeginDestroyed|FinishDestroyed
        if (size != 0xE0 && !purge) return;
        if (purge) g_purgeFrees.fetch_add(1, std::memory_order_relaxed);
        FreeRec r{}; r.seq = ++g_seq; r.qpc = Qpc(); r.block = block; r.size = size; r.lr = lr; r.flags = flags;
        r.tid = GetCurrentThreadId(); r.swap = g_presentedSwaps.load(std::memory_order_relaxed);
        SafeLD32(base, block + 0x10, r.w10); SafeLD32(base, block + 0x14, r.w14);
        SafeLD32(base, block + 0x28, r.outer); SafeLD32(base, block + 0x2C, r.nameIdx); SafeLD32(base, block + 0x30, r.nameNum);
        SafeLD32(base, block + 0x34, r.cls); SafeLD32(base, block + 0x3C, r.w3c); SafeLD32(base, block + 0x4C, r.w4c);
        if (r.outer) { SafeLD64(base, r.outer + 8, r.outerFlags); SafeLD32(base, r.outer + 0x2C, r.outerNameIdx); SafeLD32(base, r.outer + 0x30, r.outerNameNum); SafeLD32(base, r.outer + 0x34, r.outerCls); }
        Backtrace(base, sp, lr, r.bt);
        std::lock_guard lock(g_mutex);
        g_free[g_freeN++ % kFreeRing] = r;
    }

    // ---- FName resolution by scanning guest memory once (diagnostic path only) ----
    struct NameTable { bool tried = false, ok = false; uint32_t data = 0, off = 0; bool wide = false; } g_names;

    template<class F> void ForEachCommitted(uint8_t* base, F&& f)
    {
        uint8_t* p = base; uint8_t* end = base + 0x100000000ull;
        while (p < end)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;
            uint8_t* rs = (uint8_t*)mbi.BaseAddress; size_t rn = mbi.RegionSize;
            if (rs + rn > end) rn = end - rs;
            const bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY));
            if (readable) f(rs, rn);
            p = rs + rn;
        }
    }

    void FindNameTable(uint8_t* base, uint32_t probeIdx)
    {
        g_names.tried = true;
        static const char ascii[] = "MaterialInstanceConstant";
        uint8_t wide[sizeof(ascii) * 2]; for (size_t i = 0; i < sizeof(ascii); i++) { wide[i * 2] = 0; wide[i * 2 + 1] = uint8_t(ascii[i]); }
        std::vector<std::pair<uint32_t, bool>> hits; // (string guest address, wide)
        ForEachCommitted(base, [&](uint8_t* rs, size_t rn) {
            for (size_t i = 0; i + sizeof(ascii) <= rn; i++)
            {
                if (rs[i] == 'M' && !std::memcmp(rs + i, ascii, sizeof(ascii))) hits.emplace_back(uint32_t(rs + i - base), false);
                if (rs[i] == 0 && i + sizeof(wide) <= rn && rs[i + 1] == 'M' && !std::memcmp(rs + i, wide, sizeof(wide))) hits.emplace_back(uint32_t(rs + i - base), true);
            }
        });
        if (g_out) std::fprintf(g_out, "name-scan: %zu string hits for '%s'\n", hits.size(), ascii);
        for (auto [s, w] : hits)
        {
            for (uint32_t off : { 8u, 12u, 16u, 20u, 24u })
            {
                const uint32_t entry = s - off; const uint32_t be = __builtin_bswap32(entry);
                uint32_t found = 0;
                ForEachCommitted(base, [&](uint8_t* rs, size_t rn) {
                    if (found) return;
                    for (size_t i = 0; i + 4 <= rn; i += 4) if (*(uint32_t*)(rs + i) == be) { const uint32_t a = uint32_t(rs + i - base); const uint32_t data = a - probeIdx * 4; uint32_t e0; if (data < a && SafeLD32(base, data, e0)) { char buf[16]{}; if (SafeRead(base, e0 + off, buf, w ? 8 : 4)) { const bool none = w ? (buf[0]==0&&buf[1]=='N'&&buf[3]=='o'&&buf[5]=='n'&&buf[7]=='e') : !std::memcmp(buf, "None", 4); if (none) { found = data; return; } } } }
                });
                if (found) { g_names.ok = true; g_names.data = found; g_names.off = off; g_names.wide = w; if (g_out) std::fprintf(g_out, "name-scan: FName::Names.Data=%08x entry string offset=%u wide=%d (MIC entry %08x)\n", found, off, int(w), entry); return; }
            }
        }
        if (g_out) std::fprintf(g_out, "name-scan: table not found\n");
    }

    std::string Name(uint8_t* base, uint32_t idx, uint32_t num, uint32_t probeIdx)
    {
        if (!g_names.tried) FindNameTable(base, probeIdx);
        char out[128];
        if (!g_names.ok || idx > 0x200000) { std::snprintf(out, sizeof(out), "<name %u_%u>", idx, num); return out; }
        uint32_t entry; if (!SafeLD32(base, g_names.data + idx * 4, entry) || !entry) { std::snprintf(out, sizeof(out), "<name %u_%u?>", idx, num); return out; }
        char raw[256]{}; if (!SafeRead(base, entry + g_names.off, raw, sizeof(raw) - 2)) { std::snprintf(out, sizeof(out), "<name %u_%u!>", idx, num); return out; }
        std::string s;
        if (g_names.wide) { for (int i = 0; i + 1 < int(sizeof(raw)); i += 2) { const char c = raw[i + 1]; if (!raw[i] && !c) break; s.push_back(raw[i] ? '?' : (c >= 32 && c < 127 ? c : '?')); if (s.size() > 96) break; } }
        else { for (int i = 0; raw[i] && i < 96; i++) s.push_back(raw[i] >= 32 && raw[i] < 127 ? raw[i] : '?'); }
        if (num) { std::snprintf(out, sizeof(out), "_%u", num - 1); s += out; }
        return s;
    }

    std::string ObjectName(uint8_t* base, uint32_t obj, uint32_t probeIdx)
    {
        uint32_t idx, num, cls, clsIdx = 0, clsNum = 0;
        if (!obj || !SafeLD32(base, obj + 0x2C, idx) || !SafeLD32(base, obj + 0x30, num) || !SafeLD32(base, obj + 0x34, cls)) return "<unreadable>";
        std::string s = Name(base, idx, num, probeIdx);
        if (cls && SafeLD32(base, cls + 0x2C, clsIdx) && SafeLD32(base, cls + 0x30, clsNum)) s = Name(base, clsIdx, clsNum, probeIdx) + " " + s;
        return s;
    }

    void PrintBt(const uint32_t* bt) { for (int i = 0; i < kDepth && bt[i]; i++) std::fprintf(g_out, " %08x", bt[i]); }

    void PrintFree(const FreeRec& f, const char* tag, uint32_t probeIdx, uint8_t* base)
    {
        std::fprintf(g_out, "  %s seq=%llu t=%.4f swap=%u tid=%x block=%08x size=%#x flags=%016llx lr=%08x name=%s outer=%08x(%s flags=%016llx) class=%08x w10=%08x w14=%08x w3c=%08x w4c=%08x bt=",
            tag, (unsigned long long)f.seq, Sec(f.qpc), f.swap, f.tid, f.block, f.size, (unsigned long long)f.flags, f.lr,
            Name(base, f.nameIdx, f.nameNum, probeIdx).c_str(), f.outer, f.outer ? Name(base, f.outerNameIdx, f.outerNameNum, probeIdx).c_str() : "-", (unsigned long long)f.outerFlags,
            f.cls, f.w10, f.w14, f.w3c, f.w4c);
        PrintBt(f.bt); std::fprintf(g_out, "\n");
    }

    void Dump(uint8_t* base, uint32_t a, uint32_t n)
    {
        for (uint32_t o = 0; o < n; o += 32)
        {
            std::fprintf(g_out, "    +%03x:", o);
            for (uint32_t j = 0; j < 32 && o + j < n; j += 4) { uint32_t v; std::fprintf(g_out, SafeLD32(base, a + o + j, v) ? " %08x" : " ????????", v); }
            std::fprintf(g_out, "\n");
        }
    }


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
    // Armed delay: after the Nth SetSkeletalMesh event (default 2 = the hand-in mesh swap) apply a
    // much larger per-draw delay for a few seconds so the rendering thread cannot finish the frame
    // in flight before the game thread's garbage collector purges the old materials.
    std::atomic<uint64_t> g_armedUntil{0}; std::atomic<uint32_t> g_swapEvents{0};
    uint32_t EnvU32(const char* name, uint32_t def) { const char* s = getenv(name); return s ? uint32_t(strtoul(s, nullptr, 10)) : def; }
    uint32_t ArmedDelayUs() { static const uint32_t v = EnvU32("LO_ISSUE12_RT_DELAY_ARMED_US", 0); return v; }
    void NoteSwapEvent()
    {
        static const uint32_t armOn = EnvU32("LO_ISSUE12_ARM_ON_SWAP", 2);
        static const uint32_t armSeconds = EnvU32("LO_ISSUE12_ARM_SECONDS", 3);
        if (++g_swapEvents == armOn && ArmedDelayUs()) g_armedUntil = Qpc() + uint64_t(armSeconds) * uint64_t(g_qpf.QuadPart);
    }
    uint32_t CurrentDelayUs() { const uint64_t until = g_armedUntil.load(std::memory_order_relaxed); return (until && Qpc() < until) ? ArmedDelayUs() : DelayUs(); }
    void SpinUs(uint32_t us) { if (!us) return; const uint64_t until = Qpc() + uint64_t(us) * uint64_t(g_qpf.QuadPart) / 1000000ull; while (Qpc() < until) _mm_pause(); }

    // Returns false when the draw should be skipped (stale material found and skip mode enabled).
    bool Validate(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t proxy = ctx.r3.u32;
        uint32_t data, num;
        if (!SafeLD32(base, proxy + 0x134, data) || !SafeLD32(base, proxy + 0x138, num) || !data || num > 16) return true;
        struct Stale { uint32_t lod, index, mat, w0, w4; } stale[64]; int staleN = 0; int total = 0;
        for (uint32_t l = 0; l < num; l++)
        {
            uint32_t iData, iNum;
            if (!SafeLD32(base, data + l * 12, iData) || !SafeLD32(base, data + l * 12 + 4, iNum) || !iData || iNum > 256) continue;
            for (uint32_t e = 0; e < iNum; e++)
            {
                uint32_t mat, w0 = 0, w4 = 0; total++;
                if (!SafeLD32(base, iData + e * 8, mat)) continue;
                bool bad = mat == 0 || (mat & 3) || !SafeLD32(base, mat, w0) || w0 < kCodeLo || w0 >= kCodeHi;
                if (bad && staleN < 64) { SafeLD32(base, mat + 4, w4); stale[staleN++] = { l, e, mat, w0, w4 }; }
            }
        }
        if (!staleN) return true;

        const uint32_t swap = g_presentedSwaps.load(std::memory_order_relaxed); const uint64_t now = Qpc();
        std::lock_guard lock(g_mutex);
        int slot = -1; for (int i = 0; i < g_staleN; i++) if (g_staleProxy[i] == proxy) slot = i;
        if (slot < 0 && g_staleN < 16) { slot = g_staleN++; g_staleProxy[slot] = proxy; g_staleCount[slot] = 0; }
        const uint32_t repeat = slot >= 0 ? ++g_staleCount[slot] : 0;
        const bool verbose = repeat == 1 || repeat == 2 || repeat == 10 || repeat == 100 || (repeat % 1000) == 0;
        const uint32_t probeIdx = [&] { for (size_t i = g_freeN; i-- > (g_freeN > kFreeRing ? g_freeN - kFreeRing : 0);) if (g_free[i % kFreeRing].block == stale[0].mat) return g_free[i % kFreeRing].nameIdx; return 0u; }();
        if (g_out)
        {
            std::fprintf(g_out, "\n### STALE MATERIAL in DrawDynamicElements proxy=%08x repeat=%u t=%.4f swap=%u tid=%x dpg=%u view=%08x pdi=%08x lods=%u entries=%u stale=%d purge_frees_total=%u\n",
                proxy, repeat, Sec(now), swap, unsigned(GetCurrentThreadId()), ctx.r6.u32, ctx.r5.u32, ctx.r4.u32, num, total, staleN, g_purgeFrees.load());
            for (int i = 0; i < staleN; i++) std::fprintf(g_out, "  lod=%u index=%u material=%08x [0]=%08x [4]=%08x\n", stale[i].lod, stale[i].index, stale[i].mat, stale[i].w0, stale[i].w4);
            if (verbose)
            {
                uint32_t bt[kDepth]; Backtrace(base, ctx.r1.u32, uint32_t(ctx.lr), bt); std::fprintf(g_out, "  draw-thread backtrace:"); PrintBt(bt); std::fprintf(g_out, "\n");
                std::fprintf(g_out, "  proxy bytes:\n"); Dump(base, proxy, 0x160);
                for (uint32_t off : { 0x104u, 0x108u, 0x10Cu, 0x110u, 0x114u, 0x118u, 0x11Cu, 0x120u })
                { uint32_t p, w; if (SafeLD32(base, proxy + off, p) && p && SafeLD32(base, p, w)) std::fprintf(g_out, "  proxy+%03x -> %08x first word %08x%s\n", off, p, w, (w >= kCodeLo && w < kCodeHi) ? "" : " (not a vtable)"); }
                for (uint32_t l = 0; l < num; l++)
                {
                    uint32_t iData, iNum; if (!SafeLD32(base, data + l * 12, iData) || !SafeLD32(base, data + l * 12 + 4, iNum)) continue;
                    std::fprintf(g_out, "  LOD %u records (%u):", l, iNum);
                    for (uint32_t e = 0; e < iNum && e < 64; e++) { uint32_t m, u, w = 0; SafeLD32(base, iData + e * 8, m); SafeLD32(base, iData + e * 8 + 4, u); SafeLD32(base, m, w); std::fprintf(g_out, " [%u]=%08x/%u(vt=%08x)", e, m, u, w); }
                    std::fprintf(g_out, "\n");
                }
                for (int i = 0; i < staleN; i++)
                {
                    const uint32_t mat = stale[i].mat; bool any = false;
                    std::fprintf(g_out, "  history for material %08x:\n", mat);
                    const size_t f0 = g_freeN > kFreeRing ? g_freeN - kFreeRing : 0;
                    long long lastFreeSeq = -1;
                    for (size_t k = g_freeN; k-- > f0;) { const FreeRec& f = g_free[k % kFreeRing]; if (f.block == mat) { PrintFree(f, "FREE ", probeIdx, base); std::fprintf(g_out, "    (freed %.4f s before this draw)\n", Sec(now) - Sec(f.qpc)); any = true; if (lastFreeSeq < 0) lastFreeSeq = (long long)f.seq; if (i == 0) { std::fprintf(g_out, "    resolved: %s  outer=%s\n", Name(base, f.nameIdx, f.nameNum, probeIdx).c_str(), ObjectName(base, f.outer, probeIdx).c_str()); std::fprintf(g_out, "    outer now:\n"); Dump(base, f.outer, 0x40); } break; } }
                    const size_t a0 = g_allocN > kAllocRing ? g_allocN - kAllocRing : 0;
                    for (size_t k = g_allocN; k-- > a0;) { const AllocRec& a = g_alloc[k % kAllocRing]; if (a.block == mat) { std::fprintf(g_out, "  ALLOC seq=%llu t=%.4f swap=%u tid=%x block=%08x size=%#x lr=%08x bt=", (unsigned long long)a.seq, Sec(a.qpc), a.swap, a.tid, a.block, a.size, a.lr); PrintBt(a.bt); std::fprintf(g_out, "\n"); any = true; break; } }
                    if (!any) std::fprintf(g_out, "  (no alloc/free record in ring)\n");
                    if (i == 0 && lastFreeSeq >= 0)
                    {
                        std::fprintf(g_out, "  purge/0xE0 frees around seq %lld (window -40..+40):\n", lastFreeSeq);
                        for (size_t k = f0; k < g_freeN; k++) { const FreeRec& f = g_free[k % kFreeRing]; if ((long long)f.seq >= lastFreeSeq - 40 && (long long)f.seq <= lastFreeSeq + 40) PrintFree(f, f.block == mat ? "FREE*" : "free ", probeIdx, base); }
                    }
                }
                std::fflush(g_out);
            }
            else std::fflush(g_out);
        }
        if (repeat == 1) LOG_ERROR("issue12 probe: stale material {:#x} in proxy {:#x} (lod {} index {}), see probe file", stale[0].mat, proxy, stale[0].lod, stale[0].index);
        static const bool skip = getenv("LO_ISSUE12_SKIP_STALE") && strcmp(getenv("LO_ISSUE12_SKIP_STALE"), "0") != 0;
        return !skip;
    }
}

PPC_FUNC(sub_82295B08)
{
    const uint32_t size = ctx.r4.u32, lr = uint32_t(ctx.lr), sp = ctx.r1.u32;
    __imp__sub_82295B08(ctx, base);
    if (size == 0xE0 && issue12::Enabled()) issue12::OnAlloc(base, ctx.r3.u32, size, lr, sp);
}

PPC_FUNC(sub_82298990)
{
    if (ctx.r4.u32 && issue12::Enabled()) issue12::OnFree(base, ctx.r3.u32, ctx.r4.u32, uint32_t(ctx.lr), ctx.r1.u32);
    __imp__sub_82298990(ctx, base);
}

PPC_FUNC(sub_823CB350)
{
    if (issue12::Enabled() && !issue12::Validate(ctx, base)) return;
    issue12::SpinUs(issue12::CurrentDelayUs());
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

// When another TU (gc_render_flush.cpp) owns the guest entry points, expose these as chain
// functions it calls after its own work instead of defining the hooks here.
#ifdef ISSUE12_EXTERNAL_GC_HOOKS
#define ISSUE12_GC_HOOK(name) void issue12_chain_##name(PPCContext& __restrict ctx, uint8_t* base)
#else
#define ISSUE12_GC_HOOK(name) PPC_FUNC(name)
#endif

ISSUE12_GC_HOOK(sub_8249A568) // CollectGarbage
{
    char d[160]; std::snprintf(d, sizeof(d), "keep=%08llx fullpurge=%u", (unsigned long long)ctx.r3.u64, ctx.r4.u32);
    if (issue12::Enabled()) issue12::Event(base, ctx, "CollectGarbage_enter", d);
    __imp__sub_8249A568(ctx, base);
    if (issue12::Enabled()) issue12::Event(base, ctx, "CollectGarbage_exit", d);
}

ISSUE12_GC_HOOK(sub_822FD0A8) // IncrementalPurgeGarbage
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
    if (issue12::Enabled()) { char d[320]; std::snprintf(d, sizeof(d), "comp=%08x %s newmesh=%08x %s flag=%u", ctx.r3.u32, issue12::ObjName(base, ctx.r3.u32).c_str(), ctx.r4.u32, issue12::ObjName(base, ctx.r4.u32).c_str(), ctx.r5.u32); issue12::Event(base, ctx, "SetSkeletalMesh", d); issue12::NoteSwapEvent(); }
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
}
