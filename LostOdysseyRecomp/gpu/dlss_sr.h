#pragma once

#if defined(LO_GPU_PLUME)
#include "render_resolution.h"
#include "upscaling_plan.h"

#include <cstdint>
#include <optional>

namespace plume {
struct VulkanCommandList;
struct VulkanDevice;
struct VulkanTexture;
}

namespace gpu::temporal { struct TemporalFrameInputs; }

namespace gpu::dlss {

// Unknown is never submitted to NGX. The renderer must retain its spatial
// fallback until it can prove the scene boundary's color encoding.
enum class SrColorSpace : uint8_t { Unknown, Linear, DisplayEncoded };

// DLSS 5 Neural Rendering runs this many times in a row on the SR output.
inline constexpr uint32_t kMaxNeuralRenderingPasses = 4;

// SR model: the NVSDK_NGX_DLSS_Hint_Render_Preset value both NGX backends set
// for every quality mode before they create the feature. Settings dlss_model
// 0 selects M, 1 selects L.
inline constexpr uint8_t kRenderPresetL = 12;
inline constexpr uint8_t kRenderPresetM = 13;
inline char RenderPresetLetter(uint8_t preset) { return preset ? char('A' + preset - 1) : '-'; }

// Feature-creation key. The renderer supplies the device epoch with the frame
// configuration; Controller does not own output textures or renderer extents.
struct SrConfig {
    resolution::Size renderExtent{};
    resolution::Size outputExtent{};
    upscaling::DlssQuality quality = upscaling::DlssQuality::Quality;
    SrColorSpace colorSpace = SrColorSpace::Unknown;
    uint64_t deviceEpoch = 0;
    bool depthInverted = false;
    bool autoExposure = false;
    // 0 is off. A change recreates the SR and NR features at a drained boundary,
    // as do the model preset hints, which a feature reads when it is created.
    uint8_t neuralRenderingPasses = 0;
    uint8_t neuralRenderingPreset = 0;
    uint8_t renderPreset = kRenderPresetM;

    bool operator==(const SrConfig&) const = default;
};

enum class SrStatus : uint8_t {
    Executable,
    Bypass,
    NeedsReconfigure,
    Failed,
    // Return only after a native Vulkan/NGX call confirms device loss.
    DeviceLost,
};

// useId is nonzero when the prefix fallback batch retains the feature and
// parameters. It must be completed through OnBatchSubmitted or
// OnBatchDiscarded even when the isolated NGX list was excluded.
struct SrAttempt {
    SrStatus status = SrStatus::Bypass;
    uint64_t useId = 0;
    std::optional<int32_t> rawNgxResult;
    std::optional<int32_t> rawVkResult;
};

} // namespace gpu::dlss
#endif
