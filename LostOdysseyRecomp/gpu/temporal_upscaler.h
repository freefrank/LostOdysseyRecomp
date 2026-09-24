#pragma once

#include "frame_plan.h"
#include "temporal_frame_inputs.h"

#include <cstdint>
#include <optional>
#include <memory>

namespace plume { struct VulkanInterface; struct VulkanDevice; struct VulkanCommandList; struct VulkanTexture; }
namespace gpu::dlss { class Controller; struct EvaluateCapture; }
namespace gpu::fsr { class Controller; }

namespace gpu {
// Captured at record time. Queue completion/discard must never consult the
// subsequently selected settings or CPU plan to identify the owner.
struct SrUseToken {
    upscaling::Upscaler provider = upscaling::Upscaler::Off;
    uint64_t deviceEpoch = 0, useId = 0, requestSignature = 0, geometryEpoch = 0;
    explicit operator bool() const { return useId != 0; }
};
inline bool CanQueryNgxSizing(const upscaling::SizingKey& key) {
    return key.provider == upscaling::Upscaler::Dlss && key.outputWidth && key.outputHeight;
}
// The recorded token owns completion. Callers may change settings before these
// callbacks run; unsupported providers never acquire an NGX use.
template<class Owner> void RouteSubmitted(SrUseToken token, uint64_t serial, Owner& owner) {
    if (token.provider == upscaling::Upscaler::Dlss && token.useId && serial) owner.OnBatchSubmitted(token.useId, serial);
}
template<class Owner> void RouteDiscarded(SrUseToken token, Owner& owner) {
    if (token.provider == upscaling::Upscaler::Dlss && token.useId) owner.OnBatchDiscarded(token.useId);
}

enum class SrResultStatus : uint8_t { Ready, Unavailable, NeedsReconfigure, Failed, DeviceLost, InputUnavailable };
// Both preparation and recording must distinguish per-frame input loss
// from capability/resource failure. Never latch a request off for the former.
enum class SrFailureAction : uint8_t { None, FrameFallback, Reconfigure, DisableRequest, StopDevice };
inline constexpr SrFailureAction FailureAction(SrResultStatus status) {
    switch (status) {
    case SrResultStatus::Ready: return SrFailureAction::None;
    case SrResultStatus::InputUnavailable: return SrFailureAction::FrameFallback;
    case SrResultStatus::NeedsReconfigure: return SrFailureAction::Reconfigure;
    case SrResultStatus::DeviceLost: return SrFailureAction::StopDevice;
    case SrResultStatus::Unavailable:
    case SrResultStatus::Failed: return SrFailureAction::DisableRequest;
    }
    return SrFailureAction::DisableRequest;
}
struct SrResult {
    SrResultStatus status = SrResultStatus::Unavailable;
    upscaling::Upscaler requestedProvider = upscaling::Upscaler::Off;
    upscaling::Upscaler actualProvider = upscaling::Upscaler::Off;
    SrUseToken token{};
    std::optional<int32_t> rawResult, rawVkResult;
};

struct SrDispatchOptions {
    bool fsrSharpening = false;
    float fsrSharpness = 0.0f;
};

struct SrRequest {
    const frame_plan::FramePlan& plan;
    const temporal::TemporalFrameInputs& inputs;
    SrDispatchOptions options{};
};

inline bool ValidSrRequest(const SrRequest& request) {
    const auto& plan = request.plan;
    if (!upscaling::MatchesSrProvider(plan.requestedUpscaler, plan.consumer) ||
        plan.requiresReadback || plan.inputProbe ||
        !plan.width || !plan.height || !plan.output.width || !plan.output.height ||
        request.inputs.plan.deviceEpoch != plan.deviceEpoch ||
        request.inputs.plan.requestSignature != plan.requestSignature ||
        request.inputs.plan.geometryEpoch != plan.geometryEpoch ||
        request.inputs.plan.consumer != plan.consumer ||
        !request.inputs.CompleteForConsumer()) return false;
    return plan.requestedUpscaler == upscaling::Upscaler::Dlss ? upscaling::KnownDlssQuality(plan.dlssQuality) :
        plan.requestedUpscaler == upscaling::Upscaler::Fsr && upscaling::KnownFsrQuality(plan.fsrQuality);
}

// Borrows the NGX controller and owns the FSR adapter. Both use the caller's queue.
class TemporalUpscaler {
public:
    explicit TemporalUpscaler(dlss::Controller& controller);
    ~TemporalUpscaler();
    upscaling::OutputSizing QuerySizing(const plume::VulkanInterface& api, const plume::VulkanDevice& device,
        const upscaling::SizingKey& key);
    SrResult Prepare(plume::VulkanDevice& device, const SrRequest& request);
    SrResult RecordIsolated(plume::VulkanCommandList& commands, const SrRequest& request,
        plume::VulkanTexture& output, dlss::EvaluateCapture* capture = nullptr);
    void OnSubmitted(SrUseToken token, uint64_t checkedSerial);
    void OnDiscarded(SrUseToken token);
    void ReleaseCompleted(uint64_t completedSerial);
    bool HasFeatureState() const;
    void ReleaseFeatureAfterGpuDrain();
    void ShutdownAfterGpuDrain();
    void AbandonAfterDeviceLoss();
private:
    dlss::Controller& dlss_;
    std::unique_ptr<fsr::Controller> fsr_;
};
} // namespace gpu
