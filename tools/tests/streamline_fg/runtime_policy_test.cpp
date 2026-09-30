#include "gpu/dlss_fg_runtime_policy.h"
#include <cstdio>

using namespace gpu;
using namespace gpu::dlss_fg;
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main() {
    CHECK(!IsNgxCreationFailure(nullptr));
    CHECK(!IsNgxCreationFailure(""));
    CHECK(!IsNgxCreationFailure("VK validation PRESENT_AFTER_WRITE"));
    CHECK(!IsNgxCreationFailure("NGX loaded - app id"));
    CHECK(IsNgxCreationFailure("[dlssg] NGX create feature failed 0xbad0000b"));
    CHECK(IsNgxCreationFailure("commonEntry.cpp: [dlss_g] NGX create feature failed 0xbad0000b"));

    frame_plan::FramePlan plan;
    plan.requestedUpscaler = upscaling::Upscaler::Fsr;
    plan.consumer = upscaling::TemporalConsumer::FsrSr;
    plan.deviceEpoch = 1;
    plan.requestSignature = 55;
    plan.output = {{1920, 1080}, 0, 0, 1920, 1080};
    const auto key = MakeFeatureKey(plan, 1280, 720, 1920, 1080, 3, 37);
    FeatureRuntimeState state;
    CHECK(state.Phase() == RuntimePhase::Off);
    CHECK(!state.Enabled());
    CHECK(state.Request(key));
    CHECK(state.Phase() == RuntimePhase::Pending && !state.Enabled());
    // A runtime eOk can be observed in the background with no generated frame.
    state.Presented(true);
    CHECK(state.Enabled());
    // The late Streamline callback must revoke the previous apparent success.
    state.Fail();
    CHECK(!state.Enabled() && state.Phase() == RuntimePhase::Unavailable);
    bool noStorm = true;
    for (int i = 0; i < 10000; ++i) {
        state.SuspendInputs();
        noStorm &= !state.Request(key);
    }
    CHECK(noStorm);
    CHECK(state.Phase() == RuntimePhase::Unavailable);
    plan.cpuSerial++; plan.geometryEpoch++;
    CHECK(MakeFeatureKey(plan, 1280, 720, 1920, 1080, 3, 37) == key);
    plan.dlssQuality = upscaling::DlssQuality::Performance; // unrelated to FSR
    CHECK(MakeFeatureKey(plan, 1280, 720, 1920, 1080, 3, 37) == key);
    plan.fsrQuality = upscaling::FsrQuality::Performance;
    const auto newQuality = MakeFeatureKey(plan, 1280, 720, 1920, 1080, 3, 37);
    CHECK(newQuality != key);
    CHECK(state.Request(newQuality));
    state.Presented(false);
    CHECK(!state.Enabled());
    CHECK(!state.Request(newQuality));
    state.ResourceBoundary(); // verified resize/window boundary, not scene type
    CHECK(state.Request(newQuality));
    state.Presented(true);
    CHECK(state.Enabled());
    state.SuspendInputs();
    CHECK(!state.Enabled());
    CHECK(state.Request(newQuality));
    state.Presented(true);
    CHECK(state.Enabled());
    state.Fail();
    auto changed = newQuality;
    changed.deviceEpoch++;
    CHECK(state.Request(changed));
    state.Presented(false);
    changed.outputWidth++;
    CHECK(state.Request(changed));
    state.Presented(false);
    changed.inputWidth++;
    CHECK(state.Request(changed));
    state.Presented(false);
    changed.format++;
    CHECK(state.Request(changed));
    state.Presented(false);
    changed.buffers++;
    CHECK(state.Request(changed));
    state.Presented(false);
    changed.outputX++;
    CHECK(state.Request(changed));
    state.Presented(false);
    changed.requestSignature++;
    CHECK(state.Request(changed));
    state.Presented(false);
    changed.provider++;
    CHECK(state.Request(changed));
    CHECK(!state.Enabled()); // SetOptions eOn alone cannot announce Ready
    state.Presented(true);
    CHECK(state.Enabled());
    PresentCounterWindow counter;
    CHECK(!counter.Observe(true, true)); // initial cumulative SDK count
    CHECK(counter.Observe(true, true));
    CHECK(!counter.Observe(false, true)); // blocked: skipped query
    CHECK(!counter.Observe(true, true));  // recovery delta spans the gap
    CHECK(counter.Observe(true, true));
    CHECK(!counter.Observe(true, false)); // rejected present
    CHECK(!counter.Observe(true, true));
    CHECK(counter.Observe(true, true));
    counter.Reset(); // resize quiesce / unsubmitted cancellation
    CHECK(!counter.Observe(true, true));
    std::printf("runtime recovery policy: %d checks passed\n", checks);
}
