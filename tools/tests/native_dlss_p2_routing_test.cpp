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

    // Z out of [0, 1] range after transform
    float zQuad[24];
    std::copy_n(posQuad, 24, zQuad);
    for (int i = 0; i < 6; ++i) zQuad[i * 4 + 2] = 2.0f;
    Require(!CheckQuadCoverage(zQuad, 0.0f, 0.0f, 1707.0f, 960.0f, 0, 0, 1707, 960, ndcScale, ndcOffset, halfPixel).ok,
        "Z out of range rejected");

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
