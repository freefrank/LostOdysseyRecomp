#include <stdafx.h>
#include <array>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <os/logger.h>
#include <gpu/renderer.h>
#include <gpu/occlusion_history.h>
#include "occlusion_history.h"

// Policy::History (LO_ZPD_MODE=history): the game's deferred occlusion reads,
// answered without waiting for the host GPU (gpu/occlusion_history.h).
//
// Guest facts (docs/notes/occlusion-queries.md):
// - 0x833180B4 selects the UE3 deferred path; the game only ever stores 0
//   there (827C94EC). sub_823CBF68 then keeps the queries in the view state's
//   maps (primitive component, projected shadow key) and sub_823C0910 reads
//   them through sub_823CE960 at the start of the next frame.
// - Those reads call sub_823CF390 (a GetData loop with no timeout) from
//   823CEC44 (primitives: r31 = UPrimitiveComponent*) and 823CEACC (projected
//   shadows: r30 light component, r28 primitive, r29 a flag bit).
// - sub_823BAB50 renders a scene; FSceneRenderer::Render calls it at 825B2AA4
//   for the main view, scene captures from elsewhere. All of them share the
//   query pool at [0x83235AB8].
// - The sun flare (sub_823D5430) still allocates and reads its query in the
//   same frame; it keeps its Fast answer (owner caller 0x823D5688).

extern "C" PPC_FUNC(__imp__sub_823BAB50);
extern "C" PPC_FUNC(__imp__sub_823CE960);
extern "C" PPC_FUNC(__imp__sub_823CF3F0);

namespace
{
constexpr uint32_t kDeferredOcclusionFlag = 0x833180B4;
constexpr uint32_t kOcclusionEnabled = 0x83235AB4;   // the game's own switch (normally 1)
constexpr uint32_t kQueryPool = 0x83235AB8;          // pool: +0x18 objects, +0x20 next index
constexpr uint32_t kMainSceneRender = 0x825B2AA8;  // return address in FSceneRenderer::Render
constexpr uint32_t kPrimitiveRead = 0x823CEC48;    // return address of the read at 823CEC44
constexpr uint32_t kShadowRead = 0x823CEAD0;       // return address of the read at 823CEACC
constexpr uint32_t kSunFlareAllocation = 0x823D5688;
constexpr uint32_t kViewState = 4;                 // FViewInfo -> FSceneViewState*
constexpr uint32_t kViewMatrix = 64;               // FViewInfo: 4x4 view matrix

// Hysteresis 4: an object measured visible within its last four counts stays
// drawn. On psvita it cut the objects drawn late during camera motion by about
// 40% for about 2% more draws.
gpu::occlusion::HistoryAnswers g_answers{4};
bool g_inMainRender = false;
std::unordered_map<uint32_t, std::array<float, 16>> g_lastView; // view state -> last view matrix
std::vector<std::pair<uint64_t, uint32_t>> g_completed;

bool Enabled() { return gpu::renderer::OcclusionHistoryEnabled(); }

void LogStats(uint8_t* base)
{
    const auto& s = g_answers.GetStats();
    const uint32_t pool = PPC_LOAD_U32(kQueryPool);
    LOG_INFO("occlusion history f{} reads={} ready={} measured={} zero={} unknown={} stale={} cut={} gap={} hysteresis={} capture={} pool={}",
        g_answers.Frame(), s.reads, s.ready, s.measured, s.zero, s.unknown, s.stale, s.cut, s.gap, s.hysteresis, s.capture,
        pool ? PPC_LOAD_U32(pool + 0x18) : 0);
}
}

uint64_t occlusion_history::IssueOwner(uint32_t query, uint32_t caller)
{
    if (!Enabled() || caller == kSunFlareAllocation) return 0;
    return g_answers.Issue(query, caller, g_inMainRender);
}

bool occlusion_history::Read(PPCContext& ctx, uint8_t* base)
{
    const uint32_t lr = uint32_t(ctx.lr);
    if ((lr != kPrimitiveRead && lr != kShadowRead) || !Enabled()) return false;
    const bool primitive = lr == kPrimitiveRead;
    const uint64_t key = primitive ? uint64_t(ctx.r31.u32)
        : (uint64_t(ctx.r30.u32) << 32) | (ctx.r28.u32 + ctx.r29.u32);
    const uint32_t query = ctx.r3.u32, out = ctx.r4.u32;
    // One GetData call, as the loop's first (size 4, D3DGETDATA_FLUSH). Not the
    // hooked sub_823CF3F0: its not-ready result asks the command processor to
    // wait for the GPU.
    const auto r3 = ctx.r3, r4 = ctx.r4, r5 = ctx.r5, r6 = ctx.r6;
    const uint64_t lrSaved = ctx.lr;
    ctx.r5.u64 = 4;
    ctx.r6.u64 = 1;
    __imp__sub_823CF3F0(ctx, base);
    const int32_t result = ctx.r3.s32;
    ctx.r3 = r3; ctx.r4 = r4; ctx.r5 = r5; ctx.r6 = r6; ctx.lr = lrSaved;
    const bool ready = result != 1 && result >= 0;
    const uint32_t answer = g_answers.Read(primitive ? gpu::occlusion::HistoryAnswers::Kind::Primitive
                                                     : gpu::occlusion::HistoryAnswers::Kind::Shadow,
        key, query, g_inMainRender, ready, ready ? PPC_LOAD_U32(out) : 0);
    PPC_STORE_U32(out, answer);
    ctx.r3.u64 = 1; // what the loop returns once GetData succeeds
    return true;
}

// Scene render: sets the deferred flag; the main one starts a frame.
PPC_FUNC(sub_823BAB50)
{
    if (!Enabled()) {
        __imp__sub_823BAB50(ctx, base);
        return;
    }
    PPC_STORE_U32(kDeferredOcclusionFlag, 1);
    const bool main = uint32_t(ctx.lr) == kMainSceneRender;
    if (main) {
        g_answers.BeginFrame();
        if (g_answers.Frame() % 600 == 0) LogStats(base);
        // With the flag the game restarts its query pool only in a scene pass
        // that tests occlusion (after reading last frame's queries, 823C0A20),
        // while the sun flare allocates in every main render: a stretch without
        // such a pass would grow the pool by an object a frame. Restart it here
        // as the same-frame path does (823BABE0). Nothing allocates between
        // this point and the deferred read.
        if (PPC_LOAD_U32(kOcclusionEnabled))
            if (const uint32_t pool = PPC_LOAD_U32(kQueryPool)) PPC_STORE_U32(pool + 0x20, 0);
    }
    const bool outer = g_inMainRender;
    g_inMainRender = main;
    __imp__sub_823BAB50(ctx, base);
    g_inMainRender = outer;
}

// Query result pass of one view (r4 = FViewInfo): collects the host's counts
// and checks the main view for a camera cut before the game reads.
PPC_FUNC(sub_823CE960)
{
    const uint32_t view = ctx.r4.u32;
    if (Enabled() && g_inMainRender && view) {
        g_completed.clear();
        gpu::renderer::TakeOcclusionCompletions(g_completed);
        for (const auto& [owner, samples] : g_completed) g_answers.Completed(owner, samples);
        std::array<float, 16> matrix;
        for (uint32_t i = 0; i < 16; ++i) {
            const uint32_t word = PPC_LOAD_U32(view + kViewMatrix + 4 * i);
            std::memcpy(&matrix[i], &word, sizeof(word));
        }
        const auto [last, first] = g_lastView.try_emplace(PPC_LOAD_U32(view + kViewState), matrix);
        if (first || gpu::occlusion::ViewCut(last->second.data(), matrix.data())) g_answers.Cut();
        last->second = matrix;
    }
    __imp__sub_823CE960(ctx, base);
}
