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

namespace gpu::dlss { class Controller; }

namespace gpu::upscaling {
enum class Upscaler : uint32_t { Off = 0, Dlss = 1 };
enum class DlssQuality : uint32_t { Quality = 0, Balanced = 1, Performance = 2 };
enum class TemporalConsumer : uint32_t { None = 0, LegacyTaa = 1, DlssInputs = 2, DlssSr = 3 };
// P1's input-only route and P2's native SR route have different output handling,
// but both require the same complete temporal input contract and request-level
// fallback semantics.
inline constexpr bool IsDlssConsumer(TemporalConsumer consumer) {
    return consumer == TemporalConsumer::DlssInputs || consumer == TemporalConsumer::DlssSr;
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
    bool operator==(const SizingKey&) const = default;
};

enum class SizingState : uint32_t { Pending, Ready, Unavailable, Error };

struct ModeSizing {
    SizingState state = SizingState::Pending;
    resolution::Size optimal{};
    resolution::Size minimum{};
    resolution::Size maximum{};
    std::optional<int32_t> ngxResult;
    bool operator==(const ModeSizing&) const = default;
};

struct OutputSizing {
    SizingKey key{};
    uint64_t revision = 0;
    std::array<ModeSizing, 3> modes{};
    bool operator==(const OutputSizing&) const = default;
};

// Immutable CPU-frame input. video owns publication; the plan producer takes
// one snapshot and never rereads live backend state for that frame.
struct BackendDeviceSnapshot {
    backend::Backend backend = backend::Backend::D3D12;
    uint64_t deviceEpoch = 0;
    bool deviceReady = false;
    bool dlssAvailable = false;
    bool operator==(const BackendDeviceSnapshot&) const = default;
};

// Implemented by lane A. Lookup returns an exact-key cached state or a Pending
// state after recording the latest request; it never waits for GPU work.
class SizingCache {
public:
    OutputSizing LookupOrRequestSizing(const SizingKey& key);
    std::optional<SizingKey> TakeSizingRequest();
    void PublishSizing(OutputSizing sizing);
    void ResetSizing(uint64_t deviceEpoch);
private:
    std::mutex mutex_;
    std::array<std::optional<OutputSizing>, 8> entries_{};
    std::optional<SizingKey> pending_;
    std::optional<SizingKey> inFlight_;
    uint64_t deviceEpoch_ = 0;
    uint64_t revision_ = 0;
};

// GPU-worker-only service declaration. Its implementation delegates to the
// controller's P1 sizing query without exposing NGX types to CPU planning.
struct SizingService {
    static OutputSizing QueryOutputSizing(dlss::Controller& controller,
        const plume::VulkanInterface& vulkanInterface, const plume::VulkanDevice& device,
        const SizingKey& key);
};
} // namespace gpu::upscaling
