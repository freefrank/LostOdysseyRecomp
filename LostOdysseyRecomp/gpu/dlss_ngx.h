#pragma once

#if defined(LO_GPU_PLUME)
#include <plume_vulkan.h>
#include "dlss_nr_state.h"
#include "dlss_sr.h"
#include "dlss_submission_lifetime.h"
#include "upscaling_plan.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32)
namespace plume { struct D3D12Device; struct D3D12CommandList; struct D3D12Texture; }
#endif

namespace gpu::dlss {
struct EvaluateCapture;
namespace nr { struct Snippet; }

// Runtime events after the probe report. Without a sink they go to stderr.
using LogSink = void (*)(const char* line);
void SetLogSink(LogSink sink);
#if defined(_WIN32)
// Compiles HLSL compute source (entry "main") to DXIL for the D3D12 NR format
// bridge. Without one, NR stays off on D3D12.
using ComputeShaderCompiler = bool (*)(const char* source, std::vector<uint8_t>& dxil);
void SetComputeShaderCompiler(ComputeShaderCompiler compiler);
#endif

enum class ProbeState : uint8_t {
    NotProbed,
    SdkDisabled,
    Unavailable,
    ApiError,
    Available,
};

struct ApiCall {
    std::string name;
    int32_t result = 0;
};

struct OptimalSettings {
    std::string quality;
    uint32_t optimalWidth = 0, optimalHeight = 0;
    uint32_t minWidth = 0, minHeight = 0;
    uint32_t maxWidth = 0, maxHeight = 0;
    float sharpness = 0.0f;
    std::optional<int32_t> result;
};

struct CapabilityValue {
    std::optional<int32_t> raw;
    std::optional<int32_t> value;
};

struct ExtensionStatus {
    std::string state;
    std::string reason;
    int32_t createFailure = VK_SUCCESS;
    std::vector<std::string> required;
};

enum class CapabilityDecision : uint8_t { Proceed, Unavailable, ApiError };
struct CapabilityDecisionResult {
    CapabilityDecision decision = CapabilityDecision::ApiError;
    std::string reason;
};

struct ProbeReport {
    ProbeState state = ProbeState::NotProbed;
    std::string reason;
    std::string sdkVersion;
    std::string runtimePath;
    std::string deviceName;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint32_t driverVersion = 0;
    std::string driverVersionText;
    std::optional<uint32_t> featureSupport;
    uint32_t requestedOutputWidth = 1920;
    uint32_t requestedOutputHeight = 1080;
    CapabilityValue srAvailable;
    CapabilityValue needsUpdatedDriver;
    CapabilityValue minDriverVersionMajor;
    CapabilityValue minDriverVersionMinor;
    CapabilityValue featureInitResult;
    ExtensionStatus instanceExtensions;
    ExtensionStatus deviceExtensions;
    bool srImplemented = false;
    bool srEvaluated = false;
    bool frameGenerationProbed = false;
    bool frameGenerationImplemented = false;
    std::vector<ApiCall> calls;
    std::vector<OptimalSettings> optimalSettings;
};

const char* ProbeStateName(ProbeState state);
ProbeState ClassifyNgxResult(int32_t result, int32_t successResult, int32_t featureNotSupportedResult);
CapabilityDecisionResult ClassifySuperSamplingCapabilities(const CapabilityValue& available,
    const CapabilityValue& needsUpdatedDriver, const CapabilityValue& minDriverMajor,
    const CapabilityValue& minDriverMinor, const CapabilityValue& featureInitResult,
    int32_t successResult, int32_t featureNotSupportedResult);
// Probe executable policy only. Game fallback consumes ProbeReport and never
// inherits this process exit policy.
int ProbeExitCode(ProbeState state);
int ProbeExitCode(const ProbeReport& report);
bool HasValidOptimalSettings(const ProbeReport& report);

// Own one controller from interface creation through interface destruction.
// Its hook userdata is borrowed by Plume and NGX calls are serialized by the
// GPU command worker that invokes ProbeOnce.
class Controller {
public:
    Controller(std::filesystem::path applicationDataPath, std::filesystem::path runtimePath);
    ~Controller() = default;

    plume::VulkanExtensionHooks ExtensionHooks();
    // A live Streamline device shares NGX with native SR. Its probe must not
    // shut down that device runtime, even when the selected upscaler is FSR.
    void ProbeOnce(const plume::VulkanInterface& vulkanInterface, const plume::VulkanDevice& device,
        bool retainRuntimeForFrameGeneration = false);
    upscaling::OutputSizing QueryOutputSizing(const plume::VulkanInterface& vulkanInterface,
        const plume::VulkanDevice& device, const upscaling::SizingKey& key);

    // Starts one persistent device session using the initialized interface
    // retained by ProbeOnce. Executable here means the session and capability
    // map are ready; it does not mean a frame was recorded. QueryOutputSizing
    // uses that capability map and must not shut down an active session.
    SrStatus EnsureSession(const plume::VulkanDevice& device);
    bool NeedsFeatureRecreate(const SrConfig& config) const;
    // Persistent capability/parameter blocks alone do not require a drain.
    bool HasFeatureState() const {
        return feature_ || HasNeuralRenderingFeature() || featureConfigValid_ || featureFailed_ || !srUses_.Empty();
    }

    // Lane B owns one prefix, isolated NGX, and continuation primary list per
    // GPU slot. This method exclusively begins, records, and ends the isolated
    // list; it neither submits nor waits, and it never records on the prefix
    // or continuation list. output remains renderer-owned.
    SrAttempt RecordIsolated(plume::VulkanCommandList& isolatedCommandList, const SrConfig& config,
        const temporal::TemporalFrameInputs& inputs, plume::VulkanTexture& output, EvaluateCapture* capture = nullptr);
#if defined(_WIN32)
    void ProbeOnce(const plume::D3D12Device& device);
    upscaling::OutputSizing QueryOutputSizing(const plume::D3D12Device& device, const upscaling::SizingKey& key);
    SrStatus EnsureSession(const plume::D3D12Device& device);
    SrAttempt RecordIsolated(plume::D3D12CommandList& isolatedCommandList, const SrConfig& config,
        const temporal::TemporalFrameInputs& inputs, plume::D3D12Texture& output, EvaluateCapture* capture = nullptr);
#endif

    // Lane B reports the prefix/fallback batch outcome. A failed isolated list
    // is never submitted, but its nonzero useId remains live until one of
    // these notifications. release is driven only by the shared submission
    // serial domain after GPU completion.
    void OnBatchSubmitted(uint64_t useId, uint64_t submissionSerial);
    void OnBatchDiscarded(uint64_t useId);
    void ReleaseCompletedThrough(uint64_t submissionSerial);

    // DLSS 5 Neural Rendering model controls for the next evaluates. A change
    // drops the model's history once.
    void SetNeuralRenderingTuning(const NeuralRenderingTuning& tuning);
    // The NR settings page. Gameplay keeps the last DLSS frame before NR; the
    // preview runs the model on it with the page's values, on the thread that
    // records the present, between BeginGpuCommands and the presentation draw.
    // It returns a texture with that frame on its left half and the model's
    // answer on its right, or null without a held frame. A pass-count or preset
    // change rebuilds the preview features: wait for the present GPU first when
    // NeuralRenderingPreviewNeedsRebuild says so.
    bool NeuralRenderingPreviewAvailable() const { return nrCaptured_; }
    bool NeuralRenderingPreviewNeedsRebuild(uint32_t passes, uint32_t preset) const;
    plume::RenderTexture* RecordNeuralRenderingPreview(plume::RenderCommandList& list, uint32_t passes,
        uint32_t preset, const NeuralRenderingTuning& tuning);
    // After the present GPU is idle, once the page has closed.
    void ReleaseNeuralRenderingPreview();

    // Call at a controlled drained boundary. Reconfiguration releases the
    // feature only; final shutdown also releases parameters and the session.
    void ReleaseFeatureAfterGpuDrain();
    void ShutdownAfterGpuDrain();
    bool ShutdownComplete() const {
        return srUses_.Empty() && !feature_ && !featureParameters_ && !HasNeuralRenderingFeature() &&
            !HasNeuralRenderingPreviewFeature() && !nrParameters_ && !nrSnippetInitialized_ && !capabilityParameters_ &&
            !sessionInitialized_ && !runtimeRetainedForFg_;
    }
    void AbandonUsesAfterDeviceLoss();
    const ProbeReport& Report() const { return report_; }

private:
    static bool QueryInstance(void* userData, std::vector<VkExtensionProperties>& required, std::string& reason);
    static bool QueryDevice(void* userData, VkInstance instance, VkPhysicalDevice device,
                            std::vector<VkExtensionProperties>& required, std::string& reason);
    bool QueryInstanceExtensions(std::vector<VkExtensionProperties>& required, std::string& reason);
    bool QueryDeviceExtensions(VkInstance instance, VkPhysicalDevice device,
                               std::vector<VkExtensionProperties>& required, std::string& reason);
    void RecordCall(const char* name, int32_t result, bool failed = false);
    bool CreateApplicationDataPath(std::string& reason);
    SrStatus AllocateParameters();
#if defined(_WIN32)
    SrStatus AllocateParametersD3D12();
#endif
    void QueryNeuralRenderingExtensions(VkPhysicalDevice device, std::vector<VkExtensionProperties>& required);
    bool HasNeuralRenderingFeature() const {
        for (const void* feature : nrFeatures_) if (feature) return true;
        return false;
    }
    bool HasNeuralRenderingPreviewFeature() const {
        for (const void* feature : nrPreviewFeatures_) if (feature) return true;
        return false;
    }
    // NGX snippet loaded and initialized for the session's backend, or null.
    const nr::Snippet* InitializedNeuralRenderingSnippet();
    bool AllocateNeuralRenderingParameters();
    // Null keeps NR off this frame; the reason is logged once.
    const nr::Snippet* NeuralRenderingSnippet(const SrConfig& config);
    void FailNeuralRendering(const char* operation, int32_t result);
    void RecordNeuralRendering(VkCommandBuffer commandBuffer, const SrConfig& config,
                               const temporal::TemporalFrameInputs& inputs, plume::VulkanTexture& output, bool reset);
    void CaptureNeuralRenderingInput(VkCommandBuffer commandBuffer, const SrConfig& config,
                                     const temporal::TemporalFrameInputs& inputs, plume::VulkanTexture& output);
    plume::RenderTexture* RecordNeuralRenderingPreview(plume::VulkanCommandList& list, uint32_t passes,
                                                       uint32_t preset, const NeuralRenderingTuning& tuning);
#if defined(_WIN32)
    void RecordNeuralRendering(plume::D3D12CommandList& list, const SrConfig& config,
                               const temporal::TemporalFrameInputs& inputs, plume::D3D12Texture& output, bool reset);
    void CaptureNeuralRenderingInput(plume::D3D12CommandList& list, const SrConfig& config,
                                     const temporal::TemporalFrameInputs& inputs, plume::D3D12Texture& output);
    plume::RenderTexture* RecordNeuralRenderingPreview(plume::D3D12CommandList& list, uint32_t passes,
                                                       uint32_t preset, const NeuralRenderingTuning& tuning);
    bool CreateNeuralRenderingBridgePipeline(plume::RenderDevice& device);
    bool CreateNeuralRenderingBridge(plume::RenderDevice& device, plume::RenderFormat format,
                                     uint32_t width, uint32_t height, uint32_t passes);
#endif
    // Shared by the preview's rebuild and its release at a drained boundary.
    void ReleaseNeuralRenderingPreviewFeatures();
    // The capture's source sizes, for creating the textures that hold it.
    bool CreateNeuralRenderingCapture(plume::RenderDevice& device, const SrConfig& config, plume::RenderFormat colorFormat,
                                      plume::RenderFormat depthFormat, plume::RenderFormat motionFormat);
    void ReleaseNeuralRendering();

    enum class Backend : uint8_t { None, Vulkan, D3D12 };

    std::filesystem::path applicationDataPath_;
    std::filesystem::path runtimePath_;
    ProbeReport report_;
    const plume::VulkanInterface* sessionInterface_ = nullptr;
    const plume::VulkanDevice* sessionDevice_ = nullptr;
#if defined(_WIN32)
    const plume::D3D12Device* sessionDeviceD3D12_ = nullptr;
#endif
    Backend backend_ = Backend::None;
    VkInstance sessionInstance_ = VK_NULL_HANDLE;
    // Opaque SDK-owned objects keep SDK declarations out of the public API.
    void* capabilityParameters_ = nullptr;
    void* featureParameters_ = nullptr;
    void* feature_ = nullptr;
    SrConfig featureConfig_{};
    SubmissionLifetime srUses_;
    bool probeAttempted_ = false;
    bool apiFailure_ = false;
    bool sessionInitialized_ = false;
    // Device-runtime ownership is separate from SR capability availability.
    // Cleared only by successful owner shutdown after FG has been released.
    bool runtimeRetainedForFg_ = false;
    bool sharingRuntimeWithFg_ = false;
    bool featureConfigValid_ = false;
    bool sessionFailed_ = false;
    // Only bounded sizing requests may reopen a cleaned-up initialization.
    // Device loss, failed cleanup and live feature resources are not retryable.
    bool sessionRetryable_ = false;
    bool featureFailed_ = false;
    uint64_t lastSrAttemptFrameId_ = 0;

    // DLSS 5 Neural Rendering (NGX feature 18) on the SR output, run
    // SrConfig::neuralRenderingPasses times in a row, each pass on the last
    // one's answer with its own feature. The user supplies nvngx_dlssnr.dll
    // beside the SR runtime or names it with LO_DLSS_NR_PATH; Windows only.
    // Its failures stay here and never fail SR.
    // Vulkan: the snippet's device extensions were enabled. D3D12: always.
    bool nrSupported_ = false;
    bool nrMissingAtDeviceCreation_ = false;
    std::string nrUnsupportedReason_ = "the Vulkan device extensions were not queried";
    bool nrSnippetInitialized_ = false;
    bool nrFailed_ = false;
    bool nrReset_ = true;
    bool nrSkipLogged_ = false;
    void* nrParameters_ = nullptr;
    std::array<void*, kMaxNeuralRenderingPasses> nrFeatures_{};
    uint32_t nrPasses_ = 0;
    // FP16 ping-pong images: the SR output's copy and each pass's answer.
    std::array<std::unique_ptr<plume::RenderTexture>, 2> nrImages_;
    uint32_t nrWidth_ = 0, nrHeight_ = 0;
    NeuralRenderingTuning nrTuning_{};
    bool nrTuningChanged_ = false;
    // The settings page's preview. Gameplay holds its last DLSS frame before NR
    // (output-size color, render-size depth and motion at offset 0); the page
    // runs its own features on it. They count for shutdown, not for the
    // renderer's feature state.
    std::unique_ptr<plume::RenderTexture> nrCaptureColor_, nrCaptureDepth_, nrCaptureMotion_;
    SrConfig nrCaptureConfig_{};
    bool nrCaptured_ = false;
    uint32_t nrCaptureCount_ = 0;
    std::array<void*, kMaxNeuralRenderingPasses> nrPreviewFeatures_{};
    uint32_t nrPreviewPasses_ = 0, nrPreviewPreset_ = 0;
    std::array<std::unique_ptr<plume::RenderTexture>, 2> nrPreviewImages_;
    std::unique_ptr<plume::RenderTexture> nrPreviewComposite_;
    NeuralRenderingTuning nrPreviewTuning_{};
    // Evaluates still to run before the static frame's answer settles.
    uint32_t nrPreviewSettle_ = 0;
    bool nrPreviewReset_ = true, nrPreviewCreated_ = false;
#if defined(_WIN32)
    // D3D12 has no format-converting blit: the SR output is copied to nrStage_
    // and converted to and from the FP16 images by a small compute pass.
    std::unique_ptr<plume::RenderTexture> nrStage_;
    std::unique_ptr<plume::RenderPipelineLayout> nrBridgeLayout_;
    std::unique_ptr<plume::RenderShader> nrBridgeShader_;
    std::unique_ptr<plume::RenderPipeline> nrBridgePipeline_;
    std::unique_ptr<plume::RenderDescriptorSet> nrEncodeSet_, nrDecodeSet_;
    std::unique_ptr<plume::RenderDescriptorSet> nrPreviewEncodeSet_, nrPreviewDecodeSet_;
#endif
};
} // namespace gpu::dlss
#endif
