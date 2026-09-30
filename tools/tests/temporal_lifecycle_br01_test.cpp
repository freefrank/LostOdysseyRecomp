// BR-01: drive the production temporal lifecycle with a controllable clock.
// SR and DLAA both enter through RouteConsumer. This does not execute NGX,
// HistoryOwner GPU capture, or a scene image comparison.
#include "gpu/temporal_frame_inputs.h"
#include "gpu/temporal_lifecycle.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

unsigned checks = 0;

void Check(bool condition, const std::string& description) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL [%u]: %s\n", checks, description.c_str());
        std::exit(1);
    }
}

using Clock = std::chrono::steady_clock;

struct LifecycleHistory {
    int resets = 0;
    bool inputsComplete = false;
    bool completed = false;
    void Reset() {
        ++resets;
        inputsComplete = false;
        completed = false;
    }
    bool InputsComplete() const { return inputsComplete; }
    bool Completed() const { return completed; }
};

struct Step {
    uint64_t epoch = 0;
    int resets = 0;
    int resetDelta = 0;
    uint64_t epochDelta = 0;
    bool jitter = false;
    bool allowHistory = false;
    bool stableGrid = false;
    bool engaged = false;
    bool longReset = false;
    bool frameEndReset = false;
    bool gap = false;
    bool complete = false;
    bool gapAlreadyReset = false;
    bool inputsComplete = false;
    Clock::time_point frameTime{};
};

// Independent of one another. Capture can succeed and a later scene rejection
// or a missed submit can still make the frame incomplete.
struct FrameSignals {
    bool inputsComplete = false;
    bool sceneReady = false;
    bool submitted = false;
};

// Renderer order: resolve consumers, apply a pre-draw gap, capture inputs,
// then evaluate and commit the frame end. The clock test supplies the three
// completion signals; the owner test performs the real capture.
class ProductionDriver {
public:
    Step Advance(uint64_t frame, Clock::time_point now, gpu::temporal::FrameStartPolicy policy, bool established) {
        return AdvanceSpan(frame, now, now, policy, {established, established, established});
    }

    Step AdvanceSpan(uint64_t frame, Clock::time_point start, Clock::time_point end,
        gpu::temporal::FrameStartPolicy policy, FrameSignals signals) {
        policy.frame = frame;
        policy.supportedFrame = supportedFrame_;
        const auto started = gpu::temporal::ResolveFrameStartConsumers(policy);
        experiment_ = started.experiment;
        inputProbe_ = started.inputProbe;
        dlssSr_ = started.dlssSr;
        jitter_ = started.jitter;
        allowHistory_ = started.allowHistory;
        stableGrid_ = started.stableGrid;
        const int resetsBefore = history_.resets;
        const uint64_t epochBefore = epoch_;
        const bool longReset = gpu::temporal::ApplyTemporalLongInterval(&history_, start, frame,
            inputProbe_, dlssSr_, policy.diagnosticJitter, policy.forcedJitter, frameTime_,
            jitter_, supportedFrame_, epoch_, gapResetFrame_);
        // DLSS completion is InputsComplete(), not the legacy TAA Completed() bit.
        // Set after the gap reset, matching CaptureColorInputs later in the frame.
        history_.inputsComplete = signals.inputsComplete;
        history_.completed = false;
        const auto decision = gpu::temporal::EvaluateTemporalFrameEnd(history_, end, frame,
            experiment_, inputProbe_, dlssSr_, signals.sceneReady, signals.submitted, frameTime_, gapResetFrame_);
        gpu::temporal::CommitTemporalFrameEnd(history_, decision, end, supportedFrame_, epoch_, frameTime_);
        Step step;
        step.epoch = epoch_;
        step.resets = history_.resets;
        step.resetDelta = history_.resets - resetsBefore;
        step.epochDelta = epoch_ - epochBefore;
        step.jitter = jitter_;
        step.allowHistory = allowHistory_;
        step.stableGrid = stableGrid_;
        step.engaged = decision.engaged;
        step.longReset = longReset;
        step.frameEndReset = decision.reset;
        step.gap = decision.gap;
        step.complete = decision.complete;
        step.gapAlreadyReset = decision.gapAlreadyReset;
        step.inputsComplete = history_.inputsComplete;
        step.frameTime = frameTime_;
        return step;
    }

    uint64_t epoch() const { return epoch_; }

private:
    LifecycleHistory history_{};
    bool experiment_ = false, inputProbe_ = false, dlssSr_ = false;
    bool jitter_ = false, allowHistory_ = false, stableGrid_ = false;
    uint64_t epoch_ = 1;
    uint64_t supportedFrame_ = ~0ull;
    uint64_t gapResetFrame_ = ~0ull;
    Clock::time_point frameTime_{};
};

gpu::frame_plan::FramePlan DlssPlan(gpu::upscaling::DlssQuality quality, uint32_t width, uint32_t height) {
    gpu::frame_plan::FramePlan plan;
    plan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
    plan.requestedUpscaler = gpu::upscaling::Upscaler::Dlss;
    plan.dlssQuality = quality;
    plan.effectiveAA = 3;
    plan.legacyAA = 3;
    plan.width = width;
    plan.height = height;
    plan.inputProbe = false;
    return plan;
}

gpu::temporal::FrameStartPolicy PolicyFromRoute(const gpu::temporal::ConsumerRoute& route) {
    gpu::temporal::FrameStartPolicy policy;
    policy.legacyTaa = route.legacyTaa;
    policy.dlssInputs = route.dlssInputs;
    policy.dlssSr = route.dlssSr;
    policy.inputProbe = route.inputProbe;
    policy.diagnosticJitter = -1;
    policy.forcedJitter = false;
    policy.motionSupportsDlss = true;
    return policy;
}

std::vector<Step> RunDlssScenario(const gpu::temporal::FrameStartPolicy& policy, const char* name) {
    ProductionDriver driver;
    std::vector<Step> trace;
    uint64_t frame = 1;
    Clock::time_point now{};
    const auto push = [&](Clock::time_point stamp, bool established) {
        trace.push_back(driver.Advance(frame, stamp, policy, established));
        ++frame;
    };

    push(now, false);
    Check(trace.back().engaged && trace.back().jitter && !trace.back().allowHistory && !trace.back().stableGrid,
        std::string(name) + " frame start keeps DLSS jitter");
    Check(trace.back().resetDelta == 1 && !trace.back().longReset && trace.back().frameEndReset && !trace.back().inputsComplete,
        std::string(name) + " incomplete inputs still reset once");

    now += std::chrono::milliseconds(16);
    push(now, false);
    Check(trace.back().resetDelta == 1 && trace.back().jitter, std::string(name) + " reset continues before inputs exist");

    now += std::chrono::milliseconds(16);
    push(now, true);
    Check(trace.back().engaged && trace.back().complete && trace.back().resetDelta == 0 && trace.back().jitter &&
            trace.back().inputsComplete && trace.back().frameTime == now,
        std::string(name) + " reset stops once inputs are established");
    const uint64_t stableEpoch = driver.epoch();
    const int stableResets = trace.back().resets;
    const auto stableStart = now;

    for (int i = 0; i < 20; ++i) {
        now += std::chrono::milliseconds(16);
        push(now, true);
        Check(trace.back().resetDelta == 0 && trace.back().epochDelta == 0 && trace.back().jitter &&
                trace.back().inputsComplete && !trace.back().longReset && !trace.back().frameEndReset &&
                trace.back().frameTime == now,
            std::string(name) + " steady 16ms frame keeps history and jitter");
    }
    Check(now - stableStart > std::chrono::milliseconds(250), std::string(name) + " steady run exceeds 250ms");
    Check(driver.epoch() == stableEpoch && trace.back().resets == stableResets,
        std::string(name) + " exceeding 250ms across 16ms frames does not reset");

    now += std::chrono::milliseconds(250);
    push(now, true);
    Check(trace.back().resetDelta == 0 && trace.back().epochDelta == 0 && !trace.back().gap && trace.back().jitter,
        std::string(name) + " a 250ms interval is not a gap");

    now += std::chrono::milliseconds(300);
    push(now, true);
    Check(trace.back().longReset && !trace.back().frameEndReset && trace.back().gap && trace.back().gapAlreadyReset &&
            trace.back().complete && trace.back().resetDelta == 1 && trace.back().epochDelta == 1 &&
            trace.back().jitter && trace.back().inputsComplete && trace.back().frameTime == now,
        std::string(name) + " a 300ms interval resets once and keeps jitter");
    const uint64_t recoveredEpoch = driver.epoch();
    const int recoveredResets = trace.back().resets;

    for (int i = 0; i < 20; ++i) {
        now += std::chrono::milliseconds(16);
        push(now, true);
        Check(trace.back().resetDelta == 0 && trace.back().epochDelta == 0 && trace.back().jitter &&
                trace.back().inputsComplete && trace.back().frameTime == now,
            std::string(name) + " history recovers after the single gap");
    }
    Check(driver.epoch() == recoveredEpoch && trace.back().resets == recoveredResets,
        std::string(name) + " recovery across more than 250ms does not reset again");
    return trace;
}

void CheckRoutes() {
    const auto srPlan = DlssPlan(gpu::upscaling::DlssQuality::Quality, 1707, 960);
    const auto dlaaPlan = DlssPlan(gpu::upscaling::DlssQuality::Dlaa, 2560, 1440);
    Check(srPlan.dlssQuality != dlaaPlan.dlssQuality && srPlan.width != dlaaPlan.width, "SR and DLAA plans differ");
    const auto srRoute = gpu::temporal::RouteConsumer(srPlan, false);
    const auto dlaaRoute = gpu::temporal::RouteConsumer(dlaaPlan, false);
    for (const auto* route : {&srRoute, &dlaaRoute}) {
        Check(route->dlssSr && route->dlssInputs && !route->inputProbe && !route->legacyTaa && route->effectiveAA == 0,
            "ordinary DLSS route is native SR without the input probe");
    }
    const auto sr = RunDlssScenario(PolicyFromRoute(srRoute), "SR");
    const auto dlaa = RunDlssScenario(PolicyFromRoute(dlaaRoute), "DLAA");
    Check(sr.size() == dlaa.size(), "SR and DLAA scenarios take the same number of frames");
    for (size_t i = 0; i < sr.size(); ++i) {
        Check(sr[i].epoch == dlaa[i].epoch && sr[i].resets == dlaa[i].resets && sr[i].jitter == dlaa[i].jitter &&
                sr[i].longReset == dlaa[i].longReset && sr[i].frameEndReset == dlaa[i].frameEndReset &&
                sr[i].inputsComplete == dlaa[i].inputsComplete,
            "DLAA follows the same temporal lifecycle as SR");
    }
}

void CheckGapJitterSelection() {
    LifecycleHistory history;
    bool jitter = true;
    uint64_t epoch = 1, supported = ~0ull, gapFrame = ~0ull;
    const auto start = Clock::time_point{};
    const auto hitch = start + std::chrono::milliseconds(300);
    Check(gpu::temporal::ApplyTemporalLongInterval(&history, hitch, 4, false, true, 0, false,
            start, jitter, supported, epoch, gapFrame),
        "DLSS long interval is detected with diagnostic jitter disabled");
    Check(jitter && epoch == 2 && history.resets == 1 && gapFrame == 4,
        "diagnostic jitter cannot disable DLSS SR jitter after a gap");

    jitter = true;
    epoch = 1;
    history = {};
    gapFrame = ~0ull;
    Check(gpu::temporal::ApplyTemporalLongInterval(&history, hitch, 5, false, false, -1, false,
            start, jitter, supported, epoch, gapFrame),
        "legacy long interval still runs");
    Check(!jitter && epoch == 2, "legacy long interval does not force jitter on");

    jitter = false;
    epoch = 1;
    history = {};
    gapFrame = ~0ull;
    Check(gpu::temporal::ApplyTemporalLongInterval(&history, hitch, 6, true, false, 0, false,
            start, jitter, supported, epoch, gapFrame),
        "input-probe long interval still runs");
    Check(jitter && epoch == 2, "input probe keeps jitter across a gap");
}

void CheckModifiedGapBranches(const gpu::temporal::FrameStartPolicy& policy, const char* name) {
    const FrameSignals complete{true, true, true};
    const FrameSignals sceneRejected{true, false, true};
    const FrameSignals notSubmitted{true, true, false};
    ProductionDriver driver;
    uint64_t frame = 1;
    Clock::time_point now{};
    auto step = driver.AdvanceSpan(frame++, now, now, policy, complete);
    Check(step.complete && step.resetDelta == 0 && step.jitter, std::string(name) + " baseline complete frame");

    const auto gapStart = now + std::chrono::milliseconds(300);
    const auto gapEnd = gapStart + std::chrono::milliseconds(16);
    step = driver.AdvanceSpan(frame++, gapStart, gapEnd, policy, complete);
    Check(step.longReset && !step.frameEndReset && step.gap && step.gapAlreadyReset && step.complete &&
            step.epochDelta == 1 && step.resetDelta == 1 && step.jitter && step.inputsComplete && step.frameTime == gapEnd,
        std::string(name) + " complete gap still takes one epoch");

    now = gapEnd + std::chrono::milliseconds(16);
    step = driver.AdvanceSpan(frame++, now, now, policy, sceneRejected);
    Check(!step.longReset && step.frameEndReset && !step.complete && step.epochDelta == 1 && step.resetDelta == 1 &&
            !step.inputsComplete && step.jitter,
        std::string(name) + " a later scene rejection clears history without a new gap");

    now += std::chrono::milliseconds(16);
    step = driver.AdvanceSpan(frame++, now, now, policy, complete);
    Check(step.resetDelta == 0 && step.inputsComplete, std::string(name) + " complete frame resumes after scene rejection");

    const auto lateStart = now + std::chrono::milliseconds(200);
    const auto lateEnd = now + std::chrono::milliseconds(300);
    step = driver.AdvanceSpan(frame++, lateStart, lateEnd, policy, complete);
    Check(!step.longReset && step.frameEndReset && step.gap && !step.gapAlreadyReset && step.complete &&
            step.epochDelta == 1 && step.resetDelta == 1 && step.jitter && !step.inputsComplete && step.frameTime == lateEnd,
        std::string(name) + " frame-end threshold resets once when frame start stayed within 250ms");

    ProductionDriver gapReject;
    uint64_t rejectFrame = 1;
    Clock::time_point rejectNow{};
    gapReject.AdvanceSpan(rejectFrame++, rejectNow, rejectNow, policy, complete);
    const auto rejectStart = rejectNow + std::chrono::milliseconds(300);
    step = gapReject.AdvanceSpan(rejectFrame, rejectStart, rejectStart + std::chrono::milliseconds(16), policy, sceneRejected);
    Check(step.longReset && step.frameEndReset && step.gapAlreadyReset && !step.complete &&
            step.epochDelta == 2 && step.resetDelta == 2 && !step.inputsComplete && step.jitter,
        std::string(name) + " scene rejection after a gap reset still clears history");

    ProductionDriver gapSubmit;
    uint64_t submitFrame = 1;
    Clock::time_point submitNow{};
    gapSubmit.AdvanceSpan(submitFrame++, submitNow, submitNow, policy, complete);
    const auto submitStart = submitNow + std::chrono::milliseconds(300);
    step = gapSubmit.AdvanceSpan(submitFrame, submitStart, submitStart + std::chrono::milliseconds(16), policy, notSubmitted);
    Check(step.longReset && step.frameEndReset && step.gapAlreadyReset && !step.complete &&
            step.epochDelta == 2 && step.resetDelta == 2 && !step.inputsComplete && step.jitter,
        std::string(name) + " missing submit after a gap reset still clears history");
}

void CheckRejectedDlssDoesNotForceJitter() {
    gpu::temporal::FrameStartPolicy policy;
    policy.dlssInputs = true;
    policy.dlssSr = true;
    policy.motionSupportsDlss = false;
    policy.diagnosticJitter = 0;
    const auto started = gpu::temporal::ResolveFrameStartConsumers(policy);
    Check(started.rejectDlss && !started.dlssSr && !started.inputProbe && !started.jitter &&
            !gpu::temporal::TemporalConsumerActive(started.experiment, started.inputProbe, started.dlssSr),
        "rejected DLSS is not a temporal consumer and does not force jitter");
}

} // namespace

int main(int argc, char** argv) {
    const bool gapBranchOnly = argc > 1 && std::strcmp(argv[1], "--gap-branch") == 0;
    if (!gapBranchOnly) {
        CheckRoutes();
        CheckGapJitterSelection();
        CheckRejectedDlssDoesNotForceJitter();
    }
    const auto sr = gpu::temporal::RouteConsumer(DlssPlan(gpu::upscaling::DlssQuality::Quality, 1707, 960), false);
    const auto dlaa = gpu::temporal::RouteConsumer(DlssPlan(gpu::upscaling::DlssQuality::Dlaa, 2560, 1440), false);
    CheckModifiedGapBranches(PolicyFromRoute(sr), "SR");
    CheckModifiedGapBranches(PolicyFromRoute(dlaa), "DLAA");
    std::printf("PASS: %u BR-01 temporal lifecycle checks (%s, no image comparison)\n",
        checks, gapBranchOnly ? "gap branch" : "clock only");
    return 0;
}
