#include "gpu/dlss_fg_policy.h"
#include <cstdio>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool value, const char* reason) {
    ++checks;
    if (!value) throw std::runtime_error(reason);
}
}
int main() {
    try {
        using namespace gpu;
        using namespace upscaling;
        using namespace dlss_fg;
        frame_plan::FramePlan plan{};
        plan.requestedUpscaler = Upscaler::Dlss; plan.consumer = TemporalConsumer::DlssSr;
        plan.width = 640; plan.height = 360; plan.output = {{1280,720},0,0,1280,720};
        plan.deviceEpoch = 4; plan.geometryEpoch = 2; plan.requestSignature = 42;
        Check(CompositePlanSupported(plan), "DLSS Quality can feed experimental composited FG");
        plan.dlssQuality = DlssQuality::Dlaa;
        Check(CompositePlanSupported(plan), "DLAA can feed experimental composited FG");
        plan.requestedUpscaler = Upscaler::Fsr; plan.consumer = TemporalConsumer::FsrSr;
        for (auto quality : {FsrQuality::Quality, FsrQuality::Balanced, FsrQuality::Performance, FsrQuality::NativeAA}) {
            plan.fsrQuality = quality;
            const auto original = plan;
            Check(CompositePlanSupported(plan) && plan == original, "FSR quality and plan remain independently owned");
        }
        auto mismatch = plan; mismatch.consumer = TemporalConsumer::DlssSr;
        Check(!CompositePlanSupported(mismatch), "mismatched SR provider cannot feed FG");
        mismatch.consumer = TemporalConsumer::DlssInputs;
        Check(!CompositePlanSupported(mismatch), "input probe is not a successful SR composite");
        mismatch.consumer = TemporalConsumer::None; mismatch.requestedUpscaler = Upscaler::Off;
        Check(!CompositePlanSupported(mismatch), "native fallback does not pretend to be an SR composite");

        auto next = plan; ++next.cpuSerial;
        Check(SameHistoryConfiguration(plan, next), "each new real frame does not reset history");
        next.dlssQuality = DlssQuality::Balanced;
        Check(SameHistoryConfiguration(plan, next), "saved DLSS quality does not reset FSR history");
        next.fsrQuality = FsrQuality::Quality;
        Check(!SameHistoryConfiguration(plan, next), "FSR quality change resets only FG's local history");
        next = plan; ++next.deviceEpoch;
        Check(!SameHistoryConfiguration(plan, next), "device epoch change resets history");
        next = plan; ++next.geometryEpoch;
        Check(!SameHistoryConfiguration(plan, next), "geometry epoch change resets history");
        next = plan; ++next.requestSignature;
        Check(!SameHistoryConfiguration(plan, next), "request change resets history");
        next = plan; next.requestedUpscaler = Upscaler::Dlss; next.consumer = TemporalConsumer::DlssSr;
        Check(!SameHistoryConfiguration(plan, next), "provider change resets history");
        next = plan; next.output.height += 1;
        Check(!SameHistoryConfiguration(plan, next), "resize resets local history");

        Continuity continuity;
        continuity.Observe(false, false, Interruption::NoInputs, 0);
        continuity.Observe(false, false, Interruption::InvalidInputs, 20);
        Check(continuity.Samples() == 2 && continuity.Enabled() == 0 && continuity.LongestInterruption(30) == 30,
            "repeated off samples do not restart the interruption clock (including epoch zero)");
        continuity.Observe(true, true, Interruption::None, 50);
        Check(continuity.Enabled() == 1 && continuity.Resets() == 1 && continuity.Resumes() == 1 &&
            continuity.LongestInterruption(60) == 50, "first valid input resumes with reset and captures the full gap");
        // Synthetic input-valid frames stand for a continuous sequence. There
        // is intentionally no pause/movie/scene-type argument in this policy.
        for (unsigned i = 0; i < 48; ++i) continuity.Observe(true, false, Interruption::None, 60 + i);
        Check(continuity.Enabled() == 49 && continuity.Samples() == 51 && continuity.Resets() == 1,
            "continuous valid frames do not cause blanket scene disables or repeated resets");
        continuity.Suspend(Interruption::ResourceBoundary, 200);
        continuity.Suspend(Interruption::ResourceBoundary, 240);
        Check(continuity.Samples() == 51 && continuity.LongestInterruption(300) == 100,
            "resize/minimize boundary is timed without inventing presented-frame samples");
        continuity.Observe(true, true, Interruption::None, 400);
        Check(continuity.LongestInterruption(500) == 200 && continuity.Resumes() == 2 && continuity.Resets() == 2,
            "boundary recovery records reset, preserves original gap and auto-resumes");
        continuity.Suspend(Interruption::Canceled, 600);
        Check(continuity.Reason() == Interruption::Canceled && continuity.LongestInterruption(590) == 200,
            "cancellation has an explicit reason and clock underflow is rejected");
        std::printf("FG combination/continuity policy: %u checks passed (synthetic, no GPU)\n", checks);
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
