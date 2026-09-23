#pragma once

#include "gpu/render_resolution.h"
#include "gpu/upscaling_plan.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>

struct PPCContext;
namespace settings { struct Config; }

namespace gpu::frame_plan
{
    struct FramePlan
    {
        uint64_t cpuSerial = 0;
        uint64_t geometryEpoch = 0;
        uint32_t width = 1280;
        uint32_t height = 720;
        uint64_t requestSignature = 0;
        uint64_t deviceEpoch = 0;
        uint64_t sizingRevision = 0;
        upscaling::OutputRegion output{};
        uint32_t legacyWidth = 1280;
        uint32_t legacyHeight = 720;
        upscaling::Upscaler requestedUpscaler = upscaling::Upscaler::Off;
        upscaling::DlssQuality dlssQuality = upscaling::DlssQuality::Quality;
        uint32_t legacyAA = 0;
        uint32_t effectiveAA = 0;
        uint32_t scalingQuality = 1;
        upscaling::TemporalConsumer consumer = upscaling::TemporalConsumer::None;
        bool requiresReadback = false;
        bool inputProbe = false;
        bool failed = false;
        upscaling::FsrQuality fsrQuality = upscaling::FsrQuality::Quality;
        upscaling::FrameGeneration frameGeneration = upscaling::FrameGeneration::Off;
        bool operator==(const FramePlan&) const = default;
    };
    enum class SurfaceRole : uint32_t { Unknown = 0, Scene = 1, Fixed = 2 };

    inline FramePlan Choose(uint64_t serial, uint64_t epoch, uint32_t mode, uint32_t drawableWidth, uint32_t drawableHeight, bool resolveReadback = false)
    {
        if (resolveReadback) {
            FramePlan plan{serial, epoch, 1280, 720};
            plan.output = {{drawableWidth, drawableHeight}, 0, 0, drawableWidth, drawableHeight};
            plan.requiresReadback = true;
            return plan;
        }
        const auto size = resolution::ResolveInternalSize(mode, drawableWidth, drawableHeight);
        FramePlan plan{serial, epoch, size.width, size.height};
        plan.legacyWidth = plan.width;
        plan.legacyHeight = plan.height;
        plan.output = {{drawableWidth, drawableHeight}, 0, 0, drawableWidth, drawableHeight};
        return plan;
    }
    inline FramePlan Reduced(FramePlan plan, uint32_t fallbackHeight)
    {
        const uint32_t height = std::clamp(fallbackHeight, 720u, plan.height > 720 ? plan.height - 1 : 720u);
        plan.width = uint32_t((uint64_t(height) * plan.width + plan.height / 2) / plan.height);
        plan.height = height;
        return plan;
    }
    struct FailureState
    {
        FramePlan plan{};
        uint64_t requestSignature = 0;
        uint32_t cappedHeight = 0;
    };
    enum class FailureReason : uint32_t { Unknown, DlssOutOfMemory, DlssUnavailable, SizingUnavailable, BackendUnavailable, InvalidInput };
    struct PlanFailure
    {
        uint64_t geometryEpoch = 0;
        uint64_t requestSignature = 0;
        uint32_t fallbackHeight = 720;
        FailureReason reason = FailureReason::Unknown;
        bool operator==(const PlanFailure&) const = default;
    };
    inline bool MatchesPlanFailure(const FramePlan& plan, const PlanFailure& failure) {
        return plan.cpuSerial && failure.geometryEpoch == plan.geometryEpoch && failure.requestSignature == plan.requestSignature;
    }
    struct PresentationDecision {
        uint32_t requestedAA = 0;
        uint32_t scalingQuality = 1;
        bool bypassAA = false;
    };
    inline PresentationDecision ResolvePresentationDecision(const FramePlan* sourcePlan, bool sceneAAApplied,
        uint32_t fallbackAA, uint32_t fallbackScalingQuality) {
        if (!sourcePlan || !sourcePlan->cpuSerial) return {fallbackAA, fallbackScalingQuality, false};
        return {sourcePlan->effectiveAA, sourcePlan->scalingQuality, sceneAAApplied};
    }
    inline uint64_t FullRequestSignature(const FramePlan& plan, uint32_t internalResolution, resolution::Size recommendedInput) {
        const auto mix = [](uint64_t value, uint64_t input) { return (value ^ input) * 0x9E3779B185EBCA87ull; };
        uint64_t value = 0x6C6F706C616E7631ull;
        value = mix(value, uint32_t(plan.requestedUpscaler));
        value = mix(value, plan.requestedUpscaler == upscaling::Upscaler::Dlss ? uint32_t(plan.dlssQuality) : 0);
        value = mix(value, plan.legacyAA); value = mix(value, plan.scalingQuality); value = mix(value, internalResolution);
        value = mix(value, plan.output.drawable.width); value = mix(value, plan.output.drawable.height);
        value = mix(value, plan.output.x); value = mix(value, plan.output.y); value = mix(value, plan.output.width); value = mix(value, plan.output.height);
        value = mix(value, plan.deviceEpoch); value = mix(value, plan.requiresReadback);
        if (plan.requestedUpscaler == upscaling::Upscaler::Fsr) value = mix(value, uint32_t(plan.fsrQuality));
        if (plan.frameGeneration != upscaling::FrameGeneration::Off) value = mix(value, uint32_t(plan.frameGeneration));
        value = mix(value, recommendedInput.width); return mix(value, recommendedInput.height);
    }
    inline uint64_t RequestSignature(uint32_t mode, uint32_t drawableWidth, uint32_t drawableHeight, bool resolveReadback)
    {
        uint64_t signature = mode;
        signature = signature * 0x9E3779B185EBCA87ull + drawableWidth;
        signature = signature * 0x9E3779B185EBCA87ull + drawableHeight;
        return signature * 2 + resolveReadback + 1;
    }
    struct PlannerInput {
        uint32_t internalResolution = 0, antialiasing = 0, scalingQuality = 1;
        upscaling::Upscaler upscaler = upscaling::Upscaler::Off;
        upscaling::DlssQuality quality = upscaling::DlssQuality::Quality;
        upscaling::OutputRegion output{};
        upscaling::BackendDeviceSnapshot device{};
        const upscaling::OutputSizing* sizing = nullptr;
        bool readback = false, inputProbeRequested = false;
        upscaling::FsrQuality fsrQuality = upscaling::FsrQuality::Quality;
        upscaling::FrameGeneration frameGeneration = upscaling::FrameGeneration::Off;
    };
    inline uint64_t InputRequestSignature(const PlannerInput& input)
    {
        FramePlan request = Choose(0, 0, input.internalResolution, input.output.width, input.output.height, input.readback);
        request.output = input.output;
        request.deviceEpoch = input.device.deviceEpoch;
        request.requestedUpscaler = input.upscaler;
        request.dlssQuality = upscaling::NormalizeDlssQuality(input.quality);
        request.fsrQuality = upscaling::NormalizeFsrQuality(input.fsrQuality);
        request.frameGeneration = input.frameGeneration;
        request.legacyAA = input.antialiasing;
        request.scalingQuality = input.scalingQuality;
        request.requiresReadback = input.readback;
        resolution::Size recommended{request.width, request.height};
        if (input.upscaler == upscaling::Upscaler::Dlss && input.sizing &&
            input.sizing->key.provider == upscaling::Upscaler::Dlss &&
            input.sizing->key.deviceEpoch == input.device.deviceEpoch &&
            input.sizing->key.outputWidth == input.output.width && input.sizing->key.outputHeight == input.output.height &&
            input.sizing->key.outputX == input.output.x && input.sizing->key.outputY == input.output.y) {
            const auto& mode = input.sizing->modes[upscaling::DlssQualityIndex(input.quality)];
            if (upscaling::ModeReadyForOutput(mode, request.dlssQuality, {input.output.width, input.output.height}))
                recommended = mode.optimal;
        }
        if (input.upscaler == upscaling::Upscaler::Fsr && input.sizing &&
            input.sizing->key == upscaling::SizingKey{input.device.deviceEpoch, input.output.width,
                input.output.height, upscaling::Upscaler::Fsr, input.output.x, input.output.y}) {
            const auto& mode = input.sizing->modes[uint32_t(request.fsrQuality)];
            if (mode.state == upscaling::SizingState::Ready && mode.optimal.width && mode.optimal.height &&
                mode.optimal.width <= input.output.width && mode.optimal.height <= input.output.height &&
                (request.fsrQuality != upscaling::FsrQuality::NativeAA || mode.optimal == resolution::Size{input.output.width,input.output.height}))
                recommended = mode.optimal;
        }
        return FullRequestSignature(request, input.internalResolution, recommended);
    }
    inline FramePlan AdvanceCpuPlan(FailureState& state, uint64_t cpuSerial, uint64_t& geometryEpoch,
        uint64_t reportedFailedEpoch, uint32_t fallbackHeight, uint32_t mode,
        uint32_t drawableWidth, uint32_t drawableHeight, bool resolveReadback);
    enum class DlssExecutionOutcome : uint8_t { Submitted, Fallback };
    enum class DlssEffectReason : uint8_t {
        None = 0,
        AwaitingGpuFrame,
        DeviceNotReady,
        NeedsVulkanRestart,
        CapabilityUnavailable,
        SizingPending,
        SizingUnavailable,
        SizingError,
        InputProbeOnly,
        NoEligibleScene,
        MotionPipelinePending,
        UnknownColorEncoding,
        FeatureReconfigurePending,
        PromotionUnavailable,
        RequestFailure,
        GpuWorkStopped,
        UnsupportedProjection,
    };
    struct DlssExecutionObservation {
        FramePlan plan{};
        uint64_t renderFrame = 0;
        uint64_t submissionSerial = 0;
        DlssExecutionOutcome outcome = DlssExecutionOutcome::Fallback;
        DlssEffectReason reason = DlssEffectReason::NoEligibleScene;
        upscaling::Upscaler actualProvider = upscaling::Upscaler::Dlss;
    };
    using UpscalerExecutionObservation = DlssExecutionObservation; // Source compatibility for the existing NGX status fixtures.
    struct PlannerObservation {
        bool hasPlan = false;
        FramePlan plan{};
        std::optional<PlanFailure> latched;
        std::optional<FailureReason> persistentFailure;
        std::optional<DlssExecutionObservation> execution;
    };
    class PlannerState {
    public:
        FramePlan Begin(PlannerInput input) {
            input.quality = upscaling::NormalizeDlssQuality(input.quality);
            input.fsrQuality = upscaling::NormalizeFsrQuality(input.fsrQuality);
            std::lock_guard lock(mutex_);
            const uint64_t incomingSignature = InputRequestSignature(input);
            const bool newRequest = !lastFinal_ || lastFinal_->requestSignature != incomingSignature;
            if (newRequest) {
                legacy_ = {};
                dlssDisabled_.reset();
                ClearAttemptMailbox();
            }
            const bool matchesLatchedAttempt = latched_ && lastFinal_ &&
                MatchesPlanFailure(*lastFinal_, *latched_) && latched_->requestSignature == incomingSignature;
            const bool legacyRetry = matchesLatchedAttempt &&
                !upscaling::IsDlssConsumer(lastFinal_->consumer) && !upscaling::IsSrConsumer(lastFinal_->consumer);

            FramePlan p = AdvanceCpuPlan(legacy_, ++serial_, epoch_,
                legacyRetry ? latched_->geometryEpoch : ~0ull,
                legacyRetry ? latched_->fallbackHeight : 720,
                input.internalResolution, input.output.width, input.output.height, input.readback);
            p.output = input.output;
            p.legacyWidth = p.width;
            p.legacyHeight = p.height;
            p.deviceEpoch = input.device.deviceEpoch;
            p.requestedUpscaler = input.upscaler;
            p.dlssQuality = input.quality;
            p.fsrQuality = input.fsrQuality;
            p.frameGeneration = input.frameGeneration;
            p.legacyAA = input.antialiasing;
            p.effectiveAA = input.antialiasing;
            p.scalingQuality = input.scalingQuality;
            p.requiresReadback = input.readback;
            p.inputProbe = !input.readback && input.inputProbeRequested && input.upscaler == upscaling::Upscaler::Dlss;
            p.consumer = p.legacyAA == 3 ? upscaling::TemporalConsumer::LegacyTaa : upscaling::TemporalConsumer::None;

            const auto requestedBase = Choose(0, 0, input.internalResolution, input.output.width, input.output.height, input.readback);
            resolution::Size recommended{requestedBase.width, requestedBase.height};
            if (input.upscaler == upscaling::Upscaler::Dlss && input.sizing &&
                input.sizing->key.provider == upscaling::Upscaler::Dlss &&
                input.sizing->key.deviceEpoch == input.device.deviceEpoch &&
                input.sizing->key.outputWidth == input.output.width && input.sizing->key.outputHeight == input.output.height &&
                input.sizing->key.outputX == input.output.x && input.sizing->key.outputY == input.output.y) {
                const auto& mode = input.sizing->modes[upscaling::DlssQualityIndex(input.quality)];
                p.sizingRevision = input.sizing->revision;
                const bool modeReady = upscaling::ModeReadyForOutput(mode, input.quality,
                    {input.output.width, input.output.height});
                if (modeReady) recommended = mode.optimal;
                if (p.inputProbe && input.device.Available(upscaling::Upscaler::Dlss) && modeReady) {
                    p.width = mode.optimal.width;
                    p.height = mode.optimal.height;
                    p.effectiveAA = 0;
                    p.consumer = upscaling::TemporalConsumer::DlssInputs;
                } else if (!input.readback && !p.inputProbe &&
                    input.device.Available(upscaling::Upscaler::Dlss) && modeReady) {
                    p.width = mode.optimal.width;
                    p.height = mode.optimal.height;
                    p.effectiveAA = 0;
                    p.consumer = upscaling::TemporalConsumer::DlssSr;
                }
            }
            if (input.upscaler == upscaling::Upscaler::Fsr && input.sizing &&
                input.sizing->key == upscaling::SizingKey{input.device.deviceEpoch, input.output.width,
                    input.output.height, upscaling::Upscaler::Fsr, input.output.x, input.output.y}) {
                const auto& mode = input.sizing->modes[uint32_t(input.fsrQuality)];
                p.sizingRevision = input.sizing->revision;
                const bool ready = mode.state == upscaling::SizingState::Ready &&
                    mode.optimal.width && mode.optimal.height && mode.optimal.width <= input.output.width &&
                    mode.optimal.height <= input.output.height &&
                    (input.fsrQuality != upscaling::FsrQuality::NativeAA ||
                        mode.optimal == resolution::Size{input.output.width, input.output.height});
                if (ready) recommended = mode.optimal;
                if (ready && !input.readback && input.device.Available(upscaling::Upscaler::Fsr)) {
                    p.width = mode.optimal.width;
                    p.height = mode.optimal.height;
                    p.effectiveAA = 0;
                    p.consumer = upscaling::TemporalConsumer::FsrSr;
                }
            }
            p.requestSignature = FullRequestSignature(p, input.internalResolution, recommended);

            const auto selectLegacy = [&] {
                p.inputProbe = false;
                p.width = p.legacyWidth;
                p.height = p.legacyHeight;
                p.effectiveAA = p.legacyAA;
                p.consumer = p.legacyAA == 3 ? upscaling::TemporalConsumer::LegacyTaa : upscaling::TemporalConsumer::None;
            };
            const bool dlssDisabled = dlssDisabled_ && dlssDisabled_->signature == p.requestSignature;
            if (legacyRetry || dlssDisabled) {
                selectLegacy();
            }
            if (latched_) ClearAttemptMailbox();

            const bool changed = !lastFinal_ || lastFinal_->requestSignature != p.requestSignature ||
                lastFinal_->width != p.width || lastFinal_->height != p.height ||
                lastFinal_->consumer != p.consumer ||
                !upscaling::SameEffectiveQuality(p.requestedUpscaler, lastFinal_->dlssQuality, p.dlssQuality,
                    lastFinal_->fsrQuality, p.fsrQuality) || lastFinal_->frameGeneration != p.frameGeneration ||
                lastFinal_->output != p.output || lastFinal_->deviceEpoch != p.deviceEpoch;
            if (lastFinal_) p.geometryEpoch = changed ? ++epoch_ : lastFinal_->geometryEpoch;
            // A changed geometry is a new attempt. Drop an execution that belongs
            // to the previous one so quality/size A-B-A cannot revive the first A.
            if (changed && execution_) {
                const bool sameExecution = execution_->plan.deviceEpoch == p.deviceEpoch &&
                    execution_->plan.requestSignature == p.requestSignature &&
                    execution_->plan.geometryEpoch == p.geometryEpoch &&
                    execution_->plan.requestedUpscaler == p.requestedUpscaler;
                if (!sameExecution) execution_.reset();
            }
            if (!upscaling::IsDlssConsumer(p.consumer) && !upscaling::IsSrConsumer(p.consumer)) legacy_.plan = p;
            lastFinal_ = p;

            bool known = false;
            for (size_t i = 0; i < attemptCount_; ++i)
                known |= attempts_[i].geometryEpoch == p.geometryEpoch && attempts_[i].requestSignature == p.requestSignature;
            if (!known) {
                attempts_[nextAttempt_] = p;
                nextAttempt_ = (nextAttempt_ + 1) % attempts_.size();
                if (attemptCount_ < attempts_.size()) ++attemptCount_;
            }
            return p;
        }
        PlannerObservation Observe()
        {
            std::lock_guard lock(mutex_);
            PlannerObservation observed;
            if (lastFinal_) {
                observed.hasPlan = true;
                observed.plan = *lastFinal_;
            }
            observed.latched = latched_;
            if (dlssDisabled_ && lastFinal_ && dlssDisabled_->signature == lastFinal_->requestSignature)
                observed.persistentFailure = dlssDisabled_->reason;
            observed.execution = execution_;
            return observed;
        }
        bool ReportFailure(const PlanFailure& failure)
        {
            std::lock_guard lock(mutex_);
            for (size_t i = 0; i < attemptCount_; ++i) {
                if (MatchesPlanFailure(attempts_[i], failure)) {
                    latched_ = failure;
                    if (upscaling::IsDlssConsumer(attempts_[i].consumer) || upscaling::IsSrConsumer(attempts_[i].consumer))
                        dlssDisabled_ = DisabledRequest{failure.requestSignature, failure.reason};
                    return true;
                }
            }
            return false;
        }
        // GPU frames may lag the newest CPU serial. Identity is the device,
        // request, and geometry; an older render frame never replaces a newer one.
        bool ReportExecution(const DlssExecutionObservation& observation)
        {
            return ReportUpscalerExecution(observation);
        }
        bool ReportUpscalerExecution(const UpscalerExecutionObservation& observation)
        {
            std::lock_guard lock(mutex_);
            if (!lastFinal_) return false;
            const auto& plan = observation.plan;
            if (!plan.cpuSerial || plan.cpuSerial > lastFinal_->cpuSerial) return false;
            if (plan.requestedUpscaler != lastFinal_->requestedUpscaler ||
                plan.deviceEpoch != lastFinal_->deviceEpoch || plan.requestSignature != lastFinal_->requestSignature ||
                plan.geometryEpoch != lastFinal_->geometryEpoch) return false;
            if (observation.outcome == DlssExecutionOutcome::Submitted &&
                (!upscaling::MatchesSrProvider(observation.actualProvider, plan.consumer) ||
                 plan.requestedUpscaler != observation.actualProvider || !observation.submissionSerial ||
                 plan.consumer != lastFinal_->consumer)) return false;
            if (observation.outcome == DlssExecutionOutcome::Fallback &&
                observation.actualProvider != plan.requestedUpscaler) return false;
            if (execution_ && observation.renderFrame <= execution_->renderFrame) return false;
            execution_ = observation;
            return true;
        }
    private:
        void ClearAttemptMailbox()
        {
            latched_.reset();
            attemptCount_ = 0;
            nextAttempt_ = 0;
        }
        struct DisabledRequest {
            uint64_t signature = 0;
            FailureReason reason = FailureReason::Unknown;
        };
        std::mutex mutex_;
        uint64_t serial_ = 0, epoch_ = 0;
        FailureState legacy_{};
        std::optional<PlanFailure> latched_;
        // Survives ClearAttemptMailbox. newRequest clears it with the old latch.
        std::optional<DisabledRequest> dlssDisabled_;
        std::array<FramePlan, 8> attempts_{};
        size_t attemptCount_ = 0;
        size_t nextAttempt_ = 0;
        std::optional<FramePlan> lastFinal_;
        std::optional<DlssExecutionObservation> execution_;
    };

    // Active is only a matching submitted DLSS SR frame. The appended values
    // name waits and stops that are not a submitted effect.
    enum class DlssEffectPhase : uint8_t {
        Inactive = 0,
        Active,
        NeedsVulkanRestart,
        DeviceUnavailable,
        TemporaryFallback,
        AwaitingExecution,
        InputProbeOnly,
        GpuStopped,
    };
    // Menu classification for the values currently shown, which may not have
    // been applied. BackendChangePending means the edited backend is not the
    // committed device, so its DLSS capability is still unknown.
    enum class DlssMenuStatus : uint8_t {
        Inactive = 0,
        Active,
        NeedsVulkanRestart,
        DeviceUnavailable,
        TemporaryFallback,
        BackendChangePending,
    };
    struct DlssEffectSnapshot {
        upscaling::BackendDeviceSnapshot device{};
        bool hasPlan = false;
        upscaling::Upscaler plannedRequest = upscaling::Upscaler::Off;
        upscaling::DlssQuality plannedQuality = upscaling::DlssQuality::Quality;
        upscaling::TemporalConsumer consumer = upscaling::TemporalConsumer::None;
        uint32_t inputWidth = 0, inputHeight = 0;
        uint32_t outputWidth = 0, outputHeight = 0;
        // CPU plan identity for a status line. Not a second classification.
        uint64_t cpuSerial = 0, geometryEpoch = 0, requestSignature = 0;
        upscaling::SizingState sizingState = upscaling::SizingState::Pending;
        bool sizingKnown = false;
        std::optional<FailureReason> failure;
        DlssEffectReason reason = DlssEffectReason::None;
        // Present only when this device may still show that GPU frame.
        // plannedQuality and the sizes above stay the CPU plan.
        std::optional<DlssExecutionObservation> execution;
        DlssEffectPhase phase = DlssEffectPhase::Inactive;
    };
    inline DlssEffectSnapshot DescribeDlssRuntime(const upscaling::BackendDeviceSnapshot& device,
        const PlannerObservation& observed, const upscaling::OutputSizing* sizing)
    {
        DlssEffectSnapshot snapshot;
        snapshot.device = device;
        snapshot.hasPlan = observed.hasPlan;
        if (observed.hasPlan) {
            snapshot.plannedRequest = observed.plan.requestedUpscaler;
            snapshot.plannedQuality = observed.plan.dlssQuality;
            snapshot.consumer = observed.plan.consumer;
            snapshot.inputWidth = observed.plan.width;
            snapshot.inputHeight = observed.plan.height;
            snapshot.outputWidth = observed.plan.output.width;
            snapshot.outputHeight = observed.plan.output.height;
            snapshot.cpuSerial = observed.plan.cpuSerial;
            snapshot.geometryEpoch = observed.plan.geometryEpoch;
            snapshot.requestSignature = observed.plan.requestSignature;
            if (observed.persistentFailure) snapshot.failure = observed.persistentFailure;
            else if (observed.latched && MatchesPlanFailure(observed.plan, *observed.latched))
                snapshot.failure = observed.latched->reason;
        }
        bool sizingReady = false;
        if (sizing && observed.hasPlan && sizing->key.provider == upscaling::Upscaler::Dlss &&
            observed.plan.requestedUpscaler == upscaling::Upscaler::Dlss &&
            sizing->key.outputX == observed.plan.output.x && sizing->key.outputY == observed.plan.output.y &&
            sizing->key.deviceEpoch == device.deviceEpoch &&
            sizing->key.deviceEpoch == observed.plan.deviceEpoch &&
            sizing->key.outputWidth == observed.plan.output.width &&
            sizing->key.outputHeight == observed.plan.output.height) {
            const auto& mode = sizing->modes[upscaling::DlssQualityIndex(observed.plan.dlssQuality)];
            snapshot.sizingState = mode.state;
            snapshot.sizingKnown = true;
            sizingReady = upscaling::ModeReadyForOutput(mode, observed.plan.dlssQuality,
                {observed.plan.output.width, observed.plan.output.height});
        }
        const bool wantsDlss = observed.hasPlan && snapshot.plannedRequest == upscaling::Upscaler::Dlss;
        const bool epochMatches = observed.hasPlan && device.deviceEpoch == observed.plan.deviceEpoch;
        const bool executionMatches = wantsDlss && observed.execution && epochMatches &&
            observed.execution->plan.requestedUpscaler == upscaling::Upscaler::Dlss &&
            observed.execution->plan.requestedUpscaler == observed.plan.requestedUpscaler &&
            observed.execution->plan.deviceEpoch == observed.plan.deviceEpoch &&
            observed.execution->plan.requestSignature == observed.plan.requestSignature &&
            observed.execution->plan.geometryEpoch == observed.plan.geometryEpoch &&
            observed.execution->plan.cpuSerial && observed.execution->plan.cpuSerial <= observed.plan.cpuSerial;
        const bool exposeExecution = executionMatches && device.deviceReady && !device.gpuWorkStopped;
        const auto finish = [&](DlssEffectPhase phase, DlssEffectReason reason, bool showExecution) {
            snapshot.phase = phase;
            snapshot.reason = reason;
            if (showExecution && exposeExecution) snapshot.execution = observed.execution;
            return snapshot;
        };
        if (!wantsDlss) return finish(DlssEffectPhase::Inactive, DlssEffectReason::None, false);
        if (device.gpuWorkStopped) return finish(DlssEffectPhase::GpuStopped, DlssEffectReason::GpuWorkStopped, false);
        if (device.backend != backend::Backend::Vulkan)
            return finish(DlssEffectPhase::NeedsVulkanRestart, DlssEffectReason::NeedsVulkanRestart, false);
        if (!device.deviceReady || !epochMatches)
            return finish(DlssEffectPhase::TemporaryFallback, DlssEffectReason::DeviceNotReady, false);
        if (!device.dlssAvailable)
            return finish(DlssEffectPhase::DeviceUnavailable, DlssEffectReason::CapabilityUnavailable, false);
        if (snapshot.failure) return finish(DlssEffectPhase::TemporaryFallback, DlssEffectReason::RequestFailure, false);
        if (!snapshot.sizingKnown || snapshot.sizingState == upscaling::SizingState::Pending)
            return finish(DlssEffectPhase::TemporaryFallback, DlssEffectReason::SizingPending, false);
        if (snapshot.sizingState == upscaling::SizingState::Unavailable)
            return finish(DlssEffectPhase::TemporaryFallback, DlssEffectReason::SizingUnavailable, false);
        if (snapshot.sizingState == upscaling::SizingState::Error || !sizingReady)
            return finish(DlssEffectPhase::TemporaryFallback, DlssEffectReason::SizingError, false);
        if (snapshot.consumer == upscaling::TemporalConsumer::DlssInputs)
            return finish(DlssEffectPhase::InputProbeOnly, DlssEffectReason::InputProbeOnly, false);
        if (snapshot.consumer != upscaling::TemporalConsumer::DlssSr)
            return finish(DlssEffectPhase::TemporaryFallback, DlssEffectReason::None, false);
        if (!exposeExecution) return finish(DlssEffectPhase::AwaitingExecution, DlssEffectReason::AwaitingGpuFrame, false);
        if (observed.execution->outcome != DlssExecutionOutcome::Submitted || !observed.execution->submissionSerial ||
            observed.execution->plan.consumer != upscaling::TemporalConsumer::DlssSr)
            return finish(DlssEffectPhase::TemporaryFallback, observed.execution->reason, true);
        return finish(DlssEffectPhase::Active, DlssEffectReason::None, true);
    }
    inline DlssMenuStatus ClassifyDlssMenu(const DlssEffectSnapshot& running, upscaling::Upscaler displayedUpscaler,
        upscaling::DlssQuality displayedQuality, backend::Backend displayedBackend)
    {
        if (displayedBackend != running.device.backend) return DlssMenuStatus::BackendChangePending;
        if (displayedUpscaler != upscaling::Upscaler::Dlss) return DlssMenuStatus::Inactive;
        if (running.device.backend != backend::Backend::Vulkan) return DlssMenuStatus::NeedsVulkanRestart;
        if (running.device.deviceReady && !running.device.dlssAvailable) return DlssMenuStatus::DeviceUnavailable;
        if (!running.device.deviceReady) return DlssMenuStatus::TemporaryFallback;
        if (running.phase == DlssEffectPhase::Active && running.plannedRequest == upscaling::Upscaler::Dlss &&
            running.plannedQuality == upscaling::NormalizeDlssQuality(displayedQuality))
            return DlssMenuStatus::Active;
        return DlssMenuStatus::TemporaryFallback;
    }
    // The CPU owns retry sizing. A failure only caps the matching request, so a
    // later tick cannot recreate its original oversized plan before the GPU has
    // reported a different request.
    inline FramePlan AdvanceCpuPlan(FailureState& state, uint64_t cpuSerial, uint64_t& geometryEpoch,
        uint64_t reportedFailedEpoch, uint32_t fallbackHeight, uint32_t mode,
        uint32_t drawableWidth, uint32_t drawableHeight, bool resolveReadback)
    {
        const uint64_t signature = RequestSignature(mode, drawableWidth, drawableHeight, resolveReadback);
        if (state.requestSignature != signature)
        {
            state.requestSignature = signature;
            state.plan = {};
            state.cappedHeight = 0;
        }
        FramePlan requested = Choose(cpuSerial, geometryEpoch, mode, drawableWidth, drawableHeight, resolveReadback);
        if (state.cappedHeight && requested.height > state.cappedHeight)
            requested = Reduced(requested, state.cappedHeight);
        const bool failedCurrent = state.plan.cpuSerial && reportedFailedEpoch == state.plan.geometryEpoch;
        if (failedCurrent)
        {
            if (state.plan.height <= 720)
            {
                state.plan.cpuSerial = cpuSerial;
                state.plan.failed = true;
                return state.plan;
            }
            state.cappedHeight = std::clamp(fallbackHeight, 720u, state.plan.height - 1);
            requested = Reduced(Choose(cpuSerial, geometryEpoch, mode, drawableWidth, drawableHeight, resolveReadback), state.cappedHeight);
        }
        if (!state.plan.cpuSerial || failedCurrent || requested.width != state.plan.width || requested.height != state.plan.height)
            ++geometryEpoch;
        requested.geometryEpoch = geometryEpoch;
        requested.failed = false;
        state.plan = requested;
        return state.plan;
    }

    class CommandTags
    {
    public:
        void Store(uint32_t address, FramePlan plan) { std::lock_guard lock(m_mutex); m_tags[address] = plan; }
        std::optional<FramePlan> Take(uint32_t address)
        {
            std::lock_guard lock(m_mutex);
            const auto found = m_tags.find(address);
            if (found == m_tags.end()) return std::nullopt;
            const auto plan = found->second;
            m_tags.erase(found);
            return plan;
        }
    private:
        std::mutex m_mutex;
        std::unordered_map<uint32_t, FramePlan> m_tags;
    };

    namespace wire
    {
        constexpr uint32_t PlanBase = 0x7F20;
        constexpr uint32_t CatalogBase = 0x7F40;
        // Keep both older packet layouts readable at this same register base.
        constexpr uint32_t Magic = 0x4C4F4650; // "LOFP"
        constexpr uint32_t PlanMagic = 0x4C4F4632; // "LOF2"
        constexpr uint32_t Version = 3;
        constexpr uint32_t PlanWordCount = 24;
        constexpr uint32_t PlanWordCapacity = CatalogBase - PlanBase;
        static_assert(PlanWordCount <= PlanWordCapacity);
        static_assert(PlanBase + PlanWordCount <= CatalogBase);
        constexpr uint32_t CatalogMagic = 0x4C4F4341; // "LOCA"
        constexpr uint32_t PackFlags(const FramePlan& plan)
        {
            return uint32_t(plan.requestedUpscaler) | (uint32_t(plan.dlssQuality) << 2) |
                ((uint32_t(plan.consumer) & 0x3u) << 4) | ((plan.legacyAA & 0xFu) << 6) |
                ((plan.effectiveAA & 0xFu) << 10) | ((plan.scalingQuality & 0xFu) << 14) |
                (uint32_t(plan.requiresReadback) << 18) | (uint32_t(plan.inputProbe) << 19) |
                (uint32_t(plan.failed) << 20) |
                ((uint32_t(plan.consumer) & 4u) << 19) |
                (uint32_t(plan.fsrQuality) << 22) | (uint32_t(plan.frameGeneration) << 24);
        }
        constexpr bool UnpackFlags(FramePlan& plan, uint32_t flags, uint32_t version)
        {
            if (version != 2 && version != Version) return false;
            if (flags & (version == 2 ? ~0x1FFFFFu : ~0x1FFFFFFu)) return false;
            plan.requestedUpscaler = upscaling::Upscaler(flags & 0x3u);
            plan.dlssQuality = upscaling::DlssQuality((flags >> 2) & 0x3u);
            plan.consumer = upscaling::TemporalConsumer(((flags >> 4) & 0x3u) |
                (version == Version ? ((flags >> 19) & 4u) : 0u));
            plan.legacyAA = (flags >> 6) & 0xFu;
            plan.effectiveAA = (flags >> 10) & 0xFu;
            plan.scalingQuality = (flags >> 14) & 0xFu;
            plan.requiresReadback = (flags & (1u << 18)) != 0;
            plan.inputProbe = (flags & (1u << 19)) != 0;
            plan.failed = (flags & (1u << 20)) != 0;
            if (version == Version) {
                plan.fsrQuality = upscaling::FsrQuality((flags >> 22) & 0x3u);
                plan.frameGeneration = upscaling::FrameGeneration((flags >> 24) & 1u);
            }
            return upscaling::KnownUpscaler(plan.requestedUpscaler) &&
                (version != 2 || plan.requestedUpscaler != upscaling::Upscaler::Fsr) &&
                upscaling::KnownDlssQuality(plan.dlssQuality) &&
                upscaling::KnownFsrQuality(plan.fsrQuality) &&
                upscaling::KnownFrameGeneration(plan.frameGeneration) && upscaling::KnownTemporalConsumer(plan.consumer);
        }
        inline std::array<uint32_t, PlanWordCount> EncodePlan(const FramePlan& plan)
        {
            return { PlanMagic, Version, uint32_t(plan.cpuSerial), uint32_t(plan.cpuSerial >> 32),
                uint32_t(plan.geometryEpoch), uint32_t(plan.geometryEpoch >> 32),
                uint32_t(plan.requestSignature), uint32_t(plan.requestSignature >> 32),
                uint32_t(plan.deviceEpoch), uint32_t(plan.deviceEpoch >> 32),
                uint32_t(plan.sizingRevision), uint32_t(plan.sizingRevision >> 32), plan.width, plan.height,
                plan.legacyWidth, plan.legacyHeight, plan.output.drawable.width, plan.output.drawable.height,
                plan.output.x, plan.output.y, plan.output.width, plan.output.height, PackFlags(plan), PlanMagic };
        }
        struct PlanStage
        {
            bool active = false;
            bool legacy = false;
            uint32_t version = 0;
            bool flagsValid = false;
            FramePlan plan{};
            std::optional<FramePlan> Write(uint32_t index, uint32_t value)
            {
                if (index == PlanBase) { legacy = value == Magic; active = legacy || value == PlanMagic;
                    version = 0; flagsValid = false; plan = {}; return std::nullopt; }
                if (!active || index < PlanBase || index >= PlanBase + PlanWordCount) return std::nullopt;
                if (!legacy && index == PlanBase + 1) {
                    if (value != 2 && value != Version) { active = false; return std::nullopt; }
                    version = value;
                }
                if (legacy && index > PlanBase + 7) return std::nullopt;
                if (legacy) {
                    switch (index - PlanBase) {
                    case 1: plan.cpuSerial = (plan.cpuSerial & 0xFFFFFFFF00000000ull) | value; break;
                    case 2: plan.cpuSerial = (plan.cpuSerial & 0xFFFFFFFFull) | (uint64_t(value) << 32); break;
                    case 3: plan.geometryEpoch = (plan.geometryEpoch & 0xFFFFFFFF00000000ull) | value; break;
                    case 4: plan.geometryEpoch = (plan.geometryEpoch & 0xFFFFFFFFull) | (uint64_t(value) << 32); break;
                    case 5: plan.width = value; break;
                    case 6: plan.height = value; break;
                    case 7: active = false; if (value == Magic && plan.width && plan.height) return plan; break;
                    }
                    return std::nullopt;
                }
                switch (index - PlanBase) {
                case 2: plan.cpuSerial = (plan.cpuSerial & 0xFFFFFFFF00000000ull) | value; break;
                case 3: plan.cpuSerial = (plan.cpuSerial & 0xFFFFFFFFull) | (uint64_t(value) << 32); break;
                case 4: plan.geometryEpoch = (plan.geometryEpoch & 0xFFFFFFFF00000000ull) | value; break;
                case 5: plan.geometryEpoch = (plan.geometryEpoch & 0xFFFFFFFFull) | (uint64_t(value) << 32); break;
                case 6: plan.requestSignature = (plan.requestSignature & 0xFFFFFFFF00000000ull) | value; break;
                case 7: plan.requestSignature = (plan.requestSignature & 0xFFFFFFFFull) | (uint64_t(value) << 32); break;
                case 8: plan.deviceEpoch = (plan.deviceEpoch & 0xFFFFFFFF00000000ull) | value; break;
                case 9: plan.deviceEpoch = (plan.deviceEpoch & 0xFFFFFFFFull) | (uint64_t(value) << 32); break;
                case 10: plan.sizingRevision = (plan.sizingRevision & 0xFFFFFFFF00000000ull) | value; break;
                case 11: plan.sizingRevision = (plan.sizingRevision & 0xFFFFFFFFull) | (uint64_t(value) << 32); break;
                case 12: plan.width = value; break;
                case 13: plan.height = value; break;
                case 14: plan.legacyWidth = value; break;
                case 15: plan.legacyHeight = value; break;
                case 16: plan.output.drawable.width = value; break;
                case 17: plan.output.drawable.height = value; break;
                case 18: plan.output.x = value; break;
                case 19: plan.output.y = value; break;
                case 20: plan.output.width = value; break;
                case 21: plan.output.height = value; break;
                case 22: flagsValid = UnpackFlags(plan, value, version); break;
                case 23: active = false; if (value == PlanMagic && flagsValid && plan.width && plan.height) return plan; break;
                }
                return std::nullopt;
            }
        };
        struct CatalogStage
        {
            bool active = false;
            SurfaceRole role = SurfaceRole::Unknown;
            uint32_t surfaceInfo = 0, colorInfo = 0;
            bool Write(uint32_t index, uint32_t value, SurfaceRole& committedRole, uint32_t& committedSurface, uint32_t& committedColor);
        };
    }

    void PublishDrawable(uint32_t width, uint32_t height);
    void BeginCpuFrame();
    FramePlan CpuPlan();
    // Copy of the committed device, the last CPU plan, the cached sizing, and
    // the newest matching GPU execution. Safe on the UI thread. Do not call it
    // from inside PlannerState::Begin or while already holding the sizing or
    // device lock. It does not read settings, NGX, or renderer objects.
    // phase == Active only for a matching submitted DLSS SR frame. A renderer
    // rejection is included only after ReportPlanFailure; a skip that does not
    // report one leaves the planned consumer in place.
    DlssEffectSnapshot CurrentDlssEffect();
    void ReportDlssExecution(const DlssExecutionObservation& observation);
    void ReportUpscalerExecution(const UpscalerExecutionObservation& observation);
    std::optional<UpscalerExecutionObservation> CurrentUpscalerExecution();
    // One classified snapshot after a device publish. Not a per-frame hook.
    void NoteCurrentDlssStatus();
    // A full P1 producer takes Config, drawable, and one immutable backend/device
    // snapshot. Implementation remains with lane A.
    FramePlan BeginCpuFrame(const settings::Config& config, const upscaling::OutputRegion& output,
        const upscaling::BackendDeviceSnapshot& device, upscaling::SizingCache& sizing);
    std::optional<upscaling::SizingKey> TakeSizingRequest();
    void PublishSizing(upscaling::OutputSizing sizing);
    void ResetSizing(uint64_t deviceEpoch);
    std::optional<FramePlan> CurrentProducerPlan();
    void ReportFailedEpoch(uint64_t geometryEpoch, uint32_t fallbackHeight);
    void ReportPlanFailure(const PlanFailure& failure);
    void TagReservedCommand(uint32_t ring, uint32_t commandAddress);
    bool BeginRenderCommand(uint32_t commandAddress);
    void BeginTaggedRenderCommand(uint32_t commandAddress, uint32_t currentStack);
    void EndRenderCommand();
    std::optional<FramePlan> RenderPlan();
    void DeviceStreamReady(uint32_t device);
    void DeviceStreamReadyFromContext(uint32_t device, const PPCContext& context);
    void DeviceStreamDestroyed();
    void EnsureCurrentPlanQueued(uint32_t device, uint32_t currentStack);
    bool QueuePlanOnDevice(PPCContext& context, uint32_t device, const FramePlan& plan);
    bool QueueCatalogOnDevice(PPCContext& context, uint32_t device, SurfaceRole role, uint32_t surfaceInfo, uint32_t colorInfo);
    bool QueueMainDisplayCatalog(PPCContext& context, uint32_t device);
    // Shared host-private Type-0 writer. It copies a live PPC context, gives
    // the helper a private guest stack/backlink and never keeps host locks
    // while the guest command writer may roll over its ring.
    bool EmitPrivatePacket(uint32_t device, uint32_t registerBase, std::span<const uint32_t> words);

    // First event, then again at fixed counts. `repeats` is the number of
    // later identical events; a reason change resets it and logs immediately.
    inline bool EmitSparseRepeat(uint32_t& repeats, bool changed)
    {
        if (changed) {
            repeats = 0;
            return true;
        }
        if (repeats == ~0u) return false;
        ++repeats;
        return repeats == 64 || repeats == 256 || repeats == 1024 || repeats == 4096 || repeats == 16384;
    }
}
