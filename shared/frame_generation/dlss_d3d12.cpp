#include "d3d12.h"
#if defined(_WIN32) && defined(FRAMEGEN_WITH_DLSS)
#include <sl.h>
#include <sl_dlss_g.h>
#include <sl_reflex.h>
#include <sl_security.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace framegen {
namespace {
std::atomic<unsigned> creationErrors{0};
void SdkLog(sl::LogType type,const char* message) {
    if (type==sl::LogType::eError && message && std::strstr(message,"NGX create feature failed")) ++creationErrors;
    if (type==sl::LogType::eWarn || type==sl::LogType::eError)
        std::fprintf(stderr,"DLSS FG SDK: %s\n",message ? message : "(null)");
}
bool Check(sl::Result result,const char* operation,std::string& reason) {
    if (result==sl::Result::eOk) return true;
    reason=std::string(operation)+" result="+std::to_string(int(result)); return false;
}
template<class T> T Export(HMODULE module,const char* name) { return reinterpret_cast<T>(GetProcAddress(module,name)); }
void MatrixToSl(const Matrix& m,sl::float4x4& out) {
    for (unsigned r=0;r<4;++r) out.setRow(r,{m[4*r],m[4*r+1],m[4*r+2],m[4*r+3]});
}
sl::Constants Constants(const D3D12Frame& f,bool reset) {
    sl::Constants c{}; const auto& a=f.camera;
    MatrixToSl(a.viewToClip,c.cameraViewToClip); MatrixToSl(a.clipToView,c.clipToCameraView);
    MatrixToSl(a.clipToPrevious,c.clipToPrevClip); MatrixToSl(a.previousToClip,c.prevClipToClip);
    c.cameraPos={a.position[0],a.position[1],a.position[2]};
    c.cameraRight={a.right[0],a.right[1],a.right[2]};
    c.cameraUp={a.up[0],a.up[1],a.up[2]};
    c.cameraFwd={a.forward[0],a.forward[1],a.forward[2]};
    c.cameraNear=a.nearPlane; c.cameraFar=a.farPlane; c.cameraFOV=a.fovRadians; c.cameraAspectRatio=a.aspect;
    c.jitterOffset={a.jitterX,a.jitterY}; c.mvecScale={1.0f/f.inputWidth,1.0f/f.inputHeight};
    c.cameraPinholeOffset={0,0}; c.depthInverted=a.depthReversed ? sl::eTrue : sl::eFalse;
    c.cameraMotionIncluded=sl::eTrue; c.motionVectors3D=sl::eFalse;
    c.motionVectorsJittered=sl::eFalse; c.motionVectorsDilated=sl::eFalse;
    c.orthographicProjection=sl::eFalse; c.reset=reset ? sl::eTrue : sl::eFalse;
    return c;
}
class DlssSession final : public D3D12Session {
    HMODULE module_=nullptr;
    bool sdkInitialized_=false, used_=false;
    ComPtr<ID3D12Device> proxyDevice_;
    ComPtr<IDXGIFactory4> proxyFactory_;
    ComPtr<IDXGISwapChain1> swapchain_;
    PFun_slShutdown* shutdown_=nullptr;
    PFun_slFreeResources* free_=nullptr;
    PFun_slGetFeatureFunction* featureFunction_=nullptr;
    PFun_slGetNewFrameToken* newToken_=nullptr;
    PFun_slSetConstants* constants_=nullptr;
    PFun_slSetTagForFrame* tags_=nullptr;
    PFun_slGetNativeInterface* native_=nullptr;
    PFun_slDLSSGSetOptions* setOptions_=nullptr;
    PFun_slDLSSGGetState* getState_=nullptr;
    PFun_slReflexSleep* sleep_=nullptr;
    PFun_slPCLSetMarker* marker_=nullptr;
    sl::ViewportHandle viewport_{0};
    sl::FrameToken* token_=nullptr;
    sl::DLSSGOptions options_{};
    uint32_t tokenId_=0;
    unsigned errorsSeen_=0;
    bool Mark(sl::PCLMarker mark,std::string& reason) {
        return !token_ || Check(marker_(mark,*token_),"slPCLSetMarker",reason);
    }
    template<class T> bool Import(sl::Feature f,const char* name,T& out,std::string& reason) {
        void* p=nullptr;
        if (!Check(featureFunction_(f,name,p),name,reason) || !p) return false;
        out=reinterpret_cast<T>(p); return true;
    }
public:
    ~DlssSession() override { std::string reason; if (!Shutdown(reason)) { std::fprintf(stderr,"DLSS FG shutdown: %s\n",reason.c_str()); std::terminate(); } }
    bool Initialize(ID3D12Device* device,IDXGIFactory4* factory,const Config& cfg,
        const std::filesystem::path& directory,std::string& reason) {
        if (cfg.provider!=Provider::Dlss || !InitializeBase(device,factory,cfg,reason)) return false;
        std::error_code ec; const auto root=std::filesystem::absolute(directory,ec);
        if (ec) { reason="invalid Streamline runtime directory"; return false; }
        for (const wchar_t* name:{L"sl.interposer.dll",L"sl.common.dll",L"sl.dlss_g.dll",L"sl.reflex.dll",L"sl.pcl.dll"}) {
            const auto path=root/name;
            if (!std::filesystem::is_regular_file(path,ec) || !sl::security::verifyEmbeddedSignature(path.c_str())) {
                reason="missing or unsigned NVIDIA runtime: "+path.string(); return false;
            }
        }
        if (!std::filesystem::is_regular_file(root/L"nvngx_dlssg.dll",ec)) { reason="nvngx_dlssg.dll missing"; return false; }
        module_=LoadLibraryExW((root/L"sl.interposer.dll").c_str(),nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module_) { reason="Streamline DLL load failed"; return false; }
        auto init=Export<PFun_slInit*>(module_,"slInit");
        auto setDevice=Export<PFun_slSetD3DDevice*>(module_,"slSetD3DDevice");
        auto upgrade=Export<PFun_slUpgradeInterface*>(module_,"slUpgradeInterface");
        auto supported=Export<PFun_slIsFeatureSupported*>(module_,"slIsFeatureSupported");
        shutdown_=Export<PFun_slShutdown*>(module_,"slShutdown");
        free_=Export<PFun_slFreeResources*>(module_,"slFreeResources");
        featureFunction_=Export<PFun_slGetFeatureFunction*>(module_,"slGetFeatureFunction");
        newToken_=Export<PFun_slGetNewFrameToken*>(module_,"slGetNewFrameToken");
        constants_=Export<PFun_slSetConstants*>(module_,"slSetConstants");
        tags_=Export<PFun_slSetTagForFrame*>(module_,"slSetTagForFrame");
        native_=Export<PFun_slGetNativeInterface*>(module_,"slGetNativeInterface");
        if (!init || !setDevice || !upgrade || !supported || !shutdown_ || !free_ || !featureFunction_ ||
            !newToken_ || !constants_ || !tags_ || !native_) { reason="Streamline mandatory export absent"; return false; }
        const sl::Feature features[]={sl::kFeatureDLSS_G,sl::kFeatureReflex,sl::kFeaturePCL};
        const wchar_t* paths[]={root.c_str()};
        sl::Preferences p{}; p.pathsToPlugins=paths; p.numPathsToPlugins=1;
        p.featuresToLoad=features; p.numFeaturesToLoad=3; p.renderAPI=sl::RenderAPI::eD3D12;
        p.flags=sl::PreferenceFlags::eUseManualHooking|sl::PreferenceFlags::eDisableCLStateTracking|
            sl::PreferenceFlags::eUseFrameBasedResourceTagging;
        p.logMessageCallback=SdkLog; p.logLevel=sl::LogLevel::eDefault;
        p.applicationId=0; p.projectId="bb5fe48b-f929-4b9a-a72b-98a23141a7c9";
        p.engine=sl::EngineType::eCustom; p.engineVersion="ReusableFrameGeneration";
        if (!Check(init(p,sl::kSDKVersion),"slInit(D3D12)",reason)) return false;
        sdkInitialized_=true; errorsSeen_=creationErrors.load();
        if (!Check(setDevice(device),"slSetD3DDevice",reason)) return false;
        auto luid=device->GetAdapterLuid(); sl::AdapterInfo adapter{};
        adapter.deviceLUID=reinterpret_cast<uint8_t*>(&luid); adapter.deviceLUIDSizeInBytes=sizeof(luid);
        if (!Check(supported(sl::kFeatureDLSS_G,adapter),"DLSS FG adapter support",reason)) return false;
        void* proxy=device;
        if (!Check(upgrade(&proxy),"upgrade D3D12 device",reason) || proxy==device) return false;
        proxyDevice_.Attach(static_cast<ID3D12Device*>(proxy));
        proxy=factory;
        if (!Check(upgrade(&proxy),"upgrade DXGI factory",reason) || proxy==factory) return false;
        proxyFactory_.Attach(static_cast<IDXGIFactory4*>(proxy));
        PFun_slReflexSetOptions* reflex=nullptr;
        if (!Import(sl::kFeatureDLSS_G,"slDLSSGSetOptions",setOptions_,reason) ||
            !Import(sl::kFeatureDLSS_G,"slDLSSGGetState",getState_,reason) ||
            !Import(sl::kFeatureReflex,"slReflexSetOptions",reflex,reason) ||
            !Import(sl::kFeatureReflex,"slReflexSleep",sleep_,reason) ||
            !Import(sl::kFeaturePCL,"slPCLSetMarker",marker_,reason)) return false;
        sl::ReflexOptions ro{}; ro.mode=sl::ReflexMode::eLowLatency;
        return Check(reflex(ro),"Reflex enable",reason);
    }
    HRESULT CreateQueue(const D3D12_COMMAND_QUEUE_DESC& desc,REFIID iid,void** out) override {
        if (!out) return E_POINTER; *out=nullptr;
        ComPtr<ID3D12CommandQueue> intercepted;
        auto hr=proxyDevice_->CreateCommandQueue(&desc,IID_PPV_ARGS(&intercepted));
        if (FAILED(hr)) return hr;
        // Native queues go to the renderer/NGX; only creation/present APIs use SL proxies.
        void* raw=nullptr;
        if (native_(intercepted.Get(),&raw)!=sl::Result::eOk || !raw) return E_FAIL;
        ComPtr<ID3D12CommandQueue> queue; queue.Attach(static_cast<ID3D12CommandQueue*>(raw));
        return queue->QueryInterface(iid,out);
    }
    HRESULT CreateSwapchain(ID3D12CommandQueue* queue,HWND window,const DXGI_SWAP_CHAIN_DESC1& desc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,IDXGISwapChain1** out) override {
        if (!out || swapchain_) return E_INVALIDARG;
        const auto hr=proxyFactory_->CreateSwapChainForHwnd(queue,window,&desc,fullscreen,nullptr,out);
        if (SUCCEEDED(hr)) { queue_=queue; swapchain_=*out; }
        return hr;
    }
    bool SubmitStart(std::string& reason) override {
        return D3D12Session::SubmitStart(reason) && Mark(sl::PCLMarker::eRenderSubmitStart,reason);
    }
    bool PresentStart(std::string& reason) override {
        return Mark(sl::PCLMarker::eRenderSubmitEnd,reason) && Mark(sl::PCLMarker::ePresentStart,reason);
    }
private:
    bool ReleaseNativeFeature(std::string& reason) override {
        if (sdkInitialized_ && used_) {
            if (!free_ || !Check(free_(sl::kFeatureDLSS_G,viewport_),"SL free FG",reason)) return false;
            used_=false;
        }
        return true;
    }
    bool PrepareNative(const D3D12Frame& f,ID3D12GraphicsCommandList*,bool reset,std::string& reason) override {
        if (!swapchain_) { reason="DLSS FG swapchain not created"; return false; }
        if (creationErrors.load()!=errorsSeen_) { errorsSeen_=creationErrors.load(); reason="DLSS FG asynchronous feature creation failed"; return false; }
        token_=nullptr;
        if (!Check(newToken_(token_,&++tokenId_),"SL frame token",reason) || !token_) return false;
        if (!Check(sleep_(*token_),"Reflex sleep",reason) || !Mark(sl::PCLMarker::eSimulationStart,reason) ||
            !Mark(sl::PCLMarker::eSimulationEnd,reason)) return false;
        sl::DLSSGState state{};
        if (!Check(getState_(viewport_,state,nullptr),"DLSS FG capabilities",reason)) return false;
        caps_={true,state.numFramesToGenerateMax,state.bIsDynamicMFGSupported==sl::eTrue};
        const auto selected=Select(requested_,caps_);
        if (!selected.Enabled()) { reason="DLSS FG mode/multiplier unsupported: "+std::to_string(int(selected.rejection)); return false; }
        options_.mode=requested_.mode==Mode::Dynamic ? sl::DLSSGMode::eDynamic : sl::DLSSGMode::eOn;
        options_.numFramesToGenerate=requested_.generatedFrames;
        options_.dynamicTargetFrameRate=requested_.targetFrameRate;
        options_.queueParallelismMode=sl::DLSSGQueueParallelismMode::eBlockPresentingClientQueue;
        options_.enableUserInterfaceRecomposition=sl::eFalse;
        options_.numBackBuffers=f.backBuffers; options_.colorWidth=f.width; options_.colorHeight=f.height;
        options_.colorBufferFormat=uint32_t(f.format); options_.mvecDepthWidth=f.inputWidth; options_.mvecDepthHeight=f.inputHeight;
        options_.depthBufferFormat=DXGI_FORMAT_R32_FLOAT; options_.mvecBufferFormat=DXGI_FORMAT_R16G16_FLOAT;
        used_=true;
        if (!Check(setOptions_(viewport_,options_),"DLSS FG options",reason)) return false;
        const auto constants=Constants(f,reset);
        sl::Resource depth{sl::ResourceType::eTex2d,f.depth,nullptr,nullptr,uint32_t(f.depthState)};
        sl::Resource motion{sl::ResourceType::eTex2d,f.motion,nullptr,nullptr,uint32_t(f.motionState)};
        const sl::Extent extent{0,0,f.inputWidth,f.inputHeight};
        const sl::ResourceTag tags[]={ {&depth,sl::kBufferTypeDepth,sl::eValidUntilPresent,&extent},
            {&motion,sl::kBufferTypeMotionVectors,sl::eValidUntilPresent,&extent},
            {nullptr,sl::kBufferTypeHUDLessColor,sl::eValidUntilPresent},
            {nullptr,sl::kBufferTypeUIColorAndAlpha,sl::eValidUntilPresent} };
        return Check(constants_(constants,*token_,viewport_),"SL constants",reason) &&
            Check(tags_(*token_,viewport_,tags,4,nullptr),"SL input tags",reason);
    }
    bool DisableNative(std::string& reason) override {
        if (!sdkInitialized_ || !setOptions_) return true;
        if (token_ && tags_) {
            const sl::ResourceTag tags[]={ {nullptr,sl::kBufferTypeDepth,sl::eValidUntilPresent},
                {nullptr,sl::kBufferTypeMotionVectors,sl::eValidUntilPresent},
                {nullptr,sl::kBufferTypeHUDLessColor,sl::eValidUntilPresent},
                {nullptr,sl::kBufferTypeUIColorAndAlpha,sl::eValidUntilPresent} };
            if (!Check(tags_(*token_,viewport_,tags,4,nullptr),"SL revoke tags",reason)) return false;
        }
        options_.mode=sl::DLSSGMode::eOff;
        if (!Check(setOptions_(viewport_,options_),"DLSS FG off",reason)) return false;
        token_=nullptr;
        return true;
    }
    bool PresentedNative(bool accepted,std::string& reason) override {
        if (!token_) { statistics_.RawPresent(accepted); return true; }
        if (!Mark(sl::PCLMarker::ePresentEnd,reason)) return false;
        sl::DLSSGState s{};
        const bool queried=Check(getState_(viewport_,s,nullptr),"DLSS FG present state",reason);
        const bool healthy=queried && s.status==sl::DLSSGStatus::eOk && creationErrors.load()==errorsSeen_;
        active_=accepted && prepared_ && healthy;
        statistics_.Observe(queried,accepted,active_,s.numFramesActuallyPresented);
        if (!healthy) {
            if (queried) reason="DLSS FG runtime status="+std::to_string(uint32_t(s.status));
            active_=false; history_.Reset(); errorsSeen_=creationErrors.load();
            std::string ignored; if (!DisableNative(ignored)) { reason+="; "+ignored; return false; }
        }
        token_=nullptr; return true;
    }
    bool ShutdownNative(std::string& reason) override {
        swapchain_.Reset();
        // Proxies contain vtables from the interposer and must die before unload.
        proxyFactory_.Reset(); proxyDevice_.Reset();
        if (sdkInitialized_ && !Check(shutdown_(),"slShutdown",reason)) return false;
        sdkInitialized_=false;
        if (module_) { if (!FreeLibrary(module_)) { reason="Streamline unload failed"; return false; } module_=nullptr; }
        return true;
    }
};
}
std::unique_ptr<D3D12Session> CreateDlssD3D12(ID3D12Device* d,IDXGIFactory4* f,const Config& c,
    const std::filesystem::path& path,std::string& reason) {
    auto out=std::make_unique<DlssSession>();
    if (!out->Initialize(d,f,c,path,reason)) return {};
    return out;
}
} // namespace framegen
#endif
