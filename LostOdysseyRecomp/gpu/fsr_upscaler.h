#pragma once

#if defined(LO_GPU_PLUME)
#include "temporal_frame_inputs.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace plume { struct VulkanCommandList; struct VulkanDevice; struct VulkanTexture; }
namespace gpu::dlss { struct EvaluateCapture; }

namespace gpu::fsr {

enum class Status : uint8_t { Ready, Unavailable, NeedsReconfigure, Failed, DeviceLost };

// Camera values are derived from the current guest projection, never guessed.
// For the canonical inverted/infinite depth input, cameraNear is FLT_MAX and
// cameraFar is the actual finite near distance, per the FSR3 SDK convention.
struct FrameMetadata {
    bool cameraValid = false;
    float cameraNear = 0.0f, cameraFar = 0.0f;
    float verticalFovRadians = 0.0f, viewSpaceToMetersFactor = 0.0f;
    float frameTimeDeltaMilliseconds = 0.0f;
    float depthScale = 0.0f, depthBias = 0.0f;
};

struct Config {
    uint32_t renderWidth = 0, renderHeight = 0;
    uint32_t outputWidth = 0, outputHeight = 0;
    upscaling::FsrQuality quality = upscaling::FsrQuality::Quality;
    uint64_t deviceEpoch = 0;
    bool operator==(const Config&) const = default;
};

struct Attempt {
    Status status = Status::Unavailable;
    uint64_t useId = 0;
    std::optional<int32_t> sdkResult, vkResult;
};

struct Diagnostics {
    double lastPrepareMilliseconds = 0.0;
    std::string failedApi;
    int64_t rawResult = 0;
    uint64_t lastDispatchRenderFrameId = 0;
    bool lastDispatchReset = false;
    bool lastResetForFrameGap = false;
};

// Uses the SDK's quality presets; persisted project enum values are distinct.
std::optional<resolution::Size> RecommendedRenderSize(resolution::Size output,
    upscaling::FsrQuality quality);

// Owns the SDK context, three SDK-described shared outputs, and conversion
// textures. The caller owns command submission, its serial, and output texture.
class Controller {
public:
    Controller();
    ~Controller();
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    Status EnsureSession(plume::VulkanDevice& device, const Config& config);
    Attempt RecordIsolated(plume::VulkanCommandList& commands, const Config& config,
        const temporal::TemporalFrameInputs& inputs, const FrameMetadata& frame,
        plume::VulkanTexture& output, dlss::EvaluateCapture* capture = nullptr);
    void OnBatchSubmitted(uint64_t useId, uint64_t serial);
    void OnBatchDiscarded(uint64_t useId);
    void ReleaseCompletedThrough(uint64_t serial);
    bool HasFeatureState() const;
    const Diagnostics& LastDiagnostics() const;
    void ReleaseFeatureAfterGpuDrain();
    void ShutdownAfterGpuDrain();
    void AbandonUsesAfterDeviceLoss();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gpu::fsr
#endif
