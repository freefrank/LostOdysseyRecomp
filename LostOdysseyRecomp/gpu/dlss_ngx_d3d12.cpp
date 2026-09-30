#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#include "dlss_ngx.h"
#if defined(LO_GPU_PLUME) && defined(_WIN32)
#include "dlss_evaluate_capture.h"
#include <plume_d3d12.h>
#include <cmath>
#include <cstdio>
#include <string>
#if defined(LO_DLSS_SDK)
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_d3d.h>
#endif

namespace gpu::dlss {
namespace {
#if defined(LO_DLSS_SDK)
constexpr char kProjectId[] = "bb5fe48b-f929-4b9a-a72b-98a23141a7c9";
struct DiscoveryInfo {
    std::wstring dataPath, runtimePath;
    const wchar_t* paths[1]{};
    NVSDK_NGX_FeatureCommonInfo featureInfo{};
    NVSDK_NGX_FeatureDiscoveryInfo discovery{};
    DiscoveryInfo(const std::filesystem::path& data, const std::filesystem::path& runtime)
        : dataPath(data.wstring()), runtimePath(runtime.wstring()) {
        paths[0]=runtimePath.c_str();
        featureInfo.PathListInfo.Path=paths;
        featureInfo.PathListInfo.Length=runtimePath.empty()?0u:1u;
        discovery.SDKVersion=NVSDK_NGX_Version_API;
        discovery.FeatureID=NVSDK_NGX_Feature_SuperSampling;
        discovery.Identifier.IdentifierType=NVSDK_NGX_Application_Identifier_Type_Project_Id;
        discovery.Identifier.v.ProjectDesc={kProjectId,NVSDK_NGX_ENGINE_TYPE_CUSTOM,"LostOdysseyRecomp"};
        discovery.ApplicationDataPath=dataPath.c_str();
        discovery.FeatureInfo=&featureInfo;
    }
};
NVSDK_NGX_PerfQuality_Value Quality(upscaling::DlssQuality quality) {
    switch (quality) {
    case upscaling::DlssQuality::Quality: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
    case upscaling::DlssQuality::Balanced: return NVSDK_NGX_PerfQuality_Value_Balanced;
    case upscaling::DlssQuality::Performance: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
    case upscaling::DlssQuality::Dlaa: return NVSDK_NGX_PerfQuality_Value_DLAA;
    }
    return NVSDK_NGX_PerfQuality_Value_MaxQuality;
}
int FeatureFlags(const SrConfig& config) {
    int flags=NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    if (config.colorSpace==SrColorSpace::Linear) flags|=NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
    if (config.depthInverted) flags|=NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
    if (config.autoExposure) flags|=NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    return flags;
}
bool ValidTexture(const plume::D3D12Texture& texture, const plume::D3D12Device& device,
    D3D12_RESOURCE_STATES state, plume::RenderFormat format=plume::RenderFormat::UNKNOWN) {
    return texture.device==&device && texture.d3d && texture.allocation &&
        texture.desc.dimension==plume::RenderTextureDimension::TEXTURE_2D &&
        texture.desc.mipLevels==1 && texture.desc.arraySize==1 &&
        texture.desc.multisampling.sampleCount==1 && texture.desc.width && texture.desc.height &&
        (format==plume::RenderFormat::UNKNOWN || texture.desc.format==format) &&
        texture.resourceStates==state &&
        texture.layout==(state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS ?
            plume::RenderTextureLayout::GENERAL : plume::RenderTextureLayout::SHADER_READ);
}
bool LostDevice(HRESULT result) {
    return result==DXGI_ERROR_DEVICE_REMOVED || result==DXGI_ERROR_DEVICE_RESET || result==DXGI_ERROR_DEVICE_HUNG;
}
HRESULT BeginIsolated(plume::D3D12CommandList& list) {
    if (list.open || !list.d3d || !list.commandAllocator || !list.queue ||
        list.queue->type!=plume::RenderCommandListType::DIRECT) return E_INVALIDARG;
    list.invalidateCachedNativeState();
    list.resetRootBindingStats();
    HRESULT result=list.commandAllocator->Reset();
    if (SUCCEEDED(result)) result=list.d3d->Reset(list.commandAllocator,nullptr);
    if (SUCCEEDED(result)) list.open=true;
    return result;
}
HRESULT EndIsolated(plume::D3D12CommandList& list) {
    if (!list.open) return E_INVALIDARG;
    list.resetSamplePositions();
    const HRESULT result=list.d3d->Close();
    list.open=false;
    list.invalidateCachedNativeState();
    return result;
}
#endif
}

void Controller::ProbeOnce(const plume::D3D12Device& device) {
    if (probeAttempted_ || backend_!=Backend::None) return;
    probeAttempted_=true;
    backend_=Backend::D3D12;
    sessionDeviceD3D12_=&device;
#if defined(LO_DLSS_SDK)
    if (!device.d3d || !device.adapter) {
        report_.state=ProbeState::Unavailable; report_.reason="D3D12 device or adapter unavailable"; return;
    }
    DXGI_ADAPTER_DESC1 desc{};
    if (SUCCEEDED(device.adapter->GetDesc1(&desc))) {
        report_.vendorId=desc.VendorId; report_.deviceId=desc.DeviceId;
        const int bytes=WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,nullptr,0,nullptr,nullptr);
        if (bytes>1) {
            std::string name(size_t(bytes), '\0');
            WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,name.data(),bytes,nullptr,nullptr);
            name.pop_back(); report_.deviceName=std::move(name);
        }
    }
    DiscoveryInfo info(applicationDataPath_,runtimePath_);
    NVSDK_NGX_FeatureRequirement requirements{};
    auto result=NVSDK_NGX_D3D12_GetFeatureRequirements(device.adapter,&info.discovery,&requirements);
    RecordCall("D3D12_GetFeatureRequirements",int32_t(result),NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result)) { report_.state=ProbeState::ApiError; report_.reason="D3D12 requirements query failed"; return; }
    report_.featureSupport=uint32_t(requirements.FeatureSupported);
    if (requirements.FeatureSupported!=NVSDK_NGX_FeatureSupportResult_Supported) {
        report_.state=ProbeState::Unavailable; report_.reason="D3D12 NGX Super Sampling unavailable"; return;
    }
    std::string reason;
    if (!CreateApplicationDataPath(reason)) { report_.state=ProbeState::ApiError; report_.reason=reason; return; }
    result=NVSDK_NGX_D3D12_Init_with_ProjectID(kProjectId,NVSDK_NGX_ENGINE_TYPE_CUSTOM,
        "LostOdysseyRecomp",info.dataPath.c_str(),device.d3d,&info.featureInfo);
    RecordCall("D3D12_Probe_Init",int32_t(result),NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result)) { report_.state=ProbeState::ApiError; report_.reason="D3D12 NGX probe initialization failed"; return; }
    NVSDK_NGX_Parameter* params=nullptr;
    result=NVSDK_NGX_D3D12_GetCapabilityParameters(&params);
    RecordCall("D3D12_Probe_GetCapabilityParameters",int32_t(result),NVSDK_NGX_FAILED(result));
    if (!NVSDK_NGX_FAILED(result) && params) {
        const auto read=[&](const char* key,CapabilityValue& value) {
            int raw=0; const auto status=NVSDK_NGX_Parameter_GetI(params,key,&raw);
            value.raw=int32_t(status); if (!NVSDK_NGX_FAILED(status)) value.value=raw;
            RecordCall("D3D12_Probe_Parameter_GetI",int32_t(status),NVSDK_NGX_FAILED(status));
        };
        read(NVSDK_NGX_EParameter_SuperSampling_Available,report_.srAvailable);
        read(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver,report_.needsUpdatedDriver);
        read(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor,report_.minDriverVersionMajor);
        read(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor,report_.minDriverVersionMinor);
        read(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult,report_.featureInitResult);
        const auto classified=ClassifySuperSamplingCapabilities(report_.srAvailable,report_.needsUpdatedDriver,
            report_.minDriverVersionMajor,report_.minDriverVersionMinor,report_.featureInitResult,
            int32_t(NVSDK_NGX_Result_Success),int32_t(NVSDK_NGX_Result_FAIL_FeatureNotSupported));
        report_.state=classified.decision==CapabilityDecision::Proceed ? ProbeState::Available :
            classified.decision==CapabilityDecision::Unavailable ? ProbeState::Unavailable : ProbeState::ApiError;
        report_.reason=classified.reason;
        if (report_.state==ProbeState::Available) {
            for (const auto quality : {upscaling::DlssQuality::Quality,
                    upscaling::DlssQuality::Balanced,upscaling::DlssQuality::Performance}) {
                OptimalSettings settings{};
                settings.quality=quality==upscaling::DlssQuality::Quality ? "quality" :
                    quality==upscaling::DlssQuality::Balanced ? "balanced" : "performance";
                const auto optimal=NGX_DLSS_GET_OPTIMAL_SETTINGS(params,1920,1080,Quality(quality),
                    &settings.optimalWidth,&settings.optimalHeight,&settings.maxWidth,&settings.maxHeight,
                    &settings.minWidth,&settings.minHeight,&settings.sharpness);
                settings.result=int32_t(optimal);
                RecordCall("D3D12_Probe_GetOptimalSettings",int32_t(optimal),NVSDK_NGX_FAILED(optimal));
                report_.optimalSettings.push_back(std::move(settings));
            }
            if (!HasValidOptimalSettings(report_)) {
                report_.state=ProbeState::ApiError;
                report_.reason="D3D12 NGX optimal settings unavailable";
            }
        }
        const auto destroy=NVSDK_NGX_D3D12_DestroyParameters(params);
        RecordCall("D3D12_Probe_DestroyParameters",int32_t(destroy),NVSDK_NGX_FAILED(destroy));
        if (NVSDK_NGX_FAILED(destroy)) {
            capabilityParameters_=params;
            sessionInitialized_=true;
            sessionFailed_=true;
            report_.state=ProbeState::ApiError; report_.reason="D3D12 probe cleanup failed";
        }
    } else {
        report_.state=ProbeState::ApiError; report_.reason="D3D12 capability query failed";
        if (params) {
            const auto destroy=NVSDK_NGX_D3D12_DestroyParameters(params);
            RecordCall("D3D12_Probe_DestroyParameters",int32_t(destroy),NVSDK_NGX_FAILED(destroy));
            if (NVSDK_NGX_FAILED(destroy)) {
                capabilityParameters_=params; sessionInitialized_=true; sessionFailed_=true;
            }
        }
    }
    if (capabilityParameters_) return; // Keep the failed cleanup owned for a drained retry.
    result=NVSDK_NGX_D3D12_Shutdown1(device.d3d);
    RecordCall("D3D12_Probe_Shutdown1",int32_t(result),NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result)) {
        report_.state=ProbeState::ApiError; report_.reason="D3D12 probe shutdown failed";
        sessionFailed_=true; sessionInitialized_=true;
    } else if (!capabilityParameters_) {
        sessionInitialized_=false;
    }
#else
    (void)device;
#endif
}

SrStatus Controller::EnsureSession(const plume::D3D12Device& device) {
#if !defined(LO_DLSS_SDK)
    (void)device; return SrStatus::Bypass;
#else
    if (backend_!=Backend::D3D12 || sessionDeviceD3D12_!=&device) return SrStatus::NeedsReconfigure;
    if (report_.state==ProbeState::Unavailable) return SrStatus::Bypass;
    if (report_.state!=ProbeState::Available || sessionFailed_) return SrStatus::Failed;
    if (sessionInitialized_) return capabilityParameters_ ? SrStatus::Executable : SrStatus::Failed;
    std::string reason;
    if (!CreateApplicationDataPath(reason)) { sessionFailed_=true; return SrStatus::Failed; }
    DiscoveryInfo info(applicationDataPath_,runtimePath_);
    auto result=NVSDK_NGX_D3D12_Init_with_ProjectID(kProjectId,NVSDK_NGX_ENGINE_TYPE_CUSTOM,
        "LostOdysseyRecomp",info.dataPath.c_str(),device.d3d,&info.featureInfo);
    RecordCall("D3D12_Session_Init",int32_t(result),NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result)) { sessionFailed_=true; return SrStatus::Failed; }
    NVSDK_NGX_Parameter* params=nullptr;
    result=NVSDK_NGX_D3D12_GetCapabilityParameters(&params);
    RecordCall("D3D12_Session_GetCapabilityParameters",int32_t(result),NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result) || !params) {
        if (params) {
            const auto destroy=NVSDK_NGX_D3D12_DestroyParameters(params);
            RecordCall("D3D12_Session_DestroyParameters",int32_t(destroy),NVSDK_NGX_FAILED(destroy));
            if (NVSDK_NGX_FAILED(destroy)) capabilityParameters_=params;
        }
        if (!capabilityParameters_) {
            const auto shutdown=NVSDK_NGX_D3D12_Shutdown1(device.d3d);
            RecordCall("D3D12_Session_Shutdown1",int32_t(shutdown),NVSDK_NGX_FAILED(shutdown));
            if (NVSDK_NGX_FAILED(shutdown)) sessionInitialized_=true;
        } else sessionInitialized_=true;
        sessionFailed_=true; return SrStatus::Failed;
    }
    capabilityParameters_=params;
    sessionInitialized_=true;
    sessionRetryable_=false;
    return SrStatus::Executable;
#endif
}

upscaling::OutputSizing Controller::QueryOutputSizing(const plume::D3D12Device& device,
    const upscaling::SizingKey& key) {
    upscaling::OutputSizing sizing{}; sizing.key=key;
    const auto setAll=[&](upscaling::SizingState state) {
        for (auto& mode:sizing.modes) { mode.state=state; mode.issue=upscaling::SizingIssue::Prerequisite; }
    };
    if (!key.outputWidth || !key.outputHeight) { setAll(upscaling::SizingState::Error); return sizing; }
#if !defined(LO_DLSS_SDK)
    (void)device; setAll(upscaling::SizingState::Unavailable); return sizing;
#else
    const auto status=EnsureSession(device);
    if (status!=SrStatus::Executable) {
        setAll(status==SrStatus::Bypass ? upscaling::SizingState::Unavailable : upscaling::SizingState::Error);
        return sizing;
    }
    for (const auto quality:upscaling::kDlssQualityModes) {
        auto& mode=sizing.modes[upscaling::DlssQualityIndex(quality)];
        NVSDK_NGX_Parameter* params=nullptr;
        const auto get=NVSDK_NGX_D3D12_GetCapabilityParameters(&params);
        RecordCall("D3D12_Sizing_GetCapabilityParameters",int32_t(get),NVSDK_NGX_FAILED(get));
        if (NVSDK_NGX_FAILED(get) || !params) {
            mode.state=upscaling::SizingState::Error; mode.issue=upscaling::SizingIssue::CapabilityParameters;
            mode.ngxResult=int32_t(get);
            if (params) NVSDK_NGX_D3D12_DestroyParameters(params);
            continue;
        }
        uint32_t optimalWidth=0,optimalHeight=0,maxWidth=0,maxHeight=0,minWidth=0,minHeight=0;
        float sharpness=0;
        const auto result=NGX_DLSS_GET_OPTIMAL_SETTINGS(params,key.outputWidth,key.outputHeight,Quality(quality),
            &optimalWidth,&optimalHeight,&maxWidth,&maxHeight,&minWidth,&minHeight,&sharpness);
        RecordCall("D3D12_Sizing_GetOptimalSettings",int32_t(result),NVSDK_NGX_FAILED(result));
        mode.ngxResult=int32_t(result);
        mode.optimal={optimalWidth,optimalHeight}; mode.minimum={minWidth,minHeight}; mode.maximum={maxWidth,maxHeight};
        if (NVSDK_NGX_FAILED(result)) {
            mode.state=result==NVSDK_NGX_Result_FAIL_FeatureNotSupported ?
                upscaling::SizingState::Unavailable : upscaling::SizingState::Error;
            mode.issue=upscaling::SizingIssue::OptimalQuery;
        } else {
            unsigned checkedWidth=0,checkedHeight=0;
            const auto wr=NVSDK_NGX_Parameter_GetUI(params,NVSDK_NGX_Parameter_OutWidth,&checkedWidth);
            const auto hr=NVSDK_NGX_Parameter_GetUI(params,NVSDK_NGX_Parameter_OutHeight,&checkedHeight);
            mode.optimalWidthResult=int32_t(wr); mode.optimalHeightResult=int32_t(hr);
            mode.issue=NVSDK_NGX_FAILED(wr)||NVSDK_NGX_FAILED(hr)||checkedWidth!=optimalWidth||checkedHeight!=optimalHeight
                ? upscaling::SizingIssue::OptimalRead : upscaling::ResolveDlssSizing(mode,quality,{key.outputWidth,key.outputHeight});
            mode.state=mode.issue==upscaling::SizingIssue::None ? upscaling::SizingState::Ready : upscaling::SizingState::Error;
        }
        const auto destroy=NVSDK_NGX_D3D12_DestroyParameters(params);
        mode.cleanupResult=int32_t(destroy);
        RecordCall("D3D12_Sizing_DestroyParameters",int32_t(destroy),NVSDK_NGX_FAILED(destroy));
        if (NVSDK_NGX_FAILED(destroy)) {
            mode.state=upscaling::SizingState::Error; mode.issue=upscaling::SizingIssue::CleanupFailed;
            sessionFailed_=true;
        }
    }
    return sizing;
#endif
}

SrStatus Controller::AllocateParametersD3D12() {
#if !defined(LO_DLSS_SDK)
    return SrStatus::Bypass;
#else
    if (backend_!=Backend::D3D12 || !sessionInitialized_) return SrStatus::Failed;
    if (featureParameters_) return SrStatus::Executable;
    NVSDK_NGX_Parameter* params=nullptr;
    const auto result=NVSDK_NGX_D3D12_AllocateParameters(&params);
    RecordCall("D3D12_AllocateParameters",int32_t(result),NVSDK_NGX_FAILED(result));
    if (NVSDK_NGX_FAILED(result) || !params) return SrStatus::Failed;
    featureParameters_=params; return SrStatus::Executable;
#endif
}

SrAttempt Controller::RecordIsolated(plume::D3D12CommandList& isolated, const SrConfig& config,
    const temporal::TemporalFrameInputs& inputs, plume::D3D12Texture& output, EvaluateCapture* capture) {
    SrAttempt attempt{};
#if !defined(LO_DLSS_SDK)
    (void)isolated; (void)config; (void)inputs; (void)output; (void)capture;
    return attempt;
#else
    const auto early=[&](const char* reason) {
        if (capture) { capture->stage="validation"; capture->reason=reason; }
        return attempt;
    };
    if (config.colorSpace==SrColorSpace::Unknown ||
        !upscaling::ValidDlssRenderExtent(config.quality,config.renderExtent,config.outputExtent) ||
        backend_!=Backend::D3D12 || !sessionDeviceD3D12_) return early("invalid_configuration");
    const auto status=EnsureSession(*sessionDeviceD3D12_);
    if (status!=SrStatus::Executable) { attempt.status=status; return early("session_unavailable"); }
    if (NeedsFeatureRecreate(config) && featureConfigValid_) {
        attempt.status=SrStatus::NeedsReconfigure; return early("needs_reconfigure");
    }
    if (featureFailed_) { attempt.status=SrStatus::NeedsReconfigure; return early("feature_failed_needs_reconfigure"); }
    if (inputs.renderFrameId && inputs.renderFrameId==lastSrAttemptFrameId_) return early("duplicate_frame");
    const auto& device=*sessionDeviceD3D12_;
    const auto validRegion=[&](const temporal::TextureRegion& region, plume::RenderFormat format) {
        if (!region.Complete() || region.width!=config.renderExtent.width ||
            region.height!=config.renderExtent.height) return false;
        const auto& native=*static_cast<const plume::D3D12Texture*>(region.texture);
        return ValidTexture(native,device,D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,format) &&
            region.allocation.width==native.desc.width && region.allocation.height==native.desc.height;
    };
    const bool hybrid=inputs.motionState==temporal::MotionState::Hybrid;
    const auto validMask=[&] {
        if (!hybrid) return true;
        const auto& region=inputs.motionInvalidity;
        if (!region.Complete() || region.x || region.y || region.width!=config.renderExtent.width ||
            region.height!=config.renderExtent.height) return false;
        const auto& native=*static_cast<const plume::D3D12Texture*>(region.texture);
        return ValidTexture(native,device,D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,plume::RenderFormat::R8_UNORM) &&
            region.allocation.width==native.desc.width && region.allocation.height==native.desc.height;
    };
    if (!inputs.CompleteForConsumer() || !validRegion(inputs.color,
            config.colorSpace==SrColorSpace::Linear ? plume::RenderFormat::R16G16B16A16_FLOAT : plume::RenderFormat::R8G8B8A8_UNORM) ||
        !validRegion(inputs.depth,plume::RenderFormat::R32_FLOAT) ||
        !validRegion(inputs.motion,plume::RenderFormat::R16G16_FLOAT) || !validMask() ||
        !temporal::MatchesDepthConvention(inputs.depthConvention,config.depthInverted) ||
        config.deviceEpoch!=inputs.plan.deviceEpoch || inputs.plan.consumer!=upscaling::TemporalConsumer::DlssSr ||
        !ValidTexture(output,device,D3D12_RESOURCE_STATE_UNORDERED_ACCESS) ||
        !(output.desc.flags & plume::RenderTextureFlag::UNORDERED_ACCESS) ||
        output.desc.width!=config.outputExtent.width || output.desc.height!=config.outputExtent.height ||
        !std::isfinite(inputs.jitter.pixelX) || !std::isfinite(inputs.jitter.pixelY) ||
        !std::isfinite(inputs.preExposure) || inputs.preExposure<=0 ||
        !std::isfinite(inputs.exposureScale) || inputs.exposureScale<=0 ||
        (config.colorSpace==SrColorSpace::Linear && inputs.colorEncoding!=temporal::ColorEncoding::HdrLinear) ||
        (config.colorSpace==SrColorSpace::DisplayEncoded && inputs.colorEncoding!=temporal::ColorEncoding::Sdr))
        return early("invalid_inputs_or_resource_state");
    attempt.useId=srUses_.Record();
    if (!attempt.useId) { attempt.status=SrStatus::Failed; return early("use_id_unavailable"); }
    lastSrAttemptFrameId_=inputs.renderFrameId;
    const auto failed=[&](const char* reason, std::optional<int32_t> raw=std::nullopt, HRESULT hr=S_OK) {
        attempt.status=FAILED(hr)&&LostDevice(hr) ? SrStatus::DeviceLost : SrStatus::Failed;
        attempt.rawNgxResult=raw;
        featureFailed_=true;
        if (capture) {capture->stage="failed"; capture->reason=reason;}
        return attempt;
    };
    const HRESULT begin=BeginIsolated(isolated);
    RecordCall("D3D12_ResetIsolated",int32_t(begin),FAILED(begin));
    if (FAILED(begin)) return failed("isolated_begin_failed",std::nullopt,begin);
    auto* params=static_cast<NVSDK_NGX_Parameter*>(featureParameters_);
    bool created=false;
    if (!feature_) {
        if (AllocateParametersD3D12()!=SrStatus::Executable) {
            const HRESULT close=EndIsolated(isolated);
            RecordCall("D3D12_CloseIsolated",int32_t(close),FAILED(close));
            return failed("feature_parameters_failed",std::nullopt,close);
        }
        params=static_cast<NVSDK_NGX_Parameter*>(featureParameters_);
        NVSDK_NGX_DLSS_Create_Params create{};
        create.Feature.InWidth=config.renderExtent.width; create.Feature.InHeight=config.renderExtent.height;
        create.Feature.InTargetWidth=config.outputExtent.width; create.Feature.InTargetHeight=config.outputExtent.height;
        create.Feature.InPerfQualityValue=Quality(config.quality);
        create.InFeatureCreateFlags=FeatureFlags(config);
        create.InEnableOutputSubrects=false;
        NVSDK_NGX_Handle* handle=nullptr;
        const auto result=NGX_D3D12_CREATE_DLSS_EXT(isolated.d3d,1,1,&handle,params,&create);
        RecordCall("D3D12_CREATE_DLSS_EXT",int32_t(result),NVSDK_NGX_FAILED(result));
        if (capture) capture->createResult=int32_t(result);
        if (handle) feature_=handle; // A partial create is retired after the batch, too.
        if (NVSDK_NGX_FAILED(result) || !handle) {
            const HRESULT close=EndIsolated(isolated);
            RecordCall("D3D12_CloseIsolated",int32_t(close),FAILED(close));
            return failed("feature_create_failed",int32_t(result),close);
        }
        featureConfig_=config; featureConfigValid_=true; created=true;
        report_.srImplemented=true;
    }
    NVSDK_NGX_D3D12_DLSS_Eval_Params evaluate{};
    evaluate.Feature.pInColor=static_cast<plume::D3D12Texture*>(inputs.color.texture)->d3d;
    evaluate.Feature.pInOutput=output.d3d;
    evaluate.pInDepth=static_cast<plume::D3D12Texture*>(inputs.depth.texture)->d3d;
    evaluate.pInMotionVectors=static_cast<plume::D3D12Texture*>(inputs.motion.texture)->d3d;
    evaluate.pInBiasCurrentColorMask=hybrid ? static_cast<plume::D3D12Texture*>(inputs.motionInvalidity.texture)->d3d : nullptr;
    evaluate.InJitterOffsetX=float(inputs.jitter.pixelX); evaluate.InJitterOffsetY=float(inputs.jitter.pixelY);
    evaluate.InRenderSubrectDimensions={config.renderExtent.width,config.renderExtent.height};
    evaluate.InReset=created||inputs.resetHistory ? 1:0;
    evaluate.InMVScaleX=1; evaluate.InMVScaleY=1;
    evaluate.InColorSubrectBase={inputs.color.x,inputs.color.y};
    evaluate.InDepthSubrectBase={inputs.depth.x,inputs.depth.y};
    evaluate.InMVSubrectBase={inputs.motion.x,inputs.motion.y};
    evaluate.InBiasCurrentColorSubrectBase={0,0};
    evaluate.InOutputSubrectBase={0,0};
    evaluate.InPreExposure=inputs.preExposure; evaluate.InExposureScale=inputs.exposureScale;
    if (capture) {
        capture->omitted=true; capture->reason="d3d12_gpu_capture_unavailable";
        capture->stage="evaluate";
        capture->sdk.jitterX=evaluate.InJitterOffsetX; capture->sdk.jitterY=evaluate.InJitterOffsetY;
        capture->sdk.mvScaleX=1; capture->sdk.mvScaleY=1;
        capture->sdk.preExposure=evaluate.InPreExposure; capture->sdk.exposureScale=evaluate.InExposureScale;
        capture->sdk.colorX=inputs.color.x; capture->sdk.colorY=inputs.color.y;
        capture->sdk.depthX=inputs.depth.x; capture->sdk.depthY=inputs.depth.y;
        capture->sdk.mvX=inputs.motion.x; capture->sdk.mvY=inputs.motion.y;
        capture->sdk.outputX=0; capture->sdk.outputY=0;
        capture->sdk.renderWidth=config.renderExtent.width; capture->sdk.renderHeight=config.renderExtent.height;
        capture->sdk.reset=evaluate.InReset!=0; capture->sdk.featureCreated=created;
        capture->sdk.inputHistoryReset=inputs.resetHistory;
        capture->sdk.biasCurrentColorBound=hybrid;
    }
    const auto result=NGX_D3D12_EVALUATE_DLSS_EXT(isolated.d3d,
        static_cast<NVSDK_NGX_Handle*>(feature_),params,&evaluate);
    RecordCall("D3D12_EVALUATE_DLSS_EXT",int32_t(result),NVSDK_NGX_FAILED(result));
    if (capture) { capture->vendorResult=int32_t(result); capture->evaluated=true; }
    const HRESULT close=EndIsolated(isolated);
    RecordCall("D3D12_CloseIsolated",int32_t(close),FAILED(close));
    if (FAILED(close)) return failed("isolated_end_failed",int32_t(result),close);
    if (NVSDK_NGX_FAILED(result)) return failed("evaluate_failed",int32_t(result));
    report_.srEvaluated=true;
    attempt.status=SrStatus::Executable;
    return attempt;
#endif
}
} // namespace gpu::dlss
#endif
