#pragma once

#include "backend_selection.h"
#include "render_resolution.h"

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>

namespace plume {
struct VulkanInterface;
struct VulkanDevice;
}

namespace gpu { class TemporalUpscaler; }

namespace gpu::upscaling {
enum class Upscaler : uint32_t { Off = 0, Dlss = 1, Fsr = 2 };
inline constexpr bool KnownUpscaler(Upscaler value) { return uint32_t(value) <= uint32_t(Upscaler::Fsr); }
// Persisted IDs and the two-bit frame-plan wire field. Append, never renumber.
enum class DlssQuality : uint32_t { Quality = 0, Balanced = 1, Performance = 2, Dlaa = 3 };
inline constexpr std::array kDlssQualityModes{
    DlssQuality::Quality, DlssQuality::Balanced, DlssQuality::Performance, DlssQuality::Dlaa};
static_assert(uint32_t(DlssQuality::Dlaa) + 1 == kDlssQualityModes.size());
inline constexpr bool KnownDlssQuality(DlssQuality quality) {
    return uint32_t(quality) < kDlssQualityModes.size();
}
inline constexpr DlssQuality NormalizeDlssQuality(DlssQuality quality) {
    return KnownDlssQuality(quality) ? quality : DlssQuality::Quality;
}
inline constexpr uint32_t DlssQualityIndex(DlssQuality quality) {
    return uint32_t(NormalizeDlssQuality(quality));
}
// Persisted independently from DLSS quality; SDK quality IDs are not these IDs.
enum class FsrQuality : uint32_t { Quality = 0, Balanced = 1, Performance = 2, NativeAA = 3 };
inline constexpr bool KnownFsrQuality(FsrQuality value) { return uint32_t(value) <= uint32_t(FsrQuality::NativeAA); }
inline constexpr FsrQuality NormalizeFsrQuality(FsrQuality value) {
    return KnownFsrQuality(value) ? value : FsrQuality::Quality;
}
enum class FrameGeneration : uint32_t { Off = 0, Dlss2x = 1 };
inline constexpr bool KnownFrameGeneration(FrameGeneration value) { return uint32_t(value) <= uint32_t(FrameGeneration::Dlss2x); }
// DLAA consumes the output content extent, not drawable bars or guest padding.
// Compatibility may propose a separately labelled native trial, but actual
// DLAA resources and SDK creation/evaluation must still use real 1:1 extents.
inline constexpr bool ValidDlssRenderExtent(DlssQuality quality, resolution::Size render,
    resolution::Size output) {
    return KnownDlssQuality(quality) && render.width && render.height && output.width && output.height &&
        (quality != DlssQuality::Dlaa || render == output);
}
enum class TemporalConsumer : uint32_t { None = 0, LegacyTaa = 1, DlssInputs = 2, DlssSr = 3, FsrSr = 4 };
inline constexpr bool KnownTemporalConsumer(TemporalConsumer value) { return uint32_t(value) <= uint32_t(TemporalConsumer::FsrSr); }
inline constexpr bool RequiresMotionDepth(TemporalConsumer consumer, FrameGeneration frameGeneration = FrameGeneration::Off) {
    return consumer == TemporalConsumer::DlssInputs || consumer == TemporalConsumer::DlssSr ||
        consumer == TemporalConsumer::FsrSr || frameGeneration != FrameGeneration::Off;
}
// P1's input-only route and P2's native SR route have different output handling,
// but both require the same complete temporal input contract and request-level
// fallback semantics.
inline constexpr bool IsDlssConsumer(TemporalConsumer consumer) {
    return consumer == TemporalConsumer::DlssInputs || consumer == TemporalConsumer::DlssSr;
}
inline constexpr bool IsSrConsumer(TemporalConsumer consumer) {
    return consumer == TemporalConsumer::DlssSr || consumer == TemporalConsumer::FsrSr;
}
inline constexpr Upscaler ProviderForConsumer(TemporalConsumer consumer) {
    return consumer == TemporalConsumer::FsrSr ? Upscaler::Fsr : IsDlssConsumer(consumer) ? Upscaler::Dlss : Upscaler::Off;
}
inline constexpr bool MatchesSrProvider(Upscaler provider, TemporalConsumer consumer) {
    return IsSrConsumer(consumer) && ProviderForConsumer(consumer) == provider;
}
inline constexpr bool SameEffectiveQuality(Upscaler provider, DlssQuality aDlss, DlssQuality bDlss,
    FsrQuality aFsr, FsrQuality bFsr) {
    switch (provider) {
    case Upscaler::Dlss: return aDlss == bDlss;
    case Upscaler::Fsr: return aFsr == bFsr;
    case Upscaler::Off: return true;
    }
    return false;
}

struct OutputRegion {
    resolution::Size drawable{};
    uint32_t x = 0, y = 0, width = 1280, height = 720;
    bool operator==(const OutputRegion&) const = default;
};

inline constexpr OutputRegion ResolveOutputRegion(resolution::Size drawable) {
    if (!drawable.width || !drawable.height) return {{}, 0, 0, 0, 0};
    // Preserve configured wide output. Taller outputs present the 16:9 logical
    // surface in the centered content region, leaving top/bottom bars outside it.
    if (uint64_t(drawable.width) * 9 < uint64_t(drawable.height) * 16) {
        const uint32_t height = uint32_t((uint64_t(drawable.width) * 9) / 16);
        return {drawable, 0, (drawable.height - height) / 2, drawable.width, height};
    }
    return {drawable, 0, 0, drawable.width, drawable.height};
}

struct SizingKey {
    uint64_t deviceEpoch = 0;
    uint32_t outputWidth = 0, outputHeight = 0;
    // Exact provider/device/content-area key; DLSS publishes four mode slots.
    Upscaler provider = Upscaler::Dlss;
    uint32_t outputX = 0, outputY = 0;
    bool operator==(const SizingKey&) const = default;
};

enum class SizingState : uint32_t { Pending, Ready, Unavailable, Error };

// CPU-side diagnostics only; no change to persisted quality IDs or wire plans.
enum class SizingIssue : uint8_t {
    None, Prerequisite, CapabilityParameters, OptimalQuery, OptimalRead,
    ZeroExtent, InvalidRange, DlaaExtentMismatch, CleanupFailed, NativeDlaaTrial
};
inline constexpr const char* SizingIssueName(SizingIssue issue) {
    switch (issue) {
    case SizingIssue::None: return "none";
    case SizingIssue::Prerequisite: return "prerequisite";
    case SizingIssue::CapabilityParameters: return "capability_parameters";
    case SizingIssue::OptimalQuery: return "optimal_query";
    case SizingIssue::OptimalRead: return "optimal_read";
    case SizingIssue::ZeroExtent: return "zero_extent";
    case SizingIssue::InvalidRange: return "invalid_range";
    case SizingIssue::DlaaExtentMismatch: return "dlaa_extent_mismatch";
    case SizingIssue::CleanupFailed: return "cleanup_failed";
    case SizingIssue::NativeDlaaTrial: return "application_native_dlaa_trial";
    }
    return "unknown";
}

struct ModeSizing {
    SizingState state = SizingState::Pending;
    // Input selected for planning. Normally the vendor optimum; a labelled
    // native trial preserves the original recommendation in vendorOptimal.
    resolution::Size optimal{};
    resolution::Size minimum{};
    resolution::Size maximum{};
    std::optional<int32_t> ngxResult;
    SizingIssue issue = SizingIssue::None;
    std::optional<int32_t> optimalWidthResult, optimalHeightResult, cleanupResult;
    std::optional<resolution::Size> vendorOptimal;
    bool operator==(const ModeSizing&) const = default;
};

inline bool ModeReadyForOutput(const ModeSizing& mode, DlssQuality quality, resolution::Size output) {
    return mode.state == SizingState::Ready && ValidDlssRenderExtent(quality, mode.optimal, output);
}

struct OutputSizing {
    SizingKey key{};
    uint64_t revision = 0;
    std::array<ModeSizing, kDlssQualityModes.size()> modes{};
    bool operator==(const OutputSizing&) const = default;
};

// Immutable CPU-frame input. The device owner publishes one complete value.
// Readers copy it and never follow a device pointer or NGX report.
struct BackendDeviceSnapshot {
    backend::Backend backend = backend::Backend::D3D12;
    uint64_t deviceEpoch = 0;
    bool deviceReady = false;
    bool dlssAvailable = false;
    // Terminal for this device lifetime. Readers must not treat an older
    // submitted DLSS frame as still running after the owner publishes this.
    bool gpuWorkStopped = false;
    bool fsrAvailable = false;
    bool Available(Upscaler provider) const {
        return backend == backend::Backend::Vulkan && deviceReady && !gpuWorkStopped &&
            (provider == Upscaler::Dlss ? dlssAvailable : provider == Upscaler::Fsr && fsrAvailable);
    }
    bool operator==(const BackendDeviceSnapshot&) const = default;
};

// The snapshot fields are stored together, including gpuWorkStopped. A reader
// cannot observe a new epoch paired with an old backend or DLSS flag.
void PublishDeviceCapability(BackendDeviceSnapshot snapshot);
BackendDeviceSnapshot PublishedDeviceCapability();

// Implemented by lane A. Lookup returns an exact-key cached state or a Pending
// state after recording the latest request; it never waits for GPU work.
class SizingCache {
public:
    using Clock = uint64_t(*)(); // monotonic milliseconds; injectable in CPU tests
    explicit SizingCache(Clock clock = nullptr) : clock_(clock ? clock : &SteadyMilliseconds) {}
    OutputSizing LookupOrRequestSizing(const SizingKey& key);
    std::optional<SizingKey> TakeSizingRequest();
    void PublishSizing(OutputSizing sizing);
    void ResetSizing(uint64_t deviceEpoch);
    // Copy a cached result without recording a GPU request. Unknown and stale
    // epochs are not treated as a permanent device failure.
    std::optional<OutputSizing> Peek(const SizingKey& key);
private:
    std::mutex mutex_;
    std::array<std::optional<OutputSizing>, 8> entries_{};
    struct RetryState { unsigned failures = 0; uint64_t notBefore = 0; };
    std::array<RetryState,8> retries_{};
    Clock clock_;
    static uint64_t SteadyMilliseconds();
    std::optional<SizingKey> pending_;
    std::optional<SizingKey> inFlight_;
    uint64_t deviceEpoch_ = 0;
    uint64_t revision_ = 0;
};

// GPU-worker-only service declaration. Its implementation delegates to the
// controller's P1 sizing query without exposing NGX types to CPU planning.
struct SizingService {
    static OutputSizing QueryOutputSizing(TemporalUpscaler& upscaler,
        const plume::VulkanInterface& vulkanInterface, const plume::VulkanDevice& device,
        const SizingKey& key);
};
} // namespace gpu::upscaling
