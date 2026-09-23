#pragma once

#include "gpu/frame_plan.h"
#include "settings/config.h"
#include <os/logger.h>

#include <mutex>
#include <optional>
#include <string>

namespace gpu::frame_plan
{
    inline const char* UpscalerName(upscaling::Upscaler value)
    {
        switch (value) {
        case upscaling::Upscaler::Off: return "Off";
        case upscaling::Upscaler::Dlss: return "Dlss";
        case upscaling::Upscaler::Fsr: return "Fsr";
        }
        return "Unknown";
    }
    inline const char* DlssQualityName(upscaling::DlssQuality value)
    {
        switch (upscaling::NormalizeDlssQuality(value)) {
        case upscaling::DlssQuality::Quality: return "Quality";
        case upscaling::DlssQuality::Balanced: return "Balanced";
        case upscaling::DlssQuality::Performance: return "Performance";
        case upscaling::DlssQuality::Dlaa: return "Dlaa";
        }
        return "Unknown";
    }
    inline const char* TemporalConsumerName(upscaling::TemporalConsumer value)
    {
        switch (value) {
        case upscaling::TemporalConsumer::None: return "None";
        case upscaling::TemporalConsumer::LegacyTaa: return "LegacyTaa";
        case upscaling::TemporalConsumer::DlssInputs: return "DlssInputs";
        case upscaling::TemporalConsumer::DlssSr: return "DlssSr";
        case upscaling::TemporalConsumer::FsrSr: return "FsrSr";
        }
        return "Unknown";
    }
    inline const char* SizingStateName(upscaling::SizingState value)
    {
        switch (value) {
        case upscaling::SizingState::Pending: return "Pending";
        case upscaling::SizingState::Ready: return "Ready";
        case upscaling::SizingState::Unavailable: return "Unavailable";
        case upscaling::SizingState::Error: return "Error";
        }
        return "Unknown";
    }
    inline const char* FailureReasonName(FailureReason value)
    {
        switch (value) {
        case FailureReason::Unknown: return "Unknown";
        case FailureReason::DlssOutOfMemory: return "DlssOutOfMemory";
        case FailureReason::DlssUnavailable: return "DlssUnavailable";
        case FailureReason::SizingUnavailable: return "SizingUnavailable";
        case FailureReason::BackendUnavailable: return "BackendUnavailable";
        case FailureReason::InvalidInput: return "InvalidInput";
        }
        return "Unknown";
    }
    inline const char* DlssEffectReasonName(DlssEffectReason value)
    {
        switch (value) {
        case DlssEffectReason::None: return "None";
        case DlssEffectReason::AwaitingGpuFrame: return "AwaitingGpuFrame";
        case DlssEffectReason::DeviceNotReady: return "DeviceNotReady";
        case DlssEffectReason::NeedsVulkanRestart: return "NeedsVulkanRestart";
        case DlssEffectReason::CapabilityUnavailable: return "CapabilityUnavailable";
        case DlssEffectReason::SizingPending: return "SizingPending";
        case DlssEffectReason::SizingUnavailable: return "SizingUnavailable";
        case DlssEffectReason::SizingError: return "SizingError";
        case DlssEffectReason::InputProbeOnly: return "InputProbeOnly";
        case DlssEffectReason::NoEligibleScene: return "NoEligibleScene";
        case DlssEffectReason::MotionPipelinePending: return "MotionPipelinePending";
        case DlssEffectReason::UnknownColorEncoding: return "UnknownColorEncoding";
        case DlssEffectReason::FeatureReconfigurePending: return "FeatureReconfigurePending";
        case DlssEffectReason::PromotionUnavailable: return "PromotionUnavailable";
        case DlssEffectReason::RequestFailure: return "RequestFailure";
        case DlssEffectReason::GpuWorkStopped: return "GpuWorkStopped";
        case DlssEffectReason::UnsupportedProjection: return "UnsupportedProjection";
        }
        return "Unknown";
    }
    inline const char* DlssEffectPhaseName(DlssEffectPhase value)
    {
        switch (value) {
        case DlssEffectPhase::Inactive: return "Inactive";
        case DlssEffectPhase::Active: return "Active";
        case DlssEffectPhase::NeedsVulkanRestart: return "NeedsVulkanRestart";
        case DlssEffectPhase::DeviceUnavailable: return "DeviceUnavailable";
        case DlssEffectPhase::TemporaryFallback: return "TemporaryFallback";
        case DlssEffectPhase::AwaitingExecution: return "AwaitingExecution";
        case DlssEffectPhase::InputProbeOnly: return "InputProbeOnly";
        case DlssEffectPhase::GpuStopped: return "GpuStopped";
        }
        return "Unknown";
    }

    // Readable label of the existing phase and execution outcome.
    // Submitted means the output was adopted and the checked submit succeeded.
    inline const char* DlssStatusLabel(const DlssEffectSnapshot& snapshot)
    {
        if (snapshot.phase == DlssEffectPhase::Inactive) return "Off";
        if (snapshot.phase == DlssEffectPhase::Active && snapshot.execution &&
            snapshot.execution->outcome == DlssExecutionOutcome::Submitted)
            return "Submitted";
        if (snapshot.execution && snapshot.execution->outcome == DlssExecutionOutcome::Fallback)
            return "Fallback";
        return DlssEffectPhaseName(snapshot.phase);
    }

    inline std::string FormatDlssStatus(const DlssEffectSnapshot& snapshot)
    {
        const bool submitted = snapshot.phase == DlssEffectPhase::Active && snapshot.execution &&
            snapshot.execution->outcome == DlssExecutionOutcome::Submitted;
        const auto quality = snapshot.hasPlan ? snapshot.plannedQuality : upscaling::DlssQuality::Quality;
        const auto request = snapshot.hasPlan ? snapshot.plannedRequest : upscaling::Upscaler::Off;
        std::string line = fmt::format(
            "dlss status={} requested={}({}) quality={}({}) input={}x{} output={}x{} reason={} failure={} sizing={} phase={} consumer={} backend={}({}) device_epoch={} geometry_epoch={} request_signature={:#x} cpu_serial={}",
            DlssStatusLabel(snapshot), UpscalerName(request), uint32_t(request),
            DlssQualityName(quality), uint32_t(quality),
            snapshot.inputWidth, snapshot.inputHeight, snapshot.outputWidth, snapshot.outputHeight,
            DlssEffectReasonName(snapshot.reason),
            snapshot.failure ? FailureReasonName(*snapshot.failure) : "none",
            snapshot.sizingKnown ? fmt::format("{}({})", SizingStateName(snapshot.sizingState), uint32_t(snapshot.sizingState)) : "unknown",
            DlssEffectPhaseName(snapshot.phase), TemporalConsumerName(snapshot.consumer),
            backend::Name(snapshot.device.backend), uint32_t(snapshot.device.backend),
            snapshot.device.deviceEpoch, snapshot.geometryEpoch, snapshot.requestSignature, snapshot.cpuSerial);
        if (snapshot.execution) {
            const auto& plan = snapshot.execution->plan;
            line += fmt::format(" executed_quality={}({}) executed_input={}x{} executed_output={}x{} render_frame={}",
                DlssQualityName(plan.dlssQuality), uint32_t(plan.dlssQuality),
                plan.width, plan.height, plan.output.width, plan.output.height, snapshot.execution->renderFrame);
            if (submitted)
                line += fmt::format(" submission_serial={} submit=checked_not_gpu_complete", snapshot.execution->submissionSerial);
        } else {
            line += " executed_quality=none render_frame=none";
        }
        return line;
    }

    struct DlssStatusLogKey {
        DlssEffectPhase phase = DlssEffectPhase::Inactive;
        DlssEffectReason reason = DlssEffectReason::None;
        bool hasFailure = false;
        FailureReason failure = FailureReason::Unknown;
        upscaling::Upscaler request = upscaling::Upscaler::Off;
        upscaling::DlssQuality quality = upscaling::DlssQuality::Quality;
        upscaling::TemporalConsumer consumer = upscaling::TemporalConsumer::None;
        upscaling::SizingState sizing = upscaling::SizingState::Pending;
        bool sizingKnown = false;
        backend::Backend backend = backend::Backend::D3D12;
        uint32_t inputWidth = 0, inputHeight = 0, outputWidth = 0, outputHeight = 0;
        bool hasExecution = false;
        DlssExecutionOutcome outcome = DlssExecutionOutcome::Fallback;
        upscaling::DlssQuality executedQuality = upscaling::DlssQuality::Quality;
        uint32_t executedWidth = 0, executedHeight = 0, executedOutputWidth = 0, executedOutputHeight = 0;
        uint64_t requestSignature = 0, geometryEpoch = 0, deviceEpoch = 0;
        bool operator==(const DlssStatusLogKey&) const = default;
    };

    inline DlssStatusLogKey StatusLogKey(const DlssEffectSnapshot& snapshot)
    {
        DlssStatusLogKey key;
        key.phase = snapshot.phase;
        key.reason = snapshot.reason;
        key.hasFailure = snapshot.failure.has_value();
        if (snapshot.failure) key.failure = *snapshot.failure;
        key.request = snapshot.plannedRequest;
        key.quality = snapshot.plannedQuality;
        key.consumer = snapshot.consumer;
        key.sizing = snapshot.sizingState;
        key.sizingKnown = snapshot.sizingKnown;
        key.backend = snapshot.device.backend;
        key.inputWidth = snapshot.inputWidth;
        key.inputHeight = snapshot.inputHeight;
        key.outputWidth = snapshot.outputWidth;
        key.outputHeight = snapshot.outputHeight;
        key.requestSignature = snapshot.requestSignature;
        key.geometryEpoch = snapshot.geometryEpoch;
        key.deviceEpoch = snapshot.device.deviceEpoch;
        if (snapshot.execution) {
            key.hasExecution = true;
            key.outcome = snapshot.execution->outcome;
            key.executedQuality = snapshot.execution->plan.dlssQuality;
            key.executedWidth = snapshot.execution->plan.width;
            key.executedHeight = snapshot.execution->plan.height;
            key.executedOutputWidth = snapshot.execution->plan.output.width;
            key.executedOutputHeight = snapshot.execution->plan.output.height;
        }
        return key;
    }

    struct DlssExecutionLogStamp {
        bool valid = false;
        DlssExecutionOutcome outcome = DlssExecutionOutcome::Fallback;
        DlssEffectReason reason = DlssEffectReason::None;
        upscaling::Upscaler request = upscaling::Upscaler::Off;
        upscaling::DlssQuality quality = upscaling::DlssQuality::Quality;
        upscaling::TemporalConsumer consumer = upscaling::TemporalConsumer::None;
        uint32_t width = 0, height = 0, outputWidth = 0, outputHeight = 0;
        uint64_t requestSignature = 0, geometryEpoch = 0, deviceEpoch = 0;
        bool operator==(const DlssExecutionLogStamp&) const = default;
    };

    inline DlssExecutionLogStamp ExecutionStamp(const DlssExecutionObservation& observation)
    {
        DlssExecutionLogStamp stamp;
        stamp.valid = true;
        stamp.outcome = observation.outcome;
        stamp.reason = observation.reason;
        stamp.request = observation.plan.requestedUpscaler;
        stamp.quality = observation.plan.dlssQuality;
        stamp.consumer = observation.plan.consumer;
        stamp.width = observation.plan.width;
        stamp.height = observation.plan.height;
        stamp.outputWidth = observation.plan.output.width;
        stamp.outputHeight = observation.plan.output.height;
        stamp.requestSignature = observation.plan.requestSignature;
        stamp.geometryEpoch = observation.plan.geometryEpoch;
        stamp.deviceEpoch = observation.plan.deviceEpoch;
        return stamp;
    }

    struct DlssStatusLogGate {
        std::mutex mutex;
        bool have = false;
        bool havePrevious = false;
        bool collapsing = false;
        DlssStatusLogKey key{};
        DlssStatusLogKey previous{};
        DlssExecutionLogStamp execution{};
        uint32_t repeats = 0;
        uint32_t jitter = 0;

        void Reset()
        {
            std::lock_guard lock(mutex);
            have = false;
            havePrevious = false;
            collapsing = false;
            key = {};
            previous = {};
            execution = {};
            repeats = 0;
            jitter = 0;
        }

        // True when this observation is a new semantic execution. Frame counters
        // are ignored, so a steady submit does not need another status snapshot.
        bool ExecutionChanged(const DlssExecutionObservation& observation)
        {
            std::lock_guard lock(mutex);
            const auto stamp = ExecutionStamp(observation);
            if (execution.valid && execution == stamp) return false;
            execution = stamp;
            return true;
        }

        std::optional<std::string> Consider(const DlssEffectSnapshot& snapshot)
        {
            std::lock_guard lock(mutex);
            const auto next = StatusLogKey(snapshot);
            if (have && next == key) {
                if (repeats != ~0u) ++repeats;
                if (!collapsing) return std::nullopt;
                collapsing = false;
                auto line = FormatDlssStatus(snapshot);
                line += fmt::format(" repeats={} jitter_changes={}", repeats, jitter);
                jitter = 0;
                return line;
            }
            const uint32_t prior = have ? repeats : 0;
            repeats = 0;
            // A request or quality change, and any state that is not a bounce
            // between the last two keys, is kept. Only that bounce is sampled.
            const bool requestSwitch = have && (next.request != key.request || next.quality != key.quality);
            const bool bounce = have && havePrevious && next == previous && !requestSwitch;
            previous = key;
            havePrevious = have;
            key = next;
            if (!have || !bounce) {
                have = true;
                collapsing = false;
                jitter = 0;
                auto line = FormatDlssStatus(snapshot);
                if (prior) line += fmt::format(" prior_repeats={}", prior);
                return line;
            }
            collapsing = true;
            if (jitter != ~0u) ++jitter;
            if (jitter == 1 || jitter % 16 == 0) {
                auto line = FormatDlssStatus(snapshot);
                line += fmt::format(" jitter_changes={}", jitter);
                return line;
            }
            return std::nullopt;
        }
    };

    inline DlssStatusLogGate& StatusLogGate()
    {
        static DlssStatusLogGate gate;
        return gate;
    }
    inline void ResetDlssStatusLog() { StatusLogGate().Reset(); }
    inline bool ExecutionLogChanged(const DlssExecutionObservation& observation)
    {
        return StatusLogGate().ExecutionChanged(observation);
    }
    inline void NoteDlssRuntime(const DlssEffectSnapshot& snapshot)
    {
        auto line = StatusLogGate().Consider(snapshot);
        if (!line) return;
        LOG_INFO("{}", *line);
    }
}

namespace settings
{
    inline void LogSettingsSaved(const Config& value)
    {
        LOG_INFO("settings saved: {}x{} internal_resolution={} mode={} backend={}({}) AA={} frame_rate={} upscaler={}({}) dlss_quality={}({}) language={} (backend/game language apply at restart)",
            value.width, value.height, value.internalResolution, uint32_t(value.windowMode),
            gpu::backend::Name(value.graphicsBackend), uint32_t(value.graphicsBackend), value.antialiasing, value.frameRate,
            gpu::frame_plan::UpscalerName(value.upscaler), uint32_t(value.upscaler),
            gpu::frame_plan::DlssQualityName(value.dlssQuality), uint32_t(value.dlssQuality), value.gameLanguage);
    }
}
