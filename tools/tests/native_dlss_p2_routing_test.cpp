#include "gpu/frame_plan.h"
#include "gpu/temporal_frame_inputs.h"
#include "gpu/color_qualification.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void Require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

gpu::upscaling::OutputSizing ReadySizing() {
    gpu::upscaling::OutputSizing sizing;
    sizing.key = {7, 1920, 1080};
    for (auto& mode : sizing.modes) {
        mode.state = gpu::upscaling::SizingState::Ready;
        mode.optimal = {960, 540};
    }
    return sizing;
}

gpu::frame_plan::PlannerInput Input(const gpu::upscaling::OutputSizing& sizing, bool probe, bool readback = false,
    gpu::upscaling::Upscaler upscaler = gpu::upscaling::Upscaler::Dlss) {
    gpu::frame_plan::PlannerInput input;
    input.internalResolution = 2;
    input.antialiasing = 3;
    input.upscaler = upscaler;
    input.quality = gpu::upscaling::DlssQuality::Quality;
    input.output = {{1920, 1080}, 0, 0, 1920, 1080};
    input.device = {gpu::backend::Backend::Vulkan, 7, true, true};
    input.sizing = &sizing;
    input.inputProbeRequested = probe;
    input.readback = readback;
    return input;
}

void TestColorQualificationPolicy() {
    using namespace gpu::color_qualification;
    uint64_t producerFrame = ~0ull;
    uint32_t qW = 0, qH = 0;
    Require(!IsHostTextureQualified(producerFrame, 100), "initial texture is unqualified");
    MarkHostTextureProducer(producerFrame, qW, qH, 100, 1707, 960);
    Require(IsHostTextureQualified(producerFrame, 100), "marked producer is qualified for its frame");
    Require(!IsHostTextureQualified(producerFrame, 101), "frame mismatch invalidates producer qualification");

    // Alpha-only or depth-only draw preservation
    OnDrawWriteHostTexture(producerFrame, qW, qH, 0); // mask 0
    Require(IsHostTextureQualified(producerFrame, 100), "mask 0 does not invalidate producer");
    OnDrawWriteHostTexture(producerFrame, qW, qH, 8); // alpha-only write (bit 3 set, bits 0..2 clear)
    Require(IsHostTextureQualified(producerFrame, 100), "alpha-only write does not invalidate producer");

    // RGB draw modification
    OnDrawWriteHostTexture(producerFrame, qW, qH, 1); // Red write
    Require(!IsHostTextureQualified(producerFrame, 100), "RGB modification invalidates producer");

    // ResolvedSurface helper
    uint64_t sdrWriteOrdinal = 0;
    Require(sdrWriteOrdinal == 0, "initial sdrWriteOrdinal is 0");
    MarkSurfaceResolved(sdrWriteOrdinal, 500);
    Require(sdrWriteOrdinal == 500, "marked resolved ordinal is 500");
    InvalidateSurfaceResolved(sdrWriteOrdinal);
    Require(sdrWriteOrdinal == 0, "invalidated sdrWriteOrdinal is 0");

    // CheckProducerPipeline cases
    ProducerPipelineCheck validPipe{
        .vs = kTonemapVS,
        .ps = kTonemapPS,
        .c10xBits = kTonemapC10XBits,
        .colorMask = 7, // RGB write
        .blend = 1 | (0 << 5) | (0 << 8), // ONE / ZERO / ADD
        .depthControl = 0,
        .modeCull = 0,
        .colorControl = 0,
        .guestTargetFormat = 0,
        .targetExpBias = 0,
        .vtxFmt = 4,
        .sharedFlags = 0,
        .debugOverrides = false
    };
    Require(CheckProducerPipeline(validPipe) == ProducerRejectReason::None, "exact captured tonemap pipeline qualifies");

    auto invalidC10 = validPipe;
    invalidC10.c10xBits = 0x3f800000; // 1.0f
    Require(CheckProducerPipeline(invalidC10) == ProducerRejectReason::C10Mismatch, "c10 mismatch rejects producer");

    auto invalidBlend = validPipe;
    invalidBlend.blend = 0; // ZERO / ZERO / ADD
    Require(CheckProducerPipeline(invalidBlend) == ProducerRejectReason::BlendMismatch, "blend mismatch rejects producer");

    auto invalidTargetFmt = validPipe;
    invalidTargetFmt.guestTargetFormat = 6; // UNORM instead of FP16
    Require(CheckProducerPipeline(invalidTargetFmt) == ProducerRejectReason::TargetFormatMismatch, "target format mismatch rejects producer");

    auto invalidFlags = validPipe;
    invalidFlags.sharedFlags = 8; // vs raw/no VTE
    Require(CheckProducerPipeline(invalidFlags) == ProducerRejectReason::SharedFlagsRejected, "shared.flags bit 8 rejects producer");

    // Geometric quad check in physical domain
    // 6 vertices of full quad in NDC: [-1, -1] to [1, 1], z=0, w=1
    // Tri 1: (-1,-1), (1,-1), (-1, 1)
    // Tri 2: (1,-1), (1, 1), (-1, 1)
    float ndcScale[3] = {1.0f, 1.0f, 1.0f};
    float ndcOffset[3] = {0.0f, 0.0f, 0.0f};
    float halfPixel[2] = {0.0f, 0.0f};
    float posQuad[24] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };
    // 1440p Quality physical input 1707x960 (guest 1280x720) positive case
    auto quadRes1440 = CheckQuadCoverage(posQuad, 0.0f, 0.0f, 1707.0f, 960.0f,
        0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel);
    Require(quadRes1440.ok, "1707x960 physical quad coverage succeeds");

    // 853x480 padded allocation negative case (e.g. scissor/allocation padded to 864x480)
    auto quadPadded = CheckQuadCoverage(posQuad, 0.0f, 0.0f, 853.0f, 480.0f,
        0, 0, 840, 480, ndcScale, ndcOffset, halfPixel);
    Require(!quadPadded.ok && quadPadded.rejectReason == ProducerRejectReason::ScissorMismatch,
        "scissor mismatch on padded allocation rejects producer");

    // P2 has a distinct contract: real fractional viewport, with only fully
    // covered integer pixel centers (and no padded allocation) published.
    const auto postQuad = [&](const float* p, float w, float h, int right, int bottom) {
        return CheckPostprocessQuadCoverage(std::span<const float, 24>(p, 24),
            0.0f, 0.0f, w, h, 0, 0, right, bottom,
            ndcScale, ndcOffset, halfPixel, 320, 320);
    };
    const auto fractional = postQuad(posQuad, 285.221875f, 161.333333f, 285, 161);
    Require(fractional.ok && fractional.coveredWidth == 285 && fractional.coveredHeight == 161,
        "fractional P2 viewport proves only 285x161 pixel centers");
    Require(!CheckQuadCoverage(posQuad, 0, 0, 285.221875f, 161.333333f,
        0, 0, 285, 161, ndcScale, ndcOffset, halfPixel).ok,
        "SDR integer-viewport contract remains unchanged");
    const auto clipped = postQuad(posQuad, 285.8f, 161.8f, 285, 161);
    Require(clipped.ok && clipped.coveredWidth == 285 && clipped.coveredHeight == 161,
        "fractional tail outside the scissor is not published");
    const auto smallRect = CheckPostprocessQuadCoverage(posQuad, 0, 0, 2.2f, 2.2f,
        0, 0, 2, 2, ndcScale, ndcOffset, halfPixel, 2, 2);
    Require(smallRect.ok && smallRect.coveredWidth == 2 && smallRect.coveredHeight == 2,
        "P2 exact rectangular quad covers both pixel centers per axis");

    // The old 1e-3f corner tolerance merges these near-corners, but the two
    // triangle diagonal edges straddle pixel center (1.5, 0.5).
    constexpr float delta = 0.0005f;
    const float seamPhysical[12] = {0, 0, 2 - delta, 0, 0, 2 - delta,
        2, delta, 2, 2, delta, 2};
    float seamQuad[24]{};
    for (unsigned i = 0; i < 6; ++i) {
        seamQuad[4 * i] = seamPhysical[2 * i] / 1.1f - 1.0f;
        seamQuad[4 * i + 1] = 1.0f - seamPhysical[2 * i + 1] / 1.1f;
        seamQuad[4 * i + 3] = 1.0f;
    }
    const auto oldToleranceSeam = CheckQuadCoveragePixels(seamQuad, 0, 0, 2.2f, 2.2f,
        0, 0, 2, 2, ndcScale, ndcOffset, halfPixel, 2, 2, false);
    Require(oldToleranceSeam.ok, "former corner tolerance accepts adversarial shared-edge crack");
    const auto seam = CheckPostprocessQuadCoverage(seamQuad, 0, 0, 2.2f, 2.2f,
        0, 0, 2, 2, ndcScale, ndcOffset, halfPixel, 2, 2);
    Require(!seam.ok && seam.rejectReason == ProducerRejectReason::TopologyInvalid,
        "P2 exact corners reject sub-epsilon diagonal crack");
    float nearRectangle[24];
    std::copy_n(posQuad, 24, nearRectangle);
    // Same two right-bottom vertices shift together: no shared-corner gap,
    // but the bottom edge is slanted instead of an exact rectangular edge.
    nearRectangle[5] -= delta / 1.1f;
    nearRectangle[13] = nearRectangle[5];
    Require(CheckQuadCoveragePixels(nearRectangle, 0, 0, 2.2f, 2.2f,
        0, 0, 2, 2, ndcScale, ndcOffset, halfPixel, 2, 2, false).ok,
        "former tolerance accepts almost rectangular slanted edge");
    const auto approximate = CheckPostprocessQuadCoverage(nearRectangle, 0, 0, 2.2f, 2.2f,
        0, 0, 2, 2, ndcScale, ndcOffset, halfPixel, 2, 2);
    Require(!approximate.ok && approximate.rejectReason == ProducerRejectReason::TopologyInvalid,
        "P2 rejects nearly rectangular slanted edge below old epsilon");
    float insetQuad[24];
    std::copy_n(posQuad, 24, insetQuad);
    for (unsigned i = 0; i < 6; ++i) {
        insetQuad[4 * i] = posQuad[4 * i] < 0 ? -0.6875f : 0.6875f;
        insetQuad[4 * i + 1] = posQuad[4 * i + 1] < 0 ? -0.6875f : 0.6875f;
    }
    const auto insetAsFull = CheckPostprocessQuadCoverage(insetQuad, 0, 0, 8, 8,
        0, 0, 8, 8, ndcScale, ndcOffset, halfPixel, 8, 8);
    Require(!insetAsFull.ok && insetAsFull.rejectReason == ProducerRejectReason::CoverageNotFull,
        "inset quad cannot claim the full area without a clear background");
    const auto inset = CheckPostprocessQuadCoverage(insetQuad, 0, 0, 8, 8,
        0, 0, 8, 8, ndcScale, ndcOffset, halfPixel, 8, 8, true);
    Require(inset.ok && inset.coveredWidth == 8 && inset.coveredHeight == 8 &&
        inset.writtenX == 1 && inset.writtenY == 1 &&
        inset.writtenWidth == 6 && inset.writtenHeight == 6,
        "exact inset quad proves only its internal pixel centers");
    Require(CheckPostprocessQuadCoverage(seamQuad, 0, 0, 2.2f, 2.2f,
        0, 0, 2, 2, ndcScale, ndcOffset, halfPixel, 2, 2, true).rejectReason ==
        ProducerRejectReason::TopologyInvalid, "inset path also rejects shared-edge crack");
    const auto fractionalScissor = CheckPostprocessQuadCoverage(posQuad,
        0, 0, 285.221875f, 161.333333f, 1, 0, 285, 161,
        ndcScale, ndcOffset, halfPixel, 320, 320);
    Require(!fractionalScissor.ok && fractionalScissor.rejectReason == ProducerRejectReason::ScissorMismatch,
        "scissor excluding the first pixel center rejects P2");

    // Partial quad
    float partialQuad[24] = {
        -0.5f, -0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f
    };
    auto partialRes = CheckQuadCoverage(partialQuad, 0.0f, 0.0f, 1707.0f, 960.0f,
        0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel);
    Require(!partialRes.ok && partialRes.rejectReason == ProducerRejectReason::CoverageNotFull,
        "partial quad coverage rejected");
    const auto postPartial = postQuad(partialQuad, 285.221875f, 161.333333f, 285, 161);
    Require(!postPartial.ok && postPartial.rejectReason == ProducerRejectReason::CoverageNotFull,
        "partially covering P2 quad rejected");

    // Non-finite coordinates
    float nanQuad[24];
    std::copy_n(posQuad, 24, nanQuad);
    nanQuad[0] = NAN;
    Require(!CheckQuadCoverage(nanQuad, 0.0f, 0.0f, 1707.0f, 960.0f, 0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel).ok,
        "NaN vertex rejected");

    // W != 1
    float wQuad[24];
    std::copy_n(posQuad, 24, wQuad);
    wQuad[3] = 2.0f;
    Require(!CheckQuadCoverage(wQuad, 0.0f, 0.0f, 1707.0f, 960.0f, 0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel).ok,
        "W!=1 vertex rejected");
    Require(postQuad(wQuad, 285.221875f, 161.333333f, 285, 161).rejectReason == ProducerRejectReason::WNotOne,
        "P2 rejects W!=1");

    // Z out of [0, 1] range after transform
    float zQuad[24];
    std::copy_n(posQuad, 24, zQuad);
    for (int i = 0; i < 6; ++i) zQuad[i * 4 + 2] = 2.0f;
    Require(!CheckQuadCoverage(zQuad, 0.0f, 0.0f, 1707.0f, 960.0f, 0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel).ok,
        "Z out of range rejected");
    Require(postQuad(zQuad, 285.221875f, 161.333333f, 285, 161).rejectReason == ProducerRejectReason::ZOutOfRange,
        "P2 rejects transformed Z outside [0, 1]");

    auto reject = [&](const float* pos, float vpX, float vpY, float vpW, float vpH,
        ProducerRejectReason reason, const char* message) {
        const auto got = CheckQuadCoverage(std::span<const float, 24>(pos, 24), vpX, vpY, vpW, vpH,
            0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel);
        Require(!got.ok && got.rejectReason == reason, message);
    };

    // Non-zero origin is outside the width/height-only token.
    reject(posQuad, 1.0f, 0.0f, 1707.0f, 960.0f, ProducerRejectReason::CoverageNotFull,
        "non-zero physicalVpX rejected");
    reject(posQuad, 0.0f, 2.0f, 1707.0f, 960.0f, ProducerRejectReason::CoverageNotFull,
        "non-zero physicalVpY rejected");

    reject(posQuad, 0.0f, 0.0f, 1707.5f, 960.0f, ProducerRejectReason::CoverageNotFull,
        "fractional physical width rejected");
    reject(posQuad, 0.0f, 0.0f, 1707.0f, 960.25f, ProducerRejectReason::CoverageNotFull,
        "fractional physical height rejected");
    reject(posQuad, 0.0f, 0.0f, 4294967296.0f, 960.0f, ProducerRejectReason::CoverageNotFull,
        "2^32 physical width rejected before uint32 cast");
    reject(posQuad, 0.0f, 0.0f, 1707.0f, 2147483648.0f, ProducerRejectReason::CoverageNotFull,
        "INT32_MAX+ overflow physical height rejected before scissor int32 cast");

    const float inf = std::numeric_limits<float>::infinity();
    Require(!CheckQuadCoverage(posQuad, 0.0f, 0.0f, inf, 960.0f, 0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel).ok,
        "non-finite physical width rejected");
    Require(!CheckQuadCoverage(posQuad, NAN, 0.0f, 1707.0f, 960.0f, 0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel).ok,
        "non-finite physical origin rejected");

    float zLo[24];
    std::copy_n(posQuad, 24, zLo);
    zLo[2] = -0.0005f;
    Require(!CheckQuadCoverage(zLo, 0.0f, 0.0f, 1707.0f, 960.0f, 0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel).ok &&
        CheckQuadCoverage(zLo, 0.0f, 0.0f, 1707.0f, 960.0f, 0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel).rejectReason == ProducerRejectReason::ZOutOfRange,
        "transformed Z=-0.0005 rejected");
    float zHi[24];
    std::copy_n(posQuad, 24, zHi);
    zHi[2] = 1.0005f;
    Require(!CheckQuadCoverage(zHi, 0.0f, 0.0f, 1707.0f, 960.0f, 0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel).ok &&
        CheckQuadCoverage(zHi, 0.0f, 0.0f, 1707.0f, 960.0f, 0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel).rejectReason == ProducerRejectReason::ZOutOfRange,
        "transformed Z=1.0005 rejected");

    // xmax/ymax equal the last pixel center: exclude-edge, not coverage.
    float lastCenterQuad[24];
    std::copy_n(posQuad, 24, lastCenterQuad);
    const float rightAtLastCenter = 1.0f - 1.0f / 1707.0f;
    const float bottomAtLastCenter = 1.0f / 960.0f - 1.0f;
    lastCenterQuad[4] = rightAtLastCenter;
    lastCenterQuad[12] = rightAtLastCenter;
    lastCenterQuad[16] = rightAtLastCenter;
    lastCenterQuad[1] = bottomAtLastCenter;
    lastCenterQuad[5] = bottomAtLastCenter;
    lastCenterQuad[13] = bottomAtLastCenter;
    const auto lastCenter = CheckQuadCoverage(lastCenterQuad, 0.0f, 0.0f, 1707.0f, 960.0f,
        0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel);
    Require(!lastCenter.ok && lastCenter.rejectReason == ProducerRejectReason::CoverageNotFull,
        "right/bottom equal to last pixel center rejected");

    // Shared-corner gap: triangle 2 inset by 0.2px, inside the old 0.5px merge window.
    float gapQuad[24];
    std::copy_n(posQuad, 24, gapQuad);
    gapQuad[12] = 1.0f - 0.4f / 1707.0f;
    gapQuad[16] = 1.0f - 0.4f / 1707.0f;
    const auto gap = CheckQuadCoverage(gapQuad, 0.0f, 0.0f, 1707.0f, 960.0f,
        0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel);
    Require(!gap.ok && gap.rejectReason == ProducerRejectReason::TopologyInvalid,
        "triangle shared-corner gap rejected");
    float wrongTopology[24];
    std::copy_n(posQuad, 24, wrongTopology);
    wrongTopology[12] = wrongTopology[0];
    wrongTopology[13] = wrongTopology[1];
    Require(postQuad(wrongTopology, 285.221875f, 161.333333f, 285, 161).rejectReason ==
        ProducerRejectReason::TopologyInvalid, "P2 rejects two triangles without four corners");

    // Net RB Swap Identity check
    uint32_t f0 = 0;
    uint32_t f3 = (0x160a << 1);
    Require(CheckNetIdentityRBSwap(f0, f3, true), "capture exact 160a00 with resolve RB swap yields net identity");
    Require(!CheckNetIdentityRBSwap(f0, f3, false), "160a00 without RB swap is not net identity");
    uint32_t f3_identity = (0x88 << 1);
    Require(CheckNetIdentityRBSwap(f0, f3_identity, false), "unswizzled fetch without RB swap yields net identity");
}
}

int main() {
    TestColorQualificationPolicy();
    const auto sizing = ReadySizing();
    gpu::frame_plan::PlannerState planner;
    const auto sr = planner.Begin(Input(sizing, false));
    Require(sr.consumer == gpu::upscaling::TemporalConsumer::DlssSr, "ready DLSS request routes to native SR");
    Require(sr.width == 960 && sr.height == 540, "native SR uses recommended input extent");
    Require(gpu::upscaling::IsDlssConsumer(sr.consumer), "native SR is a DLSS consumer");

    Require(planner.ReportFailure({sr.geometryEpoch, sr.requestSignature, sr.legacyHeight,
        gpu::frame_plan::FailureReason::InvalidInput}), "native SR failure latches matching request");
    const auto fallback = planner.Begin(Input(sizing, false));
    Require(fallback.consumer == gpu::upscaling::TemporalConsumer::LegacyTaa,
        "native SR failure disables only the matching next CPU request");
    Require(fallback.width == fallback.legacyWidth && fallback.height == fallback.legacyHeight,
        "native SR fallback preserves current-frame legacy geometry policy");

    gpu::frame_plan::PlannerState probePlanner;
    const auto probe = probePlanner.Begin(Input(sizing, true));
    Require(probe.consumer == gpu::upscaling::TemporalConsumer::DlssInputs && probe.inputProbe,
        "P1 input probe remains distinct from native SR");
    gpu::frame_plan::PlannerState readbackPlanner;
    const auto readback = readbackPlanner.Begin(Input(sizing, true, true));
    Require(readback.width == 1280 && readback.height == 720 &&
        !gpu::upscaling::IsDlssConsumer(readback.consumer),
        "readback retains 1280x720 legacy geometry and never routes to SR");
    gpu::frame_plan::PlannerState offPlanner;
    const auto off = offPlanner.Begin(Input(sizing, false, false, gpu::upscaling::Upscaler::Off));
    Require(!gpu::upscaling::IsDlssConsumer(off.consumer),
        "upscaler off remains outside native SR even with ready sizing");
    const auto route = gpu::temporal::RouteConsumer(sr, false);
    Require(route.dlssInputs && route.dlssSr && !route.inputProbe && route.effectiveAA == 0,
        "native SR routing suppresses legacy temporal consumers");
    gpu::temporal::TemporalFrameInputs inputs;
    inputs.plan = sr;
    Require(inputs.colorEncoding == gpu::temporal::ColorEncoding::Unknown,
        "temporal inputs never infer SDR encoding from storage");
    return 0;
}
