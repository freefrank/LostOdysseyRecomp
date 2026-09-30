#pragma once

#include "frame_plan.h"
#include <cstdint>
#include <string_view>

namespace gpu::dlss_fg {

// The pinned Streamline common plugin logs this exact failure when its lazy
// NGX creation returns false. It is separate from API errors and FG statistics.
inline bool IsNgxCreationFailure(const char* message) {
    return message && std::string_view(message).find("NGX create feature failed") != std::string_view::npos;
}

// Retry identity deliberately excludes CPU frame, temporal/reset and geometry
// epochs, and texture addresses. Transient missing inputs must not turn a
// broken creation into another attempt on every frame.
struct FeatureKey {
    uint64_t deviceEpoch = 0, requestSignature = 0;
    uint32_t provider = 0, quality = 0;
    uint32_t inputWidth = 0, inputHeight = 0;
    uint32_t outputX = 0, outputY = 0, outputWidth = 0, outputHeight = 0;
    uint32_t buffers = 0, format = 0;
    bool operator==(const FeatureKey&) const = default;
};
inline FeatureKey MakeFeatureKey(const frame_plan::FramePlan& plan,
    uint32_t inputWidth, uint32_t inputHeight, uint32_t outputWidth,
    uint32_t outputHeight, uint32_t buffers, uint32_t format) {
    const auto quality = plan.requestedUpscaler == upscaling::Upscaler::Dlss ? uint32_t(plan.dlssQuality) :
        plan.requestedUpscaler == upscaling::Upscaler::Fsr ? uint32_t(plan.fsrQuality) : 0u;
    return {plan.deviceEpoch, plan.requestSignature, uint32_t(plan.requestedUpscaler), quality,
        inputWidth, inputHeight, plan.output.x, plan.output.y, outputWidth, outputHeight, buffers, format};
}

enum class RuntimePhase : uint8_t { Off, Pending, Ready, Unavailable };
inline const char* Name(RuntimePhase phase) {
    switch (phase) {
    case RuntimePhase::Off: return "off";
    case RuntimePhase::Pending: return "pending";
    case RuntimePhase::Ready: return "ready";
    case RuntimePhase::Unavailable: return "unavailable";
    }
    return "unknown";
}

// Presentation-thread policy only. It never authorizes freeing resources or
// fabricates GPU completion. Ready means a checked runtime result, not physical
// display, generated output, validation-clean status or full-game acceptance.
class FeatureRuntimeState {
public:
    bool Request(const FeatureKey& key) {
        if (phase_ == RuntimePhase::Unavailable && key == failedKey_ && boundary_ == failedBoundary_)
            return false;
        activeKey_ = key;
        phase_ = RuntimePhase::Pending;
        return true;
    }
    void Presented(bool runtimeOkay) {
        if (phase_ != RuntimePhase::Pending) return;
        if (runtimeOkay) phase_ = RuntimePhase::Ready;
        else Fail();
    }
    void Fail() {
        failedKey_ = activeKey_;
        failedBoundary_ = boundary_;
        phase_ = RuntimePhase::Unavailable;
    }
    void SuspendInputs() {
        // Missing inputs, pause and scene-reset do not clear a failure latch.
        if (phase_ != RuntimePhase::Unavailable) phase_ = RuntimePhase::Off;
    }
    void ResourceBoundary() {
        // Called only after the existing, checked resize/mode quiesce.
        ++boundary_;
        SuspendInputs();
    }
    RuntimePhase Phase() const { return phase_; }
    bool Enabled() const { return phase_ == RuntimePhase::Ready; }
private:
    FeatureKey activeKey_{}, failedKey_{};
    uint64_t boundary_ = 0, failedBoundary_ = 0;
    RuntimePhase phase_ = RuntimePhase::Off;
};

// GetState counts presents since its previous query. After a skipped/failed
// query, the next count spans multiple host frames and cannot identify a
// generated interval. Do not interpret that recovery delta as a 2x frame.
class PresentCounterWindow {
public:
    bool Observe(bool queriedSuccessfully, bool accepted) {
        const bool now = queriedSuccessfully && accepted;
        const bool contiguous = previous_ && now;
        previous_ = now;
        return contiguous;
    }
    void Reset() { previous_ = false; }
private:
    bool previous_ = false;
};

} // namespace gpu::dlss_fg
