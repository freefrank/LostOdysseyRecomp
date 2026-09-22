#pragma once

#include <chrono>
#include <cstdint>

namespace gpu::temporal {

// One predicate for "this frame has a temporal consumer". Legacy TAA, the DLSS
// input probe, and native DLSS SR — including DLAA, which uses the same SR
// route — all count. Frame start, the pre-draw gap check, and frame end must
// share it. Checking only the experiment and probe flags drops ordinary SR.
inline constexpr bool TemporalConsumerActive(bool experiment, bool inputProbe, bool dlssSr) noexcept {
    return experiment || inputProbe || dlssSr;
}

inline constexpr std::chrono::milliseconds TemporalHistoryGapLimit{250};

inline bool TemporalHistoryIntervalExceeded(std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point last) noexcept {
    return now - last > TemporalHistoryGapLimit;
}

// DLSS SR, DLAA, and the input probe keep subpixel jitter across a gap.
// Legacy TAA keeps the diagnostic override, or the forced-jitter request.
inline constexpr bool JitterAfterLongInterval(bool inputProbe, bool dlssSr,
    int diagnosticJitter, bool forcedJitter) noexcept {
    if (inputProbe || dlssSr) return true;
    return diagnosticJitter >= 0 ? diagnosticJitter == 1 : forcedJitter;
}

// Mirrors the once-per-frame consumer selection in the renderer. DLSS flags are
// applied after the legacy/diagnostic selection, and a rejected DLSS request
// must not leave jitter forced on.
struct FrameStartPolicy {
    bool legacyTaa = false;
    bool dlssInputs = false;
    bool dlssSr = false;
    bool inputProbe = false;
    bool resolveReadback = false;
    bool forced = false;
    bool forcedHistory = false;
    bool forcedJitter = false;
    bool forcedStable = false;
    uint64_t frame = 0;
    uint64_t supportedFrame = ~0ull;
    int diagnosticJitter = -1;
    int diagnosticHistory = -1;
    bool motionSupportsDlss = true;
};

struct FrameStartConsumers {
    bool experiment = false;
    bool inputProbe = false;
    bool dlssSr = false;
    bool jitter = false;
    bool allowHistory = false;
    bool stableGrid = false;
    bool rejectDlss = false;
};

inline void ApplyDlssConsumerFrameStart(bool inputProbe, bool dlssSr,
    bool& jitter, bool& allowHistory, bool& stableGrid) noexcept {
    if (!inputProbe && !dlssSr) return;
    jitter = true;
    allowHistory = false;
    stableGrid = false;
}

inline FrameStartConsumers ResolveFrameStartConsumers(const FrameStartPolicy& policy) noexcept {
    FrameStartConsumers out;
    const bool selected = policy.legacyTaa && !policy.resolveReadback;
    const bool forbidLegacy = policy.dlssInputs;
    out.experiment = selected || (!forbidLegacy && policy.forced);
    out.allowHistory = selected || (!forbidLegacy && policy.forcedHistory);
    out.jitter = (selected && policy.supportedFrame != ~0ull && policy.supportedFrame + 1 == policy.frame) ||
        (!forbidLegacy && policy.forcedJitter);
    out.stableGrid = selected || (!forbidLegacy && policy.forcedStable);
    if (policy.diagnosticJitter >= 0) out.jitter = policy.diagnosticJitter == 1;
    if (policy.diagnosticHistory >= 0) out.allowHistory = policy.diagnosticHistory == 1;
    out.inputProbe = policy.inputProbe;
    out.dlssSr = policy.dlssSr;
    if ((out.inputProbe || out.dlssSr) && !policy.motionSupportsDlss) {
        out.rejectDlss = true;
        out.inputProbe = false;
        out.dlssSr = false;
    }
    ApplyDlssConsumerFrameStart(out.inputProbe, out.dlssSr, out.jitter, out.allowHistory, out.stableGrid);
    return out;
}

// Called when a new scene frame starts, before BeginFrame. A hitch clears
// history once and records the frame so present-time does not bump the epoch
// again for that same interval. Jitter stays on for every DLSS consumer.
template <class History>
bool ApplyTemporalLongInterval(History* history, std::chrono::steady_clock::time_point now, uint64_t frame,
    bool inputProbe, bool dlssSr, int diagnosticJitter, bool forcedJitter,
    std::chrono::steady_clock::time_point frameTime, bool& jitter, uint64_t& supportedFrame,
    uint64_t& epoch, uint64_t& gapResetFrame) {
    if (!history || !TemporalHistoryIntervalExceeded(now, frameTime)) return false;
    history->Reset();
    supportedFrame = ~0ull;
    jitter = JitterAfterLongInterval(inputProbe, dlssSr, diagnosticJitter, forcedJitter);
    ++epoch;
    gapResetFrame = frame;
    return true;
}

struct TemporalFrameEndDecision {
    bool engaged = false;
    bool gap = false;
    bool complete = false;
    bool reset = false;
    bool gapAlreadyReset = false;
};

// Experiment completion is the legacy resolve. DLSS SR, DLAA, and the input
// probe become complete when the captured inputs are complete and submitted.
template <class History>
TemporalFrameEndDecision EvaluateTemporalFrameEnd(const History& history,
    std::chrono::steady_clock::time_point now, uint64_t frame,
    bool experiment, bool inputProbe, bool dlssSr, bool sceneReady, bool submittedThisFrame,
    std::chrono::steady_clock::time_point frameTime, uint64_t gapResetFrame) {
    TemporalFrameEndDecision decision;
    decision.engaged = TemporalConsumerActive(experiment, inputProbe, dlssSr);
    if (!decision.engaged) return decision;
    decision.gap = TemporalHistoryIntervalExceeded(now, frameTime);
    const bool historyReady = experiment ? history.Completed() : history.InputsComplete();
    decision.complete = sceneReady && historyReady && submittedThisFrame;
    decision.gapAlreadyReset = gapResetFrame == frame;
    // A complete frame whose gap was already applied before BeginFrame must not
    // take a second epoch. Scene rejection or a missing submit is a separate
    // failure and still clears history, including on that same gap frame.
    decision.reset = !decision.complete || (decision.gap && !decision.gapAlreadyReset);
    return decision;
}

template <class History>
void CommitTemporalFrameEnd(History& history, const TemporalFrameEndDecision& decision,
    std::chrono::steady_clock::time_point now, uint64_t& supportedFrame, uint64_t& epoch,
    std::chrono::steady_clock::time_point& frameTime) {
    if (!decision.engaged) return;
    if (decision.reset) {
        history.Reset();
        supportedFrame = ~0ull;
        ++epoch;
    }
    frameTime = now;
}

} // namespace gpu::temporal
