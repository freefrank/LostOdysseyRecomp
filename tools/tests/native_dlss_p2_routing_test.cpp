#include "gpu/frame_plan.h"
#include "gpu/temporal_frame_inputs.h"

#include <cstdlib>
#include <iostream>

namespace {
void Require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

gpu::upscaling::OutputSizing ReadySizing() {
    gpu::upscaling::OutputSizing sizing;
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
}

int main() {
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
