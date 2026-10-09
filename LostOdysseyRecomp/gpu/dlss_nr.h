#pragma once

// DLSS 5 Neural Rendering pieces shared by the Vulkan and D3D12 NGX paths.
#if defined(LO_GPU_PLUME) && defined(LO_DLSS_SDK) && defined(_WIN32)
#include "dlss_ngx.h"

#include <nvsdk_ngx.h>
#include <nvsdk_ngx_vk.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace gpu::temporal { struct TemporalFrameInputs; }

namespace gpu::dlss::nr {
// NVIDIA has published no header for it; the feature id is the SDK's reserved
// slot 18, and the parameter names are the ones the signed Streamline 2.13
// sl.dlss_nr plugin passes to the same NGX feature.
inline constexpr NVSDK_NGX_Feature kFeature = NVSDK_NGX_Feature_Reserved18;
// The settings page's preview runs a held frame again; it settles after a few
// evaluates, and each change runs them again.
inline constexpr uint32_t kPreviewSettleEvaluates = 8;

// The user's nvngx_dlssnr.dll. Community copies are modified, so the driver's
// NGX core refuses to load them; the snippet's own NGX exports are called here.
// The Init argument orders were read from the exports: version, then a
// parameter block, as in the SDK's snippet declarations.
struct Snippet {
    struct Vulkan {
        NVSDK_NGX_Result (NVSDK_CONV* init)(unsigned long long, const wchar_t*, VkInstance, VkPhysicalDevice,
            VkDevice, PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr, NVSDK_NGX_Version,
            const NVSDK_NGX_Parameter*) = nullptr;
        NVSDK_NGX_Result (NVSDK_CONV* create)(VkDevice, VkCommandBuffer, NVSDK_NGX_Feature,
            NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**) = nullptr;
        NVSDK_NGX_Result (NVSDK_CONV* evaluate)(VkCommandBuffer, const NVSDK_NGX_Handle*,
            const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback) = nullptr;
        NVSDK_NGX_Result (NVSDK_CONV* release)(NVSDK_NGX_Handle*) = nullptr;
        NVSDK_NGX_Result (NVSDK_CONV* shutdown)(VkDevice) = nullptr;
        bool Complete() const { return init && create && evaluate && release; }
    } vk;
    struct D3D12 {
        NVSDK_NGX_Result (NVSDK_CONV* init)(unsigned long long, const wchar_t*, ID3D12Device*, NVSDK_NGX_Version,
            const NVSDK_NGX_Parameter*) = nullptr;
        NVSDK_NGX_Result (NVSDK_CONV* create)(ID3D12GraphicsCommandList*, NVSDK_NGX_Feature,
            NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**) = nullptr;
        NVSDK_NGX_Result (NVSDK_CONV* evaluate)(ID3D12GraphicsCommandList*, const NVSDK_NGX_Handle*,
            const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback) = nullptr;
        NVSDK_NGX_Result (NVSDK_CONV* release)(NVSDK_NGX_Handle*) = nullptr;
        NVSDK_NGX_Result (NVSDK_CONV* shutdown)(ID3D12Device*) = nullptr;
        bool Complete() const { return init && create && evaluate && release; }
    } d3d12;
};

// Beside the SR runtime, or LO_DLSS_NR_PATH.
std::filesystem::path SnippetPath(const std::filesystem::path& runtimeDirectory);
// Loaded once per process and never unloaded. Null with a reason when the DLL
// is missing or is not a usable copy.
const Snippet* LoadSnippet(const std::filesystem::path& runtimeDirectory, std::string& reason);

// Every evaluate reads the model controls; a feature takes the preset when it
// is created.
void SetControls(NVSDK_NGX_Parameter* parameters, uint32_t width, uint32_t height, bool depthInverted,
                 uint32_t preset, const NeuralRenderingTuning& tuning, uint32_t pass);
// Color and output cover the output size; depth and motion the render size.
struct FrameRegions {
    uint32_t outputWidth = 0, outputHeight = 0, renderWidth = 0, renderHeight = 0;
    uint32_t depthX = 0, depthY = 0, motionX = 0, motionY = 0;
    float motionScale = 1.0f;
};
void SetFrame(NVSDK_NGX_Parameter* parameters, const FrameRegions& regions, bool reset);
FrameRegions GameFrame(const SrConfig& config, const temporal::TemporalFrameInputs& inputs);

void Log(const char* format, ...);
// The one SetComputeShaderCompiler installed, or null.
ComputeShaderCompiler ShaderCompiler();
} // namespace gpu::dlss::nr
#endif
