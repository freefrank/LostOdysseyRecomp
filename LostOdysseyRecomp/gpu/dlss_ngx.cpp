#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "dlss_ngx.h"

#if defined(LO_GPU_PLUME)
#include "temporal_frame_inputs.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <codecvt>
#include <locale>
#if defined(LO_DLSS_SDK)
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>
#include <nvsdk_ngx_vk.h>
#endif

namespace gpu::dlss {
namespace {
constexpr char kProjectId[] = "bb5fe48b-f929-4b9a-a72b-98a23141a7c9";
constexpr char kSdkVersion[] = "310.9.1";
constexpr size_t kMaxRecordedCalls = 128;

#if defined(LO_DLSS_SDK)
std::wstring ToWide(const std::filesystem::path& path) {
#ifdef _WIN32
    return path.wstring();
#else
    const auto utf8 = path.u8string();
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(reinterpret_cast<const char*>(utf8.data()),
        reinterpret_cast<const char*>(utf8.data()) + utf8.size());
#endif
}

struct DiscoveryInfo {
    std::wstring appDataPath;
    std::wstring runtimePath;
    const wchar_t* paths[1] = {};
    NVSDK_NGX_FeatureCommonInfo featureInfo = {};
    NVSDK_NGX_FeatureDiscoveryInfo discovery = {};

    DiscoveryInfo(const std::filesystem::path& dataPath, const std::filesystem::path& runtime) :
        appDataPath(ToWide(dataPath)), runtimePath(ToWide(runtime)) {
        paths[0] = runtimePath.c_str();
        featureInfo.PathListInfo.Path = paths;
        featureInfo.PathListInfo.Length = runtimePath.empty() ? 0u : 1u;
        discovery.SDKVersion = NVSDK_NGX_Version_API;
        discovery.FeatureID = NVSDK_NGX_Feature_SuperSampling;
        discovery.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
        discovery.Identifier.v.ProjectDesc = {kProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "LostOdysseyRecomp"};
        discovery.ApplicationDataPath = appDataPath.c_str();
        discovery.FeatureInfo = &featureInfo;
    }
};

bool CopyExtensions(NVSDK_NGX_Result result, uint32_t count, VkExtensionProperties* source,
                    std::vector<VkExtensionProperties>& required, std::string& reason) {
    if (NVSDK_NGX_FAILED(result)) {
        reason = "NGX extension requirement query failed (raw=" + std::to_string(int32_t(result)) + ")";
        return false;
    }
    if (count != 0 && source == nullptr) {
        reason = "NGX returned an empty extension array";
        return false;
    }
    required.insert(required.end(), source, source + count);
    return true;
}

void CopyExtensionNames(const std::vector<VkExtensionProperties>& extensions, std::vector<std::string>& destination) {
    destination.clear();
    destination.reserve(extensions.size());
    for (const auto& extension : extensions) destination.emplace_back(extension.extensionName);
}

NVSDK_NGX_PerfQuality_Value ToNgxQuality(upscaling::DlssQuality quality) {
    switch (quality) {
    case upscaling::DlssQuality::Quality: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
    case upscaling::DlssQuality::Balanced: return NVSDK_NGX_PerfQuality_Value_Balanced;
    case upscaling::DlssQuality::Performance: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
    }
    return NVSDK_NGX_PerfQuality_Value_MaxQuality;
}

bool IsFinitePositive(float value) {
    return std::isfinite(value) && value > 0.0f;
}

bool ValidImage(const plume::VulkanTexture& texture, const plume::VulkanDevice* device) {
    return texture.device == device && texture.vk != VK_NULL_HANDLE && texture.imageView != VK_NULL_HANDLE &&
        texture.imageFormat != VK_FORMAT_UNDEFINED && texture.allocation != VK_NULL_HANDLE &&
        texture.imageSubresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT &&
        texture.imageSubresourceRange.levelCount == 1 && texture.imageSubresourceRange.layerCount == 1 &&
        texture.desc.width && texture.desc.height;
}

NVSDK_NGX_Resource_VK ImageResource(const plume::VulkanTexture& texture, bool readWrite) {
    return NVSDK_NGX_Create_ImageView_Resource_VK(texture.imageView, texture.vk, texture.imageSubresourceRange,
        texture.imageFormat, texture.desc.width, texture.desc.height, readWrite);
}

int DlssFlags(const SrConfig& config) {
    int flags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    if (config.colorSpace == SrColorSpace::Linear) flags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
    if (config.depthInverted) flags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
    if (config.autoExposure) flags |= NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    return flags;
}

struct BeginCommandResult {
    std::optional<VkResult> reset;
    std::optional<VkResult> begin;
};

// This is the dedicated, renderer-owned isolated primary list only. Keep the
// public Plume bookkeeping equivalent to VulkanCommandList::begin while using
// native calls so callers retain the actual VkResult diagnostics.
BeginCommandResult BeginIsolatedCommandList(plume::VulkanCommandList& list) {
    if (list.vk == VK_NULL_HANDLE) return {};
    const auto reset = vkResetCommandBuffer(list.vk, 0);
    if (reset != VK_SUCCESS) return {reset, std::nullopt};
    list.activeGraphicsDescriptorSets.clear();
    list.recording = false;
    list.externalCommandsOpen = false;
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    const auto begin = vkBeginCommandBuffer(list.vk, &beginInfo);
    if (begin == VK_SUCCESS) list.recording = true;
    return {reset, begin};
}

std::optional<VkResult> EndIsolatedCommandList(plume::VulkanCommandList& list) {
    if (!list.recording || list.externalCommandsOpen || list.activeRenderPass != VK_NULL_HANDLE) return std::nullopt;
    const auto end = vkEndCommandBuffer(list.vk);
    if (end != VK_SUCCESS) return end;
    list.targetFramebuffer = nullptr;
    list.activeComputePipelineLayout = nullptr;
    list.activeGraphicsPipelineLayout = nullptr;
    list.activeRaytracingPipelineLayout = nullptr;
    list.activeGraphicsDescriptorSets.clear();
    list.recording = false;
    list.externalCommandsOpen = false;
    return end;
}
#endif
}

const char* ProbeStateName(ProbeState state) {
    switch (state) {
    case ProbeState::NotProbed: return "not_probed";
    case ProbeState::SdkDisabled: return "sdk_disabled";
    case ProbeState::Unavailable: return "unavailable";
    case ProbeState::ApiError: return "api_error";
    case ProbeState::Available: return "available";
    }
    return "unknown";
}

ProbeState ClassifyNgxResult(int32_t result, int32_t successResult, int32_t featureNotSupportedResult) {
    if (result == successResult) return ProbeState::Available;
    if (result == featureNotSupportedResult) return ProbeState::Unavailable;
    return ProbeState::ApiError;
}

CapabilityDecisionResult ClassifySuperSamplingCapabilities(const CapabilityValue& available,
    const CapabilityValue& needsUpdatedDriver, const CapabilityValue& minDriverMajor,
    const CapabilityValue& minDriverMinor, const CapabilityValue& featureInitResult,
    int32_t successResult, int32_t featureNotSupportedResult) {
    const auto readable = [successResult](const CapabilityValue& value) {
        return value.raw && *value.raw == successResult && value.value;
    };
    if (!readable(available) || !readable(needsUpdatedDriver) || !readable(minDriverMajor) ||
        !readable(minDriverMinor) || !readable(featureInitResult))
        return {CapabilityDecision::ApiError, "one or more NGX Super Sampling capability reads failed"};
    if (*available.value == 0)
        return {CapabilityDecision::Unavailable, "NGX capability parameters report Super Sampling unavailable"};
    if (*needsUpdatedDriver.value != 0)
        return {CapabilityDecision::Unavailable, "NGX requires driver " + std::to_string(*minDriverMajor.value) + "." + std::to_string(*minDriverMinor.value)};
    switch (ClassifyNgxResult(*featureInitResult.value, successResult, featureNotSupportedResult)) {
    case ProbeState::Available: return {CapabilityDecision::Proceed, {}};
    case ProbeState::Unavailable: return {CapabilityDecision::Unavailable, "NGX FeatureInitResult reports Super Sampling unsupported"};
    default: return {CapabilityDecision::ApiError, "NGX FeatureInitResult is not success"};
    }
}

int ProbeExitCode(ProbeState state) {
    switch (state) {
    case ProbeState::Available: return 0;
    case ProbeState::SdkDisabled:
    case ProbeState::Unavailable: return 77;
    case ProbeState::NotProbed:
    case ProbeState::ApiError: return 1;
    }
    return 1;
}

int ProbeExitCode(const ProbeReport& report) { return ProbeExitCode(report.state); }

bool HasValidOptimalSettings(const ProbeReport& report) {
    if (report.requestedOutputWidth != 1920 || report.requestedOutputHeight != 1080 || report.optimalSettings.size() != 3)
        return false;
    for (const auto& setting : report.optimalSettings) {
        if (!setting.result || *setting.result == 0 || setting.optimalWidth == 0 || setting.optimalHeight == 0 ||
            setting.minWidth == 0 || setting.minHeight == 0 || setting.maxWidth == 0 || setting.maxHeight == 0)
            return false;
    }
    return true;
}

Controller::Controller(std::filesystem::path applicationDataPath, std::filesystem::path runtimePath) :
    applicationDataPath_(std::move(applicationDataPath)), runtimePath_(std::move(runtimePath)) {
    report_.sdkVersion = kSdkVersion;
    report_.runtimePath = runtimePath_.string();
#if !defined(LO_DLSS_SDK)
    report_.state = ProbeState::SdkDisabled;
    report_.reason = "built without LO_ENABLE_DLSS and a usable local NGX SDK";
#endif
}

void Controller::RecordCall(const char* name, int32_t result, bool failed) {
    if (report_.calls.size() < kMaxRecordedCalls) report_.calls.push_back({name, result});
    apiFailure_ |= failed;
}

bool Controller::CreateApplicationDataPath(std::string& reason) {
    std::error_code ec;
    std::filesystem::create_directories(applicationDataPath_, ec);
    if (!ec) return true;
    reason = "NGX application data directory is not writable";
    return false;
}

plume::VulkanExtensionHooks Controller::ExtensionHooks() {
#if defined(LO_DLSS_SDK)
    return {this, QueryInstance, QueryDevice};
#else
    return {};
#endif
}

bool Controller::QueryInstance(void* userData, std::vector<VkExtensionProperties>& required, std::string& reason) {
    return static_cast<Controller*>(userData)->QueryInstanceExtensions(required, reason);
}

bool Controller::QueryDevice(void* userData, VkInstance instance, VkPhysicalDevice device,
                             std::vector<VkExtensionProperties>& required, std::string& reason) {
    return static_cast<Controller*>(userData)->QueryDeviceExtensions(instance, device, required, reason);
}

bool Controller::QueryInstanceExtensions(std::vector<VkExtensionProperties>& required, std::string& reason) {
#if defined(LO_DLSS_SDK)
    if (!CreateApplicationDataPath(reason)) return false;
    DiscoveryInfo info(applicationDataPath_, runtimePath_);
    uint32_t count = 0;
    VkExtensionProperties* extensions = nullptr;
    const auto result = NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&info.discovery, &count, &extensions);
    const bool failed = NVSDK_NGX_FAILED(result);
    RecordCall("GetFeatureInstanceExtensionRequirements", int32_t(result), failed);
    const bool copied = CopyExtensions(result, count, extensions, required, reason);
    CopyExtensionNames(required, report_.instanceExtensions.required);
    report_.instanceExtensions.reason = copied ? "" : reason;
    return copied;
#else
    (void)required;
    reason = "NGX SDK disabled";
    return false;
#endif
}

bool Controller::QueryDeviceExtensions(VkInstance instance, VkPhysicalDevice device,
                                       std::vector<VkExtensionProperties>& required, std::string& reason) {
#if defined(LO_DLSS_SDK)
    sessionInstance_ = instance;
    if (!CreateApplicationDataPath(reason)) return false;
    DiscoveryInfo info(applicationDataPath_, runtimePath_);
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(device, &properties);
    report_.deviceName = properties.deviceName;
    report_.vendorId = properties.vendorID;
    report_.deviceId = properties.deviceID;
    report_.driverVersion = properties.driverVersion;
    if (report_.vendorId == 0x10de) {
        report_.driverVersionText = std::to_string(report_.driverVersion >> 22) + "." +
            std::to_string((report_.driverVersion >> 14) & 0xff);
    }
    NVSDK_NGX_FeatureRequirement requirements = {};
    auto result = NVSDK_NGX_VULKAN_GetFeatureRequirements(instance, device, &info.discovery, &requirements);
    const bool requirementsFailed = NVSDK_NGX_FAILED(result);
    RecordCall("GetFeatureRequirements", int32_t(result), requirementsFailed);
    if (requirementsFailed) {
        reason = "GetFeatureRequirements failed (raw=" + std::to_string(int32_t(result)) + ")";
        report_.deviceExtensions.reason = reason;
        return false;
    }
    report_.featureSupport = uint32_t(requirements.FeatureSupported);
    if (requirements.FeatureSupported != NVSDK_NGX_FeatureSupportResult_Supported) {
        reason = "NGX reports Super Sampling unsupported (flags=" + std::to_string(*report_.featureSupport) + ")";
        report_.deviceExtensions.reason = reason;
        return false;
    }
    uint32_t count = 0;
    VkExtensionProperties* extensions = nullptr;
    result = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, device, &info.discovery, &count, &extensions);
    const bool failed = NVSDK_NGX_FAILED(result);
    RecordCall("GetFeatureDeviceExtensionRequirements", int32_t(result), failed);
    const bool copied = CopyExtensions(result, count, extensions, required, reason);
    CopyExtensionNames(required, report_.deviceExtensions.required);
    report_.deviceExtensions.reason = copied ? "" : reason;
    return copied;
#else
    (void)instance; (void)device; (void)required;
    reason = "NGX SDK disabled";
    return false;
#endif
}

void Controller::ProbeOnce(const plume::VulkanInterface& vulkanInterface, const plume::VulkanDevice& device) {
    if (probeAttempted_) return;
    probeAttempted_ = true;
    if (report_.deviceName.empty()) report_.deviceName = device.physicalDeviceProperties.deviceName;
    if (report_.vendorId == 0) report_.vendorId = device.physicalDeviceProperties.vendorID;
    if (report_.deviceId == 0) report_.deviceId = device.physicalDeviceProperties.deviceID;
    if (report_.driverVersion == 0) report_.driverVersion = device.physicalDeviceProperties.driverVersion;
    if (report_.vendorId == 0x10de && report_.driverVersionText.empty()) {
        report_.driverVersionText = std::to_string(report_.driverVersion >> 22) + "." +
            std::to_string((report_.driverVersion >> 14) & 0xff);
    }

#if !defined(LO_DLSS_SDK)
    return;
#else
    const auto& instanceStatus = vulkanInterface.getExternalExtensionStatus();
    const auto& deviceStatus = device.getExternalExtensionStatus();
    report_.instanceExtensions.state = instanceStatus.state == plume::VulkanExtensionState::Enabled ? "enabled" :
        instanceStatus.state == plume::VulkanExtensionState::Disabled ? "disabled" : "not_requested";
    report_.instanceExtensions.reason = instanceStatus.reason;
    report_.instanceExtensions.createFailure = int32_t(instanceStatus.createFailure);
    report_.deviceExtensions.state = deviceStatus.state == plume::VulkanExtensionState::Enabled ? "enabled" :
        deviceStatus.state == plume::VulkanExtensionState::Disabled ? "disabled" : "not_requested";
    report_.deviceExtensions.reason = deviceStatus.reason;
    report_.deviceExtensions.createFailure = int32_t(deviceStatus.createFailure);
    if (instanceStatus.state != plume::VulkanExtensionState::Enabled || deviceStatus.state != plume::VulkanExtensionState::Enabled) {
        report_.state = (apiFailure_ || instanceStatus.createFailure != VK_SUCCESS || deviceStatus.createFailure != VK_SUCCESS)
            ? ProbeState::ApiError : ProbeState::Unavailable;
        report_.reason = device.getExternalExtensionStatus().reason.empty()
            ? vulkanInterface.getExternalExtensionStatus().reason : device.getExternalExtensionStatus().reason;
        if (report_.reason.empty()) report_.reason = "NGX Vulkan extension group was not enabled";
        return;
    }

    if (!report_.featureSupport) {
        report_.state = ProbeState::ApiError;
        report_.reason = "GetFeatureRequirements was not completed by the device extension hook";
        return;
    }
    if (*report_.featureSupport != NVSDK_NGX_FeatureSupportResult_Supported) {
        report_.state = ProbeState::Unavailable;
        report_.reason = "NGX reports Super Sampling unsupported (flags=" + std::to_string(*report_.featureSupport) + ")";
        return;
    }

    // ProbeOnce deliberately owns a standalone Init/capability/Shutdown cycle.
    // The retained context is only used by a later persistent EnsureSession.
    sessionInterface_ = &vulkanInterface;
    sessionDevice_ = &device;
    sessionInstance_ = vulkanInterface.instance;
    DiscoveryInfo info(applicationDataPath_, runtimePath_);
    auto result = NVSDK_NGX_VULKAN_Init_with_ProjectID(kProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "LostOdysseyRecomp",
        info.appDataPath.c_str(), vulkanInterface.instance, device.physicalDevice, device.vk, vkGetInstanceProcAddr,
        vkGetDeviceProcAddr, &info.featureInfo);
    RecordCall("Init_with_ProjectID", int32_t(result), NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result)) {
        report_.state = ClassifyNgxResult(int32_t(result), int32_t(NVSDK_NGX_Result_Success),
            int32_t(NVSDK_NGX_Result_FAIL_FeatureNotSupported));
        report_.reason = report_.state == ProbeState::Unavailable ? "NGX initialization reports Super Sampling unsupported"
            : "NGX initialization failed";
        return;
    }

    NVSDK_NGX_Parameter* parameters = nullptr;
    result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters);
    RecordCall("GetCapabilityParameters", int32_t(result), NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result) || parameters == nullptr) {
        report_.state = ProbeState::ApiError;
        report_.reason = "GetCapabilityParameters failed";
    } else {
        auto readCapability = [&](const char* name, const char* key, CapabilityValue& value) {
            int rawValue = 0;
            const auto capabilityResult = NVSDK_NGX_Parameter_GetI(parameters, key, &rawValue);
            value.raw = int32_t(capabilityResult);
            if (!NVSDK_NGX_FAILED(capabilityResult)) value.value = rawValue;
            RecordCall(name, int32_t(capabilityResult), NVSDK_NGX_FAILED(capabilityResult));
            return !NVSDK_NGX_FAILED(capabilityResult);
        };
        readCapability("Parameter_GetI(SuperSampling_Available)", NVSDK_NGX_EParameter_SuperSampling_Available, report_.srAvailable);
        readCapability("Parameter_GetI(SuperSampling_NeedsUpdatedDriver)", NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, report_.needsUpdatedDriver);
        readCapability("Parameter_GetI(SuperSampling_MinDriverVersionMajor)", NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, report_.minDriverVersionMajor);
        readCapability("Parameter_GetI(SuperSampling_MinDriverVersionMinor)", NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, report_.minDriverVersionMinor);
        readCapability("Parameter_GetI(SuperSampling_FeatureInitResult)", NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, report_.featureInitResult);
        const auto capabilityDecision = ClassifySuperSamplingCapabilities(report_.srAvailable, report_.needsUpdatedDriver,
            report_.minDriverVersionMajor, report_.minDriverVersionMinor, report_.featureInitResult,
            int32_t(NVSDK_NGX_Result_Success), int32_t(NVSDK_NGX_Result_FAIL_FeatureNotSupported));
        if (capabilityDecision.decision != CapabilityDecision::Proceed) {
            report_.state = capabilityDecision.decision == CapabilityDecision::Unavailable ? ProbeState::Unavailable : ProbeState::ApiError;
            report_.reason = capabilityDecision.reason;
        } else {
            const struct { const char* name; NVSDK_NGX_PerfQuality_Value value; } qualities[] = {
                {"quality", NVSDK_NGX_PerfQuality_Value_MaxQuality},
                {"balanced", NVSDK_NGX_PerfQuality_Value_Balanced},
                {"performance", NVSDK_NGX_PerfQuality_Value_MaxPerf},
            };
            bool optimalFailure = false;
            for (const auto& quality : qualities) {
                OptimalSettings settings;
                settings.quality = quality.name;
                const auto optimalResult = NGX_DLSS_GET_OPTIMAL_SETTINGS(parameters, 1920, 1080, quality.value,
                    &settings.optimalWidth, &settings.optimalHeight, &settings.maxWidth, &settings.maxHeight,
                    &settings.minWidth, &settings.minHeight, &settings.sharpness);
                settings.result = int32_t(optimalResult);
                RecordCall((std::string("DLSS_GetOptimalSettings(") + quality.name + ")").c_str(), int32_t(optimalResult));
                report_.optimalSettings.push_back(settings);
                optimalFailure |= NVSDK_NGX_FAILED(optimalResult);
            }
            if (optimalFailure || !HasValidOptimalSettings(report_)) {
                report_.state = ProbeState::ApiError;
                report_.reason = "NGX optimal-settings query failed or returned a zero dimension";
            } else {
                report_.state = ProbeState::Available;
                report_.reason = "NGX Super Sampling capability and optimal settings queried";
            }
        }
        const auto destroyResult = NVSDK_NGX_VULKAN_DestroyParameters(parameters);
        RecordCall("DestroyParameters", int32_t(destroyResult));
        if (NVSDK_NGX_FAILED(destroyResult)) {
            report_.state = ProbeState::ApiError;
            report_.reason += (report_.reason.empty() ? "" : "; ");
            report_.reason += "NGX capability parameter destruction failed";
        }
    }
    const auto shutdownResult = NVSDK_NGX_VULKAN_Shutdown1(device.vk);
    RecordCall("Shutdown1", int32_t(shutdownResult));
    if (NVSDK_NGX_FAILED(shutdownResult)) {
        report_.state = ProbeState::ApiError;
        report_.reason += (report_.reason.empty() ? "" : "; ");
        report_.reason += "NGX shutdown failed";
    }
#endif
}

upscaling::OutputSizing Controller::QueryOutputSizing(const plume::VulkanInterface& vulkanInterface,
    const plume::VulkanDevice& device, const upscaling::SizingKey& key) {
    upscaling::OutputSizing sizing;
    sizing.key = key;
    const auto setAll = [&](upscaling::SizingState state, std::optional<int32_t> result = std::nullopt) {
        for (auto& mode : sizing.modes) { mode.state = state; mode.ngxResult = result; }
    };
    if (!key.outputWidth || !key.outputHeight) { setAll(upscaling::SizingState::Error); return sizing; }
#if !defined(LO_DLSS_SDK)
    setAll(upscaling::SizingState::Unavailable);
    return sizing;
#else
    const auto& instanceStatus = vulkanInterface.getExternalExtensionStatus();
    const auto& deviceStatus = device.getExternalExtensionStatus();
    if (instanceStatus.state != plume::VulkanExtensionState::Enabled || deviceStatus.state != plume::VulkanExtensionState::Enabled) {
        setAll((instanceStatus.createFailure == VK_SUCCESS && deviceStatus.createFailure == VK_SUCCESS)
            ? upscaling::SizingState::Unavailable : upscaling::SizingState::Error);
        return sizing;
    }
    std::string pathReason;
    if (!CreateApplicationDataPath(pathReason)) { setAll(upscaling::SizingState::Error); return sizing; }
    NVSDK_NGX_Parameter* parameters = nullptr;
    bool temporarySession = false;
    if (sessionInterface_ == &vulkanInterface && sessionDevice_ == &device) {
        if (!sessionInitialized_) {
            const auto status = EnsureSession(device);
            if (status != SrStatus::Executable) {
                setAll(status == SrStatus::Bypass ? upscaling::SizingState::Unavailable : upscaling::SizingState::Error);
                return sizing;
            }
        }
        if (!capabilityParameters_ || sessionInstance_ != vulkanInterface.instance) {
            setAll(upscaling::SizingState::Error);
            return sizing;
        }
        parameters = static_cast<NVSDK_NGX_Parameter*>(capabilityParameters_);
    } else if (sessionInitialized_) {
        if (sessionInstance_ != vulkanInterface.instance || sessionDevice_ != &device || !capabilityParameters_) {
            setAll(upscaling::SizingState::Error);
            return sizing;
        }
        parameters = static_cast<NVSDK_NGX_Parameter*>(capabilityParameters_);
    } else {
        DiscoveryInfo info(applicationDataPath_, runtimePath_);
        NVSDK_NGX_FeatureRequirement requirements = {};
        auto result = NVSDK_NGX_VULKAN_GetFeatureRequirements(vulkanInterface.instance, device.physicalDevice,
            &info.discovery, &requirements);
        RecordCall("Sizing_GetFeatureRequirements", int32_t(result), NVSDK_NGX_FAILED(result));
        if (NVSDK_NGX_FAILED(result)) { setAll(upscaling::SizingState::Error, int32_t(result)); return sizing; }
        if (requirements.FeatureSupported != NVSDK_NGX_FeatureSupportResult_Supported) {
            setAll(upscaling::SizingState::Unavailable, int32_t(requirements.FeatureSupported));
            return sizing;
        }
        result = NVSDK_NGX_VULKAN_Init_with_ProjectID(kProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "LostOdysseyRecomp",
            info.appDataPath.c_str(), vulkanInterface.instance, device.physicalDevice, device.vk, vkGetInstanceProcAddr,
            vkGetDeviceProcAddr, &info.featureInfo);
        RecordCall("Sizing_Init_with_ProjectID", int32_t(result), NVSDK_NGX_FAILED(result));
        if (NVSDK_NGX_FAILED(result)) {
            setAll(ClassifyNgxResult(int32_t(result), int32_t(NVSDK_NGX_Result_Success),
                int32_t(NVSDK_NGX_Result_FAIL_FeatureNotSupported)) == ProbeState::Unavailable
                ? upscaling::SizingState::Unavailable : upscaling::SizingState::Error, int32_t(result));
            return sizing;
        }
        result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters);
        RecordCall("Sizing_GetCapabilityParameters", int32_t(result), NVSDK_NGX_FAILED(result));
        if (NVSDK_NGX_FAILED(result) || !parameters) {
            setAll(upscaling::SizingState::Error, int32_t(result));
            const auto shutdownResult = NVSDK_NGX_VULKAN_Shutdown1(device.vk);
            RecordCall("Sizing_Shutdown1", int32_t(shutdownResult), NVSDK_NGX_FAILED(shutdownResult));
            return sizing;
        }
        temporarySession = true;
    }
    CapabilityValue available, needsDriver, minMajor, minMinor, featureInit;
    const auto read = [&](const char* keyName, const char* parameter, CapabilityValue& value) {
        int raw = 0;
        const auto getResult = NVSDK_NGX_Parameter_GetI(parameters, parameter, &raw);
        value.raw = int32_t(getResult);
        if (!NVSDK_NGX_FAILED(getResult)) value.value = raw;
        RecordCall(keyName, int32_t(getResult), NVSDK_NGX_FAILED(getResult));
    };
    read("Sizing_Parameter_GetI(SuperSampling_Available)", NVSDK_NGX_EParameter_SuperSampling_Available, available);
    read("Sizing_Parameter_GetI(SuperSampling_NeedsUpdatedDriver)", NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, needsDriver);
    read("Sizing_Parameter_GetI(SuperSampling_MinDriverVersionMajor)", NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, minMajor);
    read("Sizing_Parameter_GetI(SuperSampling_MinDriverVersionMinor)", NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, minMinor);
    read("Sizing_Parameter_GetI(SuperSampling_FeatureInitResult)", NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, featureInit);
    const auto capability = ClassifySuperSamplingCapabilities(available, needsDriver, minMajor, minMinor, featureInit,
        int32_t(NVSDK_NGX_Result_Success), int32_t(NVSDK_NGX_Result_FAIL_FeatureNotSupported));
    if (capability.decision != CapabilityDecision::Proceed) {
        setAll(capability.decision == CapabilityDecision::Unavailable ? upscaling::SizingState::Unavailable : upscaling::SizingState::Error);
    } else {
        const NVSDK_NGX_PerfQuality_Value qualities[] = {NVSDK_NGX_PerfQuality_Value_MaxQuality,
            NVSDK_NGX_PerfQuality_Value_Balanced, NVSDK_NGX_PerfQuality_Value_MaxPerf};
        for (size_t index = 0; index < sizing.modes.size(); ++index) {
            auto& mode = sizing.modes[index];
            uint32_t optimalWidth = 0, optimalHeight = 0, maxWidth = 0, maxHeight = 0, minWidth = 0, minHeight = 0;
            float sharpness = 0.0f;
            const auto optimalResult = NGX_DLSS_GET_OPTIMAL_SETTINGS(parameters, key.outputWidth, key.outputHeight,
                qualities[index], &optimalWidth, &optimalHeight, &maxWidth, &maxHeight, &minWidth, &minHeight, &sharpness);
            mode.ngxResult = int32_t(optimalResult);
            RecordCall("Sizing_DLSS_GetOptimalSettings", int32_t(optimalResult), NVSDK_NGX_FAILED(optimalResult));
            const bool valid = !NVSDK_NGX_FAILED(optimalResult) && optimalWidth && optimalHeight && minWidth && minHeight &&
                maxWidth && maxHeight && minWidth <= optimalWidth && optimalWidth <= maxWidth &&
                minHeight <= optimalHeight && optimalHeight <= maxHeight;
            mode.state = valid ? upscaling::SizingState::Ready : upscaling::SizingState::Error;
            if (valid) { mode.optimal = {optimalWidth, optimalHeight}; mode.minimum = {minWidth, minHeight}; mode.maximum = {maxWidth, maxHeight}; }
        }
    }
    if (temporarySession) {
        const auto destroyResult = NVSDK_NGX_VULKAN_DestroyParameters(parameters);
        RecordCall("Sizing_DestroyParameters", int32_t(destroyResult), NVSDK_NGX_FAILED(destroyResult));
        const auto shutdownResult = NVSDK_NGX_VULKAN_Shutdown1(device.vk);
        RecordCall("Sizing_Shutdown1", int32_t(shutdownResult), NVSDK_NGX_FAILED(shutdownResult));
        if (NVSDK_NGX_FAILED(destroyResult) || NVSDK_NGX_FAILED(shutdownResult)) setAll(upscaling::SizingState::Error);
    }
    return sizing;
#endif
}

SrStatus Controller::EnsureSession(const plume::VulkanDevice& device) {
#if !defined(LO_DLSS_SDK)
    (void)device;
    return SrStatus::Bypass;
#else
    if (sessionDevice_ && sessionDevice_ != &device) return SrStatus::NeedsReconfigure;
    if (sessionInstance_ == VK_NULL_HANDLE) return SrStatus::Bypass;
    if (sessionInitialized_) return capabilityParameters_ ? SrStatus::Executable : SrStatus::Failed;
    if (sessionFailed_) return SrStatus::Failed;
    const auto& deviceStatus = device.getExternalExtensionStatus();
    if ((sessionInterface_ && sessionInterface_->getExternalExtensionStatus().state != plume::VulkanExtensionState::Enabled) ||
        deviceStatus.state != plume::VulkanExtensionState::Enabled)
        return SrStatus::Bypass;

    std::string reason;
    if (!CreateApplicationDataPath(reason)) { sessionFailed_ = true; return SrStatus::Failed; }
    DiscoveryInfo info(applicationDataPath_, runtimePath_);
    auto result = NVSDK_NGX_VULKAN_Init_with_ProjectID(kProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "LostOdysseyRecomp",
        info.appDataPath.c_str(), sessionInstance_, device.physicalDevice, device.vk, vkGetInstanceProcAddr,
        vkGetDeviceProcAddr, &info.featureInfo);
    RecordCall("Session_Init_with_ProjectID", int32_t(result), NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result)) { sessionFailed_ = true; return SrStatus::Failed; }

    NVSDK_NGX_Parameter* capabilities = nullptr;
    result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&capabilities);
    RecordCall("Session_GetCapabilityParameters", int32_t(result), NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result) || !capabilities) {
        const auto shutdownResult = NVSDK_NGX_VULKAN_Shutdown1(device.vk);
        RecordCall("Session_Shutdown1", int32_t(shutdownResult), NVSDK_NGX_FAILED(shutdownResult));
        sessionFailed_ = true;
        return SrStatus::Failed;
    }
    const auto read = [&](const char* name, const char* key, CapabilityValue& value) {
        int raw = 0;
        const auto getResult = NVSDK_NGX_Parameter_GetI(capabilities, key, &raw);
        value.raw = int32_t(getResult);
        if (!NVSDK_NGX_FAILED(getResult)) value.value = raw;
        RecordCall(name, int32_t(getResult), NVSDK_NGX_FAILED(getResult));
    };
    read("Session_Parameter_GetI(SuperSampling_Available)", NVSDK_NGX_EParameter_SuperSampling_Available, report_.srAvailable);
    read("Session_Parameter_GetI(SuperSampling_NeedsUpdatedDriver)", NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, report_.needsUpdatedDriver);
    read("Session_Parameter_GetI(SuperSampling_MinDriverVersionMajor)", NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, report_.minDriverVersionMajor);
    read("Session_Parameter_GetI(SuperSampling_MinDriverVersionMinor)", NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, report_.minDriverVersionMinor);
    read("Session_Parameter_GetI(SuperSampling_FeatureInitResult)", NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, report_.featureInitResult);
    const auto capability = ClassifySuperSamplingCapabilities(report_.srAvailable, report_.needsUpdatedDriver,
        report_.minDriverVersionMajor, report_.minDriverVersionMinor, report_.featureInitResult,
        int32_t(NVSDK_NGX_Result_Success), int32_t(NVSDK_NGX_Result_FAIL_FeatureNotSupported));
    if (capability.decision != CapabilityDecision::Proceed) {
        const auto destroyResult = NVSDK_NGX_VULKAN_DestroyParameters(capabilities);
        RecordCall("Session_DestroyCapabilityParameters", int32_t(destroyResult), NVSDK_NGX_FAILED(destroyResult));
        const auto shutdownResult = NVSDK_NGX_VULKAN_Shutdown1(device.vk);
        RecordCall("Session_Shutdown1", int32_t(shutdownResult), NVSDK_NGX_FAILED(shutdownResult));
        sessionFailed_ = capability.decision == CapabilityDecision::ApiError;
        return capability.decision == CapabilityDecision::Unavailable ? SrStatus::Bypass : SrStatus::Failed;
    }
    capabilityParameters_ = capabilities;
    sessionDevice_ = &device;
    sessionInitialized_ = true;
    report_.state = ProbeState::Available;
    report_.reason = "NGX persistent Super Sampling session ready";
    return SrStatus::Executable;
#endif
}

bool Controller::NeedsFeatureRecreate(const SrConfig& config) const {
    return featureFailed_ || !featureConfigValid_ || featureConfig_ != config;
}

SrStatus Controller::AllocateParameters() {
#if !defined(LO_DLSS_SDK)
    return SrStatus::Bypass;
#else
    if (!sessionInitialized_) return SrStatus::Failed;
    if (featureParameters_) return SrStatus::Executable;
    NVSDK_NGX_Parameter* parameters = nullptr;
    const auto result = NVSDK_NGX_VULKAN_AllocateParameters(&parameters);
    RecordCall("AllocateParameters", int32_t(result), NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result) || !parameters) return SrStatus::Failed;
    featureParameters_ = parameters;
    return SrStatus::Executable;
#endif
}

SrAttempt Controller::RecordIsolated(plume::VulkanCommandList& isolatedCommandList, const SrConfig& config,
    const temporal::TemporalFrameInputs& inputs, plume::VulkanTexture& output) {
    SrAttempt attempt;
#if !defined(LO_DLSS_SDK)
    (void)isolatedCommandList; (void)config; (void)inputs; (void)output;
    return attempt;
#else
    if (config.colorSpace == SrColorSpace::Unknown) return attempt;
    if (!sessionDevice_) return attempt;
    const auto session = EnsureSession(*sessionDevice_);
    if (session != SrStatus::Executable) { attempt.status = session; return attempt; }
    if (NeedsFeatureRecreate(config) && featureConfigValid_) { attempt.status = SrStatus::NeedsReconfigure; return attempt; }
    if (featureFailed_) { attempt.status = SrStatus::NeedsReconfigure; return attempt; }
    if (inputs.renderFrameId && inputs.renderFrameId == lastSrAttemptFrameId_) return attempt;

    const auto validRegion = [&](const temporal::TextureRegion& region) {
        if (!region.Complete() || region.width != config.renderExtent.width || region.height != config.renderExtent.height)
            return false;
        const auto* texture = static_cast<const plume::VulkanTexture*>(region.texture);
        return ValidImage(*texture, sessionDevice_) && region.allocation.width == texture->desc.width &&
            region.allocation.height == texture->desc.height;
    };
    if (!config.renderExtent.width || !config.renderExtent.height || !config.outputExtent.width || !config.outputExtent.height ||
        config.deviceEpoch != inputs.plan.deviceEpoch || inputs.plan.consumer != upscaling::TemporalConsumer::DlssSr ||
        !inputs.CompleteForConsumer() || !temporal::MatchesDepthConvention(inputs.depthConvention, config.depthInverted) ||
        !validRegion(inputs.color) || !validRegion(inputs.depth) || !validRegion(inputs.motion) ||
        !ValidImage(output, sessionDevice_) || output.desc.width != config.outputExtent.width || output.desc.height != config.outputExtent.height ||
        static_cast<const plume::VulkanTexture*>(inputs.depth.texture)->imageFormat != VK_FORMAT_R32_SFLOAT ||
        !std::isfinite(inputs.jitter.pixelX) || !std::isfinite(inputs.jitter.pixelY) ||
        !IsFinitePositive(inputs.preExposure) || !IsFinitePositive(inputs.exposureScale) ||
        (config.colorSpace == SrColorSpace::DisplayEncoded && inputs.colorEncoding != temporal::ColorEncoding::Sdr) ||
        (config.colorSpace == SrColorSpace::Linear && inputs.colorEncoding != temporal::ColorEncoding::HdrLinear))
        return attempt;

    // Retain parameters and any feature state through the fallback prefix batch,
    // including a failed Create/Evaluate recording whose primary is excluded.
    attempt.useId = srUses_.Record();
    if (!attempt.useId) { attempt.status = SrStatus::Failed; return attempt; }
    lastSrAttemptFrameId_ = inputs.renderFrameId;
    const auto failed = [&](std::optional<int32_t> rawNgx = std::nullopt,
                            std::optional<VkResult> rawVk = std::nullopt) {
        attempt.status = rawVk && *rawVk == VK_ERROR_DEVICE_LOST ? SrStatus::DeviceLost : SrStatus::Failed;
        attempt.rawNgxResult = rawNgx;
        if (rawVk) attempt.rawVkResult = int32_t(*rawVk);
        featureFailed_ = true;
        return attempt;
    };

    const auto begin = BeginIsolatedCommandList(isolatedCommandList);
    if (begin.reset) RecordCall("vkResetCommandBuffer", int32_t(*begin.reset), *begin.reset != VK_SUCCESS);
    if (begin.begin) RecordCall("vkBeginCommandBuffer", int32_t(*begin.begin), *begin.begin != VK_SUCCESS);
    if (!begin.reset || *begin.reset != VK_SUCCESS) return failed(std::nullopt, begin.reset);
    if (!begin.begin || *begin.begin != VK_SUCCESS) return failed(std::nullopt, begin.begin);
    const auto commandBuffer = isolatedCommandList.beginExternalCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        const auto end = EndIsolatedCommandList(isolatedCommandList);
        if (end) RecordCall("vkEndCommandBuffer", int32_t(*end), *end != VK_SUCCESS);
        return failed(std::nullopt, end);
    }

    auto* parameters = static_cast<NVSDK_NGX_Parameter*>(featureParameters_);
    bool created = false;
    if (!feature_) {
        if (AllocateParameters() != SrStatus::Executable) {
            isolatedCommandList.endExternalCommands();
            const auto end = EndIsolatedCommandList(isolatedCommandList);
            if (end) RecordCall("vkEndCommandBuffer", int32_t(*end), *end != VK_SUCCESS);
            return failed(std::nullopt, end);
        }
        parameters = static_cast<NVSDK_NGX_Parameter*>(featureParameters_);
        NVSDK_NGX_DLSS_Create_Params create = {};
        create.Feature.InWidth = config.renderExtent.width;
        create.Feature.InHeight = config.renderExtent.height;
        create.Feature.InTargetWidth = config.outputExtent.width;
        create.Feature.InTargetHeight = config.outputExtent.height;
        create.Feature.InPerfQualityValue = ToNgxQuality(config.quality);
        create.InFeatureCreateFlags = DlssFlags(config);
        create.InEnableOutputSubrects = false;
        NVSDK_NGX_Handle* handle = nullptr;
        const auto createResult = NGX_VULKAN_CREATE_DLSS_EXT1(sessionDevice_->vk, commandBuffer, 1, 1,
            &handle, parameters, &create);
        RecordCall("CREATE_DLSS_EXT1", int32_t(createResult), NVSDK_NGX_FAILED(createResult));
        if (NVSDK_NGX_FAILED(createResult) || !handle) {
            isolatedCommandList.endExternalCommands();
            const auto end = EndIsolatedCommandList(isolatedCommandList);
            if (end) RecordCall("vkEndCommandBuffer", int32_t(*end), *end != VK_SUCCESS);
            return failed(int32_t(createResult), end);
        }
        feature_ = handle;
        featureConfig_ = config;
        featureConfigValid_ = true;
        created = true;
        report_.srImplemented = true;
    }

    const auto& color = *static_cast<const plume::VulkanTexture*>(inputs.color.texture);
    const auto& depth = *static_cast<const plume::VulkanTexture*>(inputs.depth.texture);
    const auto& motion = *static_cast<const plume::VulkanTexture*>(inputs.motion.texture);
    auto colorResource = ImageResource(color, false);
    auto depthResource = ImageResource(depth, false);
    auto motionResource = ImageResource(motion, false);
    auto outputResource = ImageResource(output, true);
    NVSDK_NGX_VK_DLSS_Eval_Params evaluate = {};
    evaluate.Feature.pInColor = &colorResource;
    evaluate.Feature.pInOutput = &outputResource;
    evaluate.pInDepth = &depthResource;
    evaluate.pInMotionVectors = &motionResource;
    evaluate.InJitterOffsetX = float(inputs.jitter.pixelX);
    evaluate.InJitterOffsetY = float(inputs.jitter.pixelY);
    evaluate.InRenderSubrectDimensions = {config.renderExtent.width, config.renderExtent.height};
    evaluate.InReset = created || inputs.resetHistory ? 1 : 0;
    evaluate.InMVScaleX = 1.0f;
    evaluate.InMVScaleY = 1.0f;
    evaluate.InColorSubrectBase = {inputs.color.x, inputs.color.y};
    evaluate.InDepthSubrectBase = {inputs.depth.x, inputs.depth.y};
    evaluate.InMVSubrectBase = {inputs.motion.x, inputs.motion.y};
    evaluate.InOutputSubrectBase = {0, 0};
    evaluate.InPreExposure = inputs.preExposure;
    evaluate.InExposureScale = inputs.exposureScale;
    // Transparency and exposure resources remain null. Auto exposure is only
    // selected by SrConfig and no guest alpha/mask is bound to NGX.
#if defined(LO_NATIVE_DLSS_TEST_INJECT_EVALUATE_FAILURE)
    if (const char* inject = std::getenv("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE"); inject && *inject == '1') {
        // Test-only host injection: this records a command before reporting a
        // synthetic NGX failure. The caller must exclude this primary list and
        // submit its already-recorded prefix fallback instead.
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 0, nullptr);
        RecordCall("TEST_INJECT_DLSS_EVALUATE_FAILURE", int32_t(NVSDK_NGX_Result_FAIL_InvalidParameter), true);
        isolatedCommandList.endExternalCommands();
        const auto end = EndIsolatedCommandList(isolatedCommandList);
        if (end) RecordCall("vkEndCommandBuffer", int32_t(*end), *end != VK_SUCCESS);
        return failed(int32_t(NVSDK_NGX_Result_FAIL_InvalidParameter), end);
    }
#endif
    const auto evaluateResult = NGX_VULKAN_EVALUATE_DLSS_EXT(commandBuffer,
        static_cast<NVSDK_NGX_Handle*>(feature_), parameters, &evaluate);
    RecordCall("EVALUATE_DLSS_EXT", int32_t(evaluateResult), NVSDK_NGX_FAILED(evaluateResult));
    isolatedCommandList.endExternalCommands();
    const auto end = EndIsolatedCommandList(isolatedCommandList);
    if (end) RecordCall("vkEndCommandBuffer", int32_t(*end), *end != VK_SUCCESS);
    if (!end || *end != VK_SUCCESS) return failed(int32_t(evaluateResult), end);
    if (NVSDK_NGX_FAILED(evaluateResult)) return failed(int32_t(evaluateResult), end);
    report_.srEvaluated = true;
    attempt.status = SrStatus::Executable;
    return attempt;
#endif
}

void Controller::OnBatchSubmitted(uint64_t useId, uint64_t submissionSerial) {
    srUses_.Submit(useId, submissionSerial);
}

void Controller::OnBatchDiscarded(uint64_t useId) {
    srUses_.Discard(useId);
}

void Controller::ReleaseCompletedThrough(uint64_t submissionSerial) {
    srUses_.CompleteThrough(submissionSerial);
}

void Controller::AbandonUsesAfterDeviceLoss() {
    srUses_.AbandonAfterDeviceLoss();
    sessionFailed_ = true;
}

void Controller::ReleaseFeatureAfterGpuDrain() {
    if (!srUses_.Empty()) return;
#if defined(LO_DLSS_SDK)
    if (feature_) {
        const auto result = NVSDK_NGX_VULKAN_ReleaseFeature(static_cast<NVSDK_NGX_Handle*>(feature_));
        RecordCall("ReleaseFeature", int32_t(result), NVSDK_NGX_FAILED(result));
        feature_ = nullptr;
    }
#endif
    featureConfigValid_ = false;
    featureFailed_ = false;
    lastSrAttemptFrameId_ = 0;
}

void Controller::ShutdownAfterGpuDrain() {
    if (!srUses_.Empty()) return;
    ReleaseFeatureAfterGpuDrain();
#if defined(LO_DLSS_SDK)
    if (featureParameters_) {
        const auto result = NVSDK_NGX_VULKAN_DestroyParameters(static_cast<NVSDK_NGX_Parameter*>(featureParameters_));
        RecordCall("DestroyFeatureParameters", int32_t(result), NVSDK_NGX_FAILED(result));
        featureParameters_ = nullptr;
    }
    if (capabilityParameters_) {
        const auto result = NVSDK_NGX_VULKAN_DestroyParameters(static_cast<NVSDK_NGX_Parameter*>(capabilityParameters_));
        RecordCall("DestroyCapabilityParameters", int32_t(result), NVSDK_NGX_FAILED(result));
        capabilityParameters_ = nullptr;
    }
    if (sessionInitialized_ && sessionDevice_) {
        const auto result = NVSDK_NGX_VULKAN_Shutdown1(sessionDevice_->vk);
        RecordCall("Session_Shutdown1", int32_t(result), NVSDK_NGX_FAILED(result));
    }
#endif
    sessionInitialized_ = false;
    sessionFailed_ = false;
    sessionInterface_ = nullptr;
    sessionDevice_ = nullptr;
    sessionInstance_ = VK_NULL_HANDLE;
}
} // namespace gpu::dlss
#endif
