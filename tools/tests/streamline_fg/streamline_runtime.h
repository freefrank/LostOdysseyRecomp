#pragma once
#include <plume_vulkan.h>
#include <sl_dlss_g.h>
#include <sl_reflex.h>
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>

namespace probe {
class StreamlineRuntime {
public:
    ~StreamlineRuntime();
    bool Initialize(const std::filesystem::path& runtimeDirectory, std::string& reason);
    bool Shutdown();
    void AbandonAfterDrainFailure();
    bool FreeResources(sl::Feature feature, const sl::ViewportHandle& viewport);
    bool Requirements(sl::Feature feature, sl::FeatureRequirements& requirements, std::string& reason) const;
    bool ImportFunctions(std::string& reason);
    unsigned ErrorCount() const;
    PFN_vkGetInstanceProcAddr InstanceProc() const { return instanceProc_; }
    PFN_vkGetDeviceProcAddr DeviceProc() const { return deviceProc_; }
    PFun_slGetNewFrameToken* NewFrameToken{};
    PFun_slSetConstants* SetConstants{};
    PFun_slSetTagForFrame* SetTagForFrame{};
    PFun_slIsFeatureSupported* IsFeatureSupported{};
    PFun_slGetFeatureVersion* GetFeatureVersion{};
    PFun_slSetFeatureLoaded* SetFeatureLoaded{};
    PFun_slDLSSGSetOptions* DLSSGSetOptions{};
    PFun_slDLSSGGetState* DLSSGGetState{};
    PFun_slReflexSetOptions* ReflexSetOptions{};
    PFun_slReflexSleep* ReflexSleep{};
    PFun_slPCLSetMarker* PCLSetMarker{};
    static constexpr sl::Feature kFeatures[3] = {sl::kFeatureDLSS_G, sl::kFeatureReflex, sl::kFeaturePCL};
private:
    HMODULE module_{};
    bool initialized_{};
    bool shutdownError_{};
    PFN_vkGetInstanceProcAddr instanceProc_{};
    PFN_vkGetDeviceProcAddr deviceProc_{};
    PFun_slShutdown* shutdown_{};
    PFun_slGetFeatureFunction* getFunction_{};
    PFun_slGetFeatureRequirements* requirements_{};
    PFun_slFreeResources* freeResources_{};
};
}
