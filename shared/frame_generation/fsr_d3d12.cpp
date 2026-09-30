#include "d3d12.h"
#if defined(_WIN32) && defined(FRAMEGEN_WITH_FSR)
// SDK functions are dynamically imported; do not mark declarations dllexport.
#define FFX_API_ENTRY
#include <ffx_api/ffx_api.h>
#include <ffx_api/ffx_framegeneration.h>
#include <ffx_api/dx12/ffx_api_dx12.h>
#include <atomic>
#include <cstdio>
#include <algorithm>

namespace framegen {
namespace {
bool Check(ffxReturnCode_t result,const char* operation,std::string& reason) {
    if (result==FFX_API_RETURN_OK) return true;
    reason=std::string(operation)+" result="+std::to_string(result); return false;
}
template<class T> T Export(HMODULE module,const char* name) { return reinterpret_cast<T>(GetProcAddress(module,name)); }
uint32_t ReadState(D3D12_RESOURCE_STATES state) {
    return (state&D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) ?
        FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ : FFX_API_RESOURCE_STATE_COMPUTE_READ;
}
class FsrSession final : public D3D12Session {
    HMODULE module_=nullptr;
    PfnFfxCreateContext create_=nullptr;
    PfnFfxDestroyContext destroy_=nullptr;
    PfnFfxConfigure configure_=nullptr;
    PfnFfxQuery query_=nullptr;
    PfnFfxDispatch dispatch_=nullptr;
    ffxContext swapContext_=nullptr, fgContext_=nullptr;
    ComPtr<IDXGISwapChain4> swapchain_;
    IDXGISwapChain4* createdSwapchain_=nullptr;
    // Creation descriptors and every pointer in their chain outlive contexts.
    ffxCreateContextDescFrameGenerationSwapChainForHwndDX12 swapCreate_{};
    DXGI_SWAP_CHAIN_DESC1 nativeDescription_{};
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDescription_{};
    ffxCreateContextDescFrameGeneration createDescription_{};
    ffxCreateBackendDX12Desc backend_{};
    uint32_t contextWidth_=0,contextHeight_=0,contextInputWidth_=0,contextInputHeight_=0;
    DXGI_FORMAT contextFormat_=DXGI_FORMAT_UNKNOWN;
    bool contextReversed_=false;
    uint64_t frame_=0;
    std::atomic<bool> reset_{true};
    std::atomic<uint32_t> callbackError_{FFX_API_RETURN_OK};
    std::atomic<uint64_t> generatedDispatches_{0};
    static ffxReturnCode_t Generate(ffxDispatchDescFrameGeneration* params,void* user) {
        auto& self=*static_cast<FsrSession*>(user);
        if (!params || !self.fgContext_) return FFX_API_RETURN_ERROR_PARAMETER;
        params->reset = params->reset || self.reset_.load(std::memory_order_acquire);
        params->backbufferTransferFunction=FFX_API_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
        params->minMaxLuminance[0]=0; params->minMaxLuminance[1]=1;
        const auto result=self.dispatch_(&self.fgContext_,&params->header);
        if (result!=FFX_API_RETURN_OK) self.callbackError_.store(result,std::memory_order_release);
        else if (params->numGeneratedFrames) self.generatedDispatches_.fetch_add(1,std::memory_order_relaxed);
        return result;
    }
    bool Configure(bool enabled,std::string& reason) {
        if (!fgContext_) return true;
        ffxConfigureDescFrameGeneration c{};
        c.header.type=FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
        c.swapChain=swapchain_.Get(); c.frameGenerationEnabled=enabled; c.allowAsyncWorkloads=false;
        c.frameGenerationCallback=Generate; c.frameGenerationCallbackUserContext=this;
        // Use the SDK's ordinary composited-backbuffer presentation. HUDless/UI
        // is optional; no duplicate host Present or SR SDK context is involved.
        if (contextWidth_>uint32_t(INT32_MAX) || contextHeight_>uint32_t(INT32_MAX)) {
            reason="FSR FG rectangle exceeds signed SDK range"; return false;
        }
        c.generationRect={0,0,static_cast<int32_t>(contextWidth_),static_cast<int32_t>(contextHeight_)}; c.frameID=frame_;
        return Check(configure_(&fgContext_,&c.header),enabled ? "FSR FG enable" : "FSR FG off",reason);
    }
    bool EnsureContext(const D3D12Frame& f,std::string& reason) {
        if (fgContext_ && contextWidth_==f.width && contextHeight_==f.height &&
            contextInputWidth_==f.inputWidth && contextInputHeight_==f.inputHeight &&
            contextFormat_==f.format && contextReversed_==f.camera.depthReversed) return true;
        if (fgContext_) {
            if (!Configure(false,reason) || !WaitNative(reason) || !Check(destroy_(&fgContext_,nullptr),"FSR FG destroy on resize",reason)) return false;
            fgContext_=nullptr;
        }
        createDescription_={}; backend_={};
        createDescription_.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
        createDescription_.header.pNext=&backend_.header;
        backend_.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12; backend_.device=device_.Get();
        createDescription_.displaySize={f.width,f.height}; createDescription_.maxRenderSize={f.inputWidth,f.inputHeight};
        createDescription_.backBufferFormat=ffxApiGetSurfaceFormatDX12(f.format);
        createDescription_.flags=f.camera.depthReversed ? FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED : 0;
        if (!Check(create_(&fgContext_,&createDescription_.header,nullptr),"FSR FG context",reason)) return false;
        contextWidth_=f.width; contextHeight_=f.height; contextInputWidth_=f.inputWidth; contextInputHeight_=f.inputHeight;
        contextFormat_=f.format; contextReversed_=f.camera.depthReversed;
        caps_={true,1,false}; // The pinned FSR 3.1.4 adapter implements fixed 2x only.
        callbackError_=FFX_API_RETURN_OK; return true;
    }
public:
    ~FsrSession() override { std::string reason; if (!Shutdown(reason)) { std::fprintf(stderr,"FSR FG shutdown: %s\n",reason.c_str()); std::terminate(); } }
    bool Initialize(ID3D12Device* device,IDXGIFactory4* factory,const Config& config,
        const std::filesystem::path& runtime,std::string& reason) {
        if (config.provider!=Provider::Fsr || config.mode!=Mode::Fixed || config.generatedFrames!=1) {
            reason="FSR 3.1.4 requires fixed 2x mode"; return false;
        }
        if (!InitializeBase(device,factory,config,reason)) return false;
        std::error_code ec; const auto path=std::filesystem::absolute(runtime,ec);
        if (ec || !std::filesystem::is_regular_file(path,ec)) { reason="AMD FidelityFX DX12 runtime missing"; return false; }
        module_=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module_) { reason="AMD FidelityFX DX12 runtime load failed"; return false; }
        create_=Export<PfnFfxCreateContext>(module_,"ffxCreateContext");
        destroy_=Export<PfnFfxDestroyContext>(module_,"ffxDestroyContext");
        configure_=Export<PfnFfxConfigure>(module_,"ffxConfigure");
        query_=Export<PfnFfxQuery>(module_,"ffxQuery");
        dispatch_=Export<PfnFfxDispatch>(module_,"ffxDispatch");
        if (!create_ || !destroy_ || !configure_ || !query_ || !dispatch_) { reason="FidelityFX API export missing"; return false; }
        uint64_t count=0; ffxQueryDescGetVersions query{};
        query.header.type=FFX_API_QUERY_DESC_TYPE_GET_VERSIONS; query.device=device;
        query.createDescType=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION; query.outputCount=&count;
        if (!Check(query_(nullptr,&query.header),"FSR FG provider query",reason) || !count) { reason="FSR frame-generation provider absent"; return false; }
        return true;
    }
    HRESULT CreateSwapchain(ID3D12CommandQueue* queue,HWND window,const DXGI_SWAP_CHAIN_DESC1& desc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,IDXGISwapChain1** output) override {
        if (!output || !queue || swapContext_) return E_INVALIDARG;
        *output=nullptr; queue_=queue; nativeDescription_=desc;
        fullscreenDescription_=fullscreen ? *fullscreen : DXGI_SWAP_CHAIN_FULLSCREEN_DESC{};
        if (!fullscreen) fullscreenDescription_.Windowed=TRUE;
        swapCreate_.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12;
        swapCreate_.swapchain=&createdSwapchain_; swapCreate_.hwnd=window; swapCreate_.desc=&nativeDescription_;
        swapCreate_.fullscreenDesc=&fullscreenDescription_; swapCreate_.dxgiFactory=factory_.Get(); swapCreate_.gameQueue=queue;
        const auto code=create_(&swapContext_,&swapCreate_.header,nullptr);
        if (code!=FFX_API_RETURN_OK || !createdSwapchain_) return E_FAIL;
        swapchain_.Attach(createdSwapchain_);
        return swapchain_->QueryInterface(IID_PPV_ARGS(output));
    }
private:
    bool PrepareNative(const D3D12Frame& f,ID3D12GraphicsCommandList* commands,bool reset,std::string& reason) override {
        if (!swapchain_ || (f.format!=DXGI_FORMAT_R8G8B8A8_UNORM && f.format!=DXGI_FORMAT_B8G8R8A8_UNORM)) {
            reason="FSR FG requires an SDR RGBA8/BGRA8 swapchain"; return false;
        }
        if (!EnsureContext(f,reason)) return false;
        if (!Select(requested_,caps_).Enabled()) { reason="unsupported FSR FG mode"; return false; }
        if (callbackError_.load(std::memory_order_acquire)!=FFX_API_RETURN_OK) { reason="FSR FG dispatch callback failed"; return false; }
        if (frame_==UINT64_MAX) { reason="FSR FG frame ID overflow"; return false; }
        ++frame_; reset_.store(reset,std::memory_order_release);
        ffxDispatchDescFrameGenerationPrepare p{};
        p.header.type=FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE;
        p.frameID=frame_; p.commandList=commands; p.renderSize={f.inputWidth,f.inputHeight};
        p.jitterOffset={f.camera.jitterX,f.camera.jitterY}; p.motionVectorScale={1,1};
        p.frameTimeDelta=f.deltaMilliseconds; p.cameraNear=f.camera.nearPlane; p.cameraFar=f.camera.farPlane;
        p.cameraFovAngleVertical=f.camera.fovRadians; p.viewSpaceToMetersFactor=1.0f;
        p.depth=ffxApiGetResourceDX12(f.depth,ReadState(f.depthState));
        p.motionVectors=ffxApiGetResourceDX12(f.motion,ReadState(f.motionState));
        ffxDispatchDescFrameGenerationPrepareCameraInfo camera{};
        camera.header.type=FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_CAMERAINFO;
        std::copy(f.camera.position.begin(),f.camera.position.end(),camera.cameraPosition);
        std::copy(f.camera.right.begin(),f.camera.right.end(),camera.cameraRight);
        std::copy(f.camera.up.begin(),f.camera.up.end(),camera.cameraUp);
        std::copy(f.camera.forward.begin(),f.camera.forward.end(),camera.cameraForward);
        p.header.pNext=&camera.header;
        // FidelityFX 1.1.4 configures this frame's ID before preparing its inputs.
        if (!Configure(true,reason)) return false;
        return Check(dispatch_(&fgContext_,&p.header),"FSR FG prepare",reason);
    }
    bool DisableNative(std::string& reason) override { return Configure(false,reason); }
    bool WaitNative(std::string& reason) override {
        if (!swapContext_) return true;
        ffxDispatchDescFrameGenerationSwapChainWaitForPresentsDX12 wait{};
        wait.header.type=FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WAIT_FOR_PRESENTS_DX12;
        if (!Check(dispatch_(&swapContext_,&wait.header),"FSR FG wait for SDK presents",reason)) return false;
        if (FAILED(device_->GetDeviceRemovedReason())) { reason="FSR FG device removed during SDK completion"; return false; }
        return true;
    }
    bool PresentedNative(bool accepted,std::string& reason) override {
        if (!accepted && active_) { reason="FSR FG present rejected"; return false; }
        // Dispatch callbacks are not physical presents. Keep the counters distinct.
        if (sourceFrame_%60==0) std::fprintf(stderr,"FSR FG: generation_dispatches=%llu scope=sdk_dispatch_not_display\n",
            static_cast<unsigned long long>(generatedDispatches_.load(std::memory_order_relaxed)));
        if (callbackError_.load(std::memory_order_acquire)!=FFX_API_RETURN_OK) { reason="FSR FG callback failure"; return false; }
        active_=accepted && prepared_;
        return true;
    }
    bool ShutdownNative(std::string& reason) override {
        if (fgContext_) {
            if (!Check(destroy_(&fgContext_,nullptr),"FSR FG destroy",reason)) return false;
            fgContext_=nullptr;
        }
        if (swapContext_) {
            if (!Check(destroy_(&swapContext_,nullptr),"FSR swapchain destroy",reason)) return false;
            swapContext_=nullptr;
        }
        swapchain_.Reset(); createdSwapchain_=nullptr;
        if (module_) { if (!FreeLibrary(module_)) { reason="FidelityFX DLL unload failed"; return false; } module_=nullptr; }
        return true;
    }
};
}
std::unique_ptr<D3D12Session> CreateFsrD3D12(ID3D12Device* d,IDXGIFactory4* f,const Config& c,
    const std::filesystem::path& path,std::string& reason) {
    auto out=std::make_unique<FsrSession>();
    if (!out->Initialize(d,f,c,path,reason)) return {};
    return out;
}
} // namespace framegen
#endif
