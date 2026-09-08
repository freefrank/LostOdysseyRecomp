// Actual video/window/presentation/Plume lifecycle; no guest or controller/audio
// initialization. The stubs below are deliberately explicit test boundaries.
#include <stdafx.h>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <deque>
#include <gpu/shader/dxc_compiler.h>
#include <settings/config.h>
#include <settings/restart.h>
#include <SDL.h>
#undef main

namespace fixture {
std::mutex mutex;
settings::Config config;
std::deque<std::function<void()>> commands;
std::atomic<bool> externalPump{false};
std::atomic<unsigned> archiveWaits{0}, rendererShutdowns{0};
bool failWindow=false;
bool extraVideoReference=false, extraReferencePreserved=false;
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
SDL_Window* TestCreateWindow(const char* title,int x,int y,int w,int h,Uint32 flags) {
    if(fixture::failWindow) {
        if(fixture::extraVideoReference) fixture::Check(SDL_InitSubSystem(SDL_INIT_VIDEO)==0,"extra SDL reference failed");
        SDL_SetError("fixture-injected SDL_CreateWindow failure"); return nullptr;
    }
    return SDL_CreateWindow(title,x,y,w,h,flags);
}
void TestQuitSubSystem(Uint32 flags) {
    SDL_QuitSubSystem(flags);
    if(fixture::extraVideoReference) {
        fixture::extraReferencePreserved=SDL_WasInit(SDL_INIT_VIDEO)!=0;
        // Test releases its own extra reference on the same owner, separately.
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}
#define SDL_CreateWindow TestCreateWindow
#define SDL_QuitSubSystem TestQuitSubSystem
#include "../../LostOdysseyRecomp/gpu/video.cpp"
#undef SDL_CreateWindow
#undef SDL_QuitSubSystem

namespace settings {
Config GetConfig() { std::lock_guard lock(fixture::mutex); return fixture::config; }
bool DrawMenu(std::vector<uint32_t>&,uint64_t&,uint32_t,uint32_t) { return false; }
void PointerClick(float,float,bool) { throw std::runtime_error("unexpected pointer input"); }
}
namespace hid {
void Init() {} // No real controllers, keyboard state, or SDL joystick thread.
void SetExternalEventPump(bool v) { fixture::externalPump=v; }
void HandleControllerEvent(uint32_t,int32_t) {}
void HandleKeyboardEvent(int32_t,bool) {}
void ClearKeyboardState() {}
}
namespace debug_menu {
void Toggle() { throw std::runtime_error("unexpected debug input"); }
void Update() {
    std::deque<std::function<void()>> work;
    { std::lock_guard lock(fixture::mutex); work.swap(fixture::commands); }
    for(auto& command:work) command();
}
}
Memory::Memory() = default;
Memory g_memory; // Empty; PresentFrontbuffer/guest-memory paths are not invoked.
namespace gpu { bool SetFrameRateTarget(uint32_t) { return true; } }
namespace gpu::renderer {
bool Init() { throw std::runtime_error("LO_NO_RENDERER guard not respected"); }
void Shutdown() { ++fixture::rendererShutdowns; }
void WaitDebugCaptureArchive() {
    ++fixture::archiveWaits;
    std::ofstream("archive-wait-"+std::to_string(GetCurrentProcessId())+".txt") << "production video close/restart reached archive wait stub\n";
}
void SetOutputSize(uint32_t,uint32_t) {}
void ScaleResolvedSize(uint32_t,uint32_t&,uint32_t&) {}
plume::RenderTexture* AcquireResolvedSurface(uint32_t,uint32_t&,uint32_t&,uint32_t&) { return nullptr; }
bool SceneAAApplied(uint32_t) { return false; }
bool ReadbackResolvedSurface(uint32_t,std::vector<uint32_t>&,uint32_t&,uint32_t&) { return false; }
std::vector<uint32_t> GetResolvedAddresses() { return {}; }
void DumpRenderTargets(const char*) {}
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
        Check(!(SDL_GetWindowFlags(video::g_window)&SDL_WINDOW_SHOWN),"SDL window shown");
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
            else fixture::Owner([]{SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);});
            // Only the production window thread's normal close/restart exits successfully.
            std::this_thread::sleep_for(std::chrono::seconds(20));
            throw std::runtime_error("production close/restart did not exit");
        }
        fixture::Record("baseline");
        for(auto backend:{gpu::backend::Backend::D3D12,gpu::backend::Backend::Vulkan}) {
            if(mode=="vulkan-cycles" && backend==gpu::backend::Backend::D3D12) continue;
            fixture::Start(backend); fixture::Resize(); fixture::Stop();
            const auto calls=xenos::GetDxcStatistics().calls;
            fixture::Start(backend);
            fixture::Check(xenos::GetDxcStatistics().calls==calls,"same-process reinit recompiled presentation");
            fixture::Stop();
        }
        std::cout<<"PASS actual hidden video cycles; no guest, audio, draw, present, capture, or input injection"<<std::endl;
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL "<<e.what()<<std::endl;
        gpu::video::Shutdown();
        return 1;
    }
}
