#if defined(_WIN32) && defined(LO_ENABLE_STREAMLINE_FG)

#include "streamline_runtime.h"
#include "dlss_fg_runtime_policy.h"
#if defined(FRAMEGEN_WITH_DLSS)
// The pinned SDK header defines externally linked functions and globals.
// The linked D3D12 adapter supplies that implementation when both paths exist.
namespace sl::security { bool verifyEmbeddedSignature(const wchar_t* path); }
#else
#include <sl_security.h>
#endif
#include <atomic>
#include <cstdio>
#include <type_traits>

namespace gpu::dlss_fg {
namespace {

std::atomic<unsigned> slErrorCount{};
std::atomic<unsigned> slFeatureCreationFailures{};

template<class T>
T Export(HMODULE module, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(module, name));
}

bool Ok(sl::Result result) { return result == sl::Result::eOk; }

void Log(sl::LogType type, const char* message) {
    if (type == sl::LogType::eError) {
        ++slErrorCount;
        // This runtime deliberately loads no sl.dlss/sl.dlss_d plugin. Its
        // only NGX feature creator is DLSS-G. Do not use all SDK errors as a
        // retry/health signal, and never call back into SL from this callback.
        if (IsNgxCreationFailure(message)) ++slFeatureCreationFailures;
    }
    if (type == sl::LogType::eWarn || type == sl::LogType::eError)
        std::fprintf(stderr, "Streamline %s: %s\n", type == sl::LogType::eError ? "error" : "warning",
            message ? message : "(null)");
}

} // namespace

Runtime::~Runtime() { Shutdown(); }
unsigned Runtime::ErrorCount() const { return slErrorCount.load(); }
unsigned Runtime::FeatureCreationFailureCount() const { return slFeatureCreationFailures.load(); }

bool Runtime::Initialize(const std::filesystem::path& directory, std::string& reason) {
    if (initialized_ || module_) { reason = "Streamline initialized twice"; return false; }
    slErrorCount.store(0);
    slFeatureCreationFailures.store(0);
    const auto root = std::filesystem::absolute(directory);
    for (const wchar_t* name : {L"sl.interposer.dll", L"sl.common.dll", L"sl.dlss_g.dll", L"sl.reflex.dll", L"sl.pcl.dll"}) {
        const auto path = root / name;
        if (!std::filesystem::is_regular_file(path) || !sl::security::verifyEmbeddedSignature(path.c_str())) {
            reason = "missing or invalid NVIDIA signature for " + path.string(); return false;
        }
    }
    if (!std::filesystem::is_regular_file(root / L"nvngx_dlssg.dll")) {
        reason = "nvngx_dlssg.dll is missing"; return false;
    }
    if (std::filesystem::exists(root / L"sl.dlss.dll")) {
        reason = "sl.dlss.dll would replace native NGX SR"; return false;
    }

    module_ = LoadLibraryExW((root / L"sl.interposer.dll").c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module_) { reason = "Streamline interposer load failed: " + std::to_string(GetLastError()); return false; }
    instanceProc_ = Export<PFN_vkGetInstanceProcAddr>(module_, "vkGetInstanceProcAddr");
    deviceProc_ = Export<PFN_vkGetDeviceProcAddr>(module_, "vkGetDeviceProcAddr");
    auto init = Export<PFun_slInit*>(module_, "slInit");
    shutdown_ = Export<PFun_slShutdown*>(module_, "slShutdown");
    freeResources_ = Export<PFun_slFreeResources*>(module_, "slFreeResources");
    requirements_ = Export<PFun_slGetFeatureRequirements*>(module_, "slGetFeatureRequirements");
    getFunction_ = Export<PFun_slGetFeatureFunction*>(module_, "slGetFeatureFunction");
    NewFrameToken = Export<PFun_slGetNewFrameToken*>(module_, "slGetNewFrameToken");
    SetConstants = Export<PFun_slSetConstants*>(module_, "slSetConstants");
    SetTagForFrame = Export<PFun_slSetTagForFrame*>(module_, "slSetTagForFrame");
    IsFeatureSupported = Export<PFun_slIsFeatureSupported*>(module_, "slIsFeatureSupported");
    GetFeatureVersion = Export<PFun_slGetFeatureVersion*>(module_, "slGetFeatureVersion");
    SetFeatureLoaded = Export<PFun_slSetFeatureLoaded*>(module_, "slSetFeatureLoaded");
    if (!instanceProc_ || !deviceProc_ || !init || !shutdown_ || !freeResources_ || !requirements_ || !getFunction_ ||
        !NewFrameToken || !SetConstants || !SetTagForFrame || !IsFeatureSupported || !GetFeatureVersion || !SetFeatureLoaded) {
        reason = "Streamline interposer mandatory export absent"; Shutdown(); return false;
    }

    const wchar_t* paths[] = {root.c_str()};
    sl::Preferences preferences{};
    preferences.pathsToPlugins = paths;
    preferences.numPathsToPlugins = 1;
    preferences.pathToLogsAndData = root.c_str();
    preferences.logLevel = sl::LogLevel::eDefault;
    preferences.logMessageCallback = Log;
    preferences.featuresToLoad = kFeatures;
    preferences.numFeaturesToLoad = 3;
    preferences.renderAPI = sl::RenderAPI::eVulkan;
    preferences.flags = sl::PreferenceFlags::eUseManualHooking | sl::PreferenceFlags::eDisableCLStateTracking |
        sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    preferences.applicationId = 0;
    preferences.projectId = "bb5fe48b-f929-4b9a-a72b-98a23141a7c9";
    preferences.engine = sl::EngineType::eCustom;
    preferences.engineVersion = "LostOdysseyRecomp";
    if (!Ok(init(preferences, sl::kSDKVersion))) {
        reason = "slInit failed"; Shutdown(); return false;
    }
    initialized_ = true;
    return true;
}

bool Runtime::Requirements(sl::Feature feature, sl::FeatureRequirements& requirements, std::string& reason) const {
    if (!requirements_ || !Ok(requirements_(feature, requirements))) {
        reason = "slGetFeatureRequirements failed for feature " + std::to_string(feature); return false;
    }
    return true;
}

bool Runtime::ImportFunctions(std::string& reason) {
    const auto get = [this, &reason](sl::Feature feature, const char* name, auto& output) {
        void* function{};
        if (!getFunction_ || !Ok(getFunction_(feature, name, function)) || !function) {
            reason = std::string("slGetFeatureFunction failed: ") + name; return false;
        }
        output = reinterpret_cast<std::remove_reference_t<decltype(output)>>(function);
        return true;
    };
    return get(sl::kFeatureDLSS_G, "slDLSSGSetOptions", DLSSGSetOptions) &&
        get(sl::kFeatureDLSS_G, "slDLSSGGetState", DLSSGGetState) &&
        get(sl::kFeatureReflex, "slReflexSetOptions", ReflexSetOptions) &&
        get(sl::kFeatureReflex, "slReflexSleep", ReflexSleep) &&
        get(sl::kFeaturePCL, "slPCLSetMarker", PCLSetMarker);
}

bool Runtime::FreeResources(sl::Feature feature, const sl::ViewportHandle& viewport) {
    return initialized_ && freeResources_ && Ok(freeResources_(feature, viewport));
}

bool Runtime::Shutdown() {
    bool success = true;
    if (initialized_) {
        success = shutdown_ && Ok(shutdown_());
        if (!success) { shutdownError_ = true; return false; }
        initialized_ = false;
    }
    if (module_) {
        success = !!FreeLibrary(module_) && success;
        module_ = nullptr;
    }
    shutdownError_ |= !success;
    return !shutdownError_;
}

void Runtime::AbandonAfterDrainFailure() {
    // Keep the SDK and DLL alive if asynchronous device ownership is unknown.
    initialized_ = false;
    module_ = nullptr;
}

} // namespace gpu::dlss_fg

#endif // _WIN32 && LO_ENABLE_STREAMLINE_FG
