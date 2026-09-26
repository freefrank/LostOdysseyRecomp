// CPU-only tests of the production planner and sizing policy. No NGX execution,
// Vulkan submission, motion-response or image-quality acceptance is implied.
#include "gpu/frame_plan.h"
#include "gpu/temporal_frame_inputs.h"
#include "gpu/temporal_lifecycle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
using namespace gpu;
using namespace gpu::upscaling;
using namespace gpu::frame_plan;
unsigned checks = 0;
void Check(bool condition, const char* description) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL [%u]: %s\n", checks, description);
        std::exit(1);
    }
}
OutputSizing Sizing(OutputRegion output) {
    OutputSizing sizing;
    sizing.key = {7, output.width, output.height, Upscaler::Dlss, output.x, output.y};
    sizing.revision = 11;
    for (auto quality : kDlssQualityModes) {
        auto& mode = sizing.modes[DlssQualityIndex(quality)];
        mode.state = SizingState::Ready;
        if (quality == DlssQuality::Dlaa) mode.optimal = {output.width, output.height};
        else if (quality == DlssQuality::Quality)
            mode.optimal = {(output.width * 2 + 2) / 3, (output.height * 2 + 2) / 3};
        else if (quality == DlssQuality::Balanced)
            mode.optimal = {output.width * 3 / 5, output.height * 3 / 5};
        else mode.optimal = {output.width / 2, output.height / 2};
        mode.minimum = mode.maximum = mode.optimal;
        mode.ngxResult = 1;
    }
    return sizing;
}
PlannerInput Input(const OutputSizing& sizing, OutputRegion output) {
    PlannerInput input;
    input.internalResolution = 720; // retained for fallback, ignored by active DLAA
    input.antialiasing = 3;
    input.upscaler = Upscaler::Dlss;
    input.quality = DlssQuality::Dlaa;
    input.output = output;
    input.device = {backend::Backend::Vulkan, 7, true, true};
    input.sizing = &sizing;
    return input;
}
void RoundTrip(const FramePlan& plan) {
    wire::PlanStage receiver;
    const auto words = wire::EncodePlan(plan);
    std::optional<FramePlan> result;
    for (uint32_t i = 0; i < words.size(); ++i) {
        auto decoded = receiver.Write(wire::PlanBase + i, words[i]);
        if (i + 1 != words.size()) Check(!decoded, "wire must not publish a partial plan");
        else result = decoded;
    }
    Check(result && *result == plan, "DLAA must round-trip all 24 words without corrupting flags");
}
}

int main() {
    // Exercise the production planner -> renderer route -> jitter policy, not
    // just the displayed availability label. Saved AA remains a user request.
    for (auto quality : kDlssQualityModes) for (uint32_t aa=0;aa<=3;++aa) {
        const auto output=ResolveOutputRegion({1920,1080});
        auto sizing=Sizing(output);
        auto input=Input(sizing,output);
        input.quality=quality;input.antialiasing=aa;input.device.dlssAvailable=false;
        PlannerState planner;
        const auto spatial=[&](const FramePlan& plan) {
            const auto route=temporal::RouteConsumer(plan,false);
            Check(plan.legacyAA==aa && plan.effectiveAA==(aa==3?2:aa),"DLSS fallback preserves saved AA and substitutes SMAA only for TAA");
            Check(plan.consumer==TemporalConsumer::None && route.spatialAA==(aa!=0) &&
                !route.legacyTaa && !route.dlssInputs && !route.sr,"DLSS fallback reaches spatial renderer route");
            temporal::FrameStartPolicy policy{};
            policy.legacyTaa=route.legacyTaa;policy.dlssInputs=route.dlssInputs;
            policy.dlssSr=route.sr;policy.inputProbe=route.inputProbe;
            policy.frame=42;policy.supportedFrame=41;
            const auto start=temporal::ResolveFrameStartConsumers(policy);
            Check(!start.jitter && !start.allowHistory && !start.experiment,"fallback does not retain jitter/history from an earlier supported frame");
            RoundTrip(plan);
        };
        const auto unavailable=planner.Begin(input);spatial(unavailable);
        input.device.dlssAvailable=true;
        const auto ready=planner.Begin(input);
        Check(ready.consumer==TemporalConsumer::DlssSr && ready.effectiveAA==0 &&
            ready.geometryEpoch!=unavailable.geometryEpoch,"capability recovery restores native DLSS with a new epoch");
        Check(planner.ReportFailure({ready.geometryEpoch,ready.requestSignature,ready.legacyHeight,
            FailureReason::DlssUnavailable}),"native request failure is accepted");
        const auto failed=planner.Begin(input);spatial(failed);
        Check(failed.requestSignature==ready.requestSignature && failed.geometryEpoch!=ready.geometryEpoch,
            "latched fallback keeps request identity but invalidates temporal history epoch");
        input.upscaler=Upscaler::Off;
        const auto explicitAA=planner.Begin(input);
        Check(explicitAA.effectiveAA==aa && (explicitAA.consumer==TemporalConsumer::LegacyTaa)==(aa==3),
            "explicit AA without DLSS remains unchanged, including requested TAA");
    }
    static_assert(uint32_t(DlssQuality::Quality) == 0 && uint32_t(DlssQuality::Balanced) == 1);
    static_assert(uint32_t(DlssQuality::Performance) == 2 && uint32_t(DlssQuality::Dlaa) == 3);
    static_assert(kDlssQualityModes.size() == 4 && wire::PlanWordCount == 24 && wire::Version == 3);
    static_assert(uint32_t(DlssQuality::Dlaa) <= 0x3u);
    for (auto quality : kDlssQualityModes) {
        Check(KnownDlssQuality(quality), "all persisted modes are known");
        Check(NormalizeDlssQuality(quality) == quality, "valid modes survive config validation");
        Check(kDlssQualityModes[DlssQualityIndex(quality)] == quality, "mode index matches inventory");
    }
    Check(!ValidDlssRenderExtent(DlssQuality::Dlaa, {1280, 736}, {1280, 720}), "reject guest padding as DLAA content");
    Check(!ValidDlssRenderExtent(DlssQuality::Dlaa, {853, 480}, {1280, 720}), "reject SR input as DLAA");
    Check(!ValidDlssRenderExtent(DlssQuality::Dlaa, {1280, 720}, {1280, 800}), "reject drawable bars as DLAA output");
    Check(!ValidDlssRenderExtent(DlssQuality::Dlaa, {0, 720}, {0, 720}), "reject zero extents");
    Check(ValidDlssRenderExtent(DlssQuality::Quality, {853, 480}, {1280, 720}), "retain ordinary SR sizing");

    for (auto drawable : std::array<resolution::Size, 7>{{
            {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160},
            {3440, 1440}, {1280, 800}, {1366, 768}}}) {
        const auto output = ResolveOutputRegion(drawable);
        auto sizing = Sizing(output);
        auto input = Input(sizing, output);
        PlannerState planner;
        const auto plan = planner.Begin(input);
        Check(plan.width == output.width && plan.height == output.height, "DLAA uses exact content resolution");
        Check(plan.consumer == TemporalConsumer::DlssSr, "DLAA reuses the production temporal NGX consumer");
        Check(plan.effectiveAA == 0 && plan.legacyAA == 3, "no double AA, preserve requested fallback AA");
        Check(plan.dlssQuality == DlssQuality::Dlaa && plan.requestedUpscaler == Upscaler::Dlss, "DLAA selection survives planning");
        Check(plan.requestSignature == InputRequestSignature(input), "incoming and final signatures agree");
        Check(plan.legacyHeight == 720 && plan.sizingRevision == 11, "legacy size and sizing revision preserved");
        RoundTrip(plan);
        const auto repeat = planner.Begin(input);
        Check(repeat.geometryEpoch == plan.geometryEpoch, "stable DLAA must not reset every frame");
        Check(repeat.cpuSerial > plan.cpuSerial, "stable DLAA still advances CPU frames");
        const auto presentation = ResolvePresentationDecision(&plan, true, 3, 1);
        Check(presentation.requestedAA == 0 && presentation.bypassAA, "completed DLAA bypasses presentation AA");
        if (drawable.height == 800)
            Check(plan.output.y == 40 && plan.height == 720, "16:10 letterbox is outside DLAA content");
    }

    const auto output = ResolveOutputRegion({2560, 1440});
    auto sizing = Sizing(output);
    auto input = Input(sizing, output);
    const auto nativeIndex = DlssQualityIndex(DlssQuality::Dlaa);
    for (auto state : {SizingState::Pending, SizingState::Unavailable, SizingState::Error}) {
        sizing.modes[nativeIndex].state = state;
        PlannerState planner;
        auto fallback = planner.Begin(input);
        Check(fallback.consumer == TemporalConsumer::None && fallback.effectiveAA == 2, "unready DLAA falls back to spatial SMAA");
        Check(fallback.width == 1280 && fallback.height == 720, "unready DLAA retains legacy internal resolution");
        input.quality = DlssQuality::Quality;
        Check(planner.Begin(input).consumer == TemporalConsumer::DlssSr, "DLAA-only failure does not disable Quality");
        input.quality = DlssQuality::Dlaa;
    }
    sizing = Sizing(output);
    sizing.modes[nativeIndex].optimal = {1707, 960};
    PlannerState malformedPlanner;
    Check(malformedPlanner.Begin(input).consumer == TemporalConsumer::None, "malformed Ready DLAA is not silently accepted");
    sizing = Sizing(output);
    for (auto bad : {4u, std::numeric_limits<uint32_t>::max()}) {
        input.quality = DlssQuality(bad);
        PlannerState planner;
        const auto plan = planner.Begin(input);
        Check(!KnownDlssQuality(input.quality), "invalid config mode detected");
        Check(plan.dlssQuality == DlssQuality::Quality && plan.width == 1707, "invalid mode normalizes safely before indexing");
        Check(plan.requestSignature == InputRequestSignature(input), "invalid mode normalization keeps signatures stable");
        RoundTrip(plan);
    }
    input.quality = DlssQuality::Dlaa;
    {
        PlannerState planner;
        input.device.dlssAvailable = false;
        Check(planner.Begin(input).consumer == TemporalConsumer::None, "unavailable NGX falls back");
        input.device.dlssAvailable = true;
        input.readback = true;
        auto readback = planner.Begin(input);
        Check(!IsDlssConsumer(readback.consumer) && readback.width == 1280 && readback.height == 720, "readback never routes to DLAA");
        input.readback = false;
        input.inputProbeRequested = true;
        auto probe = planner.Begin(input);
        Check(probe.consumer == TemporalConsumer::DlssInputs && probe.width == 2560 && probe.effectiveAA == 0, "input-only DLAA probe stays input-only");
        RoundTrip(probe);
        input.inputProbeRequested = false;
    }
    {
        PlannerState planner;
        input.internalResolution = 0;
        input.upscaler = Upscaler::Off;
        const auto taa = planner.Begin(input);
        input.upscaler = Upscaler::Dlss;
        const auto dlaa = planner.Begin(input);
        Check(taa.width == dlaa.width && taa.height == dlaa.height, "TAA/DLAA same-resolution fixture");
        Check(taa.geometryEpoch != dlaa.geometryEpoch && taa.requestSignature != dlaa.requestSignature, "same-size consumer switch changes history epoch");
        // Synthetic equal-size Quality exercises quality-key changes independently of size.
        sizing.modes[0].optimal = {output.width, output.height};
        input.quality = DlssQuality::Quality;
        const auto quality = planner.Begin(input);
        Check(quality.width == dlaa.width && quality.geometryEpoch != dlaa.geometryEpoch, "same-size quality switch changes epoch");
        input.quality = DlssQuality::Dlaa;
        Check(planner.Begin(input).geometryEpoch != quality.geometryEpoch, "switch back to DLAA changes epoch");
    }
    sizing = Sizing(output);
    input = Input(sizing, output);
    {
        PlannerState planner;
        const auto active = planner.Begin(input);
        const PlanFailure failure{active.geometryEpoch, active.requestSignature, active.legacyHeight, FailureReason::DlssUnavailable};
        Check(planner.ReportFailure(failure), "accept matching failed DLAA attempt");
        const auto fallback = planner.Begin(input);
        Check(fallback.consumer == TemporalConsumer::None && fallback.width == 1280, "failed DLAA restores original request");
        Check(planner.Begin(input).consumer == TemporalConsumer::None, "failure latch prevents per-frame NGX retry");
        input.quality = DlssQuality::Quality;
        Check(planner.Begin(input).consumer == TemporalConsumer::DlssSr, "new SR request clears DLAA failure latch");
        Check(!planner.ReportFailure(failure), "stale DLAA failure cannot poison a new request");
        input.quality = DlssQuality::Dlaa;
        Check(planner.Begin(input).consumer == TemporalConsumer::DlssSr, "explicit switch back retries DLAA");
    }
    std::printf("PASS: %u DLAA CPU contract checks (no GPU or visual acceptance)\n", checks);
}
