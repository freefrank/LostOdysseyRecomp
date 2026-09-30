#include "d3d12.h"
#ifdef _WIN32
#include <limits>
namespace framegen {
namespace {
bool Failure(const char* operation,HRESULT hr,std::string& reason) {
    reason=std::string(operation)+" HRESULT="+std::to_string(static_cast<int32_t>(hr)); return false;
}
bool Valid(const D3D12Frame& f,ID3D12Device* device) {
    if (!f.lifetime || !f.depth || !f.motion || !f.width || !f.height || !f.inputWidth || !f.inputHeight ||
        !f.backBuffers || f.format==DXGI_FORMAT_UNKNOWN || !std::isfinite(f.deltaMilliseconds) || f.deltaMilliseconds<=0 ||
        !std::isfinite(f.camera.nearPlane) || !std::isfinite(f.camera.farPlane) ||
        !(f.camera.nearPlane>0) || !(f.camera.farPlane>f.camera.nearPlane) ||
        !std::isfinite(f.camera.fovRadians) || !(f.camera.fovRadians>0) || !(f.camera.fovRadians<3.142f) ||
        !std::isfinite(f.camera.aspect) || !(f.camera.aspect>0) ||
        !std::isfinite(f.camera.jitterX) || !std::isfinite(f.camera.jitterY)) return false;
    for (const auto* m:{&f.camera.viewToClip,&f.camera.clipToView,&f.camera.clipToPrevious,&f.camera.previousToClip})
        for (float v:*m) if (!std::isfinite(v)) return false;
    for (const auto* v:{&f.camera.position,&f.camera.right,&f.camera.up,&f.camera.forward})
        for (float x:*v) if (!std::isfinite(x)) return false;
    unsigned index=0;
    for (auto* resource:{f.depth,f.motion}) {
        Microsoft::WRL::ComPtr<ID3D12Device> owner;
        if (FAILED(resource->GetDevice(IID_PPV_ARGS(&owner))) || owner.Get()!=device) return false;
        const auto d=resource->GetDesc();
        if (d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || d.Width!=f.inputWidth || d.Height!=f.inputHeight ||
            d.DepthOrArraySize!=1 || d.MipLevels!=1 || d.SampleDesc.Count!=1 ||
            d.Format!=(index++ ? DXGI_FORMAT_R16G16_FLOAT : DXGI_FORMAT_R32_FLOAT)) return false;
    }
    return (f.depthState&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) &&
        (f.motionState&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
}
D3D12Session::~D3D12Session() {
    if (initialized_ && !closed_) std::terminate();
    if (event_) CloseHandle(event_);
}
bool D3D12Session::InitializeBase(ID3D12Device* device,IDXGIFactory4* factory,const Config& cfg,std::string& reason) {
    if (!device || !factory || initialized_) { reason="invalid or duplicate FG device"; return false; }
    device_=device; factory_=factory; requested_=cfg;
    auto hr=device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_));
    if (FAILED(hr)) return Failure("FG CreateFence",hr,reason);
    event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if (!event_) return Failure("FG CreateEvent",HRESULT_FROM_WIN32(GetLastError()),reason);
    initialized_=true; return true;
}
HRESULT D3D12Session::CreateQueue(const D3D12_COMMAND_QUEUE_DESC& d,REFIID iid,void** out) {
    return device_->CreateCommandQueue(&d,iid,out);
}
bool D3D12Session::SignalFence(std::string& reason) {
    if (!queue_ || !fence_ || fenceValue_==UINT64_MAX) { reason="FG completion queue/fence unavailable"; return false; }
    const auto hr=queue_->Signal(fence_.Get(),++fenceValue_);
    return SUCCEEDED(hr) || Failure("FG Signal",hr,reason);
}
bool D3D12Session::WaitFence(std::string& reason) {
    if (!fenceValue_) return true;
    auto completed=fence_->GetCompletedValue();
    if (completed==UINT64_MAX) return Failure("FG device removed",device_->GetDeviceRemovedReason(),reason);
    if (completed<fenceValue_) {
        const auto hr=fence_->SetEventOnCompletion(fenceValue_,event_);
        if (FAILED(hr)) return Failure("FG SetEventOnCompletion",hr,reason);
        if (WaitForSingleObject(event_,10000)!=WAIT_OBJECT_0) { reason="FG completion wait failed or timed out"; return false; }
        completed=fence_->GetCompletedValue();
    }
    if (completed==UINT64_MAX || completed<fenceValue_) { reason="FG fence did not complete"; return false; }
    return SUCCEEDED(device_->GetDeviceRemovedReason()) || Failure("FG device removed",device_->GetDeviceRemovedReason(),reason);
}
bool D3D12Session::Prepare(const D3D12Frame& f,ID3D12GraphicsCommandList* commands,std::string& reason) {
    if (!initialized_ || closed_ || nativeFeatureReleased_ || !commands || !Valid(f,device_.Get())) { reason="FG invalid frame input"; return false; }
    if (!Drain(reason)) return false;
    key_={f.deviceEpoch,f.temporalEpoch,f.width,f.height,f.inputWidth,f.inputHeight,uint32_t(f.format),requested_};
    if (blocked_ && *blocked_==key_) { reason="FG configuration blocked after SDK failure; change configuration or quiesce first"; return false; }
    const bool reset=history_.NeedsReset(f.frameId,key_,f.reset);
    if (!lease_.Begin(f.lifetime)) { reason="FG input overlaps previous lease"; return false; }
    sourceFrame_=f.frameId;
    if (!PrepareNative(f,commands,reset,reason)) {
        active_=false; prepared_=false; blocked_=key_; history_.Reset();
        std::string offReason;
        if (!DisableNative(offReason)) reason+="; disable failed: "+offReason;
        // The caller must still submit/retire or explicitly cancel its recording.
        return false;
    }
    prepared_=true; history_.Accepted(f.frameId,key_); return true;
}
bool D3D12Session::SubmitStart(std::string& reason) {
    if (!lease_.Pending()) return true;
    if (!lease_.SubmitStart()) { reason="FG invalid submit start"; return false; } return true;
}
bool D3D12Session::Submitted(bool success,uint64_t serial,std::string& reason) {
    if (!lease_.Pending()) return true;
    if (!lease_.Submitted(success,serial)) { reason="FG host submission failure; lease retained"; return false; } return true;
}
bool D3D12Session::Presented(bool accepted,std::string& reason) {
    if (lease_.Pending()) {
        if (!lease_.Presented(accepted)) { reason="FG rejected present; lease retained"; return false; }
        // For DLSS block-presenting-queue this orders all input reads. FSR also
        // checks its SDK-owned presentation queues in WaitNative below.
        if (!SignalFence(reason)) return false;
    }
    const bool ok=PresentedNative(accepted,reason);
    if (prepared_ && !active_) { blocked_=key_; history_.Reset(); }
    return ok;
}
bool D3D12Session::Drain(std::string& reason) {
    if (!lease_.Pending()) return true;
    if (lease_.GetPhase()!=InputLease::Phase::Presented) { reason="FG input has no accepted present"; return false; }
    if (!WaitNative(reason) || !WaitFence(reason)) return false;
    if (!lease_.Complete(lease_.Serial(),true)) { reason="FG invalid retirement"; return false; }
    return true;
}
bool D3D12Session::CancelRecorded(bool commandReset,bool producerCompleted,std::string& reason) {
    if (!lease_.Pending()) return true;
    if (lease_.GetPhase()!=InputLease::Phase::Recorded || !commandReset || !producerCompleted) {
        reason="FG cancellation lacks unsubmitted reset/producer completion"; return false;
    }
    if (!DisableNative(reason) || !WaitNative(reason)) return false;
    if (!lease_.CancelRecorded(true,true,true)) { reason="FG cancellation rejected"; return false; }
    active_=false; prepared_=false; history_.Reset(); return true;
}
bool D3D12Session::Disable(std::string& reason) {
    if (!initialized_ || closed_) return true;
    if (!DisableNative(reason)) return false;
    active_=false; prepared_=false; history_.Reset(); return true;
}
bool D3D12Session::Quiesce(std::string& reason) {
    if (!initialized_ || closed_ || nativeFeatureReleased_) return true;
    if (!Drain(reason) || !Disable(reason) || !WaitNative(reason)) return false;
    if (queue_ && (!SignalFence(reason) || !WaitFence(reason))) return false;
    blocked_.reset(); return true;
}
bool D3D12Session::Reconfigure(const Config& config,std::string& reason) {
    if (nativeFeatureReleased_ || config.provider!=requested_.provider || config.mode==Mode::Off ||
        !config.generatedFrames || !std::isfinite(config.targetFrameRate) || config.targetFrameRate<0) {
        reason="FG reconfiguration requires the existing provider and valid mode"; return false;
    }
    if (!Quiesce(reason)) return false;
    requested_=config; history_.Reset(); blocked_.reset(); return true;
}
bool D3D12Session::ReleaseFeatureAfterGpuDrain(std::string& reason) {
    if (nativeFeatureReleased_ || closed_) return true;
    if (!Quiesce(reason) || !ReleaseNativeFeature(reason)) return false;
    nativeFeatureReleased_=true; return true;
}
bool D3D12Session::Shutdown(std::string& reason) {
    if (closed_) return true;
    if (!ReleaseFeatureAfterGpuDrain(reason) || !ShutdownNative(reason)) return false;
    closed_=true; return true;
}
} // namespace framegen
#endif
