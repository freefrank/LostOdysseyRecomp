#pragma once
#include "frame_plan.h"
#include <algorithm>
#include <cstdint>

namespace gpu::dlss_fg {
// Experimental composited FG consumes the successful SR result without
// changing its provider, quality, input geometry, token or failure policy.
inline bool CompositePlanSupported(const frame_plan::FramePlan& plan) {
    return upscaling::IsSrConsumer(plan.consumer) &&
        upscaling::MatchesSrProvider(plan.requestedUpscaler, plan.consumer);
}
inline bool SameHistoryConfiguration(const frame_plan::FramePlan& a,
                                     const frame_plan::FramePlan& b) {
    return a.deviceEpoch == b.deviceEpoch && a.geometryEpoch == b.geometryEpoch &&
        a.requestSignature == b.requestSignature && a.requestedUpscaler == b.requestedUpscaler &&
        a.consumer == b.consumer && a.width == b.width && a.height == b.height &&
        a.output == b.output &&
        (a.requestedUpscaler != upscaling::Upscaler::Dlss || a.dlssQuality == b.dlssQuality) &&
        (a.requestedUpscaler != upscaling::Upscaler::Fsr || a.fsrQuality == b.fsrQuality);
}
enum class Interruption : uint8_t { None, NoInputs, InvalidInputs, SdkFailure, ResourceBoundary, Canceled };
inline const char* Name(Interruption reason) {
    switch (reason) {
    case Interruption::None: return "none";
    case Interruption::NoInputs: return "no_inputs";
    case Interruption::InvalidInputs: return "invalid_inputs";
    case Interruption::SdkFailure: return "sdk_failure";
    case Interruption::ResourceBoundary: return "resource_boundary";
    case Interruption::Canceled: return "unsubmitted_canceled";
    }
    return "unknown";
}
// O(1) diagnostics, no resource scans and no scene-type switch. Counters cover
// Prepare attempts, not monitor refreshes, SDK output or game simulation ticks.
class Continuity {
public:
    void Suspend(Interruption reason, uint64_t nowMs) {
        if (!interrupted_) { interrupted_ = true; interruptedAt_ = nowMs; }
        reason_ = reason;
    }
    void Observe(bool enabled, bool reset, Interruption reason, uint64_t nowMs) {
        ++samples_;
        if (!enabled) { Suspend(reason, nowMs); return; }
        ++enabled_;
        resets_ += reset;
        if (interrupted_) {
            longestMs_ = LongestInterruption(nowMs);
            interrupted_ = false;
            ++resumes_;
        }
        reason_ = Interruption::None;
    }
    uint64_t Samples() const { return samples_; }
    uint64_t Enabled() const { return enabled_; }
    uint64_t Resets() const { return resets_; }
    uint64_t Resumes() const { return resumes_; }
    uint64_t LongestInterruption(uint64_t nowMs) const {
        return interrupted_ && nowMs >= interruptedAt_ ?
            std::max(longestMs_, nowMs - interruptedAt_) : longestMs_;
    }
    Interruption Reason() const { return reason_; }
private:
    uint64_t samples_ = 0, enabled_ = 0, resets_ = 0, resumes_ = 0;
    uint64_t interruptedAt_ = 0, longestMs_ = 0;
    bool interrupted_ = false;
    Interruption reason_ = Interruption::None;
};
} // namespace gpu::dlss_fg
