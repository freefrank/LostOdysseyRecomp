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
        value = mix(value, uint32_t(plan.requestedUpscaler)); value = mix(value, uint32_t(plan.dlssQuality));
        value = mix(value, plan.legacyAA); value = mix(value, plan.scalingQuality); value = mix(value, internalResolution);
        value = mix(value, plan.output.drawable.width); value = mix(value, plan.output.drawable.height);
        value = mix(value, plan.output.x); value = mix(value, plan.output.y); value = mix(value, plan.output.width); value = mix(value, plan.output.height);
        value = mix(value, plan.deviceEpoch); value = mix(value, plan.requiresReadback);
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
    };
    inline uint64_t InputRequestSignature(const PlannerInput& input)
    {
        FramePlan request = Choose(0, 0, input.internalResolution, input.output.width, input.output.height, input.readback);
        request.output = input.output;
        request.deviceEpoch = input.device.deviceEpoch;
        request.requestedUpscaler = input.upscaler;
        request.dlssQuality = input.quality;
        request.legacyAA = input.antialiasing;
        request.scalingQuality = input.scalingQuality;
        request.requiresReadback = input.readback;
        resolution::Size recommended{request.width, request.height};
        if (input.sizing) {
            const auto& mode = input.sizing->modes[uint32_t(input.quality)];
            if (mode.state == upscaling::SizingState::Ready) recommended = mode.optimal;
        }
        return FullRequestSignature(request, input.internalResolution, recommended);
    }
    inline FramePlan AdvanceCpuPlan(FailureState& state, uint64_t cpuSerial, uint64_t& geometryEpoch,
        uint64_t reportedFailedEpoch, uint32_t fallbackHeight, uint32_t mode,
        uint32_t drawableWidth, uint32_t drawableHeight, bool resolveReadback);
    class PlannerState {
    public:
        FramePlan Begin(const PlannerInput& input) {
            std::lock_guard lock(mutex_);
            const uint64_t incomingSignature = InputRequestSignature(input);
            const bool newRequest = !lastFinal_ || lastFinal_->requestSignature != incomingSignature;
            if (newRequest) {
                legacy_ = {};
                dlssDisabledSignature_.reset();
                ClearAttemptMailbox();
            }
            const bool matchesLatchedAttempt = latched_ && lastFinal_ &&
                MatchesPlanFailure(*lastFinal_, *latched_) && latched_->requestSignature == incomingSignature;
            const bool legacyRetry = matchesLatchedAttempt &&
                !upscaling::IsDlssConsumer(lastFinal_->consumer);

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
            p.legacyAA = input.antialiasing;
            p.effectiveAA = input.antialiasing;
            p.scalingQuality = input.scalingQuality;
            p.requiresReadback = input.readback;
            p.inputProbe = !input.readback && input.inputProbeRequested && input.upscaler == upscaling::Upscaler::Dlss;
            p.consumer = p.legacyAA == 3 ? upscaling::TemporalConsumer::LegacyTaa : upscaling::TemporalConsumer::None;

            const auto requestedBase = Choose(0, 0, input.internalResolution, input.output.width, input.output.height, input.readback);
            resolution::Size recommended{requestedBase.width, requestedBase.height};
            if (input.sizing) {
                const auto& mode = input.sizing->modes[uint32_t(input.quality)];
                p.sizingRevision = input.sizing->revision;
                if (mode.state == upscaling::SizingState::Ready) recommended = mode.optimal;
                if (p.inputProbe && input.device.dlssAvailable && mode.state == upscaling::SizingState::Ready &&
                    mode.optimal.width && mode.optimal.height) {
                    p.width = mode.optimal.width;
                    p.height = mode.optimal.height;
                    p.effectiveAA = 0;
                    p.consumer = upscaling::TemporalConsumer::DlssInputs;
                } else if (input.upscaler == upscaling::Upscaler::Dlss && !input.readback && !p.inputProbe &&
                    input.device.dlssAvailable && mode.state == upscaling::SizingState::Ready &&
                    mode.optimal.width && mode.optimal.height) {
                    p.width = mode.optimal.width;
                    p.height = mode.optimal.height;
                    p.effectiveAA = 0;
                    p.consumer = upscaling::TemporalConsumer::DlssSr;
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
            const bool dlssDisabled = dlssDisabledSignature_ && *dlssDisabledSignature_ == p.requestSignature;
            if (legacyRetry || dlssDisabled) {
                selectLegacy();
            }
            if (latched_) ClearAttemptMailbox();

            const bool changed = !lastFinal_ || lastFinal_->requestSignature != p.requestSignature ||
                lastFinal_->width != p.width || lastFinal_->height != p.height ||
                lastFinal_->consumer != p.consumer || lastFinal_->dlssQuality != p.dlssQuality ||
                lastFinal_->output != p.output || lastFinal_->deviceEpoch != p.deviceEpoch;
            if (lastFinal_) p.geometryEpoch = changed ? ++epoch_ : lastFinal_->geometryEpoch;
            if (!upscaling::IsDlssConsumer(p.consumer)) legacy_.plan = p;
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
        bool ReportFailure(const PlanFailure& failure)
        {
            std::lock_guard lock(mutex_);
            for (size_t i = 0; i < attemptCount_; ++i) {
                if (MatchesPlanFailure(attempts_[i], failure)) {
                    latched_ = failure;
                    if (upscaling::IsDlssConsumer(attempts_[i].consumer))
                        dlssDisabledSignature_ = failure.requestSignature;
                    return true;
                }
            }
            return false;
        }
    private:
        void ClearAttemptMailbox()
        {
            latched_.reset();
            attemptCount_ = 0;
            nextAttempt_ = 0;
        }
        std::mutex mutex_;
        uint64_t serial_ = 0, epoch_ = 0;
        FailureState legacy_{};
        std::optional<PlanFailure> latched_;
        std::optional<uint64_t> dlssDisabledSignature_;
        std::array<FramePlan, 8> attempts_{};
        size_t attemptCount_ = 0;
        size_t nextAttempt_ = 0;
        std::optional<FramePlan> lastFinal_;
    };
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
        // Magic remains the legacy 8-word packet marker until lane A updates
        // the producer. PlanMagic identifies the versioned full snapshot.
        constexpr uint32_t Magic = 0x4C4F4650; // "LOFP"
        constexpr uint32_t PlanMagic = 0x4C4F4632; // "LOF2"
        constexpr uint32_t Version = 2;
        constexpr uint32_t PlanWordCount = 24;
        constexpr uint32_t PlanWordCapacity = CatalogBase - PlanBase;
        static_assert(PlanWordCount <= PlanWordCapacity);
        static_assert(PlanBase + PlanWordCount <= CatalogBase);
        constexpr uint32_t CatalogMagic = 0x4C4F4341; // "LOCA"
        constexpr uint32_t PackFlags(const FramePlan& plan)
        {
            return uint32_t(plan.requestedUpscaler) | (uint32_t(plan.dlssQuality) << 2) |
                (uint32_t(plan.consumer) << 4) | ((plan.legacyAA & 0xFu) << 6) |
                ((plan.effectiveAA & 0xFu) << 10) | ((plan.scalingQuality & 0xFu) << 14) |
                (uint32_t(plan.requiresReadback) << 18) | (uint32_t(plan.inputProbe) << 19) |
                (uint32_t(plan.failed) << 20);
        }
        constexpr void UnpackFlags(FramePlan& plan, uint32_t flags)
        {
            plan.requestedUpscaler = upscaling::Upscaler(flags & 0x3u);
            plan.dlssQuality = upscaling::DlssQuality((flags >> 2) & 0x3u);
            plan.consumer = upscaling::TemporalConsumer((flags >> 4) & 0x3u);
            plan.legacyAA = (flags >> 6) & 0xFu;
            plan.effectiveAA = (flags >> 10) & 0xFu;
            plan.scalingQuality = (flags >> 14) & 0xFu;
            plan.requiresReadback = (flags & (1u << 18)) != 0;
            plan.inputProbe = (flags & (1u << 19)) != 0;
            plan.failed = (flags & (1u << 20)) != 0;
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
            FramePlan plan{};
            std::optional<FramePlan> Write(uint32_t index, uint32_t value)
            {
                if (index == PlanBase) { legacy = value == Magic; active = legacy || value == PlanMagic; plan = {}; return std::nullopt; }
                if (!active || index < PlanBase || index >= PlanBase + PlanWordCount) return std::nullopt;
                if (!legacy && index == PlanBase + 1 && value != Version) { active = false; return std::nullopt; }
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
                case 22: UnpackFlags(plan, value); break;
                case 23: active = false; if (value == PlanMagic && plan.width && plan.height) return plan; break;
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
}
