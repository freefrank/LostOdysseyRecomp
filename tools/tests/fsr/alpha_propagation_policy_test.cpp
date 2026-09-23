#include "gpu/fsr_alpha_propagation_gpu.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

using namespace gpu::fsr_alpha;

struct DummyTexture final : plume::RenderTexture {
    std::unique_ptr<plume::RenderTextureView> createTextureView(
        const plume::RenderTextureViewDesc&) const override { return nullptr; }
    void setName(const std::string&) override {}
};

static void Check(bool value, const char* description) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}

int main() {
    Check(AuditedPostprocessPair(0x2f6bbed8149a7804ull, 0x7c260eacff1d681dull) &&
        AuditedPostprocessPair(0x2f6bbed8149a7804ull, 0x53dd5d081c7945cfull) &&
        AuditedPostprocessPair(0xd1b241c74103b6bfull, 0xee90000c755c0472ull) &&
        AuditedPostprocessPair(0x9b81c55ca39bb529ull, 0xb4b4d54a7a2d6b96ull) &&
        !AuditedPostprocessPair(0x2f6bbed8149a7804ull, 0xb4b4d54a7a2d6b96ull),
        "only the four captured VS/PS postprocess pairings are eligible");
    Check(Contains({0, 0, 432, 242}, {0, 0, 428, 240}), "428 crop within copied 432 region");
    Check(!Contains({0, 0, 420, 242}, {0, 0, 428, 240}), "unwritten fetch pixels unavailable");
    Check(!Contains({16, 0, 432, 242}, {0, 0, 428, 240}), "partial offset unavailable");
    Check(!Contains({0, 0, 448, 242}, {0, 0, 0, 240}), "empty crop unavailable");
    const MaskRect intersection = IntersectCopyValidRect({0, 0, 285, 161}, {0, 0, 288, 161});
    Check(intersection == MaskRect{0, 0, 285, 161}, "resolve copies only proven portion of actual copy");
    Check(Contains(intersection, {0, 0, 285, 161}), "consumer crop inside proven intersection valid");
    Check(!Contains(intersection, {0, 0, 286, 161}), "padding outside intersection invalid");
    Check(IntersectCopyValidRect({10, 20, 30, 40}, {25, 30, 20, 15}) == MaskRect{25, 30, 15, 15},
        "offset intersection preserves source and destination coordinates");
    Check(IntersectCopyValidRect({0, 0, 285, 161}, {286, 0, 12, 161}) == MaskRect{},
        "disjoint source and actual copy have no valid pixels");
    Check(std::strcmp(PostprocessGuardReason(false, false, false, false, false, false, false, false),
        "input_unavailable") == 0, "missing input is not mislabeled sampler unavailable");
    Check(std::strcmp(PostprocessGuardReason(true, false, false, false, false, false, false, false),
        "quad_unavailable") == 0, "bad quad is not mislabeled sampler unavailable");
    Check(std::strcmp(PostprocessGuardReason(true, true, false, false, false, false, false, false),
        "sampler_unavailable") == 0, "sampler reject reports real sampler guard");
    Check(std::strcmp(PostprocessGuardReason(true, true, true, false, false, false, false, false),
        "available") == 0, "complete guarded postprocess inputs available");
    Check(std::strcmp(PostprocessGuardReason(true, false, true, false, false, false, false, false,
        true, false), "clear_background_unavailable") == 0,
        "valid inset without a proven clear reports missing background");
    Check(std::strcmp(PostprocessGuardReason(true, true, true, false, false, false, false, false,
        true, true), "available") == 0,
        "clear and inset can satisfy the geometry guard together");
    constexpr uint64_t townVs = 0x4c87bb5b986defc8ull;
    constexpr uint64_t townOverPs = 0x03b446965d7d52b3ull;
    Check(RetainsRawAfterAuditedLocalBlend(townVs, townOverPs,
        0x01000106u, 0xF, 12, true, 0, false),
        "audited FP16 additive RGB preserves prior color and alpha");
    for (const uint64_t ps : {townOverPs, 0x342877796a1673e6ull,
        0x9791230246c5bc3cull})
        Check(RetainsRawAfterAuditedLocalBlend(townVs, ps,
            0x01000706u, 0xF, 12, true, 0, false),
            "three captured source-over PS retain a partial raw upper bound");
    Check(!RetainsRawAfterAuditedLocalBlend(townVs, 0x1111,
        0x01000706u, 0xF, 12, true, 0, false),
        "unreviewed source-over PS unavailable");
    Check(!RetainsRawAfterAuditedLocalBlend(0x1111, townOverPs,
        0x01000706u, 0xF, 12, true, 0, false),
        "source-over PS with another VS unavailable");
    Check(!RetainsRawAfterAuditedLocalBlend(townVs, townOverPs,
        0x01000106u, 0xF, 12, false, 0, false),
        "unknown target format is not the audited FP16 contract");
    Check(!RetainsRawAfterAuditedLocalBlend(townVs, townOverPs,
        0x01000106u, 0xF, 10, true, 0, false),
        "another guest format is not the audited cfmt12 contract");
    Check(!RetainsRawAfterAuditedLocalBlend(townVs, townOverPs,
        0x01000106u, 0xF, 12, true, 2, false),
        "debug pixel override rejects local blend retention");
    Check(!RetainsRawAfterAuditedLocalBlend(townVs, townOverPs,
        0x01000106u, 0xF, 12, true, 0, true),
        "forced no-blend invalidates the additive contract");
    Check(!RetainsRawAfterAuditedLocalBlend(townVs, townOverPs,
        0x01000106u, 0x8, 12, true, 0, false),
        "alpha-only draw is not an RGB writer");

    const CopyReuseSignature old{12000, 7369, 9, 17, 853, 480,
        {0, 0, 853, 480}, 9, 11, 14};
    auto next = old;
    Check(next == old, "new color ordinal may reuse exactly equal copy signature");
    ++next.rawDraws;
    Check(!(next == old), "new audited draw invalidates copy reuse");
    next = old; ++next.destinationAllocation;
    Check(!(next == old), "destination allocation replacement invalidates copy reuse");
    next = old; ++next.sourceAllocation;
    Check(!(next == old), "source allocation replacement invalidates copy reuse");
    next = old; ++next.frame;
    Check(!(next == old), "cross-frame copy reuse unavailable");
    next = old; ++next.epoch;
    Check(!(next == old), "epoch change invalidates copy reuse");
    next = old; --next.rect.width;
    Check(!(next == old), "partial rectangle invalidates copy reuse");

    // These calls exercise the runtime owner with no GPU objects. The frame
    // guard must return before attempting to use the null command list.
    PropagationGPU owner(nullptr);
    owner.BeginFrame(12000, 7369);
    owner.MarkUnsupportedPostprocess(9);
    Check(owner.HasEvidence(), "postprocess state recorded in current frame");
    owner.Invalidate(); // The renderer does this when FSR is switched Off.
    Check(!owner.HasEvidence(), "FSR Off clears the owner state");
    std::vector<std::shared_ptr<MaskLease>> uses;
    const auto staleResolve = owner.RecordResolve(nullptr, 12001, 7369, 9,
        853, 480, 0x1000, 6, 17, 1, 853, 480, {0, 0, 853, 480}, "copy", uses);
    Check(std::strcmp(staleResolve.reason, "owner_frame_epoch_mismatch") == 0,
        "stale resolve rejected before GPU commands");
    const auto staleFetch = owner.RecordFetchView(nullptr, 12001, 7369,
        0x1000, 6, 17, 1, nullptr, 853, 480, uses);
    Check(std::strcmp(staleFetch.reason, "owner_frame_epoch_mismatch") == 0,
        "stale fetch rejected before GPU commands");
    owner.BeginFrame(12001, 7370);
    Check(!owner.HasEvidence(), "new epoch cannot restore stale state");
    const auto emptyFetch = owner.RecordFetchView(nullptr, 12001, 7370,
        0x1000, 6, 17, 1, nullptr, 853, 480, uses);
    Check(std::strcmp(emptyFetch.reason, "resolved_version_unavailable") == 0,
        "new frame has no old resolved version");

    // A non-owning sentinel suffices here: the owner only compares/retains
    // this view and never dereferences its texture in these CPU-only calls.
    FrameView raw{};
    raw.coverage = Coverage::PartialCoverage;
    raw.identity.renderFrame = 12001;
    raw.identity.geometryEpoch = 7370;
    raw.identity.colorAllocation = 9;
    raw.identity.width = 853;
    raw.identity.height = 480;
    raw.lease = std::make_shared<FrameLease>();
    raw.texture = reinterpret_cast<plume::RenderTexture*>(uintptr_t(1));
    owner.PublishRaw(raw);
    Check(owner.InvalidateRawSource(9), "RGB writer invalidates a published raw source");
    Check(!owner.InvalidateRawSource(9), "same RGB writer cannot re-invalidate missing raw");
    owner.PublishRaw(raw);
    Check(!owner.InvalidateRawSource(9), "raw source remains blocked after RGB writer");
    owner.BeginFrame(12002, 7370);
    raw.identity.renderFrame = 12002;
    auto stageLease = std::make_shared<MaskLease>();
    stageLease->texture = std::make_unique<DummyTexture>();
    SourceMask stage{};
    stage.frame = 12002; stage.epoch = 7370; stage.colorAllocation = 9;
    stage.revision = 18; stage.width = 853; stage.height = 480;
    stage.validRect = {0, 0, 853, 480}; stage.stage = SourceStage::Dof;
    stage.mask = stageLease;
    owner.PublishPostprocess(stage);
    Check(owner.CurrentSource(9, 12002, 7370) != nullptr,
        "postprocess source becomes current for its color allocation");
    owner.PublishRaw(raw);
    Check(owner.CurrentSource(9, 12002, 7370) == nullptr && owner.HasRawSource(9),
        "later audited raw producer supersedes prior postprocess stage");
    owner.PublishRaw(raw);
    Check(owner.InvalidateRawSource(9), "next frame can publish a new raw source");

    owner.BeginFrame(12003, 7371);
    const MaskRect published{0, 0, 8, 8}, written{1, 1, 6, 6};
    Check(!QualifiesInsetReplacement(owner.RecentClear(9), 12003, 7371, 9, 8, 8,
        20, published, written), "no clear cannot qualify an inset");
    Check(!owner.RecordFullColorClear(12003, 7371, 9, 8, 8, 20,
        {0, 0, 7, 8}, "depth_color_tile_clear"),
        "partial clear is not a whole-attachment background");
    Check(owner.RecordFullColorClear(12003, 7371, 9, 8, 8, 20,
        {0, 0, 8, 8}, "depth_color_tile_clear"), "actual full clear recorded");
    auto* clear = owner.RecentClear(9);
    Check(QualifiesInsetReplacement(clear, 12003, 7371, 9, 8, 8,
        20, published, written), "clear then inset proves full published rectangle");
    Check(!QualifiesInsetReplacement(clear, 12002, 7371, 9, 8, 8,
        20, published, written) &&
        !QualifiesInsetReplacement(clear, 12003, 7372, 9, 8, 8,
            20, published, written) &&
        !QualifiesInsetReplacement(clear, 12003, 7371, 10, 8, 8,
            20, published, written) &&
        !QualifiesInsetReplacement(clear, 12003, 7371, 9, 9, 8,
            20, published, written),
        "clear frame, epoch, allocation and extent must all match");
    owner.InvalidateClearBackground(9, "unknown_rgb_writer");
    Check(clear->invalidatedBy && std::strcmp(clear->invalidatedBy, "unknown_rgb_writer") == 0 &&
        !QualifiesInsetReplacement(clear, 12003, 7371, 9, 8, 8, 21, published, written),
        "unknown RGB writer invalidates prior clear with reason");
    Check(owner.RecordFullColorClear(12003, 7371, 9, 8, 8, 22,
        {0, 0, 8, 8}, "depth_color_tile_clear"), "later real clear resets invalidation");
    auto olderLease = std::make_shared<MaskLease>();
    olderLease->texture = std::make_unique<DummyTexture>();
    SourceMask older{};
    older.frame = 12003; older.epoch = 7371; older.colorAllocation = 9;
    older.revision = 23; older.width = 8; older.height = 8;
    older.validRect = published; older.stage = SourceStage::Downsample;
    older.mask = olderLease;
    owner.PublishPostprocess(older);
    Check(owner.CurrentSource(9, 12003, 7371) != nullptr &&
        !QualifiesInsetReplacement(owner.RecentClear(9), 12003, 7371, 9, 8, 8,
            24, published, written), "published postprocess write consumes the clear proof");
    const SourceMask oldSnapshot = *owner.CurrentSource(9, 12003, 7371);
    Check(owner.RecordFullColorClear(12003, 7371, 9, 8, 8, 24,
        {0, 0, 8, 8}, "depth_color_tile_clear"), "later clear replaces current source");
    Check(owner.CurrentSource(9, 12003, 7371) == nullptr &&
        oldSnapshot.mask == olderLease && oldSnapshot.validRect == published &&
        oldSnapshot.mask->texture.get() == olderLease->texture.get(),
        "later clear cannot mutate an earlier snapshot lease or valid rectangle");
    owner.BeginFrame(12004, 7371);
    Check(owner.RecentClear(9) == nullptr, "new frame drops clear background proof");

    FrameView replay = raw;
    replay.identity.geometryEpoch = 7371;
    replay.identity.width = replay.identity.height = 8;
    owner.BeginFrame(12005, 7371);
    replay.identity.renderFrame = 12005;
    Check(owner.RecordFullColorClear(12005, 7371, 9, 8, 8, 1,
        {0, 0, 8, 8}, "depth_color_tile_clear"), "first full clear recorded without a raw source");
    owner.PublishRaw(replay);
    Check(owner.HasRawSource(9) && owner.RecentClear(9)->invalidatedBy &&
        std::strcmp(owner.RecentClear(9)->invalidatedBy, "raw_draw") == 0,
        "first audited raw after a first clear is accepted and consumes the clear proof");

    owner.BeginFrame(12006, 7371);
    replay.identity.renderFrame = 12006;
    owner.PublishRaw(replay);
    Check(owner.HasRawSource(9), "previous raw replay exists before clear");
    Check(owner.RecordFullColorClear(12006, 7371, 9, 8, 8, 2,
        {0, 0, 8, 8}, "depth_color_tile_clear") && !owner.HasRawSource(9),
        "full clear invalidates an existing raw source");
    owner.PublishRaw(replay);
    Check(!owner.HasRawSource(9), "old cumulative raw replay cannot be republished after clear");

    owner.BeginFrame(12007, 7371);
    replay.identity.renderFrame = 12007;
    owner.PublishRaw(replay);
    Check(owner.InvalidateRawSource(9), "unknown RGB writer blocks the old raw source");
    Check(owner.RecordFullColorClear(12007, 7371, 9, 8, 8, 3,
        {0, 0, 8, 8}, "depth_color_tile_clear"), "clear is still recorded after unknown writer");
    owner.PublishRaw(replay);
    Check(!owner.HasRawSource(9), "clear cannot lift the prior unknown-writer raw replay rejection");
    std::puts("PASS: FSR alpha resolve/fetch policy checks");
}
