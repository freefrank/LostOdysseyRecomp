#pragma once

#include "frame_plan.h"
#include "temporal_jitter.h"

#include <cstdint>

namespace plume { struct RenderTexture; }

namespace gpu::temporal {

// A texture's allocation and sampled subregion are deliberately separate. NGX
// must never infer a valid rectangle from an allocation that contains padding.
struct TextureRegion {
    plume::RenderTexture* texture = nullptr;
    resolution::Size allocation{};
    uint32_t x = 0, y = 0, width = 0, height = 0;
    bool Complete() const {
        // Reject overflowing offsets/extents before any native image access.
        return texture && width && height && x <= allocation.width && y <= allocation.height &&
            width <= allocation.width - x && height <= allocation.height - y;
    }
};

// Unknown is the only safe default. An R8_UNORM allocation is storage metadata,
// not proof of an SDR transfer function or of a pre-UI tone-map boundary.
enum class ColorEncoding : uint32_t { Unknown = 0, Sdr = 1, HdrLinear = 2 };
// Storage format does not identify near/far ordering. HistoryOwner's reviewed
// scene anchor uses reversed Z; independent synthetic producers must declare
// their own convention rather than inheriting a renderer default.
enum class DepthConvention : uint32_t { Unknown = 0, Forward = 1, Reversed = 2 };
inline constexpr bool KnownDepthConvention(DepthConvention value) {
    return value == DepthConvention::Forward || value == DepthConvention::Reversed;
}
inline constexpr bool MatchesDepthConvention(DepthConvention value, bool inverted) {
    return KnownDepthConvention(value) && (inverted == (value == DepthConvention::Reversed));
}
enum class MotionState : uint32_t { Unavailable = 0, ResetInitialization = 1, Tracked = 2 };
struct ConsumerRoute {
    bool legacyTaa = false, dlssInputs = false, dlssSr = false, inputProbe = false, spatialAA = false;
    uint32_t effectiveAA = 0;
};
inline constexpr ConsumerRoute RouteConsumer(const frame_plan::FramePlan& plan, bool localInputProbe) {
    ConsumerRoute route{};route.effectiveAA=plan.effectiveAA;
    switch(plan.consumer) {
    case upscaling::TemporalConsumer::LegacyTaa: route.legacyTaa=true;break;
    case upscaling::TemporalConsumer::DlssInputs:
        route.dlssInputs=true;route.inputProbe=localInputProbe&&plan.inputProbe;route.effectiveAA=0;break;
    case upscaling::TemporalConsumer::DlssSr:
        route.dlssInputs=true;route.dlssSr=true;route.effectiveAA=0;break;
    case upscaling::TemporalConsumer::None:
        route.spatialAA=plan.effectiveAA==1||plan.effectiveAA==2;break;
    }
    return route;
}
enum class TemporalResetReason : uint32_t {
    None = 0, FirstFrame = 1u << 0, FrameDiscontinuity = 1u << 1,
    EpochChanged = 1u << 2, AllocationChanged = 1u << 3,
    ExtentChanged = 1u << 4, ColorEncodingChanged = 1u << 5,
    ConsumerChanged = 1u << 6, IncompleteInputs = 1u << 7,
    CameraDiscontinuity = 1u << 8, PlanConfigurationChanged = 1u << 9,
};
inline constexpr TemporalResetReason operator|(TemporalResetReason a, TemporalResetReason b) {
    return TemporalResetReason(uint32_t(a) | uint32_t(b));
}
inline constexpr bool HasResetReason(TemporalResetReason bits, TemporalResetReason value) {
    return (uint32_t(bits) & uint32_t(value)) != 0;
}

// Value-only frame handoff. It owns neither plume images nor NGX objects; image
// lifetime is retained by HistoryOwner and the renderer submission fence.
struct TemporalFrameInputs {
    frame_plan::FramePlan plan{};
    uint64_t renderFrameId = 0, temporalEpoch = 0, depthAllocation = 0;
    TextureRegion color{}, depth{}, motion{};
    TextureRegion motionInvalidity{}, materialInstability{};
    JitterSample jitter{};
    ColorEncoding colorEncoding = ColorEncoding::Unknown;
    DepthConvention depthConvention = DepthConvention::Unknown;
    float preExposure = 1.0f, exposureScale = 1.0f;
    MotionState motionState = MotionState::Unavailable;
    bool currentInputsComplete = false, resetHistory = true;
    TemporalResetReason resetReasons = TemporalResetReason::FirstFrame;

    bool CompleteForConsumer() const {
        if (!currentInputsComplete || !color.Complete() || !depth.Complete()) return false;
        return !upscaling::IsDlssConsumer(plan.consumer) ||
            (KnownDepthConvention(depthConvention) && motionState != MotionState::Unavailable &&
             motion.Complete() && motionInvalidity.Complete());
    }
};
} // namespace gpu::temporal
