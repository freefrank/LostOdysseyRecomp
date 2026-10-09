#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#include "dlss_ngx.h"
#if defined(LO_GPU_PLUME) && defined(_WIN32)
#include "dlss_evaluate_capture.h"
#include "dlss_nr.h"
#include "dlss_nr_state.h"
#include "temporal_frame_inputs.h"
#include <plume_d3d12.h>
#include <plume_render_interface_builders.h>
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
// DLSS 5 NR format bridge: an RGBA8 frame (stage, from column stageX on) to
// and from the model's FP16 images, both display-encoded. Decoding keeps the
// stage's alpha.
constexpr char kNeuralRenderingBridge[]=R"(
RWTexture2D<float4> stage : register(u0);
RWTexture2D<float4> model : register(u1);
cbuffer Parameters : register(b0) { uint width; uint height; uint decode; uint stageX; };
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= width || id.y >= height) return;
    uint2 s = uint2(id.x + stageX, id.y);
    if (decode != 0) stage[s] = float4(saturate(model[id.xy].rgb), stage[s].a);
    else model[id.xy] = stage[s];
}
)";
struct BridgeParameters { uint32_t width,height,decode,stageX; };
void DescribeBridgeSet(plume::RenderDescriptorSetBuilder& set) {
    set.begin(); set.addReadWriteTexture(0); set.addReadWriteTexture(1); set.end();
}
std::unique_ptr<plume::RenderDescriptorSet> BridgeSet(plume::RenderDevice& device, plume::RenderTexture* stage,
    plume::RenderTexture* model) {
    plume::RenderDescriptorSetBuilder builder;
    DescribeBridgeSet(builder);
    auto set=builder.create(&device);
    if (!set) return set;
    set->setTexture(0,stage,plume::RenderTextureLayout::GENERAL);
    set->setTexture(1,model,plume::RenderTextureLayout::GENERAL);
    return set;
}
void RecordBridge(plume::D3D12CommandList& list, const plume::RenderPipelineLayout& layout,
    const plume::RenderPipeline& pipeline, plume::RenderDescriptorSet& set, const BridgeParameters& constants) {
    list.setComputePipelineLayout(&layout);
    list.setPipeline(&pipeline);
    list.setComputePushConstants(0,&constants,0,sizeof(constants));
    list.setComputeDescriptorSet(&set,0);
    list.dispatch((constants.width+7)/8,(constants.height+7)/8,1);
}
ID3D12Resource* NativeResource(plume::RenderTexture* texture) { return static_cast<plume::D3D12Texture*>(texture)->d3d; }
#endif
}

void Controller::ProbeOnce(const plume::D3D12Device& device) {
    if (probeAttempted_ || backend_!=Backend::None) return;
    probeAttempted_=true;
    backend_=Backend::D3D12;
    sessionDeviceD3D12_=&device;
#if defined(LO_DLSS_SDK)
    // NR needs no device setup on D3D12; the settings preview only keeps
    // frames for a player who supplied the DLL.
    std::error_code missing;
    nrSupported_=std::filesystem::is_regular_file(nr::SnippetPath(runtimePath_),missing);
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
    if (!NVSDK_NGX_FAILED(result)) {
        CaptureNeuralRenderingInput(isolated,config,inputs,output);
        RecordNeuralRendering(isolated,config,inputs,output,evaluate.InReset!=0);
    }
    const HRESULT close=EndIsolated(isolated);
    RecordCall("D3D12_CloseIsolated",int32_t(close),FAILED(close));
    if (FAILED(close)) return failed("isolated_end_failed",int32_t(result),close);
    if (NVSDK_NGX_FAILED(result)) return failed("evaluate_failed",int32_t(result));
    report_.srEvaluated=true;
    attempt.status=SrStatus::Executable;
    return attempt;
#endif
}

bool Controller::CreateNeuralRenderingBridgePipeline(plume::RenderDevice& device) {
#if !defined(LO_DLSS_SDK)
    (void)device;
    return false;
#else
    if (nrBridgePipeline_) return true;
    const auto compile=nr::ShaderCompiler();
    std::vector<uint8_t> dxil;
    if (!compile || !compile(kNeuralRenderingBridge,dxil)) return false;
    plume::RenderDescriptorSetBuilder set;
    DescribeBridgeSet(set);
    plume::RenderPipelineLayoutBuilder layout;
    layout.begin(false,false);
    layout.addPushConstant(0,0,sizeof(BridgeParameters),plume::RenderShaderStageFlag::COMPUTE);
    layout.addDescriptorSet(set);
    layout.end();
    nrBridgeLayout_=layout.create(&device);
    nrBridgeShader_=device.createShader(dxil.data(),dxil.size(),"main",plume::RenderShaderFormat::DXIL);
    if (!nrBridgeLayout_ || !nrBridgeShader_) return false;
    nrBridgePipeline_=device.createComputePipeline(
        plume::RenderComputePipelineDesc(nrBridgeLayout_.get(),nrBridgeShader_.get(),8,8,1));
    return bool(nrBridgePipeline_);
#endif
}

bool Controller::CreateNeuralRenderingBridge(plume::RenderDevice& device, plume::RenderFormat format,
    uint32_t width, uint32_t height, uint32_t passes) {
#if !defined(LO_DLSS_SDK)
    (void)device; (void)format; (void)width; (void)height; (void)passes;
    return false;
#else
    if (!CreateNeuralRenderingBridgePipeline(device)) return false;
    nrStage_=device.createTexture(plume::RenderTextureDesc::Texture2D(width,height,1,format,
        plume::RenderTextureFlag::UNORDERED_ACCESS));
    if (!nrStage_) return false;
    // Encoding feeds the first pass; decoding reads the last pass's answer.
    nrEncodeSet_=BridgeSet(device,nrStage_.get(),nrImages_[0].get());
    nrDecodeSet_=BridgeSet(device,nrStage_.get(),nrImages_[passes%2].get());
    return nrEncodeSet_ && nrDecodeSet_;
#endif
}

void Controller::CaptureNeuralRenderingInput(plume::D3D12CommandList& list, const SrConfig& config,
    const temporal::TemporalFrameInputs& inputs, plume::D3D12Texture& output) {
#if !defined(LO_DLSS_SDK)
    (void)list; (void)config; (void)inputs; (void)output;
#else
    // Only a player with the DLL pays for the copies, every fourth frame.
    if (!nrSupported_ || config.colorSpace!=SrColorSpace::DisplayEncoded) return;
    if (nrCaptured_ && ++nrCaptureCount_%4) return;
    auto& depth=*static_cast<plume::D3D12Texture*>(inputs.depth.texture);
    auto& motion=*static_cast<plume::D3D12Texture*>(inputs.motion.texture);
    if (!nrCaptureColor_ &&
        !CreateNeuralRenderingCapture(*output.device,config,output.desc.format,depth.desc.format,motion.desc.format))
        return;
    using plume::RenderTextureBarrier;
    using Layout=plume::RenderTextureLayout;
    namespace Stage=plume::RenderBarrierStage;
    plume::RenderCommandList& commands=list;
    list.invalidateCachedNativeState();
    const RenderTextureBarrier copy[]={RenderTextureBarrier(&output,Layout::COPY_SOURCE),
        RenderTextureBarrier(&depth,Layout::COPY_SOURCE),RenderTextureBarrier(&motion,Layout::COPY_SOURCE),
        RenderTextureBarrier(nrCaptureColor_.get(),Layout::COPY_DEST),RenderTextureBarrier(nrCaptureDepth_.get(),Layout::COPY_DEST),
        RenderTextureBarrier(nrCaptureMotion_.get(),Layout::COPY_DEST)};
    commands.barriers(Stage::COPY,copy,6);
    const auto region=[&](plume::RenderTexture* destination, plume::RenderTexture* source, uint32_t x, uint32_t y,
                          uint32_t width, uint32_t height) {
        const plume::RenderBox box(int32_t(x),int32_t(y),int32_t(x+width),int32_t(y+height),0,1);
        commands.copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(destination),
            plume::RenderTextureCopyLocation::Subresource(source),0,0,0,&box);
    };
    region(nrCaptureColor_.get(),&output,0,0,config.outputExtent.width,config.outputExtent.height);
    region(nrCaptureDepth_.get(),&depth,inputs.depth.x,inputs.depth.y,config.renderExtent.width,config.renderExtent.height);
    region(nrCaptureMotion_.get(),&motion,inputs.motion.x,inputs.motion.y,config.renderExtent.width,config.renderExtent.height);
    // Back to the states SR and NR expect: the output as a UAV, the inputs readable by any stage.
    commands.barriers(Stage::COMPUTE,RenderTextureBarrier(&output,Layout::GENERAL));
    const RenderTextureBarrier back[]={RenderTextureBarrier(&depth,Layout::SHADER_READ),
        RenderTextureBarrier(&motion,Layout::SHADER_READ)};
    commands.barriers(Stage::GRAPHICS|Stage::COMPUTE,back,2);
    nrCaptured_=true;
#endif
}

void Controller::RecordNeuralRendering(plume::D3D12CommandList& list, const SrConfig& config,
    const temporal::TemporalFrameInputs& inputs, plume::D3D12Texture& output, bool reset) {
#if !defined(LO_DLSS_SDK)
    (void)list; (void)config; (void)inputs; (void)output; (void)reset;
#else
    const auto* snippet=NeuralRenderingSnippet(config);
    if (!snippet) return;
    auto* parameters=static_cast<NVSDK_NGX_Parameter*>(nrParameters_);
    const uint32_t width=config.outputExtent.width, height=config.outputExtent.height;
    const uint32_t passes=config.neuralRenderingPasses;

    if (!HasNeuralRenderingFeature()) {
        // Creation records initialization work; the first evaluate waits for
        // the next recording. The model's images are FP16, as on Vulkan.
        auto& device=*output.device;
        for (auto& image:nrImages_) {
            image=device.createTexture(plume::RenderTextureDesc::Texture2D(width,height,1,
                plume::RenderFormat::R16G16B16A16_FLOAT,plume::RenderTextureFlag::UNORDERED_ACCESS));
            if (!image) return FailNeuralRendering("image allocation",0);
        }
        if (!CreateNeuralRenderingBridge(device,output.desc.format,width,height,passes))
            return FailNeuralRendering("format bridge creation",0);
        for (uint32_t pass=0;pass<passes;++pass) {
            nr::SetControls(parameters,width,height,config.depthInverted,config.neuralRenderingPreset,nrTuning_,pass);
            NVSDK_NGX_Handle* handle=nullptr;
            const auto result=snippet->d3d12.create(list.d3d,nr::kFeature,parameters,&handle);
            RecordCall("NR_D3D12_CreateFeature",int32_t(result));
            nrFeatures_[pass]=handle; // A partial create is released at the drained boundary.
            if (NVSDK_NGX_FAILED(result) || !handle) return FailNeuralRendering("CreateFeature",int32_t(result));
        }
        list.invalidateCachedNativeState();
        nrPasses_=passes; nrWidth_=width; nrHeight_=height; nrReset_=true;
        nr::Log("DLSS NR: %u feature(s) created for %ux%u", passes, width, height);
        return;
    }
    // An output or pass-count change is a drained SR reconfigure, which recreates the features.
    if (width!=nrWidth_ || height!=nrHeight_ || passes!=nrPasses_) return;

    using plume::RenderTextureBarrier;
    using Layout=plume::RenderTextureLayout;
    namespace Stage=plume::RenderBarrierStage;
    plume::RenderTexture* images[2]={nrImages_[0].get(),nrImages_[1].get()};
    plume::RenderTexture* stage=nrStage_.get();
    // The base class carries the barrier overloads.
    plume::RenderCommandList& commands=list;
    // NGX left its own heaps and root signature on the list. The output goes
    // back to UNORDERED_ACCESS right after each copy, so a failure leaves it as SR did.
    list.invalidateCachedNativeState();
    const RenderTextureBarrier toStage[]={RenderTextureBarrier(&output,Layout::COPY_SOURCE),RenderTextureBarrier(stage,Layout::COPY_DEST)};
    commands.barriers(Stage::COPY,toStage,2);
    commands.copyTexture(stage,&output);
    const RenderTextureBarrier encode[]={RenderTextureBarrier(&output,Layout::GENERAL),
        RenderTextureBarrier(stage,Layout::GENERAL),RenderTextureBarrier(images[0],Layout::GENERAL)};
    commands.barriers(Stage::COMPUTE,encode,3);
    RecordBridge(list,*nrBridgeLayout_,*nrBridgePipeline_,*nrEncodeSet_,{width,height,0,0});

    NVSDK_NGX_Parameter_SetD3d12Resource(parameters,"DLSSNR.Depth",static_cast<plume::D3D12Texture*>(inputs.depth.texture)->d3d);
    NVSDK_NGX_Parameter_SetD3d12Resource(parameters,"DLSSNR.MVec",static_cast<plume::D3D12Texture*>(inputs.motion.texture)->d3d);
    nr::SetFrame(parameters,nr::GameFrame(config,inputs),reset || nrReset_ || nrTuningChanged_);
    nrTuningChanged_=false;
    for (uint32_t pass=0;pass<passes;++pass) {
        // Pass n reads image n % 2 and answers into the other one.
        plume::RenderTexture* input=images[pass%2];
        plume::RenderTexture* answer=images[(pass+1)%2];
        const RenderTextureBarrier evaluate[]={RenderTextureBarrier(input,Layout::SHADER_READ),RenderTextureBarrier(answer,Layout::GENERAL)};
        commands.barriers(Stage::COMPUTE,evaluate,2);
        nr::SetControls(parameters,width,height,config.depthInverted,config.neuralRenderingPreset,nrTuning_,pass);
        NVSDK_NGX_Parameter_SetD3d12Resource(parameters,"DLSSNR.Color",NativeResource(input));
        NVSDK_NGX_Parameter_SetD3d12Resource(parameters,"DLSSNR.Output",NativeResource(answer));
        const auto result=snippet->d3d12.evaluate(list.d3d,static_cast<NVSDK_NGX_Handle*>(nrFeatures_[pass]),parameters,nullptr);
        RecordCall("NR_D3D12_EvaluateFeature",int32_t(result));
        list.invalidateCachedNativeState();
        if (NVSDK_NGX_FAILED(result)) return FailNeuralRendering("EvaluateFeature",int32_t(result));
    }
    const RenderTextureBarrier decode[]={RenderTextureBarrier(images[passes%2],Layout::GENERAL),RenderTextureBarrier(stage,Layout::GENERAL)};
    commands.barriers(Stage::COMPUTE,decode,2);
    RecordBridge(list,*nrBridgeLayout_,*nrBridgePipeline_,*nrDecodeSet_,{width,height,1,0});
    const RenderTextureBarrier toOutput[]={RenderTextureBarrier(stage,Layout::COPY_SOURCE),RenderTextureBarrier(&output,Layout::COPY_DEST)};
    commands.barriers(Stage::COPY,toOutput,2);
    commands.copyTexture(&output,stage);
    commands.barriers(Stage::COMPUTE,RenderTextureBarrier(&output,Layout::GENERAL));
    if (nrReset_) nr::Log("DLSS NR: first evaluate recorded for %ux%u, %u pass(es)", width, height, passes);
    nrReset_=false;
    g_neuralRenderingState.store(NeuralRenderingState::Active,std::memory_order_relaxed);
#endif
}

plume::RenderTexture* Controller::RecordNeuralRenderingPreview(plume::D3D12CommandList& list, uint32_t passes,
    uint32_t preset, const NeuralRenderingTuning& tuning) {
#if !defined(LO_DLSS_SDK)
    (void)list; (void)passes; (void)preset; (void)tuning;
    return nullptr;
#else
    const auto& config=nrCaptureConfig_;
    const uint32_t width=config.outputExtent.width, height=config.outputExtent.height;
    plume::RenderTexture* color=nrCaptureColor_.get();
    auto& device=*static_cast<plume::D3D12Texture*>(color)->device;
    if (!nrPreviewComposite_) {
        nrPreviewComposite_=device.createTexture(plume::RenderTextureDesc::Texture2D(width*2,height,1,
            static_cast<plume::D3D12Texture*>(color)->desc.format,plume::RenderTextureFlag::UNORDERED_ACCESS));
        if (!nrPreviewComposite_) return nullptr;
    }
    plume::RenderTexture* composite=nrPreviewComposite_.get();
    using plume::RenderTextureBarrier;
    using Layout=plume::RenderTextureLayout;
    namespace Stage=plume::RenderBarrierStage;
    plume::RenderCommandList& commands=list;
    auto* parameters=static_cast<NVSDK_NGX_Parameter*>(nrParameters_);
    std::string reason;
    const auto* snippet=passes ? nr::LoadSnippet(runtimePath_,reason) : nullptr;
    plume::RenderTexture* right=color; // What the right half shows when the model did not answer.
    if (passes && !HasNeuralRenderingPreviewFeature()) {
        // As in gameplay, the features evaluate from the next recording on.
        for (auto& image:nrPreviewImages_) {
            if (!image) image=device.createTexture(plume::RenderTextureDesc::Texture2D(width,height,1,
                plume::RenderFormat::R16G16B16A16_FLOAT,plume::RenderTextureFlag::UNORDERED_ACCESS));
        }
        if (nrPreviewImages_[0] && nrPreviewImages_[1] && CreateNeuralRenderingBridgePipeline(device)) {
            nrPreviewEncodeSet_=BridgeSet(device,color,nrPreviewImages_[0].get());
            nrPreviewDecodeSet_=BridgeSet(device,composite,nrPreviewImages_[passes%2].get());
            for (uint32_t pass=0;pass<passes;++pass) {
                nr::SetControls(parameters,width,height,config.depthInverted,preset,tuning,pass);
                NVSDK_NGX_Handle* handle=nullptr;
                const auto result=snippet->d3d12.create(list.d3d,nr::kFeature,parameters,&handle);
                RecordCall("NR_Preview_D3D12_CreateFeature",int32_t(result));
                nrPreviewFeatures_[pass]=handle;
                if (NVSDK_NGX_FAILED(result) || !handle) {
                    FailNeuralRenderingPreview("CreateFeature",int32_t(result));
                    break;
                }
            }
            list.invalidateCachedNativeState();
            nrPreviewPasses_=passes;
            nrPreviewPreset_=preset;
            nrPreviewSettle_=nr::kPreviewSettleEvaluates;
            nrPreviewReset_=true;
        }
    } else if (passes && HasNeuralRenderingPreviewFeature() && nrPreviewEncodeSet_ && nrPreviewDecodeSet_) {
        right=nullptr; // The last answer stays once the frame has settled.
        if (nrPreviewSettle_) {
            plume::RenderTexture* images[2]={nrPreviewImages_[0].get(),nrPreviewImages_[1].get()};
            const RenderTextureBarrier encode[]={RenderTextureBarrier(color,Layout::GENERAL),
                RenderTextureBarrier(images[0],Layout::GENERAL)};
            commands.barriers(Stage::COMPUTE,encode,2);
            RecordBridge(list,*nrBridgeLayout_,*nrBridgePipeline_,*nrPreviewEncodeSet_,{width,height,0,0});
            const RenderTextureBarrier inputs[]={RenderTextureBarrier(nrCaptureDepth_.get(),Layout::SHADER_READ),
                RenderTextureBarrier(nrCaptureMotion_.get(),Layout::SHADER_READ)};
            commands.barriers(Stage::COMPUTE,inputs,2);
            NVSDK_NGX_Parameter_SetD3d12Resource(parameters,"DLSSNR.Depth",NativeResource(nrCaptureDepth_.get()));
            NVSDK_NGX_Parameter_SetD3d12Resource(parameters,"DLSSNR.MVec",NativeResource(nrCaptureMotion_.get()));
            // The same frame again: no motion.
            nr::SetFrame(parameters,{width,height,config.renderExtent.width,config.renderExtent.height,0,0,0,0,0.0f},
                nrPreviewReset_);
            bool ran=true;
            for (uint32_t pass=0;pass<passes && ran;++pass) {
                plume::RenderTexture* input=images[pass%2];
                plume::RenderTexture* answer=images[(pass+1)%2];
                const RenderTextureBarrier evaluate[]={RenderTextureBarrier(input,Layout::SHADER_READ),
                    RenderTextureBarrier(answer,Layout::GENERAL)};
                commands.barriers(Stage::COMPUTE,evaluate,2);
                nr::SetControls(parameters,width,height,config.depthInverted,preset,tuning,pass);
                NVSDK_NGX_Parameter_SetD3d12Resource(parameters,"DLSSNR.Color",NativeResource(input));
                NVSDK_NGX_Parameter_SetD3d12Resource(parameters,"DLSSNR.Output",NativeResource(answer));
                const auto result=snippet->d3d12.evaluate(list.d3d,
                    static_cast<NVSDK_NGX_Handle*>(nrPreviewFeatures_[pass]),parameters,nullptr);
                RecordCall("NR_Preview_D3D12_EvaluateFeature",int32_t(result));
                list.invalidateCachedNativeState();
                if (NVSDK_NGX_FAILED(result)) {
                    FailNeuralRenderingPreview("EvaluateFeature",int32_t(result));
                    ran=false;
                }
            }
            if (ran) {
                const RenderTextureBarrier decode[]={RenderTextureBarrier(images[passes%2],Layout::GENERAL),
                    RenderTextureBarrier(composite,Layout::GENERAL)};
                commands.barriers(Stage::COMPUTE,decode,2);
                RecordBridge(list,*nrBridgeLayout_,*nrBridgePipeline_,*nrPreviewDecodeSet_,{width,height,1,width});
            } else right=color;
            --nrPreviewSettle_;
            nrPreviewReset_=false;
        }
    }
    // The held frame on the left, and on the right when the model did not answer.
    const RenderTextureBarrier copy[]={RenderTextureBarrier(color,Layout::COPY_SOURCE),
        RenderTextureBarrier(composite,Layout::COPY_DEST)};
    commands.barriers(Stage::COPY,copy,2);
    const plume::RenderBox box(0,0,int32_t(width),int32_t(height),0,1);
    commands.copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(composite),
        plume::RenderTextureCopyLocation::Subresource(color),0,0,0,&box);
    if (right) commands.copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(composite),
        plume::RenderTextureCopyLocation::Subresource(right),width,0,0,&box);
    return composite;
#endif
}
} // namespace gpu::dlss
#endif
