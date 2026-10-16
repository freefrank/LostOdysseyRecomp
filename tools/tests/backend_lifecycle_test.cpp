// Actual video/window/presentation/Plume lifecycle; no guest or controller/audio
// initialization. The stubs below are deliberately explicit test boundaries.
#include <stdafx.h>
#include <condition_variable>
#include <functional>
#include <future>
#include <thread>
#include <iostream>
#include <deque>
#include <filesystem>
#include <optional>
#include <gpu/shader/dxc_compiler.h>
#include <gpu/fsr_upscaler.h>
#include <gpu/xess_upscaler.h>
#include <settings/config.h>
#include <settings/restart.h>
#include <plume_render_interface.h>
#include <SDL3/SDL.h>
#undef main

namespace fixture {
std::mutex mutex;
settings::Config config;
std::deque<std::function<void()>> commands;
std::atomic<bool> externalPump{false};
std::atomic<unsigned> archiveWaits{0}, rendererShutdowns{0};
bool failWindow=false;
bool extraVideoReference=false, extraReferencePreserved=false;
std::unique_ptr<plume::RenderTexture> resolvedOwner;
plume::RenderTexture* resolved=nullptr;
std::unique_ptr<plume::RenderTexture> gainOwner;
plume::RenderTexture* gain=nullptr;
uint32_t resolvedWidth=0, resolvedHeight=0;
bool gameplayResolve=false, gainEnabled=false, settingsOpen=false, brightnessOpen=false, hdrOpen=false, calibrationSceneAvailable=false;
void Check(bool good, const char* message) {
    if (!good) throw std::runtime_error(message);
}
template<class F> auto Owner(F fn) {
    using R = std::invoke_result_t<F>;
    auto task=std::make_shared<std::packaged_task<R()>>(std::move(fn));
    auto result=task->get_future();
    { std::lock_guard lock(mutex); commands.push_back([task]{(*task)();}); }
    Check(result.wait_for(std::chrono::seconds(10))==std::future_status::ready,"window owner command timeout");
    return result.get();
}
}

// Included rather than reimplemented: enables observation of private resources.
SDL_Window* TestCreateWindowWithProperties(SDL_PropertiesID props) {
    if(fixture::failWindow) {
        if(fixture::extraVideoReference) fixture::Check(SDL_InitSubSystem(SDL_INIT_VIDEO),"extra SDL reference failed");
        SDL_SetError("fixture-injected SDL_CreateWindowWithProperties failure"); return nullptr;
    }
    return SDL_CreateWindowWithProperties(props);
}
void TestQuitSubSystem(Uint32 flags) {
    SDL_QuitSubSystem(flags);
    if(fixture::extraVideoReference) {
        fixture::extraReferencePreserved=SDL_WasInit(SDL_INIT_VIDEO)!=0;
        // Test releases its own extra reference on the same owner, separately.
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}
#define SDL_CreateWindowWithProperties TestCreateWindowWithProperties
#define SDL_QuitSubSystem TestQuitSubSystem
#include "../../LostOdysseyRecomp/gpu/video.cpp"
#undef SDL_CreateWindowWithProperties
#undef SDL_QuitSubSystem

namespace settings {
Config GetConfig() { std::lock_guard lock(fixture::mutex); return fixture::config; }
bool DrawMenu(std::vector<uint32_t>& pixels,uint64_t&,uint32_t width,uint32_t height) {
    if (!fixture::settingsOpen) return false;
    pixels.assign(size_t(width)*height,0xFF202020u);
    return true;
}
void PointerClick(float,float,bool) { throw std::runtime_error("unexpected pointer input"); }
void PointerDrag(float,float,bool) { throw std::runtime_error("unexpected pointer input"); }
bool IsOpen() { return fixture::settingsOpen; }
bool CalibrationKey(uint32_t) { return false; }
bool CaptureKey(uint32_t,bool) { return false; }
HdrCalibration GetHdrCalibration() {
    HdrCalibration result;
    result.open=fixture::hdrOpen;
    result.sceneAvailable=fixture::calibrationSceneAvailable;
    return result;
}
BrightnessCalibration GetBrightnessCalibration() {
    BrightnessCalibration result;
    result.open=fixture::brightnessOpen;
    result.sceneAvailable=fixture::calibrationSceneAvailable;
    return result;
}
bool SaveDisplayChoice(const std::string&,uint32_t) { return false; }
bool SaveGraphicsBackend(GraphicsBackend) { return false; }
namespace title_entry {
bool HintShown() { return false; }
float HintOpacity() { return 0.0f; }
const Hint* DrawHint(uint32_t,uint32_t) { return nullptr; }
}
}
namespace hid {
void Init() {} // No real controllers, keyboard state, or SDL joystick thread.
void SetExternalEventPump(bool v) { fixture::externalPump=v; }
void HandleControllerEvent(uint32_t,uint32_t) {}
void HandleKeyboardEvent(int32_t,bool) {}
void ClearKeyboardState() {}
void RefreshKeyNames() {}
void PumpHostInput() {}
}
namespace debug_menu {
void Toggle() { throw std::runtime_error("unexpected debug input"); }
void HandleInput(InputAction) { throw std::runtime_error("unexpected debug input"); }
bool IsOverlayVisible() { return false; }
void RenderOverlay(host_ui::Rasterizer&) {}
void Update() {
    std::deque<std::function<void()>> work;
    { std::lock_guard lock(fixture::mutex); work.swap(fixture::commands); }
    for(auto& command:work) command();
}
}
Memory::Memory() = default;
Memory g_memory; // Empty; PresentFrontbuffer/guest-memory paths are not invoked.
namespace gpu {
bool SetFrameRateTarget(uint32_t) { return true; }
uint32_t GetFrameRateTarget() { return 30; }
CommandProcessor g_commandProcessor;
// Production finishes a requested exit on the GPU owner thread once the
// command processor stops; a detached thread stands in for that owner.
void CommandProcessor::RequestStopForExit() {
    static std::atomic<bool> once{false};
    if(!once.exchange(true)) std::thread([]{ video::FinishRequestedExit(); }).detach();
}
}
namespace gpu::renderer {
bool Init() { throw std::runtime_error("LO_NO_RENDERER guard not respected"); }
void Shutdown() { ++fixture::rendererShutdowns; }
void WaitDebugCaptureArchive() {
    ++fixture::archiveWaits;
    std::ofstream("archive-wait-"+std::to_string(GetCurrentProcessId())+".txt") << "production video close/restart reached archive wait stub\n";
}
void SetOutputSize(uint32_t,uint32_t) {}
void ScaleResolvedSize(uint32_t,uint32_t& width,uint32_t& height) { width=fixture::resolvedWidth; height=fixture::resolvedHeight; }
plume::RenderTexture* AcquireResolvedSurface(uint32_t,uint32_t& width,uint32_t& height,uint32_t& format,
    frame_plan::FramePlan* plan, frame_generation::ResolvedHandoff* handoff) {
    if (handoff) *handoff={};
    if (plan) { plan->cpuSerial=1; plan->effectiveAA=1; plan->scalingQuality=0; }
    width=fixture::resolvedWidth; height=fixture::resolvedHeight;
    format=uint32_t(plume::RenderFormat::R8G8B8A8_UNORM);
    return fixture::resolved;
}
void CancelFgHandoffs() {}
bool SuppressPresent() { return false; }
bool DrainForFrameGenerationReconfigure() { return true; }
void SetHdrSceneEnabled(bool) {}
plume::RenderTexture* AcquireHdrResolvedSurface(uint32_t,uint32_t&,uint32_t&) { return nullptr; }
plume::RenderTexture* AcquireHdrGainSurface(uint32_t,uint32_t& width,uint32_t& height) {
    if (!fixture::gainEnabled || !fixture::gameplayResolve) return nullptr;
    width=fixture::resolvedWidth; height=fixture::resolvedHeight;
    return fixture::gain;
}
bool ResolvedScene(uint32_t,uint64_t& ordinal) {
    ordinal=fixture::gameplayResolve ? 1 : 2;
    return fixture::gameplayResolve;
}
plume::RenderTexture* AcquireResolvedWrite(uint32_t,uint64_t ordinal,bool hdr,
    uint32_t& width,uint32_t& height,uint32_t& format) {
    if (hdr || !fixture::gameplayResolve || ordinal!=1) return nullptr;
    width=fixture::resolvedWidth; height=fixture::resolvedHeight;
    format=uint32_t(plume::RenderFormat::R8G8B8A8_UNORM);
    return fixture::resolved;
}
bool SceneAAApplied(uint32_t) { return false; }
bool ReadbackResolvedSurface(uint32_t,std::vector<uint32_t>&,uint32_t&,uint32_t&) { return false; }
std::vector<uint32_t> GetResolvedAddresses() { return {}; }
void DumpRenderTargets(const char*) {}
}
namespace gpu::dred {
void ConfigureBeforeDeviceCreation(bool) {}
bool Enabled() { return false; }
void LogRemovedDevice(ID3D12Device*) {}
}
namespace modding::text_overlay {
void StopPrebuild() {}
}
namespace ui::achievement_overlay {
const Image* Update(uint32_t,uint32_t) { return nullptr; }
}
namespace ui::performance_overlay {
bool Frame(uint32_t,std::chrono::steady_clock::time_point,uint32_t,uint32_t) { return false; }
void Refresh(const Sources&) {}
const Image& Current() { static const Image empty; return empty; }
void Stop() {}
}
// Upscaler, DLSS probe, and sizing paths are reached through video init/reset
// but never configured here; the stubs mirror present_capture_test.cpp.
namespace settings {
void SetHdrDisplayInfo(HdrDisplayInfo) {}
void SetHdrCalibrationSceneAvailable(bool available) { fixture::calibrationSceneAvailable=available; }
void SetNeuralRenderingPreviewAvailable(bool) {}
NeuralRenderingTuning GetNeuralRenderingTuning() { return {}; }
}
namespace gpu {
TemporalUpscaler::TemporalUpscaler(dlss::Controller& controller) : dlss_(&controller) {}
TemporalUpscaler::~TemporalUpscaler() { fsr_.release(); xess_.release(); } // No SDK adapter is ever constructed.
void TemporalUpscaler::ReleaseCompleted(uint64_t) {}
void TemporalUpscaler::AbandonAfterDeviceLoss() {}
void TemporalUpscaler::ShutdownAfterGpuDrain() {}
bool TemporalUpscaler::ShutdownComplete() const { return true; }
upscaling::OutputSizing TemporalUpscaler::QuerySizing(const plume::D3D12Device&, const upscaling::SizingKey&) { return {}; }
}
namespace gpu::optiscaler {
const LoadResult& Initialize(bool) { static const LoadResult result; return result; }
}
namespace gpu::upscaling {
void PublishDeviceCapability(BackendDeviceSnapshot) {}
BackendDeviceSnapshot PublishedDeviceCapability() { return {}; }
OutputSizing SizingService::QueryOutputSizing(TemporalUpscaler&, const plume::VulkanInterface&, const plume::VulkanDevice&, const SizingKey&) { return {}; }
}
namespace gpu::frame_plan {
void NoteCurrentDlssStatus() {}
void ResetSizing(uint64_t) {}
void PublishSizing(upscaling::OutputSizing) {}
upscaling::OutputRegion PlannedOutputRegion(resolution::Size drawable, const settings::Config&) { return upscaling::ResolveOutputRegion(drawable); }
std::optional<upscaling::SizingKey> TakeSizingRequest() { return {}; }
}
namespace gpu::dlss {
Controller::Controller(std::filesystem::path, std::filesystem::path) {}
plume::VulkanExtensionHooks Controller::ExtensionHooks() { return {}; }
void Controller::ProbeOnce(const plume::VulkanInterface&, const plume::VulkanDevice&, bool) {}
void Controller::ProbeOnce(const plume::D3D12Device&) {}
void Controller::ReleaseCompletedThrough(uint64_t) {}
void Controller::AbandonUsesAfterDeviceLoss() {}
void Controller::ShutdownAfterGpuDrain() {}
const char* ProbeStateName(ProbeState) { return "stub"; }
void Controller::ReleaseNeuralRenderingPreview() {}
plume::RenderTexture* Controller::RecordNeuralRenderingPreview(plume::RenderCommandList&,
    uint32_t,uint32_t,const NeuralRenderingTuning&) { return nullptr; }
void SetLogSink(LogSink) {}
void SetComputeShaderCompiler(ComputeShaderCompiler) {}
}

namespace fixture {
using namespace gpu;
void Record(const char* phase) {
    DWORD handles=0; GetProcessHandleCount(GetCurrentProcess(),&handles);
    auto stats=xenos::GetDxcStatistics();
    std::cout << "STATE phase="<<phase<<" pid="<<GetCurrentProcessId()<<" handles="<<handles
        <<" user="<<GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS)<<" gdi="<<GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)
        <<" sdl_video="<<SDL_WasInit(SDL_INIT_VIDEO)<<" pump="<<externalPump.load()
        <<" dxc_calls="<<stats.calls<<" dxc_ok="<<stats.succeeded<<" dxc_fail="<<stats.rejected+stats.infrastructureFailed<<std::endl;
}
void Inspect(const char* phase) {
    Owner([=] {
        Check(video::g_window && video::g_nativeWindow,"missing actual window");
        Check((SDL_GetWindowFlags(video::g_window)&SDL_WINDOW_HIDDEN),"SDL window shown");
        Check(!IsWindowVisible(video::g_nativeWindow),"native window visible");
        Check(GetForegroundWindow()!=video::g_nativeWindow,"fixture took foreground");
        Check(GetWindowThreadProcessId(video::g_nativeWindow,nullptr)==GetCurrentThreadId(),"wrong window owner");
        RECT client{}; GetClientRect(video::g_nativeWindow,&client);
        std::cout<<"WINDOW phase="<<phase<<" owner="<<GetCurrentThreadId()<<" hwnd="<<uintptr_t(video::g_nativeWindow)
            <<" hidden=1 client="<<client.right<<"x"<<client.bottom<<std::endl;
    });
    Check(video::GetDevice() && video::GetQueue() && video::g_presentation,"real video resources not initialized");
    Check(video::g_swapChain && !video::g_swapChain->isEmpty(),"empty swapchain");
    std::cout<<"SWAPCHAIN phase="<<phase<<" backend="<<backend::Name(*video::SelectedBackend())
        <<" size="<<video::g_swapChain->getWidth()<<"x"<<video::g_swapChain->getHeight()<<std::endl;
}
void Start(backend::Backend b) {
    { std::lock_guard lock(mutex); config.graphicsBackend=b; config.width=640;config.height=360;
      config.windowMode=settings::WindowMode::Windowed; }
    _putenv_s("LO_GRAPHICS_API",b==backend::Backend::Vulkan?"vulkan":"d3d12");
    Check(video::Init(),"video Init failed");
    Check(video::SelectedBackend()==b,"unexpected fallback");
    Check(externalPump,"HID external pump wasn't enabled");
    Inspect("init"); Record("init");
}
void CheckCalibrationCopyRelease() {
    // A failed first menu submit can leave an allocated copy without an
    // accepted snapshot. The next prepared frame must release it after close.
    auto& cache=video::g_hdrCalibrationCache;
    cache.copy=video::GetDevice()->createTexture(plume::RenderTextureDesc::Texture2D(8,8,1,
        plume::RenderFormat::R16G16B16A16_FLOAT));
    Check(bool(cache.copy),"calibration test texture allocation failed");
    cache.attempted=false;
    uint64_t ticket=0; uint32_t width=0,height=0;
    Check(video::PreparePresentation(ticket,width,height),"calibration cleanup preparation failed");
    Check(!cache.copy&&!cache.scene&&!cache.ready&&!cache.attempted,
        "unsubmitted calibration copy survived menu close");
}
void FillResolved(uint32_t color) {
    const uint32_t width=resolvedWidth,height=resolvedHeight;
    const uint32_t pitch=(width+63u)&~63u;
    auto upload=video::GetDevice()->createBuffer(plume::RenderBufferDesc::UploadBuffer(uint64_t(pitch)*height*4));
    Check(bool(upload),"resolved frame upload allocation failed");
    auto* pixels=static_cast<uint32_t*>(upload->map());
    Check(pixels!=nullptr,"resolved frame upload map failed");
    for(uint32_t y=0;y<height;++y)
        for(uint32_t x=0;x<width;++x) pixels[size_t(y)*pitch+x]=color;
    upload->unmap();
    auto commands=video::GetQueue()->createCommandList();
    auto fence=video::GetDevice()->createCommandFence();
    Check(bool(commands)&&bool(fence),"resolved frame command allocation failed");
    commands->begin();
    commands->barriers(plume::RenderBarrierStage::COPY,
        plume::RenderTextureBarrier(resolved,plume::RenderTextureLayout::COPY_DEST));
    commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(resolved),
        plume::RenderTextureCopyLocation::PlacedFootprint(upload.get(),plume::RenderFormat::R8G8B8A8_UNORM,
            width,height,1,pitch));
    commands->end();
    const plume::RenderCommandList* lists[]{commands.get()};
    video::GetQueue()->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get());
    video::GetQueue()->waitForCommandFence(fence.get());
}
void FillGain() {
    const uint32_t width=resolvedWidth,height=resolvedHeight;
    const uint32_t pitch=(width+31u)&~31u;
    auto upload=video::GetDevice()->createBuffer(plume::RenderBufferDesc::UploadBuffer(uint64_t(pitch)*height*8));
    Check(bool(upload),"HDR gain upload allocation failed");
    auto* halves=static_cast<uint16_t*>(upload->map());
    Check(halves!=nullptr,"HDR gain upload map failed");
    for(uint32_t y=0;y<height;++y)
        for(uint32_t x=0;x<width;++x) {
            auto* pixel=halves+(size_t(y)*pitch+x)*4;
            pixel[0]=pixel[1]=pixel[2]=0x4000; // binary16 2.0
            pixel[3]=0x3C00;
        }
    upload->unmap();
    auto commands=video::GetQueue()->createCommandList();
    auto fence=video::GetDevice()->createCommandFence();
    Check(bool(commands)&&bool(fence),"HDR gain command allocation failed");
    commands->begin();
    commands->barriers(plume::RenderBarrierStage::COPY,
        plume::RenderTextureBarrier(gain,plume::RenderTextureLayout::COPY_DEST));
    commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(gain),
        plume::RenderTextureCopyLocation::PlacedFootprint(upload.get(),plume::RenderFormat::R16G16B16A16_FLOAT,
            width,height,1,pitch));
    commands->end();
    const plume::RenderCommandList* lists[]{commands.get()};
    video::GetQueue()->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get());
    video::GetQueue()->waitForCommandFence(fence.get());
}
uint32_t ReadCalibrationPixel() {
    const auto& cache=video::g_hdrCalibrationCache;
    Check(cache.scene&&cache.width&&cache.height,"calibration scene is empty");
    const uint32_t pitch=(cache.width+63u)&~63u;
    auto readback=video::GetDevice()->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(
        uint64_t(pitch)*cache.height*4));
    auto commands=video::GetQueue()->createCommandList();
    auto fence=video::GetDevice()->createCommandFence();
    Check(bool(readback)&&bool(commands)&&bool(fence),"calibration readback allocation failed");
    commands->begin();
    commands->barriers(plume::RenderBarrierStage::COPY,
        plume::RenderTextureBarrier(cache.scene,plume::RenderTextureLayout::COPY_SOURCE));
    commands->copyTextureRegion(plume::RenderTextureCopyLocation::PlacedFootprint(readback.get(),
        plume::RenderFormat::R8G8B8A8_UNORM,cache.width,cache.height,1,pitch),
        plume::RenderTextureCopyLocation::Subresource(cache.scene));
    commands->end();
    const plume::RenderCommandList* lists[]{commands.get()};
    video::GetQueue()->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get());
    video::GetQueue()->waitForCommandFence(fence.get());
    const auto* pixels=static_cast<const uint32_t*>(readback->map());
    Check(pixels!=nullptr,"calibration readback map failed");
    const uint32_t center=pixels[size_t(cache.height/2)*pitch+cache.width/2];
    readback->unmap();
    return center;
}
uint16_t ReadCalibrationRedHalf() {
    const auto& cache=video::g_hdrCalibrationCache;
    Check(cache.scene&&cache.width&&cache.height&&cache.extended,"HDR calibration scene is empty");
    const uint32_t pitch=(cache.width+31u)&~31u;
    auto readback=video::GetDevice()->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(
        uint64_t(pitch)*cache.height*8));
    auto commands=video::GetQueue()->createCommandList();
    auto fence=video::GetDevice()->createCommandFence();
    Check(bool(readback)&&bool(commands)&&bool(fence),"HDR calibration readback allocation failed");
    commands->begin();
    commands->barriers(plume::RenderBarrierStage::COPY,
        plume::RenderTextureBarrier(cache.scene,plume::RenderTextureLayout::COPY_SOURCE));
    commands->copyTextureRegion(plume::RenderTextureCopyLocation::PlacedFootprint(readback.get(),
        plume::RenderFormat::R16G16B16A16_FLOAT,cache.width,cache.height,1,pitch),
        plume::RenderTextureCopyLocation::Subresource(cache.scene));
    commands->end();
    const plume::RenderCommandList* lists[]{commands.get()};
    video::GetQueue()->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get());
    video::GetQueue()->waitForCommandFence(fence.get());
    const auto* halves=static_cast<const uint16_t*>(readback->map());
    Check(halves!=nullptr,"HDR calibration readback map failed");
    const uint16_t red=halves[(size_t(cache.height/2)*pitch+cache.width/2)*4];
    readback->unmap();
    return red;
}
void CheckCalibrationSceneOnSettingsOpen(bool hdrGain) {
    // Native menu activation arrives before its blur draw. The next blurred
    // resolve may still report gameplay depth draws, and subsequent menu
    // resolves replace the clean write before host Settings opens.
    resolvedWidth=64; resolvedHeight=64;
    resolvedOwner=video::GetDevice()->createTexture(plume::RenderTextureDesc::Texture2D(
        resolvedWidth,resolvedHeight,1,plume::RenderFormat::R8G8B8A8_UNORM));
    resolved=resolvedOwner.get();
    Check(resolved!=nullptr,"resolved frame allocation failed");
    gainEnabled=hdrGain;
    if(hdrGain) {
        gainOwner=video::GetDevice()->createTexture(plume::RenderTextureDesc::Texture2D(
            resolvedWidth,resolvedHeight,1,plume::RenderFormat::R16G16B16A16_FLOAT));
        gain=gainOwner.get();
        Check(gain!=nullptr,"HDR gain surface allocation failed");
        FillGain();
    }
    gameplayResolve=true;
    FillResolved(hdrGain?0xFFFFFFFFu:0xFF204080u);
    video::PresentFrontbuffer(0x1000,resolvedWidth,resolvedHeight,0,nullptr,nullptr);
    Check(!video::g_hdrCalibrationCache.copy&&!calibrationSceneAvailable,
        "ordinary gameplay allocated a calibration scene");
    video::PresentFrontbuffer(0x1000,resolvedWidth,resolvedHeight,0,nullptr,nullptr);
    Check(!video::g_hdrCalibrationCache.copy&&!calibrationSceneAvailable,
        "repeated ordinary gameplay allocated a calibration scene");

    Check(video::WaitForPresentGpu(),"gameplay present did not complete");
    video::SetCalibrationGameMenuOpen(true);
    video::CaptureCalibrationSceneBeforeDraw();
    // This frame still meets the renderer's scene-draw threshold, but has
    // already received the game's own blur. It must not replace the preview.
    FillResolved(0xFF202020u);
    video::PresentFrontbuffer(0x1000,resolvedWidth,resolvedHeight,0,nullptr,nullptr);
    Check(video::WaitForPresentGpu(),"blurred menu present did not complete");
    gameplayResolve=false;
    video::PresentFrontbuffer(0x1000,resolvedWidth,resolvedHeight,0,nullptr,nullptr);
    settingsOpen=true;
    brightnessOpen=true;
    video::PresentFrontbuffer(0x1000,resolvedWidth,resolvedHeight,0,nullptr,nullptr);
    const bool brightnessAvailable=calibrationSceneAvailable&&video::g_hdrCalibrationCache.ready&&
        video::g_hdrCalibrationCache.scene!=nullptr;
    uint32_t brightnessPixel=0,hdrPixel=0,reopenedPixel=0;
    const auto readFrozen=[&]{ return hdrGain?uint32_t(ReadCalibrationRedHalf()):ReadCalibrationPixel(); };
    if(brightnessAvailable) {
        Check(video::WaitForPresentGpu(),"calibration present did not complete");
        brightnessPixel=readFrozen();
    }
    brightnessOpen=false;
    hdrOpen=true;
    video::PresentFrontbuffer(0x1000,resolvedWidth,resolvedHeight,0,nullptr,nullptr);
    const bool hdrAvailable=calibrationSceneAvailable&&video::g_hdrCalibrationCache.ready;
    if(hdrAvailable) {
        Check(video::WaitForPresentGpu(),"HDR calibration present did not complete");
        hdrPixel=readFrozen();
    }
    settingsOpen=false;
    hdrOpen=false;
    uint64_t ticket=0; uint32_t width=0,height=0;
    Check(video::PreparePresentation(ticket,width,height),"settings close preparation failed");
    const bool retainedAcrossSettings=calibrationSceneAvailable&&video::g_hdrCalibrationCache.ready;
    settingsOpen=true;
    brightnessOpen=true;
    video::PresentFrontbuffer(0x1000,resolvedWidth,resolvedHeight,0,nullptr,nullptr);
    const bool reopenedAvailable=calibrationSceneAvailable&&video::g_hdrCalibrationCache.ready;
    if(reopenedAvailable) {
        Check(video::WaitForPresentGpu(),"reopened calibration present did not complete");
        reopenedPixel=readFrozen();
    }
    settingsOpen=false;
    brightnessOpen=false;
    video::SetCalibrationGameMenuOpen(false);
    Check(video::PreparePresentation(ticket,width,height),"native menu close preparation failed");
    Check(video::WaitForPresentGpu(),"native menu close did not complete");
    const bool releasedAfterMenu=!video::g_hdrCalibrationCache.copy&&!calibrationSceneAvailable;
    resolvedOwner.reset(); resolved=nullptr;
    gainOwner.reset(); gain=nullptr; gainEnabled=false;
    resolvedWidth=resolvedHeight=0;
    Check(brightnessAvailable,"settings calibration did not retain the last clean gameplay scene after menu overwrite");
    Check(hdrGain ? brightnessPixel>=0x3E00u :
          (brightnessPixel&0xFFu)>0x50u && ((brightnessPixel>>8)&0xFFu)>0x30u,
        "settings calibration retained the blurred menu frame instead of gameplay");
    Check(hdrAvailable&&hdrPixel==brightnessPixel,"HDR page lost the frozen gameplay scene");
    Check(retainedAcrossSettings&&reopenedAvailable&&reopenedPixel==brightnessPixel,
        "reopening Settings over the same game menu lost the frozen scene");
    Check(releasedAfterMenu,"calibration scene survived the native menu closing");
}
void Resize() {
    // Config is the production window-owner pathway; do not call SDL from the render thread.
    { std::lock_guard lock(mutex); config.width=800;config.height=450; }
    bool reached=false;
    for(unsigned n=0;n<100&&!reached;++n) {
        reached=Owner([]{int w=0,h=0;SDL_GetWindowSize(video::g_window,&w,&h);return w==800&&h==450;});
        if(!reached) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Check(reached,"owner thread did not apply settings resize");
    Check(video::g_swapChain->resize(),"native swapchain resize failed");
    Check(video::g_swapChain->getWidth()==800&&video::g_swapChain->getHeight()==450,"wrong swapchain extent after resize");
    Inspect("resized");
}
void Stop() {
    const HWND previous=video::g_nativeWindow;
    video::Shutdown();
    Check(!IsWindow(previous),"window survived Shutdown");
    Check(!video::g_windowThread.joinable()&&!video::g_window&&!video::g_nativeWindow&&!video::g_preparationWindow,"window state survived Shutdown");
    Check(!video::g_device&&!video::g_interface&&!video::g_queue&&!video::g_commandList&&!video::g_swapChain&&!video::g_presentation&&!video::g_uploadBuffer,"GPU ownership survived Shutdown");
    Check(!video::GetDevice()&&!video::GetQueue()&&!video::SelectedBackend()&&!externalPump&&!video::g_initAttempted,"published state survived Shutdown");
    Record("shutdown");
    Check(SDL_WasInit(SDL_INIT_VIDEO)==0,"SDL video subsystem retained after Shutdown (InitSubSystem reference not released)");
}
}

int main(int argc,char** argv) {
    const auto handshake=settings::restart::WaitForParentIfRestartChild();
    if(handshake==settings::restart::ChildHandshake::Invalid)return 2;
    try {
        _putenv_s("LO_BACKGROUND","1"); _putenv_s("LO_NO_RENDERER","1"); _putenv_s("LO_HEADLESS","");
        // Driver sets an isolated absolute cache path before process start.
        fixture::Check(getenv("LO_SHADER_CACHE_DIR")!=nullptr,"isolated cache path required");
        setvbuf(stdout,nullptr,_IONBF,0);
        const std::string mode=argc>1?argv[1]:"cycles";
        if(mode=="window-failure"||mode=="window-failure-external") {
            fixture::failWindow=true;
            fixture::extraVideoReference=mode=="window-failure-external";
            fixture::Check(!gpu::video::Init(),"injected window creation unexpectedly succeeded");
            fixture::Check(SDL_WasInit(SDL_INIT_VIDEO)==0&&!gpu::video::g_windowThread.joinable()&&!fixture::externalPump,"failed window creation retained resources");
            fixture::Record("window-failure-cleaned");
            fixture::Check(xenos::GetDxcStatistics().calls==0,"failed window path reached shader compile");
            fixture::Check(!fixture::extraVideoReference||fixture::extraReferencePreserved,"video cleanup released another client's reference");
            gpu::video::Shutdown();
            return 0;
        }
        if(handshake==settings::restart::ChildHandshake::Waited) {
            freopen("child-runtime.log","w",stdout);freopen("child-errors.log","w",stderr);
            std::ofstream("restart-child-pid.txt")<<GetCurrentProcessId();
            fixture::Start(gpu::backend::Backend::Vulkan);
            fixture::Check(xenos::GetDxcStatistics().calls==0,"restart child recompiled warm presentation cache");
            fixture::Stop();
            std::ofstream("restart-child-done.txt")<<"normal main return 0 pid="<<GetCurrentProcessId();
            return 0;
        }
        if(mode=="restart"||mode=="close") {
            fixture::Start(gpu::backend::Backend::Vulkan);
            fixture::Check(xenos::GetDxcStatistics().calls==0,"close/restart source recompiled warm presentation cache");
            std::ofstream("source-pid.txt")<<GetCurrentProcessId();
            if(mode=="restart") settings::restart::Request();
            else fixture::Owner([]{SDL_Event event{};event.type=SDL_EVENT_QUIT;SDL_PushEvent(&event);});
            // Only the production close/restart path, finished on the GPU owner, exits successfully.
            std::this_thread::sleep_for(std::chrono::seconds(20));
            throw std::runtime_error("production close/restart did not exit");
        }
        fixture::Record("baseline");
        for(auto backend:{gpu::backend::Backend::D3D12,gpu::backend::Backend::Vulkan}) {
            if(mode=="vulkan-cycles" && backend==gpu::backend::Backend::D3D12) continue;
            fixture::Start(backend); fixture::CheckCalibrationCopyRelease();
            fixture::CheckCalibrationSceneOnSettingsOpen(false);
            fixture::CheckCalibrationSceneOnSettingsOpen(true);
            fixture::Resize(); fixture::Stop();
            const auto calls=xenos::GetDxcStatistics().calls;
            fixture::Start(backend);
            fixture::Check(xenos::GetDxcStatistics().calls==calls,"same-process reinit recompiled presentation");
            fixture::Stop();
        }
        std::cout<<"PASS hidden video cycles and synthetic calibration presentation; no guest, audio, or input injection"<<std::endl;
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL "<<e.what()<<std::endl;
        gpu::video::Shutdown();
        return 1;
    }
}
