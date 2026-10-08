#pragma once

#include "aspect_ratio.h"
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
// Persisted. MetalFx is the Metal backend's temporal upscaler (macOS only);
// Xess is Intel XeSS-SR on the D3D12 backend (Windows only).
enum class Upscaler : uint32_t { Off = 0, Dlss = 1, Fsr = 2, MetalFx = 3, Xess = 4 };
inline constexpr bool KnownUpscaler(Upscaler value) { return uint32_t(value) <= uint32_t(Upscaler::Xess); }
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
// Check actual render dimensions, not the vendor's recommended SR dimensions.
inline constexpr bool ValidDlssRenderExtent(DlssQuality quality, resolution::Size render,
    resolution::Size output) {
    return KnownDlssQuality(quality) && render.width && render.height && output.width && output.height &&
        (quality != DlssQuality::Dlaa || render == output);
}
enum class TemporalConsumer : uint32_t { None = 0, LegacyTaa = 1, DlssInputs = 2, DlssSr = 3, FsrSr = 4, MetalFxSr = 5, XessSr = 6 };
inline constexpr bool KnownTemporalConsumer(TemporalConsumer value) { return uint32_t(value) <= uint32_t(TemporalConsumer::XessSr); }
inline constexpr bool RequiresMotionDepth(TemporalConsumer consumer, FrameGeneration frameGeneration = FrameGeneration::Off) {
    return consumer == TemporalConsumer::DlssInputs || consumer == TemporalConsumer::DlssSr ||
        consumer == TemporalConsumer::FsrSr || consumer == TemporalConsumer::MetalFxSr ||
        consumer == TemporalConsumer::XessSr || frameGeneration != FrameGeneration::Off;
}
// P1's input-only route and P2's native SR route have different output handling,
// but both require the same complete temporal input contract and request-level
// fallback semantics.
inline constexpr bool IsDlssConsumer(TemporalConsumer consumer) {
    return consumer == TemporalConsumer::DlssInputs || consumer == TemporalConsumer::DlssSr;
}
inline constexpr bool IsSrConsumer(TemporalConsumer consumer) {
    return consumer == TemporalConsumer::DlssSr || consumer == TemporalConsumer::FsrSr ||
        consumer == TemporalConsumer::MetalFxSr || consumer == TemporalConsumer::XessSr;
}
inline constexpr Upscaler ProviderForConsumer(TemporalConsumer consumer) {
    return consumer == TemporalConsumer::FsrSr ? Upscaler::Fsr : consumer == TemporalConsumer::MetalFxSr ? Upscaler::MetalFx :
        consumer == TemporalConsumer::XessSr ? Upscaler::Xess : IsDlssConsumer(consumer) ? Upscaler::Dlss : Upscaler::Off;
}
inline constexpr bool MatchesSrProvider(Upscaler provider, TemporalConsumer consumer) {
    return IsSrConsumer(consumer) && ProviderForConsumer(consumer) == provider;
}
// FSR, MetalFX and XeSS share the FsrQuality IDs and plan field. FSR and MetalFX
// share its ratios; XeSS maps the same-named SDK presets and queries their extent.
inline constexpr bool UsesFsrQuality(Upscaler provider) {
    return provider == Upscaler::Fsr || provider == Upscaler::MetalFx || provider == Upscaler::Xess;
}
inline constexpr TemporalConsumer FsrQualityConsumer(Upscaler provider) {
    return provider == Upscaler::MetalFx ? TemporalConsumer::MetalFxSr :
        provider == Upscaler::Xess ? TemporalConsumer::XessSr : TemporalConsumer::FsrSr;
}
inline constexpr bool SameEffectiveQuality(Upscaler provider, DlssQuality aDlss, DlssQuality bDlss,
    FsrQuality aFsr, FsrQuality bFsr) {
    switch (provider) {
    case Upscaler::Dlss: return aDlss == bDlss;
    // MetalFX and XeSS reuse the FSR quality IDs.
    case Upscaler::Fsr:
    case Upscaler::MetalFx:
    case Upscaler::Xess: return aFsr == bFsr;
    case Upscaler::Off: return true;
    }
    return false;
}

struct OutputRegion {
    resolution::Size drawable{};
    uint32_t x = 0, y = 0, width = 1280, height = 720;
    bool operator==(const OutputRegion&) const = default;
};

inline constexpr OutputRegion ResolveOutputRegion(resolution::Size drawable,
    aspect_ratio::Mode aspect = aspect_ratio::Mode::Auto) {
    if (!drawable.width || !drawable.height) return {{}, 0, 0, 0, 0};
    // Auto: the scene fills the drawable. Menus and movies fit their own 16:9
    // content inside it; their bars must not constrain the 3D render target.
    // A chosen aspect ratio sizes the scene for that shape; presentation centres
    // it in the drawable, so the region keeps a zero origin.
    const auto content = aspect_ratio::Fit(aspect, drawable.width, drawable.height);
    return {drawable, 0, 0, content.width, content.height};
}

// Supersampling (#332): with a temporal upscaler, a Render resolution above the
// content area becomes the upscaler's output size. Presentation scales the
// result down into the drawable, as it does a native scene of that size.
inline constexpr OutputRegion SupersampledOutputRegion(OutputRegion region, uint32_t internalResolution) {
    if (!internalResolution || !region.width || !region.height) return region;
    const auto size = resolution::ResolveInternalSize(internalResolution, region.width, region.height);
    if (size.width < region.width || size.height <= region.height) return region;
    region.width = size.width;
    region.height = size.height;
    return region;
}

// Black bars: the content, scaled uniformly to fit the drawable, leaves at least
// a pixel uncovered. A supersampled region is larger than the drawable.
inline constexpr bool Letterboxed(const OutputRegion& region) {
    if (!region.width || !region.height || !region.drawable.width || !region.drawable.height) return false;
    const uint64_t content = uint64_t(region.width) * region.drawable.height;
    const uint64_t drawable = uint64_t(region.drawable.width) * region.height;
    // Wider content leaves bars above and below it, taller content at its sides.
    return content > drawable ? content - drawable >= region.width : drawable - content >= region.height;
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
    ZeroExtent, InvalidRange, DlaaExtentMismatch, CleanupFailed
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
    }
    return "unknown";
}

struct ModeSizing {
    SizingState state = SizingState::Pending;
    // Selected render extent; DLAA resolves the NGX recommendation to native.
    resolution::Size optimal{};
    resolution::Size minimum{};
    resolution::Size maximum{};
    std::optional<int32_t> ngxResult;
    SizingIssue issue = SizingIssue::None;
    std::optional<int32_t> optimalWidthResult, optimalHeightResult, cleanupResult;
    bool operator==(const ModeSizing&) const = default;
};

// Called only after a successful NGX query and required output reads. The
// programming guide defines DLAA as 1:1 regardless of GetOptimalSettings.
// Keep the queried range, but select native input for DLAA within that range.
inline SizingIssue ResolveDlssSizing(ModeSizing& mode, DlssQuality quality, resolution::Size output) {
    if (!mode.optimal.width || !mode.optimal.height || !mode.minimum.width ||
        !mode.minimum.height || !mode.maximum.width || !mode.maximum.height ||
        !output.width || !output.height) return SizingIssue::ZeroExtent;
    const auto inRange = [&](resolution::Size size) {
        return mode.minimum.width <= size.width && size.width <= mode.maximum.width &&
            mode.minimum.height <= size.height && size.height <= mode.maximum.height;
    };
    if (!inRange(mode.optimal)) return SizingIssue::InvalidRange;
    if (quality == DlssQuality::Dlaa) {
        if (!inRange(output)) return SizingIssue::DlaaExtentMismatch;
        mode.optimal = output;
    }
    return SizingIssue::None;
}

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
    bool metalFxAvailable = false;
    bool xessAvailable = false;
    bool Available(Upscaler provider) const {
        if (!deviceReady || gpuWorkStopped) return false;
        if (backend == backend::Backend::Metal) return provider == Upscaler::MetalFx && metalFxAvailable;
        if (provider == Upscaler::Xess) return backend == backend::Backend::D3D12 && xessAvailable;
        return (backend == backend::Backend::Vulkan || backend == backend::Backend::D3D12) &&
            (provider == Upscaler::Dlss ? dlssAvailable : provider == Upscaler::Fsr && fsrAvailable);
    }
    bool operator==(const BackendDeviceSnapshot&) const = default;
};

// The snapshot fields are stored together, including gpuWorkStopped. A reader
// cannot observe a new epoch paired with the previous backend or DLSS flag.
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
    // epochs return empty; they are not treated as a permanent device failure.
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
