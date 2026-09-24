#include "temporal_upscaler.h"

#if defined(LO_GPU_PLUME)
#include "dlss_ngx.h"
#include "fsr_upscaler.h"
#include "fsr_projection.h"
#include <cfloat>

namespace gpu {
namespace {
SrResultStatus Convert(dlss::SrStatus status) {
    switch (status) {
    case dlss::SrStatus::Executable: return SrResultStatus::Ready;
    case dlss::SrStatus::Bypass: return SrResultStatus::Unavailable;
    case dlss::SrStatus::NeedsReconfigure: return SrResultStatus::NeedsReconfigure;
    case dlss::SrStatus::Failed: return SrResultStatus::Failed;
    case dlss::SrStatus::DeviceLost: return SrResultStatus::DeviceLost;
    }
    return SrResultStatus::Failed;
}
SrResultStatus Convert(fsr::Status status) {
    switch (status) {
    case fsr::Status::Ready: return SrResultStatus::Ready;
    case fsr::Status::Unavailable: return SrResultStatus::Unavailable;
    case fsr::Status::NeedsReconfigure: return SrResultStatus::NeedsReconfigure;
    case fsr::Status::Failed: return SrResultStatus::Failed;
    case fsr::Status::DeviceLost: return SrResultStatus::DeviceLost;
    }
    return SrResultStatus::Failed;
}
fsr::Config FsrConfig(const SrRequest& request) {
    return {request.inputs.color.width, request.inputs.color.height, request.plan.output.width,
        request.plan.output.height, request.plan.fsrQuality, request.plan.deviceEpoch};
}
fsr::FrameMetadata FsrMetadata(const SrRequest& request) {
    fsr::FrameMetadata frame{};
    frame.enableSharpening = request.options.fsrSharpening;
    frame.sharpness = request.options.fsrSharpness;
    if (!request.inputs.cameraValid || !request.inputs.color.height) return frame;
    const auto projection = fsr::DeriveProjection(request.inputs.cameraViewProjection,
        double(request.inputs.color.width) / request.inputs.color.height);
    if (!projection) return frame;
    frame.cameraValid = true;
    frame.cameraNear = FLT_MAX;
    frame.cameraFar = projection->nearDistance;
    frame.verticalFovRadians = projection->verticalFovRadians;
    // P1 uses the SDK default unit scale. Guest world units are not calibrated.
    frame.viewSpaceToMetersFactor = 1.0f;
    frame.frameTimeDeltaMilliseconds = request.inputs.frameTimeDeltaMilliseconds;
    frame.depthScale = projection->depthScale;
    frame.depthBias = projection->depthBias;
    return frame;
}
dlss::SrConfig DlssConfig(const SrRequest& request) {
    dlss::SrConfig config{};
    config.renderExtent = {request.inputs.color.width, request.inputs.color.height};
    config.outputExtent = {request.plan.output.width, request.plan.output.height};
    config.quality = request.plan.dlssQuality;
    config.deviceEpoch = request.plan.deviceEpoch;
    config.depthInverted = request.inputs.depthConvention == temporal::DepthConvention::Reversed;
    config.colorSpace = request.inputs.colorEncoding == temporal::ColorEncoding::Sdr ?
        dlss::SrColorSpace::DisplayEncoded : dlss::SrColorSpace::Linear;
    return config;
}
}

TemporalUpscaler::TemporalUpscaler(dlss::Controller& controller)
    : dlss_(controller), fsr_(std::make_unique<fsr::Controller>()) {}
TemporalUpscaler::~TemporalUpscaler() = default;

upscaling::OutputSizing TemporalUpscaler::QuerySizing(const plume::VulkanInterface& api,
    const plume::VulkanDevice& device, const upscaling::SizingKey& key) {
    if (CanQueryNgxSizing(key))
        return dlss_.QueryOutputSizing(api, device, key);
    upscaling::OutputSizing unavailable{};
    unavailable.key = key;
    for (uint32_t i = 0; i < unavailable.modes.size(); ++i) {
        auto& mode = unavailable.modes[i];
        mode.state = upscaling::SizingState::Unavailable;
        if (key.provider == upscaling::Upscaler::Fsr) {
            if (const auto size = fsr::RecommendedRenderSize({key.outputWidth, key.outputHeight}, upscaling::FsrQuality(i))) {
                mode.state = upscaling::SizingState::Ready;
                mode.optimal = mode.minimum = mode.maximum = *size;
            }
        }
    }
    return unavailable;
}

SrResult TemporalUpscaler::Prepare(plume::VulkanDevice& device, const SrRequest& request) {
    SrResult result{};
    result.requestedProvider = request.plan.requestedUpscaler;
    if (!ValidSrRequest(request)) return result;
    if (request.plan.requestedUpscaler == upscaling::Upscaler::Fsr) {
        result.actualProvider = upscaling::Upscaler::Fsr;
        result.status = FsrMetadata(request).cameraValid ? Convert(fsr_->EnsureSession(device, FsrConfig(request))) :
            SrResultStatus::InputUnavailable;
        return result;
    }
    result.actualProvider = upscaling::Upscaler::Dlss;
    result.status = Convert(dlss_.EnsureSession(device));
    return result;
}

SrResult TemporalUpscaler::RecordIsolated(plume::VulkanCommandList& commands,
    const SrRequest& request, plume::VulkanTexture& output, dlss::EvaluateCapture* capture) {
    SrResult result{};
    result.requestedProvider = request.plan.requestedUpscaler;
    if (!ValidSrRequest(request)) return result;
    if (request.plan.requestedUpscaler == upscaling::Upscaler::Fsr) {
        const auto attempt = fsr_->RecordIsolated(commands, FsrConfig(request), request.inputs,
            FsrMetadata(request), output, capture);
        result.actualProvider = upscaling::Upscaler::Fsr;
        result.status = Convert(attempt.status);
        result.rawResult = attempt.sdkResult;
        result.rawVkResult = attempt.vkResult;
        if (attempt.useId) result.token = {upscaling::Upscaler::Fsr, request.plan.deviceEpoch,
            attempt.useId, request.plan.requestSignature, request.plan.geometryEpoch};
        return result;
    }
    const auto attempt = dlss_.RecordIsolated(commands, DlssConfig(request), request.inputs, output, capture);
    result.actualProvider = upscaling::Upscaler::Dlss;
    result.status = Convert(attempt.status);
    result.rawResult = attempt.rawNgxResult;
    result.rawVkResult = attempt.rawVkResult;
    if (attempt.useId) result.token = {upscaling::Upscaler::Dlss, request.plan.deviceEpoch, attempt.useId,
        request.plan.requestSignature, request.plan.geometryEpoch};
    return result;
}

void TemporalUpscaler::OnSubmitted(SrUseToken token, uint64_t checkedSerial) {
    RouteSubmitted(token, checkedSerial, dlss_);
    if (token.provider == upscaling::Upscaler::Fsr && token.useId && checkedSerial) fsr_->OnBatchSubmitted(token.useId, checkedSerial);
}
void TemporalUpscaler::OnDiscarded(SrUseToken token) {
    RouteDiscarded(token, dlss_);
    if (token.provider == upscaling::Upscaler::Fsr && token.useId) fsr_->OnBatchDiscarded(token.useId);
}
void TemporalUpscaler::ReleaseCompleted(uint64_t serial) { if (serial) { dlss_.ReleaseCompletedThrough(serial); fsr_->ReleaseCompletedThrough(serial); } }
bool TemporalUpscaler::HasFeatureState() const { return dlss_.HasFeatureState() || fsr_->HasFeatureState(); }
void TemporalUpscaler::ReleaseFeatureAfterGpuDrain() { dlss_.ReleaseFeatureAfterGpuDrain(); fsr_->ReleaseFeatureAfterGpuDrain(); }
void TemporalUpscaler::ShutdownAfterGpuDrain() { dlss_.ShutdownAfterGpuDrain(); fsr_->ShutdownAfterGpuDrain(); }
void TemporalUpscaler::AbandonAfterDeviceLoss() { dlss_.AbandonUsesAfterDeviceLoss(); fsr_->AbandonUsesAfterDeviceLoss(); }
} // namespace gpu
#endif
