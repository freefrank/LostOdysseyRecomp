#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "dlss_ngx.h"
#include "dlss_evaluate_capture.h"
#include "dlss_nr.h"
#include "dlss_nr_state.h"
#include "sr_hybrid_mask.h"
#if defined(_WIN32) && defined(LO_GPU_PLUME)
#include <plume_d3d12.h>
#endif

#if defined(LO_GPU_PLUME)
#include "temporal_frame_inputs.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <codecvt>
#include <iterator>
#include <locale>
#include <type_traits>
#if defined(_WIN32)
#include <windows.h>
#endif
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

LogSink g_logSink = nullptr;
#if defined(_WIN32)
ComputeShaderCompiler g_computeShaderCompiler = nullptr;
#endif

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

#if defined(_WIN32)
// The NR snippet loads its kernels through these; the list comes from the
// integrations that drive it directly.
constexpr const char* kNeuralRenderingDeviceExtensions[] = {
    VK_NVX_BINARY_IMPORT_EXTENSION_NAME, VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME};

// The snippet only serves callers whose module is named nvngx.dll, the NGX
// core's name, and reads that name through GetModuleFileNameW. Its import is
// pointed here so it sees this executable under that name; every other query
// reaches Windows unchanged. The DLL's file is not modified.
using PFN_GetModuleFileNameW = DWORD (WINAPI*)(HMODULE, LPWSTR, DWORD);
PFN_GetModuleFileNameW g_systemGetModuleFileNameW = nullptr;
HMODULE g_executableModule = nullptr;

DWORD WINAPI SnippetGetModuleFileNameW(HMODULE module, LPWSTR buffer, DWORD size) {
    constexpr wchar_t kName[] = L"nvngx.dll";
    constexpr DWORD kLength = DWORD(std::size(kName) - 1);
    if (!module || module != g_executableModule || !buffer || !size)
        return g_systemGetModuleFileNameW(module, buffer, size);
    if (size <= kLength) {
        buffer[0] = L'\0';
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return size;
    }
    std::memcpy(buffer, kName, sizeof(kName));
    return kLength;
}

bool RedirectSnippetModuleName(HMODULE snippet) {
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&SnippetGetModuleFileNameW), &g_executableModule))
        return false;
    auto* base = reinterpret_cast<uint8_t*>(snippet);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imports.VirtualAddress) return false;
    for (auto* descriptor = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress);
         descriptor->Name; ++descriptor) {
        if (!descriptor->OriginalFirstThunk) continue;
        const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            const auto* import = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(import->Name), "GetModuleFileNameW") != 0) continue;
            const auto redirected = reinterpret_cast<ULONG_PTR>(&SnippetGetModuleFileNameW);
            if (slots->u1.Function == redirected) return true;
            DWORD protection = 0;
            if (!VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), PAGE_READWRITE, &protection))
                return false;
            g_systemGetModuleFileNameW = reinterpret_cast<PFN_GetModuleFileNameW>(slots->u1.Function);
            slots->u1.Function = redirected;
            VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), protection, &protection);
            return true;
        }
    }
    return false;
}
#endif

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
    case upscaling::DlssQuality::Dlaa: return NVSDK_NGX_PerfQuality_Value_DLAA;
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

// NGX records no barriers for application images. A full memory barrier
// orders the SR evaluate, the copies and the NR evaluate on one list.
void FullBarrier(VkCommandBuffer commandBuffer) {
    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// Same-size copy with format conversion, to destinationX; both images stay in GENERAL.
void BlitGeneral(VkCommandBuffer commandBuffer, VkImage source, VkImage destination, uint32_t width, uint32_t height,
                 uint32_t destinationX = 0) {
    VkImageBlit region = {};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffsets[1] = {int32_t(width), int32_t(height), 1};
    region.dstSubresource = region.srcSubresource;
    region.dstOffsets[0] = {int32_t(destinationX), 0, 0};
    region.dstOffsets[1] = {int32_t(destinationX + width), int32_t(height), 1};
    vkCmdBlitImage(commandBuffer, source, VK_IMAGE_LAYOUT_GENERAL, destination, VK_IMAGE_LAYOUT_GENERAL,
        1, &region, VK_FILTER_NEAREST);
}

int DlssFlags(const SrConfig& config) {
    int flags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    if (config.colorSpace == SrColorSpace::Linear) flags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
    if (config.depthInverted) flags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
    if (config.autoExposure) flags |= NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    return flags;
}

// The SR model for every quality mode; NGX reads it when the feature is created.
// The parameter block outlives the feature, so all modes are rewritten each time.
static_assert(kRenderPresetL == NVSDK_NGX_DLSS_Hint_Render_Preset_L && kRenderPresetM == NVSDK_NGX_DLSS_Hint_Render_Preset_M);
void SetRenderPreset(NVSDK_NGX_Parameter* parameters, uint8_t preset) {
    for (const char* key : {NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality,
             NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance,
             NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraQuality})
        NVSDK_NGX_Parameter_SetUI(parameters, key, preset);
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

void SetLogSink(LogSink sink) { g_logSink = sink; }
#if defined(_WIN32)
void SetComputeShaderCompiler(ComputeShaderCompiler compiler) { g_computeShaderCompiler = compiler; }
#endif

#if defined(_WIN32) && defined(LO_DLSS_SDK)
namespace nr {
void Log(const char* format, ...) {
    char line[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (g_logSink) g_logSink(line);
    else std::fprintf(stderr, "%s\n", line);
}

ComputeShaderCompiler ShaderCompiler() { return g_computeShaderCompiler; }

std::filesystem::path SnippetPath(const std::filesystem::path& runtimeDirectory) {
    wchar_t configured[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"LO_DLSS_NR_PATH", configured, MAX_PATH);
    if (length && length < MAX_PATH) return configured;
    return runtimeDirectory / L"nvngx_dlssnr.dll";
}

const Snippet* LoadSnippet(const std::filesystem::path& runtimeDirectory, std::string& reason) {
    static Snippet snippet;
    static std::string failure;
    static bool attempted = false, loaded = false;
    if (attempted) {
        reason = failure;
        return loaded ? &snippet : nullptr;
    }
    attempted = true;
    const auto path = SnippetPath(runtimeDirectory);
    // A file that is not a valid DLL would show a modal "Bad Image" dialog.
    DWORD previousMode = 0;
    const BOOL quiet = SetThreadErrorMode(GetThreadErrorMode() | SEM_FAILCRITICALERRORS, &previousMode);
    const HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    const DWORD error = GetLastError();
    if (quiet) SetThreadErrorMode(previousMode, nullptr);
    if (!module) {
        failure = "nvngx_dlssnr.dll did not load from " + path.string() + " (error " + std::to_string(error) + ")";
        reason = failure;
        return nullptr;
    }
    const auto get = [&](auto& function, const char* name) {
        function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(GetProcAddress(module, name));
    };
    get(snippet.vk.init, "NVSDK_NGX_VULKAN_Init_Ext2");
    get(snippet.vk.create, "NVSDK_NGX_VULKAN_CreateFeature1");
    get(snippet.vk.evaluate, "NVSDK_NGX_VULKAN_EvaluateFeature");
    get(snippet.vk.release, "NVSDK_NGX_VULKAN_ReleaseFeature");
    get(snippet.vk.shutdown, "NVSDK_NGX_VULKAN_Shutdown1");
    get(snippet.d3d12.init, "NVSDK_NGX_D3D12_Init_Ext");
    get(snippet.d3d12.create, "NVSDK_NGX_D3D12_CreateFeature");
    get(snippet.d3d12.evaluate, "NVSDK_NGX_D3D12_EvaluateFeature");
    get(snippet.d3d12.release, "NVSDK_NGX_D3D12_ReleaseFeature");
    get(snippet.d3d12.shutdown, "NVSDK_NGX_D3D12_Shutdown1");
    if (!snippet.vk.Complete() && !snippet.d3d12.Complete()) {
        failure = "nvngx_dlssnr.dll lacks the NGX exports";
    } else if (!RedirectSnippetModuleName(module)) {
        failure = "nvngx_dlssnr.dll does not import GetModuleFileNameW as expected";
    } else {
        // Its import now points into this executable, so it is never unloaded.
        loaded = true;
        reason.clear();
        return &snippet;
    }
    reason = failure;
    return nullptr;
}

// NGX keeps each value under the type it was set with; a reader using another
// type sees the default. The types follow the integrations that drive it.
void SetControls(NVSDK_NGX_Parameter* parameters, uint32_t width, uint32_t height, bool depthInverted,
                 uint32_t preset, const NeuralRenderingTuning& tuning, uint32_t pass) {
    NVSDK_NGX_Parameter_SetI(parameters, "DLSSNR.Enabled", 1);
    NVSDK_NGX_Parameter_SetUI(parameters, "DLSSNR.Width", width);
    NVSDK_NGX_Parameter_SetUI(parameters, "DLSSNR.Height", height);
    NVSDK_NGX_Parameter_SetI(parameters, "DLSSNR.DepthInverted", depthInverted ? 1 : 0);
    NVSDK_NGX_Parameter_SetUI(parameters, "DLSSNR.Style", tuning.style);
    NVSDK_NGX_Parameter_SetF(parameters, "DLSSNR.Intensity", tuning.intensity);
    // Passes after the first skip local tone, the default of the multipass
    // integration this follows (wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass).
    NVSDK_NGX_Parameter_SetF(parameters, "DLSSNR.LocalToneStrength", pass ? 0.0f : tuning.localTone);
    NVSDK_NGX_Parameter_SetF(parameters, "DLSSNR.LocalStructureStrength", tuning.structure);
    NVSDK_NGX_Parameter_SetF(parameters, "DLSSNR.GlobalToneStrength", tuning.globalTone);
    NVSDK_NGX_Parameter_SetF(parameters, "DLSSNR.SkinStructureStrength", tuning.skin);
    NVSDK_NGX_Parameter_SetI(parameters, "DLSSNR.UseAutoMask", tuning.autoMask ? 1 : 0);
    NVSDK_NGX_Parameter_SetI(parameters, "DLSSNR.UICorrection", 0);
    NVSDK_NGX_Parameter_SetI(parameters, "DLSSNR.Hint.Render.Preset", int(preset));
    NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_CreationNodeMask, 1);
    NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_VisibilityNodeMask, 1);
}

void SetFrame(NVSDK_NGX_Parameter* parameters, const FrameRegions& regions, bool reset) {
    NVSDK_NGX_Parameter_SetI(parameters, "DLSSNR.Reset", reset ? 1 : 0);
    const auto subrect = [&](const char* name, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
        char key[64];
        const struct { const char* field; uint32_t value; } fields[] = {
            {"BaseX", x}, {"BaseY", y}, {"Width", w}, {"Height", h}};
        for (const auto& field : fields) {
            std::snprintf(key, sizeof(key), "DLSSNR.%sSubrect%s", name, field.field);
            NVSDK_NGX_Parameter_SetI(parameters, key, int(field.value));
        }
    };
    subrect("Color", 0, 0, regions.outputWidth, regions.outputHeight);
    subrect("Output", 0, 0, regions.outputWidth, regions.outputHeight);
    subrect("Depth", regions.depthX, regions.depthY, regions.renderWidth, regions.renderHeight);
    subrect("MVec", regions.motionX, regions.motionY, regions.renderWidth, regions.renderHeight);
    NVSDK_NGX_Parameter_SetF(parameters, "DLSSNR.MVecScaleX", regions.motionScale);
    NVSDK_NGX_Parameter_SetF(parameters, "DLSSNR.MVecScaleY", regions.motionScale);
}

FrameRegions GameFrame(const SrConfig& config, const temporal::TemporalFrameInputs& inputs) {
    // Render-resolution pixel vectors over the declared MVec subrect, as for SR.
    return {config.outputExtent.width, config.outputExtent.height, config.renderExtent.width, config.renderExtent.height,
        inputs.depth.x, inputs.depth.y, inputs.motion.x, inputs.motion.y, 1.0f};
}
} // namespace nr
#endif

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
    if (copied) QueryNeuralRenderingExtensions(device, required);
    CopyExtensionNames(required, report_.deviceExtensions.required);
    report_.deviceExtensions.reason = copied ? "" : reason;
    return copied;
#else
    (void)instance; (void)device; (void)required;
    reason = "NGX SDK disabled";
    return false;
#endif
}

void Controller::ProbeOnce(const plume::VulkanInterface& vulkanInterface, const plume::VulkanDevice& device,
    bool retainRuntimeForFrameGeneration) {
    if (backend_ == Backend::D3D12) return;
    if (probeAttempted_) return;
    probeAttempted_ = true;
    sharingRuntimeWithFg_ = retainRuntimeForFrameGeneration;
    if (sharingRuntimeWithFg_) {
        // Bind even an unavailable SR probe to this live shared device, so a
        // later sizing request cannot enter the temporary Init/Shutdown path.
        sessionInterface_ = &vulkanInterface;
        sessionDevice_ = &device;
        backend_ = Backend::Vulkan;
        sessionInstance_ = vulkanInterface.instance;
    }
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

    // Standalone probes keep their original bounded Init/Shutdown behavior.
    // With live Streamline, retain NGX until the final owner shutdown instead:
    // FSR must not depend on a DLSS-only sizing query to reopen this runtime.
    sessionInterface_ = &vulkanInterface;
    sessionDevice_ = &device;
    backend_ = Backend::Vulkan;
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
            // Keep baseline SR discovery independent of optional DLAA support.
            // DLAA is queried per output by QueryOutputSizing (also --sizing).
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
        if (!retainRuntimeForFrameGeneration) {
            const auto destroyResult = NVSDK_NGX_VULKAN_DestroyParameters(parameters);
            RecordCall("DestroyParameters", int32_t(destroyResult));
            if (NVSDK_NGX_FAILED(destroyResult)) {
                report_.state = ProbeState::ApiError;
                report_.reason += (report_.reason.empty() ? "" : "; ");
                report_.reason += "NGX capability parameter destruction failed";
            }
        }
    }
    if (retainRuntimeForFrameGeneration) {
        runtimeRetainedForFg_ = true;
        capabilityParameters_ = parameters;
        sessionInitialized_ = parameters && report_.state == ProbeState::Available;
        sessionFailed_ = report_.state == ProbeState::ApiError;
        sessionRetryable_ = false;
        report_.reason += "; device runtime retained for Streamline coexistence";
        return;
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
        for (auto& mode : sizing.modes) {
            mode.state = state;
            mode.ngxResult = result;
            mode.issue = upscaling::SizingIssue::Prerequisite;
        }
    };
    if (!key.outputWidth || !key.outputHeight) { setAll(upscaling::SizingState::Error); return sizing; }
    if (backend_ == Backend::D3D12) { setAll(upscaling::SizingState::Error); return sizing; }
    if (sharingRuntimeWithFg_ && (sessionInterface_ != &vulkanInterface || sessionDevice_ != &device)) {
        setAll(upscaling::SizingState::Error);
        return sizing; // Never create a temporary session beside a live FG device.
    }
#if !defined(LO_DLSS_SDK)
    setAll(upscaling::SizingState::Unavailable);
    return sizing;
#else
    // Reopen only on a bounded sizing request for this same device, with no
    // resources left from initialization. Never clear a device-loss latch.
    if (sessionFailed_) {
        if (sessionInterface_ != &vulkanInterface || sessionDevice_ != &device ||
            sessionInstance_ != vulkanInterface.instance || !sessionRetryable_ ||
            sessionInitialized_ || capabilityParameters_ || featureParameters_ || HasFeatureState()) {
            setAll(upscaling::SizingState::Error);
            return sizing;
        }
        sessionFailed_ = false;
        sessionRetryable_ = false;
    }
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
        value.value.reset();
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
        // Share the mode inventory with OutputSizing; no parallel three-entry
        // SDK array that can go out of bounds when DLAA is appended.
        for (const auto quality : upscaling::kDlssQualityModes) {
            auto& mode = sizing.modes[upscaling::DlssQualityIndex(quality)];
            // GetOptimalSettings mutates its capability map. Give each mode a
            // separate map so missing outputs cannot inherit the previous mode.
            NVSDK_NGX_Parameter* modeParameters = nullptr;
            const auto parameterResult = NVSDK_NGX_VULKAN_GetCapabilityParameters(&modeParameters);
            RecordCall("Sizing_GetModeCapabilityParameters", int32_t(parameterResult), NVSDK_NGX_FAILED(parameterResult));
            if (NVSDK_NGX_FAILED(parameterResult) || !modeParameters) {
                mode.state = upscaling::SizingState::Error;
                mode.issue = upscaling::SizingIssue::CapabilityParameters;
                mode.ngxResult = int32_t(parameterResult);
                if (modeParameters) {
                    const auto cleanup = NVSDK_NGX_VULKAN_DestroyParameters(modeParameters);
                    mode.cleanupResult = int32_t(cleanup);
                    RecordCall("Sizing_DestroyModeParameters", int32_t(cleanup), NVSDK_NGX_FAILED(cleanup));
                    if (NVSDK_NGX_FAILED(cleanup)) mode.issue = upscaling::SizingIssue::CleanupFailed;
                }
                continue;
            }
            uint32_t optimalWidth = 0, optimalHeight = 0, maxWidth = 0, maxHeight = 0, minWidth = 0, minHeight = 0;
            float sharpness = 0.0f;
            const auto optimalResult = NGX_DLSS_GET_OPTIMAL_SETTINGS(modeParameters, key.outputWidth, key.outputHeight,
                ToNgxQuality(quality), &optimalWidth, &optimalHeight, &maxWidth, &maxHeight, &minWidth, &minHeight, &sharpness);
            mode.ngxResult = int32_t(optimalResult);
            RecordCall("Sizing_DLSS_GetOptimalSettings", int32_t(optimalResult), NVSDK_NGX_FAILED(optimalResult));
            mode.optimal = {optimalWidth, optimalHeight};
            mode.minimum = {minWidth, minHeight};
            mode.maximum = {maxWidth, maxHeight};
            if (!NVSDK_NGX_FAILED(optimalResult)) {
                // The helper returns the callback status, not the required
                // output getter statuses. Its optional min/max fallback stays.
                unsigned int checkedWidth = 0, checkedHeight = 0;
                const auto widthResult = NVSDK_NGX_Parameter_GetUI(modeParameters, NVSDK_NGX_Parameter_OutWidth, &checkedWidth);
                const auto heightResult = NVSDK_NGX_Parameter_GetUI(modeParameters, NVSDK_NGX_Parameter_OutHeight, &checkedHeight);
                mode.optimalWidthResult = int32_t(widthResult);
                mode.optimalHeightResult = int32_t(heightResult);
                if (NVSDK_NGX_FAILED(widthResult) || NVSDK_NGX_FAILED(heightResult) ||
                    checkedWidth != optimalWidth || checkedHeight != optimalHeight)
                    mode.issue = upscaling::SizingIssue::OptimalRead;
                else
                    mode.issue = upscaling::ResolveDlssSizing(mode, quality, {key.outputWidth, key.outputHeight});
            } else {
                mode.issue = upscaling::SizingIssue::OptimalQuery;
            }
            mode.state = mode.issue == upscaling::SizingIssue::None ? upscaling::SizingState::Ready :
                optimalResult == NVSDK_NGX_Result_FAIL_FeatureNotSupported ? upscaling::SizingState::Unavailable : upscaling::SizingState::Error;
            // Raw vendor extents remain in the diagnostic below; mode.optimal
            // is the selected render extent (native output for supported DLAA).
            const auto cleanup = NVSDK_NGX_VULKAN_DestroyParameters(modeParameters);
            mode.cleanupResult = int32_t(cleanup);
            RecordCall("Sizing_DestroyModeParameters", int32_t(cleanup), NVSDK_NGX_FAILED(cleanup));
            if (NVSDK_NGX_FAILED(cleanup)) {
                mode.state = upscaling::SizingState::Error;
                mode.issue = upscaling::SizingIssue::CleanupFailed;
            }
            std::fprintf(stderr, "DLSS sizing: mode=%u output=%ux%u optimal=%ux%u minimum=%ux%u maximum=%ux%u raw_ngx=0x%08x state=%u issue=%s get_width=0x%08x get_height=0x%08x cleanup=0x%08x\n",
                unsigned(quality), key.outputWidth, key.outputHeight, optimalWidth, optimalHeight,
                minWidth, minHeight, maxWidth, maxHeight, unsigned(optimalResult), unsigned(mode.state),
                upscaling::SizingIssueName(mode.issue), unsigned(mode.optimalWidthResult.value_or(0)),
                unsigned(mode.optimalHeightResult.value_or(0)), unsigned(cleanup));
        }
    }
    if (temporarySession) {
        const auto destroyResult = NVSDK_NGX_VULKAN_DestroyParameters(parameters);
        RecordCall("Sizing_DestroyParameters", int32_t(destroyResult), NVSDK_NGX_FAILED(destroyResult));
        const auto shutdownResult = NVSDK_NGX_VULKAN_Shutdown1(device.vk);
        RecordCall("Sizing_Shutdown1", int32_t(shutdownResult), NVSDK_NGX_FAILED(shutdownResult));
        if (NVSDK_NGX_FAILED(destroyResult) || NVSDK_NGX_FAILED(shutdownResult)) {
            for (auto& mode : sizing.modes) {
                mode.state = upscaling::SizingState::Error;
                mode.issue = upscaling::SizingIssue::CleanupFailed;
                mode.cleanupResult = int32_t(NVSDK_NGX_FAILED(destroyResult) ? destroyResult : shutdownResult);
            }
        }
    }
    return sizing;
#endif
}

SrStatus Controller::EnsureSession(const plume::VulkanDevice& device) {
    if (backend_ == Backend::D3D12) return SrStatus::NeedsReconfigure;
#if !defined(LO_DLSS_SDK)
    (void)device;
    return SrStatus::Bypass;
#else
    if (sessionDevice_ && sessionDevice_ != &device) return SrStatus::NeedsReconfigure;
    if (sessionInstance_ == VK_NULL_HANDLE) return SrStatus::Bypass;
    if (sessionFailed_) return SrStatus::Failed;
    if (sharingRuntimeWithFg_) {
        // Do not Init/Shutdown again to answer an SR request on a shared device.
        // Unsupported SR does not imply that the FG runtime is unavailable.
        return sessionInitialized_ && capabilityParameters_ ? SrStatus::Executable :
            report_.state == ProbeState::Unavailable ? SrStatus::Bypass : SrStatus::Failed;
    }
    if (sessionInitialized_) return capabilityParameters_ ? SrStatus::Executable : SrStatus::Failed;
    const auto& deviceStatus = device.getExternalExtensionStatus();
    if ((sessionInterface_ && sessionInterface_->getExternalExtensionStatus().state != plume::VulkanExtensionState::Enabled) ||
        deviceStatus.state != plume::VulkanExtensionState::Enabled)
        return SrStatus::Bypass;

    std::string reason;
    if (!CreateApplicationDataPath(reason)) {
        sessionFailed_ = true;
        sessionRetryable_ = true; // No NGX call or resource exists yet.
        return SrStatus::Failed;
    }
    DiscoveryInfo info(applicationDataPath_, runtimePath_);
    auto result = NVSDK_NGX_VULKAN_Init_with_ProjectID(kProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "LostOdysseyRecomp",
        info.appDataPath.c_str(), sessionInstance_, device.physicalDevice, device.vk, vkGetInstanceProcAddr,
        vkGetDeviceProcAddr, &info.featureInfo);
    RecordCall("Session_Init_with_ProjectID", int32_t(result), NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result)) {
        sessionFailed_ = true;
        // Platform errors may conceal device loss and remain terminal.
        sessionRetryable_ = result == NVSDK_NGX_Result_Fail || result == NVSDK_NGX_Result_FAIL_NotInitialized;
        return SrStatus::Failed;
    }

    NVSDK_NGX_Parameter* capabilities = nullptr;
    result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&capabilities);
    RecordCall("Session_GetCapabilityParameters", int32_t(result), NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result) || !capabilities) {
        bool clean = true;
        if (capabilities) {
            const auto destroyResult = NVSDK_NGX_VULKAN_DestroyParameters(capabilities);
            RecordCall("Session_DestroyCapabilityParameters", int32_t(destroyResult), NVSDK_NGX_FAILED(destroyResult));
            clean = !NVSDK_NGX_FAILED(destroyResult);
        }
        const auto shutdownResult = NVSDK_NGX_VULKAN_Shutdown1(device.vk);
        RecordCall("Session_Shutdown1", int32_t(shutdownResult), NVSDK_NGX_FAILED(shutdownResult));
        sessionFailed_ = true;
        sessionRetryable_ = clean && !NVSDK_NGX_FAILED(shutdownResult) &&
            (result == NVSDK_NGX_Result_Success || result == NVSDK_NGX_Result_Fail ||
             result == NVSDK_NGX_Result_FAIL_NotInitialized);
        return SrStatus::Failed;
    }
    const auto read = [&](const char* name, const char* key, CapabilityValue& value) {
        int raw = 0;
        const auto getResult = NVSDK_NGX_Parameter_GetI(capabilities, key, &raw);
        value.raw = int32_t(getResult);
        value.value.reset();
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
        const bool clean = !NVSDK_NGX_FAILED(destroyResult) && !NVSDK_NGX_FAILED(shutdownResult);
        const auto retryableResult = [](const std::optional<int32_t>& raw) {
            return raw && (*raw == int32_t(NVSDK_NGX_Result_Success) ||
                *raw == int32_t(NVSDK_NGX_Result_Fail) || *raw == int32_t(NVSDK_NGX_Result_FAIL_NotInitialized));
        };
        sessionFailed_ = !clean || capability.decision == CapabilityDecision::ApiError;
        sessionRetryable_ = clean && capability.decision == CapabilityDecision::ApiError &&
            retryableResult(report_.srAvailable.raw) && retryableResult(report_.needsUpdatedDriver.raw) &&
            retryableResult(report_.minDriverVersionMajor.raw) && retryableResult(report_.minDriverVersionMinor.raw) &&
            retryableResult(report_.featureInitResult.raw) &&
            (!report_.featureInitResult.value || retryableResult(report_.featureInitResult.value));
        return clean && capability.decision == CapabilityDecision::Unavailable ? SrStatus::Bypass : SrStatus::Failed;
    }
    capabilityParameters_ = capabilities;
    sessionDevice_ = &device;
    sessionInitialized_ = true;
    sessionRetryable_ = false;
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
    const temporal::TemporalFrameInputs& inputs, plume::VulkanTexture& output, EvaluateCapture* capture) {
    SrAttempt attempt;
    if (backend_ == Backend::D3D12) return attempt;
#if !defined(LO_DLSS_SDK)
    (void)isolatedCommandList; (void)config; (void)inputs; (void)output;
    if (capture) { capture->stage = "sdk_disabled"; capture->reason = "sdk_disabled"; }
    return attempt;
#else
    const auto early = [&](const char* reason) {
        if (capture) { capture->stage = "validation"; capture->reason = reason; }
        return attempt;
    };
    if (config.colorSpace == SrColorSpace::Unknown) return early("unknown_color_encoding");
    if (!upscaling::ValidDlssRenderExtent(config.quality, config.renderExtent, config.outputExtent)) return early("invalid_sizing");
    if (!sessionDevice_) return early("no_session_device");
    const auto session = EnsureSession(*sessionDevice_);
    if (session != SrStatus::Executable) { attempt.status = session; return early("session_unavailable"); }
    if (NeedsFeatureRecreate(config) && featureConfigValid_) { attempt.status = SrStatus::NeedsReconfigure; return early("needs_reconfigure"); }
    if (featureFailed_) { attempt.status = SrStatus::NeedsReconfigure; return early("feature_failed_needs_reconfigure"); }
    if (inputs.renderFrameId && inputs.renderFrameId == lastSrAttemptFrameId_) return early("duplicate_frame");

    const auto validRegion = [&](const temporal::TextureRegion& region) {
        if (!region.Complete() || region.width != config.renderExtent.width || region.height != config.renderExtent.height)
            return false;
        const auto* texture = static_cast<const plume::VulkanTexture*>(region.texture);
        return ValidImage(*texture, sessionDevice_) && region.allocation.width == texture->desc.width &&
            region.allocation.height == texture->desc.height;
    };
    if (!config.renderExtent.width || !config.renderExtent.height || !config.outputExtent.width || !config.outputExtent.height ||
        config.deviceEpoch != inputs.plan.deviceEpoch || inputs.plan.consumer != upscaling::TemporalConsumer::DlssSr ||
        !inputs.CompleteForConsumer() || !temporal::ValidSrHybridMask(inputs,sessionDevice_,plume::RenderTextureLayout::GENERAL) ||
        !temporal::MatchesDepthConvention(inputs.depthConvention, config.depthInverted) ||
        !validRegion(inputs.color) || !validRegion(inputs.depth) || !validRegion(inputs.motion) ||
        !ValidImage(output, sessionDevice_) || output.desc.width != config.outputExtent.width || output.desc.height != config.outputExtent.height ||
        static_cast<const plume::VulkanTexture*>(inputs.depth.texture)->imageFormat != VK_FORMAT_R32_SFLOAT ||
        !std::isfinite(inputs.jitter.pixelX) || !std::isfinite(inputs.jitter.pixelY) ||
        !IsFinitePositive(inputs.preExposure) || !IsFinitePositive(inputs.exposureScale) ||
        (config.colorSpace == SrColorSpace::DisplayEncoded && inputs.colorEncoding != temporal::ColorEncoding::Sdr) ||
        (config.colorSpace == SrColorSpace::Linear && inputs.colorEncoding != temporal::ColorEncoding::HdrLinear))
        return early("invalid_inputs");

    // Retain parameters and any feature state through the fallback prefix batch,
    // including a failed Create/Evaluate recording whose primary is excluded.
    attempt.useId = srUses_.Record();
    if (!attempt.useId) { attempt.status = SrStatus::Failed; return early("use_id_unavailable"); }
    lastSrAttemptFrameId_ = inputs.renderFrameId;
    const auto failed = [&](std::optional<int32_t> rawNgx = std::nullopt,
                            std::optional<VkResult> rawVk = std::nullopt) {
        attempt.status = rawVk && *rawVk == VK_ERROR_DEVICE_LOST ? SrStatus::DeviceLost : SrStatus::Failed;
        attempt.rawNgxResult = rawNgx;
        if (rawVk) attempt.rawVkResult = int32_t(*rawVk);
        featureFailed_ = true;
        if (capture && capture->reason.empty()) capture->reason = "isolated_record_failed";
        return attempt;
    };

    const auto begin = BeginIsolatedCommandList(isolatedCommandList);
    if (begin.reset) RecordCall("vkResetCommandBuffer", int32_t(*begin.reset), *begin.reset != VK_SUCCESS);
    if (begin.begin) RecordCall("vkBeginCommandBuffer", int32_t(*begin.begin), *begin.begin != VK_SUCCESS);
    if (!begin.reset || *begin.reset != VK_SUCCESS) return failed(std::nullopt, begin.reset);
    if (!begin.begin || *begin.begin != VK_SUCCESS) return failed(std::nullopt, begin.begin);
    if (capture) capture->stage = "create";
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
        SetRenderPreset(parameters, config.renderPreset);
        NVSDK_NGX_Handle* handle = nullptr;
        const auto createResult = NGX_VULKAN_CREATE_DLSS_EXT1(sessionDevice_->vk, commandBuffer, 1, 1,
            &handle, parameters, &create);
        RecordCall("CREATE_DLSS_EXT1", int32_t(createResult), NVSDK_NGX_FAILED(createResult));
        if (capture) capture->createResult = int32_t(createResult);
        if (NVSDK_NGX_FAILED(createResult) || !handle) {
            if (capture) capture->reason = "feature_create_failed";
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
        char line[160];
        std::snprintf(line, sizeof(line), "DLSS SR feature created: backend=vulkan %ux%u->%ux%u quality=%u preset=%c flags=0x%x",
            config.renderExtent.width, config.renderExtent.height, config.outputExtent.width, config.outputExtent.height,
            unsigned(config.quality), RenderPresetLetter(config.renderPreset), unsigned(create.InFeatureCreateFlags));
        if (g_logSink) g_logSink(line);
        else std::fprintf(stderr, "%s\n", line);
    }

    const auto& color = *static_cast<const plume::VulkanTexture*>(inputs.color.texture);
    const auto& depth = *static_cast<const plume::VulkanTexture*>(inputs.depth.texture);
    const auto& motion = *static_cast<const plume::VulkanTexture*>(inputs.motion.texture);
    auto colorResource = ImageResource(color, false);
    auto depthResource = ImageResource(depth, false);
    auto motionResource = ImageResource(motion, false);
    auto outputResource = ImageResource(output, true);
    const bool hybridMotion = inputs.motionState == temporal::MotionState::Hybrid;
    NVSDK_NGX_Resource_VK hybridBiasResource{};
    if (hybridMotion) hybridBiasResource = ImageResource(
        *static_cast<const plume::VulkanTexture*>(inputs.motionInvalidity.texture),false);
    NVSDK_NGX_VK_DLSS_Eval_Params evaluate = {};
    evaluate.Feature.pInColor = &colorResource;
    evaluate.Feature.pInOutput = &outputResource;
    evaluate.pInDepth = &depthResource;
    evaluate.pInMotionVectors = &motionResource;
    // Confidence biases toward current color. It is neither material alpha nor
    // an SDK guarantee that temporal history will be completely rejected.
    evaluate.pInBiasCurrentColorMask = hybridMotion ? &hybridBiasResource : nullptr;
    evaluate.InBiasCurrentColorSubrectBase = {0,0};
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
    capture::Parameters frozen{};
    if (capture) {
        frozen.jitterX = evaluate.InJitterOffsetX; frozen.jitterY = evaluate.InJitterOffsetY;
        frozen.mvScaleX = evaluate.InMVScaleX; frozen.mvScaleY = evaluate.InMVScaleY;
        frozen.preExposure = evaluate.InPreExposure; frozen.exposureScale = evaluate.InExposureScale;
        frozen.colorX = evaluate.InColorSubrectBase.X; frozen.colorY = evaluate.InColorSubrectBase.Y;
        frozen.depthX = evaluate.InDepthSubrectBase.X; frozen.depthY = evaluate.InDepthSubrectBase.Y;
        frozen.mvX = evaluate.InMVSubrectBase.X; frozen.mvY = evaluate.InMVSubrectBase.Y;
        frozen.outputX = evaluate.InOutputSubrectBase.X; frozen.outputY = evaluate.InOutputSubrectBase.Y;
        frozen.renderWidth = evaluate.InRenderSubrectDimensions.Width;
        frozen.renderHeight = evaluate.InRenderSubrectDimensions.Height;
        frozen.reset = evaluate.InReset != 0; frozen.featureCreated = created;
        frozen.inputHistoryReset = inputs.resetHistory;
        frozen.biasCurrentColorBound = evaluate.pInBiasCurrentColorMask != nullptr;
        capture->sdk = frozen;
    }
    // Transparency and exposure resources remain null; hybrid confidence is
    // independently bound above. Auto exposure is only
    // selected by SrConfig and no guest alpha/mask is bound to NGX.
#if defined(LO_NATIVE_DLSS_TEST_INJECT_EVALUATE_FAILURE)
    if (const char* inject = std::getenv("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE"); inject && *inject == '1') {
        if (capture) { capture->stage = "before_evaluate"; capture->reason = "test_injection_before_vendor"; }
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
    const auto evaluateResult = capture::InvokeEvaluate(commandBuffer, capture, color, output, frozen,
        [&] { return int32_t(NGX_VULKAN_EVALUATE_DLSS_EXT(commandBuffer,
            static_cast<NVSDK_NGX_Handle*>(feature_), parameters, &evaluate)); },
        [](int32_t result) { return !NVSDK_NGX_FAILED(NVSDK_NGX_Result(result)); });
    RecordCall("EVALUATE_DLSS_EXT", int32_t(evaluateResult), NVSDK_NGX_FAILED(evaluateResult));
    if (!NVSDK_NGX_FAILED(evaluateResult)) {
        CaptureNeuralRenderingInput(commandBuffer, config, inputs, output);
        RecordNeuralRendering(commandBuffer, config, inputs, output, evaluate.InReset != 0);
    }
    isolatedCommandList.endExternalCommands();
    const auto end = EndIsolatedCommandList(isolatedCommandList);
    if (end) RecordCall("vkEndCommandBuffer", int32_t(*end), *end != VK_SUCCESS);
    if (!end || *end != VK_SUCCESS) {
        if (capture) capture->reason = "isolated_end_failed";
        return failed(int32_t(evaluateResult), end);
    }
    if (NVSDK_NGX_FAILED(evaluateResult)) return failed(int32_t(evaluateResult), end);
    report_.srEvaluated = true;
    attempt.status = SrStatus::Executable;
    return attempt;
#endif
}

void Controller::QueryNeuralRenderingExtensions(VkPhysicalDevice device, std::vector<VkExtensionProperties>& required) {
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    // Only a player who supplied the DLL gets the extra device extensions.
    std::error_code error;
    if (!std::filesystem::is_regular_file(nr::SnippetPath(runtimePath_), error)) {
        nrMissingAtDeviceCreation_ = true;
        nrUnsupportedReason_ = "nvngx_dlssnr.dll was not there when the Vulkan device was created; restart the game";
        return;
    }
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());
    available.resize(count);
    std::string missing;
    for (const char* name : kNeuralRenderingDeviceExtensions) {
        const auto has = [&](const VkExtensionProperties& extension) { return std::strcmp(extension.extensionName, name) == 0; };
        if (std::none_of(available.begin(), available.end(), has))
            missing += missing.empty() ? name : std::string(", ") + name;
    }
    nrSupported_ = missing.empty();
    if (!nrSupported_) {
        nrUnsupportedReason_ = "the device lacks " + missing;
        return;
    }
    for (const char* name : kNeuralRenderingDeviceExtensions) {
        const auto has = [&](const VkExtensionProperties& extension) { return std::strcmp(extension.extensionName, name) == 0; };
        if (std::none_of(required.begin(), required.end(), has))
            required.push_back(*std::find_if(available.begin(), available.end(), has));
    }
#else
    (void)device; (void)required;
#endif
}

const nr::Snippet* Controller::NeuralRenderingSnippet(const SrConfig& config) {
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    if (!config.neuralRenderingPasses) {
        g_neuralRenderingState.store(NeuralRenderingState::Off, std::memory_order_relaxed);
        return nullptr;
    }
    if (nrFailed_) return nullptr;
    // The model is trained on display-encoded 0..1 frames. The game's DLSS input
    // is its 8-bit scene even with HDR output; presentation adds the highlights back.
    if (config.colorSpace != SrColorSpace::DisplayEncoded) {
        if (!nrSkipLogged_) nr::Log("DLSS NR: skipped while the scene color is HDR linear");
        nrSkipLogged_ = true;
        return nullptr;
    }
    std::string reason;
    const auto* snippet = nr::LoadSnippet(runtimePath_, reason);
    if (snippet && !(backend_ == Backend::D3D12 ? snippet->d3d12.Complete() : snippet->vk.Complete()))
        reason = "nvngx_dlssnr.dll lacks this backend's NGX exports";
    if (!reason.empty()) {
        nrFailed_ = true;
        g_neuralRenderingState.store(NeuralRenderingState::MissingRuntime, std::memory_order_relaxed);
        nr::Log("DLSS NR: off, %s", reason.c_str());
        return nullptr;
    }
    if (backend_ != Backend::D3D12 && !nrSupported_) {
        nrFailed_ = true;
        g_neuralRenderingState.store(nrMissingAtDeviceCreation_ ? NeuralRenderingState::MissingRuntime :
            NeuralRenderingState::Unsupported, std::memory_order_relaxed);
        nr::Log("DLSS NR: off, %s", nrUnsupportedReason_.c_str());
        return nullptr;
    }
    return InitializedNeuralRenderingSnippet() && AllocateNeuralRenderingParameters() ? snippet : nullptr;
#else
    (void)config;
    return nullptr;
#endif
}

const nr::Snippet* Controller::InitializedNeuralRenderingSnippet() {
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    std::string reason;
    const auto* snippet = nr::LoadSnippet(runtimePath_, reason);
    const bool d3d12 = backend_ == Backend::D3D12;
    if (!snippet || !(d3d12 ? snippet->d3d12.Complete() : snippet->vk.Complete()) || (!d3d12 && !nrSupported_))
        return nullptr;
    if (nrSnippetInitialized_) return snippet;
    // The core session is already up; the snippet keeps its own state.
    const auto dataPath = ToWide(applicationDataPath_);
    const auto result = d3d12 ?
        snippet->d3d12.init(0, dataPath.c_str(), sessionDeviceD3D12_->d3d, NVSDK_NGX_Version_API, nullptr) :
        snippet->vk.init(0, dataPath.c_str(), sessionInstance_, sessionDevice_->physicalDevice, sessionDevice_->vk,
            vkGetInstanceProcAddr, vkGetDeviceProcAddr, NVSDK_NGX_Version_API, nullptr);
    RecordCall(d3d12 ? "NR_D3D12_Init_Ext" : "NR_Init_Ext2", int32_t(result));
    if (NVSDK_NGX_FAILED(result)) {
        FailNeuralRendering("snippet initialization", int32_t(result));
        return nullptr;
    }
    nrSnippetInitialized_ = true;
    nr::Log("DLSS NR: snippet initialized");
    return snippet;
#else
    return nullptr;
#endif
}

bool Controller::AllocateNeuralRenderingParameters() {
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    if (nrParameters_) return true;
    NVSDK_NGX_Parameter* allocated = nullptr;
    const auto result = backend_ == Backend::D3D12 ? NVSDK_NGX_D3D12_GetCapabilityParameters(&allocated) :
        NVSDK_NGX_VULKAN_GetCapabilityParameters(&allocated);
    RecordCall("NR_GetCapabilityParameters", int32_t(result));
    if (NVSDK_NGX_FAILED(result) || !allocated) {
        FailNeuralRendering("GetCapabilityParameters", int32_t(result));
        return false;
    }
    nrParameters_ = allocated;
    return true;
#else
    return false;
#endif
}

void Controller::FailNeuralRendering(const char* operation, int32_t result) {
    nrFailed_ = true;
    g_neuralRenderingState.store(NeuralRenderingState::Failed, std::memory_order_relaxed);
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    nr::Log("DLSS NR: %s failed (0x%08x); off until the next reconfigure, DLSS SR continues", operation, unsigned(result));
#else
    (void)operation; (void)result;
#endif
}

void Controller::FailNeuralRenderingPreview(const char* operation, int32_t result) {
    nrPreviewFailed_ = true;
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    nr::Log("DLSS NR: preview %s failed (0x%08x); the preview stays off until the page closes", operation, unsigned(result));
#else
    (void)operation; (void)result;
#endif
}

void Controller::SetNeuralRenderingTuning(const NeuralRenderingTuning& tuning) {
    if (tuning == nrTuning_) return;
    nrTuning_ = tuning;
    nrTuningChanged_ = true;
}

bool Controller::CreateNeuralRenderingCapture(plume::RenderDevice& device, const SrConfig& config,
    plume::RenderFormat colorFormat, plume::RenderFormat depthFormat, plume::RenderFormat motionFormat) {
    const auto texture = [&](uint32_t width, uint32_t height, plume::RenderFormat format) {
        return device.createTexture(plume::RenderTextureDesc::Texture2D(width, height, 1, format,
            plume::RenderTextureFlag::STORAGE | plume::RenderTextureFlag::UNORDERED_ACCESS));
    };
    nrCaptureColor_ = texture(config.outputExtent.width, config.outputExtent.height, colorFormat);
    nrCaptureDepth_ = texture(config.renderExtent.width, config.renderExtent.height, depthFormat);
    nrCaptureMotion_ = texture(config.renderExtent.width, config.renderExtent.height, motionFormat);
    nrCaptureConfig_ = config;
    if (nrCaptureColor_ && nrCaptureDepth_ && nrCaptureMotion_) return true;
    nrCaptureColor_.reset();
    nrCaptureDepth_.reset();
    nrCaptureMotion_.reset();
    return false;
}

void Controller::CaptureNeuralRenderingInput(VkCommandBuffer commandBuffer, const SrConfig& config,
    const temporal::TemporalFrameInputs& inputs, plume::VulkanTexture& output) {
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    // Only a player who can run NR pays for the copies, every fourth frame.
    if (!nrSupported_ || config.colorSpace != SrColorSpace::DisplayEncoded) return;
    if (nrCaptured_ && ++nrCaptureCount_ % 4) return;
    auto& depth = *static_cast<plume::VulkanTexture*>(inputs.depth.texture);
    auto& motion = *static_cast<plume::VulkanTexture*>(inputs.motion.texture);
    if (!nrCaptureColor_) {
        if (!CreateNeuralRenderingCapture(*output.device, config, output.desc.format, depth.desc.format, motion.desc.format))
            return;
        // Kept in GENERAL for good; plume's tracking says so too.
        VkImageMemoryBarrier barriers[3] = {};
        plume::RenderTexture* images[3] = {nrCaptureColor_.get(), nrCaptureDepth_.get(), nrCaptureMotion_.get()};
        for (int i = 0; i < 3; ++i) {
            auto& image = *static_cast<plume::VulkanTexture*>(images[i]);
            barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[i].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            barriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barriers[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barriers[i].srcQueueFamilyIndex = barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[i].image = image.vk;
            barriers[i].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            image.textureLayout = plume::RenderTextureLayout::GENERAL;
        }
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0, 0, nullptr, 0, nullptr, 3, barriers);
    }
    const auto copy = [&](VkImage source, uint32_t x, uint32_t y, plume::RenderTexture* destination, uint32_t width,
                          uint32_t height) {
        VkImageCopy region = {};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.srcOffset = {int32_t(x), int32_t(y), 0};
        region.extent = {width, height, 1};
        vkCmdCopyImage(commandBuffer, source, VK_IMAGE_LAYOUT_GENERAL, static_cast<plume::VulkanTexture*>(destination)->vk,
            VK_IMAGE_LAYOUT_GENERAL, 1, &region);
    };
    FullBarrier(commandBuffer);
    copy(output.vk, 0, 0, nrCaptureColor_.get(), config.outputExtent.width, config.outputExtent.height);
    copy(depth.vk, inputs.depth.x, inputs.depth.y, nrCaptureDepth_.get(), config.renderExtent.width, config.renderExtent.height);
    copy(motion.vk, inputs.motion.x, inputs.motion.y, nrCaptureMotion_.get(), config.renderExtent.width,
        config.renderExtent.height);
    FullBarrier(commandBuffer);
    nrCaptured_ = true;
#else
    (void)commandBuffer; (void)config; (void)inputs; (void)output;
#endif
}

void Controller::RecordNeuralRendering(VkCommandBuffer commandBuffer, const SrConfig& config,
    const temporal::TemporalFrameInputs& inputs, plume::VulkanTexture& output, bool reset) {
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    const auto* snippet = NeuralRenderingSnippet(config);
    if (!snippet) return;
    auto* parameters = static_cast<NVSDK_NGX_Parameter*>(nrParameters_);
    const uint32_t width = config.outputExtent.width, height = config.outputExtent.height;
    const uint32_t passes = config.neuralRenderingPasses;

    if (!HasNeuralRenderingFeature()) {
        // Creation records initialization work; the first evaluate waits for
        // the next recording. The model's images are FP16, the format the
        // known integrations feed it.
        for (auto& image : nrImages_) {
            image = output.device->createTexture(plume::RenderTextureDesc::Texture2D(width, height, 1,
                plume::RenderFormat::R16G16B16A16_FLOAT,
                plume::RenderTextureFlag::STORAGE | plume::RenderTextureFlag::UNORDERED_ACCESS));
            if (!image) return FailNeuralRendering("image allocation", 0);
        }
        for (uint32_t pass = 0; pass < passes; ++pass) {
            nr::SetControls(parameters, width, height, config.depthInverted, config.neuralRenderingPreset, nrTuning_, pass);
            NVSDK_NGX_Handle* handle = nullptr;
            const auto result = snippet->vk.create(sessionDevice_->vk, commandBuffer, nr::kFeature, parameters, &handle);
            RecordCall("NR_CreateFeature1", int32_t(result));
            nrFeatures_[pass] = handle; // A partial create is released at the drained boundary.
            if (NVSDK_NGX_FAILED(result) || !handle) return FailNeuralRendering("CreateFeature1", int32_t(result));
        }
        VkImageMemoryBarrier barriers[2] = {};
        for (int i = 0; i < 2; ++i) {
            auto& barrier = barriers[i];
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = static_cast<plume::VulkanTexture*>(nrImages_[i].get())->vk;
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        }
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0, 0, nullptr, 0, nullptr, 2, barriers);
        nrPasses_ = passes;
        nrWidth_ = width;
        nrHeight_ = height;
        nrReset_ = true;
        nr::Log("DLSS NR: %u feature(s) created for %ux%u", passes, width, height);
        return;
    }
    // An output or pass-count change is a drained SR reconfigure, which recreates the features.
    if (width != nrWidth_ || height != nrHeight_ || passes != nrPasses_) return;

    // Pass n reads image n % 2 and answers into the other one.
    plume::VulkanTexture* images[2] = {static_cast<plume::VulkanTexture*>(nrImages_[0].get()),
                                       static_cast<plume::VulkanTexture*>(nrImages_[1].get())};
    FullBarrier(commandBuffer);
    BlitGeneral(commandBuffer, output.vk, images[0]->vk, width, height);
    FullBarrier(commandBuffer);
    auto depthResource = ImageResource(*static_cast<const plume::VulkanTexture*>(inputs.depth.texture), false);
    auto motionResource = ImageResource(*static_cast<const plume::VulkanTexture*>(inputs.motion.texture), false);
    NVSDK_NGX_Parameter_SetVoidPointer(parameters, "DLSSNR.Depth", &depthResource);
    NVSDK_NGX_Parameter_SetVoidPointer(parameters, "DLSSNR.MVec", &motionResource);
    nr::SetFrame(parameters, nr::GameFrame(config, inputs), reset || nrReset_ || nrTuningChanged_);
    nrTuningChanged_ = false;
    for (uint32_t pass = 0; pass < passes; ++pass) {
        auto colorResource = ImageResource(*images[pass % 2], false);
        auto outputResource = ImageResource(*images[(pass + 1) % 2], true);
        nr::SetControls(parameters, width, height, config.depthInverted, config.neuralRenderingPreset, nrTuning_, pass);
        NVSDK_NGX_Parameter_SetVoidPointer(parameters, "DLSSNR.Color", &colorResource);
        NVSDK_NGX_Parameter_SetVoidPointer(parameters, "DLSSNR.Output", &outputResource);
        const auto result = snippet->vk.evaluate(commandBuffer, static_cast<NVSDK_NGX_Handle*>(nrFeatures_[pass]),
            parameters, nullptr);
        RecordCall("NR_EvaluateFeature", int32_t(result));
        if (NVSDK_NGX_FAILED(result)) return FailNeuralRendering("EvaluateFeature", int32_t(result));
        FullBarrier(commandBuffer);
    }
    BlitGeneral(commandBuffer, images[passes % 2]->vk, output.vk, width, height);
    FullBarrier(commandBuffer);
    if (nrReset_) nr::Log("DLSS NR: first evaluate recorded for %ux%u, %u pass(es)", width, height, passes);
    nrReset_ = false;
    g_neuralRenderingState.store(NeuralRenderingState::Active, std::memory_order_relaxed);
#else
    (void)commandBuffer; (void)config; (void)inputs; (void)output; (void)reset;
#endif
}

plume::RenderTexture* Controller::RecordNeuralRenderingPreview(plume::RenderCommandList& list, uint32_t passes,
    uint32_t preset, const NeuralRenderingTuning& tuning) {
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    if (!nrCaptured_) return nullptr;
    passes = nrPreviewFailed_ ? 0 : std::min(passes, kMaxNeuralRenderingPasses);
    preset = std::min(preset, 3u);
    if (tuning != nrPreviewTuning_) {
        nrPreviewTuning_ = tuning;
        nrPreviewSettle_ = nr::kPreviewSettleEvaluates;
        nrPreviewReset_ = true;
    }
    // The present paths waited for the present GPU, so nothing still uses the old features.
    if (HasNeuralRenderingPreviewFeature() && (passes != nrPreviewPasses_ || preset != nrPreviewPreset_))
        ReleaseNeuralRenderingPreviewFeatures();
    if (passes && !InitializedNeuralRenderingSnippet()) passes = 0;
    if (passes && !AllocateNeuralRenderingParameters()) passes = 0;
    if (backend_ == Backend::D3D12) return RecordNeuralRenderingPreview(static_cast<plume::D3D12CommandList&>(list),
        passes, preset, tuning);
    return RecordNeuralRenderingPreview(static_cast<plume::VulkanCommandList&>(list), passes, preset, tuning);
#else
    (void)list; (void)passes; (void)preset; (void)tuning;
    return nullptr;
#endif
}

plume::RenderTexture* Controller::RecordNeuralRenderingPreview(plume::VulkanCommandList& list, uint32_t passes,
    uint32_t preset, const NeuralRenderingTuning& tuning) {
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    const auto& config = nrCaptureConfig_;
    const uint32_t width = config.outputExtent.width, height = config.outputExtent.height;
    auto& color = *static_cast<plume::VulkanTexture*>(nrCaptureColor_.get());
    auto& device = *color.device;
    const auto transitionToGeneral = [](VkCommandBuffer commandBuffer, plume::RenderTexture* texture) {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = static_cast<plume::VulkanTexture*>(texture)->vk;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        static_cast<plume::VulkanTexture*>(texture)->textureLayout = plume::RenderTextureLayout::GENERAL;
    };
    if (!nrPreviewComposite_) {
        nrPreviewComposite_ = device.createTexture(plume::RenderTextureDesc::Texture2D(width * 2, height, 1,
            color.desc.format, plume::RenderTextureFlag::STORAGE | plume::RenderTextureFlag::UNORDERED_ACCESS));
        if (!nrPreviewComposite_) return nullptr;
    }
    auto& composite = *static_cast<plume::VulkanTexture*>(nrPreviewComposite_.get());
    // Presentation leaves the composite in SHADER_READ; the copies work in GENERAL.
    plume::RenderCommandList& commands = list;
    commands.barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(&composite, plume::RenderTextureLayout::GENERAL));
    const VkCommandBuffer commandBuffer = list.beginExternalCommands();
    if (commandBuffer == VK_NULL_HANDLE) return nullptr;
    auto* parameters = static_cast<NVSDK_NGX_Parameter*>(nrParameters_);
    std::string reason;
    const auto* snippet = passes ? nr::LoadSnippet(runtimePath_, reason) : nullptr;
    VkImage right = color.vk;
    if (passes && !HasNeuralRenderingPreviewFeature()) {
        // As in gameplay, the features evaluate from the next recording on.
        for (auto& image : nrPreviewImages_) {
            if (image) continue;
            image = device.createTexture(plume::RenderTextureDesc::Texture2D(width, height, 1,
                plume::RenderFormat::R16G16B16A16_FLOAT,
                plume::RenderTextureFlag::STORAGE | plume::RenderTextureFlag::UNORDERED_ACCESS));
            if (image) transitionToGeneral(commandBuffer, image.get());
        }
        if (nrPreviewImages_[0] && nrPreviewImages_[1]) {
            for (uint32_t pass = 0; pass < passes; ++pass) {
                nr::SetControls(parameters, width, height, config.depthInverted, preset, tuning, pass);
                NVSDK_NGX_Handle* handle = nullptr;
                const auto result = snippet->vk.create(sessionDevice_->vk, commandBuffer, nr::kFeature, parameters, &handle);
                RecordCall("NR_Preview_CreateFeature1", int32_t(result));
                nrPreviewFeatures_[pass] = handle;
                if (NVSDK_NGX_FAILED(result) || !handle) {
                    FailNeuralRenderingPreview("CreateFeature1", int32_t(result));
                    break;
                }
            }
            nrPreviewPasses_ = passes;
            nrPreviewPreset_ = preset;
            nrPreviewSettle_ = nr::kPreviewSettleEvaluates;
            nrPreviewReset_ = true;
        }
    } else if (passes && HasNeuralRenderingPreviewFeature()) {
        plume::VulkanTexture* images[2] = {static_cast<plume::VulkanTexture*>(nrPreviewImages_[0].get()),
                                           static_cast<plume::VulkanTexture*>(nrPreviewImages_[1].get())};
        right = VK_NULL_HANDLE; // The last answer stays once the frame has settled.
        if (nrPreviewSettle_) {
            FullBarrier(commandBuffer);
            BlitGeneral(commandBuffer, color.vk, images[0]->vk, width, height);
            FullBarrier(commandBuffer);
            auto depthResource = ImageResource(*static_cast<const plume::VulkanTexture*>(nrCaptureDepth_.get()), false);
            auto motionResource = ImageResource(*static_cast<const plume::VulkanTexture*>(nrCaptureMotion_.get()), false);
            NVSDK_NGX_Parameter_SetVoidPointer(parameters, "DLSSNR.Depth", &depthResource);
            NVSDK_NGX_Parameter_SetVoidPointer(parameters, "DLSSNR.MVec", &motionResource);
            // The same frame again: no motion.
            nr::SetFrame(parameters, {width, height, config.renderExtent.width, config.renderExtent.height,
                0, 0, 0, 0, 0.0f}, nrPreviewReset_);
            bool ran = true;
            for (uint32_t pass = 0; pass < passes && ran; ++pass) {
                auto colorResource = ImageResource(*images[pass % 2], false);
                auto outputResource = ImageResource(*images[(pass + 1) % 2], true);
                nr::SetControls(parameters, width, height, config.depthInverted, preset, tuning, pass);
                NVSDK_NGX_Parameter_SetVoidPointer(parameters, "DLSSNR.Color", &colorResource);
                NVSDK_NGX_Parameter_SetVoidPointer(parameters, "DLSSNR.Output", &outputResource);
                const auto result = snippet->vk.evaluate(commandBuffer,
                    static_cast<NVSDK_NGX_Handle*>(nrPreviewFeatures_[pass]), parameters, nullptr);
                RecordCall("NR_Preview_EvaluateFeature", int32_t(result));
                if (NVSDK_NGX_FAILED(result)) {
                    FailNeuralRenderingPreview("EvaluateFeature", int32_t(result));
                    ran = false;
                }
                FullBarrier(commandBuffer);
            }
            right = ran ? images[passes % 2]->vk : color.vk;
            --nrPreviewSettle_;
            nrPreviewReset_ = false;
        }
    }
    FullBarrier(commandBuffer);
    BlitGeneral(commandBuffer, color.vk, composite.vk, width, height);
    if (right != VK_NULL_HANDLE) BlitGeneral(commandBuffer, right, composite.vk, width, height, width);
    FullBarrier(commandBuffer);
    list.endExternalCommands();
    return &composite;
#else
    (void)list; (void)passes; (void)preset; (void)tuning;
    return nullptr;
#endif
}

void Controller::ReleaseNeuralRenderingPreviewFeatures() {
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    if (HasNeuralRenderingPreviewFeature()) {
        std::string reason;
        const auto* snippet = nr::LoadSnippet(runtimePath_, reason);
        const auto release = !snippet ? nullptr : backend_ == Backend::D3D12 ? snippet->d3d12.release : snippet->vk.release;
        for (auto& feature : nrPreviewFeatures_) {
            if (feature && release) {
                const auto result = release(static_cast<NVSDK_NGX_Handle*>(feature));
                RecordCall("NR_Preview_ReleaseFeature", int32_t(result));
            }
            feature = nullptr;
        }
    }
#endif
    nrPreviewPasses_ = nrPreviewPreset_ = 0;
#if defined(_WIN32)
    // The decode set names the last pass's image.
    nrPreviewEncodeSet_.reset();
    nrPreviewDecodeSet_.reset();
#endif
}

void Controller::ReleaseNeuralRenderingPreview() {
    ReleaseNeuralRenderingPreviewFeatures();
    for (auto& image : nrPreviewImages_) image.reset();
    nrPreviewComposite_.reset();
    nrPreviewTuning_ = {};
    nrPreviewSettle_ = 0;
    nrPreviewReset_ = true;
    nrPreviewFailed_ = false;
}

void Controller::ReleaseNeuralRendering() {
#if defined(_WIN32) && defined(LO_DLSS_SDK)
    if (HasNeuralRenderingFeature()) {
        std::string reason;
        const auto* snippet = nr::LoadSnippet(runtimePath_, reason);
        const auto release = !snippet ? nullptr : backend_ == Backend::D3D12 ? snippet->d3d12.release : snippet->vk.release;
        for (auto& feature : nrFeatures_) {
            if (feature && release) {
                const auto result = release(static_cast<NVSDK_NGX_Handle*>(feature));
                RecordCall("NR_ReleaseFeature", int32_t(result));
            }
            feature = nullptr;
        }
    }
#endif
    for (auto& image : nrImages_) image.reset();
    ReleaseNeuralRenderingPreview();
    // A size change makes the held frame stale.
    nrCaptureColor_.reset();
    nrCaptureDepth_.reset();
    nrCaptureMotion_.reset();
    nrCaptured_ = false;
    nrCaptureCount_ = 0;
#if defined(_WIN32)
    nrEncodeSet_.reset();
    nrDecodeSet_.reset();
    nrBridgePipeline_.reset();
    nrBridgeShader_.reset();
    nrBridgeLayout_.reset();
    nrStage_.reset();
#endif
    nrPasses_ = 0;
    nrWidth_ = nrHeight_ = 0;
    nrReset_ = true;
    // A drained reconfigure retries NR; the DLL load result stays cached.
    nrFailed_ = false;
    nrSkipLogged_ = false;
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
    sessionRetryable_ = false;
    sessionFailed_ = true;
}

void Controller::ReleaseFeatureAfterGpuDrain() {
    if (!srUses_.Empty()) return;
    ReleaseNeuralRendering();
#if defined(LO_DLSS_SDK)
    if (feature_) {
        const auto result =
#if defined(_WIN32)
            backend_ == Backend::D3D12 ? NVSDK_NGX_D3D12_ReleaseFeature(static_cast<NVSDK_NGX_Handle*>(feature_)) :
#endif
            NVSDK_NGX_VULKAN_ReleaseFeature(static_cast<NVSDK_NGX_Handle*>(feature_));
        RecordCall("ReleaseFeature", int32_t(result), NVSDK_NGX_FAILED(result));
        if (NVSDK_NGX_FAILED(result)) {
            sessionFailed_ = true;
            return; // Retain a failed-release handle for a later drained cleanup.
        }
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
    if (feature_) return;
#if defined(LO_DLSS_SDK)
#if defined(_WIN32)
    // The snippet shuts down before the core session it was initialized beside.
    if (nrSnippetInitialized_) {
        std::string reason;
        if (const auto* snippet = nr::LoadSnippet(runtimePath_, reason)) {
            std::optional<NVSDK_NGX_Result> result;
            if (backend_ == Backend::D3D12) {
                if (snippet->d3d12.shutdown && sessionDeviceD3D12_) result = snippet->d3d12.shutdown(sessionDeviceD3D12_->d3d);
            } else if (snippet->vk.shutdown && sessionDevice_) {
                result = snippet->vk.shutdown(sessionDevice_->vk);
            }
            if (result) RecordCall("NR_Shutdown1", int32_t(*result));
        }
        nrSnippetInitialized_ = false;
    }
#endif
    if (nrParameters_) {
        const auto result =
#if defined(_WIN32)
            backend_ == Backend::D3D12 ? NVSDK_NGX_D3D12_DestroyParameters(static_cast<NVSDK_NGX_Parameter*>(nrParameters_)) :
#endif
            NVSDK_NGX_VULKAN_DestroyParameters(static_cast<NVSDK_NGX_Parameter*>(nrParameters_));
        RecordCall("NR_DestroyParameters", int32_t(result));
        nrParameters_ = nullptr;
    }
    if (featureParameters_) {
        const auto result =
#if defined(_WIN32)
            backend_ == Backend::D3D12 ? NVSDK_NGX_D3D12_DestroyParameters(static_cast<NVSDK_NGX_Parameter*>(featureParameters_)) :
#endif
            NVSDK_NGX_VULKAN_DestroyParameters(static_cast<NVSDK_NGX_Parameter*>(featureParameters_));
        RecordCall("DestroyFeatureParameters", int32_t(result), NVSDK_NGX_FAILED(result));
        if (NVSDK_NGX_FAILED(result)) {
            sessionFailed_ = true;
            return;
        }
        featureParameters_ = nullptr;
    }
    if (capabilityParameters_) {
        const auto result =
#if defined(_WIN32)
            backend_ == Backend::D3D12 ? NVSDK_NGX_D3D12_DestroyParameters(static_cast<NVSDK_NGX_Parameter*>(capabilityParameters_)) :
#endif
            NVSDK_NGX_VULKAN_DestroyParameters(static_cast<NVSDK_NGX_Parameter*>(capabilityParameters_));
        RecordCall("DestroyCapabilityParameters", int32_t(result), NVSDK_NGX_FAILED(result));
        if (NVSDK_NGX_FAILED(result)) {
            sessionFailed_ = true;
            return;
        }
        capabilityParameters_ = nullptr;
    }
    if (sessionInitialized_ || runtimeRetainedForFg_) {
        const auto result =
#if defined(_WIN32)
            backend_ == Backend::D3D12 && sessionDeviceD3D12_
                ? NVSDK_NGX_D3D12_Shutdown1(sessionDeviceD3D12_->d3d) :
#endif
            sessionDevice_ ? NVSDK_NGX_VULKAN_Shutdown1(sessionDevice_->vk) : NVSDK_NGX_Result_Fail;
        RecordCall("Session_Shutdown1", int32_t(result), NVSDK_NGX_FAILED(result));
        if (NVSDK_NGX_FAILED(result)) {
            sessionFailed_ = true;
            return;
        }
    }
#endif
    sessionInitialized_ = false;
    runtimeRetainedForFg_ = false;
    sharingRuntimeWithFg_ = false;
    sessionFailed_ = false;
    sessionRetryable_ = false;
    sessionInterface_ = nullptr;
    sessionDevice_ = nullptr;
#if defined(_WIN32)
    sessionDeviceD3D12_ = nullptr;
#endif
    sessionInstance_ = VK_NULL_HANDLE;
    backend_ = Backend::None;
}
} // namespace gpu::dlss
#endif
