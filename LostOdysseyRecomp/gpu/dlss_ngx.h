#pragma once

#if defined(LO_GPU_PLUME)
#include <plume_vulkan.h>
#include "dlss_sr.h"
#include "dlss_submission_lifetime.h"
#include "upscaling_plan.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gpu::dlss {
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
    void ProbeOnce(const plume::VulkanInterface& vulkanInterface, const plume::VulkanDevice& device);
    upscaling::OutputSizing QueryOutputSizing(const plume::VulkanInterface& vulkanInterface,
        const plume::VulkanDevice& device, const upscaling::SizingKey& key);

    // Starts one persistent device session using the initialized interface
    // retained by ProbeOnce. Executable here means the session and capability
    // map are ready; it does not mean a frame was recorded. QueryOutputSizing
    // uses that capability map and must not shut down an active session.
    SrStatus EnsureSession(const plume::VulkanDevice& device);
    bool NeedsFeatureRecreate(const SrConfig& config) const;
    // Persistent capability/parameter blocks alone do not require a drain.
    bool HasFeatureState() const { return feature_ || featureConfigValid_ || featureFailed_ || !srUses_.Empty(); }

    // Lane B owns one prefix, isolated NGX, and continuation primary list per
    // GPU slot. This method exclusively begins, records, and ends the isolated
    // list; it neither submits nor waits, and it never records on the prefix
    // or continuation list. output remains renderer-owned.
    SrAttempt RecordIsolated(plume::VulkanCommandList& isolatedCommandList, const SrConfig& config,
        const temporal::TemporalFrameInputs& inputs, plume::VulkanTexture& output);

    // Lane B reports the prefix/fallback batch outcome. A failed isolated list
    // is never submitted, but its nonzero useId remains live until one of
    // these notifications. release is driven only by the shared submission
    // serial domain after GPU completion.
    void OnBatchSubmitted(uint64_t useId, uint64_t submissionSerial);
    void OnBatchDiscarded(uint64_t useId);
    void ReleaseCompletedThrough(uint64_t submissionSerial);

    // Call at a controlled drained boundary. Reconfiguration releases the
    // feature only; final shutdown also releases parameters and the session.
    void ReleaseFeatureAfterGpuDrain();
    void ShutdownAfterGpuDrain();
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

    std::filesystem::path applicationDataPath_;
    std::filesystem::path runtimePath_;
    ProbeReport report_;
    const plume::VulkanInterface* sessionInterface_ = nullptr;
    const plume::VulkanDevice* sessionDevice_ = nullptr;
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
    bool featureConfigValid_ = false;
    bool sessionFailed_ = false;
    bool featureFailed_ = false;
    uint64_t lastSrAttemptFrameId_ = 0;
};
} // namespace gpu::dlss
#endif
