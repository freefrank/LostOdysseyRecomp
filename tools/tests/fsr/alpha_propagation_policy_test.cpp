#include "gpu/fsr_alpha_propagation_gpu.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

using namespace gpu::fsr_alpha;

static void Check(bool value, const char* description) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}

int main() {
    Check(Contains({0, 0, 432, 242}, {0, 0, 428, 240}), "428 crop within copied 432 region");
    Check(!Contains({0, 0, 420, 242}, {0, 0, 428, 240}), "unwritten fetch pixels unavailable");
    Check(!Contains({16, 0, 432, 242}, {0, 0, 428, 240}), "partial offset unavailable");
    Check(!Contains({0, 0, 448, 242}, {0, 0, 0, 240}), "empty crop unavailable");
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
    owner.PublishRaw(raw);
    Check(owner.InvalidateRawSource(9), "next frame can publish a new raw source");
    std::puts("PASS: FSR alpha resolve/fetch policy checks");
}
