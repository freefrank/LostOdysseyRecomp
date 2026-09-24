#include "streamline_runtime.h"
#include <sl_security.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <type_traits>

namespace probe {
namespace {
std::atomic<unsigned> slErrorCount{};
template<class T> T Export(HMODULE module, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(module, name));
}
bool Ok(sl::Result result) { return result == sl::Result::eOk; }
std::string Sha256(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0); return {};
    }
    std::array<char, 65536> data{};
    bool good = true;
    while (stream.read(data.data(), data.size()) || stream.gcount()) {
        if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(data.data()), ULONG(stream.gcount()), 0) < 0) {
            good = false; break;
        }
    }
    std::array<uint8_t, 32> digest{};
    good = good && !stream.bad() && BCryptFinishHash(hash, digest.data(), ULONG(digest.size()), 0) >= 0;
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!good) return {};
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (uint8_t byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}
void LogHash(const std::filesystem::path& path) {
    std::printf("RUNTIME_SHA256 path=%ls hash=%s\n", path.c_str(), Sha256(path).c_str());
}
bool VerifyNgxSignature(const std::filesystem::path& path) {
    WINTRUST_FILE_INFO file{}; file.cbStruct = sizeof(file); file.pcwszFilePath = path.c_str();
    WINTRUST_DATA data{}; data.cbStruct = sizeof(data); data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE; data.dwUnionChoice = WTD_CHOICE_FILE; data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const bool valid = WinVerifyTrust(nullptr, &action, &data) == ERROR_SUCCESS;
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &action, &data);
    if (!valid) return false;
    DWORD encoding{}, content{}, format{};
    HCERTSTORE store{}; HCRYPTMSG message{};
    if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, path.c_str(), CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
            CERT_QUERY_FORMAT_FLAG_BINARY, 0, &encoding, &content, &format, &store, &message, nullptr)) return false;
    DWORD len{}; bool nvidia = false;
    if (CryptMsgGetParam(message, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &len) && len) {
        std::vector<uint8_t> data(len);
        if (CryptMsgGetParam(message, CMSG_SIGNER_INFO_PARAM, 0, data.data(), &len)) {
            auto* signer = reinterpret_cast<PCMSG_SIGNER_INFO>(data.data());
            CERT_INFO cert{}; cert.Issuer = signer->Issuer; cert.SerialNumber = signer->SerialNumber;
            if (auto* ctx = CertFindCertificateInStore(store, encoding, 0, CERT_FIND_SUBJECT_CERT, &cert, nullptr)) {
                wchar_t subject[256]{};
                CertGetNameStringW(ctx, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, subject, 256);
                nvidia = wcsstr(subject, L"NVIDIA") != nullptr;
                std::printf("NGX_FG_AUTHENTICODE_SIGNER=%ls valid=%d\n", subject, int(nvidia));
                CertFreeCertificateContext(ctx);
            }
        }
    }
    CryptMsgClose(message); CertCloseStore(store, 0);
    return nvidia;
}
void Log(sl::LogType type, const char* message) {
    if (type == sl::LogType::eError) ++slErrorCount;
    if (type == sl::LogType::eWarn || type == sl::LogType::eError)
        std::printf("SL_LOG_%s=%s\n", type == sl::LogType::eError ? "ERROR" : "WARN", message ? message : "(null)");
}
}
StreamlineRuntime::~StreamlineRuntime() { Shutdown(); }
unsigned StreamlineRuntime::ErrorCount() const { return slErrorCount.load(); }
bool StreamlineRuntime::Initialize(const std::filesystem::path& directory, std::string& reason) {
    if (initialized_ || module_) { reason = "SL initialized twice"; return false; }
    slErrorCount.store(0);
    const auto root = std::filesystem::absolute(directory);
    for (const wchar_t* name : {L"sl.interposer.dll", L"sl.common.dll", L"sl.dlss_g.dll", L"sl.reflex.dll", L"sl.pcl.dll"}) {
        const auto path = root / name;
        if (!std::filesystem::is_regular_file(path) || !sl::security::verifyEmbeddedSignature(path.c_str())) {
            reason = "missing or invalid NVIDIA signature for " + path.string(); return false;
        }
        std::printf("SIGNED_RUNTIME=%ls\n", path.c_str());
        LogHash(path);
    }
    if (!VerifyNgxSignature(root / L"nvngx_dlssg.dll")) {
        reason = "nvngx_dlssg.dll lacks valid NVIDIA Authenticode signature"; return false;
    }
    LogHash(root / L"nvngx_dlssg.dll");
    LogHash(root / L"nvngx_dlss.dll");
    if (std::filesystem::exists(root / L"sl.dlss.dll")) {
        reason = "sl.dlss.dll present: native NGX SR must remain the only SR provider"; return false;
    }
    module_ = LoadLibraryExW((root / L"sl.interposer.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module_) { reason = "signed Streamline interposer absolute-path load failed: " + std::to_string(GetLastError()); return false; }
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
    preferences.logLevel = sl::LogLevel::eVerbose;
    preferences.logMessageCallback = Log;
    preferences.featuresToLoad = kFeatures;
    preferences.numFeaturesToLoad = 3;
    preferences.renderAPI = sl::RenderAPI::eVulkan;
    preferences.flags = sl::PreferenceFlags::eUseManualHooking | sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    preferences.applicationId = 0;
    preferences.projectId = "bb5fe48b-f929-4b9a-a72b-98a23141a7c9";
    preferences.engine = sl::EngineType::eCustom;
    preferences.engineVersion = "LostOdysseyRecomp";
    if (!Ok(init(preferences, sl::kSDKVersion))) {
        reason = "slInit failed for NGX project identity / required FG, Reflex, PCL plugins";
        Shutdown(); return false;
    }
    initialized_ = true;
    std::puts("SL_INIT=manual_hooking,frame_tagging,no_OTA,no_downloaded_plugins,NGX_project_identity");
    return true;
}
bool StreamlineRuntime::Requirements(sl::Feature feature, sl::FeatureRequirements& req, std::string& reason) const {
    if (!requirements_ || !Ok(requirements_(feature, req))) {
        reason = "slGetFeatureRequirements failed for feature " + std::to_string(feature); return false;
    }
    std::printf("SL_REQUIREMENTS feature=%u flags=%u graphics=%u compute=%u optical=%u instanceExt=%u deviceExt=%u features12=%u features13=%u detectedOS=%s minOS=%s detectedDriver=%s minDriver=%s HAGS_required=%d\n",
        feature, uint32_t(req.flags), req.vkNumGraphicsQueuesRequired, req.vkNumComputeQueuesRequired,
        req.vkNumOpticalFlowQueuesRequired, req.vkNumInstanceExtensions, req.vkNumDeviceExtensions,
        req.vkNumFeatures12, req.vkNumFeatures13, req.osVersionDetected.toStr().c_str(),
        req.osVersionRequired.toStr().c_str(), req.driverVersionDetected.toStr().c_str(),
        req.driverVersionRequired.toStr().c_str(),
        !!(uint32_t(req.flags) & uint32_t(sl::FeatureRequirementFlags::eHardwareSchedulingRequired)));
    return true;
}
bool StreamlineRuntime::ImportFunctions(std::string& reason) {
    const auto get = [this, &reason](sl::Feature feature, const char* name, auto& output) {
        void* p{};
        if (!Ok(getFunction_(feature, name, p)) || !p) { reason = std::string("slGetFeatureFunction: ") + name; return false; }
        output = reinterpret_cast<std::remove_reference_t<decltype(output)>>(p);
        return true;
    };
    return get(sl::kFeatureDLSS_G, "slDLSSGSetOptions", DLSSGSetOptions) &&
        get(sl::kFeatureDLSS_G, "slDLSSGGetState", DLSSGGetState) &&
        get(sl::kFeatureReflex, "slReflexSetOptions", ReflexSetOptions) &&
        get(sl::kFeatureReflex, "slReflexSleep", ReflexSleep) &&
        get(sl::kFeaturePCL, "slPCLSetMarker", PCLSetMarker);
}
bool StreamlineRuntime::FreeResources(sl::Feature feature, const sl::ViewportHandle& viewport) {
    if (!initialized_ || !freeResources_) {
        std::puts("LIFECYCLE_SL_FREE_RESOURCES=unavailable");
        return false;
    }
    const auto result = freeResources_(feature, viewport);
    std::printf("LIFECYCLE_SL_FREE_RESOURCES feature=%u result=%d\n", unsigned(feature), int(result));
    return Ok(result);
}
bool StreamlineRuntime::Shutdown() {
    bool success = true;
    if (initialized_) {
        if (shutdown_) {
            const auto result = shutdown_();
            std::printf("LIFECYCLE_SL_SHUTDOWN result=%d\n", int(result));
            success = Ok(result);
        } else {
            std::puts("LIFECYCLE_SL_SHUTDOWN=missing_export");
            success = false;
        }
        initialized_ = false;
    }
    if (module_) {
        if (!FreeLibrary(module_)) {
            std::printf("LIFECYCLE_SL_FREE_LIBRARY error=%lu\n", GetLastError());
            success = false;
        }
        module_ = nullptr;
    }
    shutdownError_ |= !success;
    return !shutdownError_;
}
void StreamlineRuntime::AbandonAfterDrainFailure() {
    // The process is exiting after an unproven GPU drain. Leave the DLL and
    // SDK sessions in place instead of retiring resources possibly in use.
    initialized_ = false;
    module_ = nullptr;
    std::puts("LIFECYCLE_SL_ABANDONED_AFTER_DRAIN_FAILURE=1");
}
}
