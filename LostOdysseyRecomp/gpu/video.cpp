#include <version.h>
#include <stdafx.h>
#include "video.h"
#include "renderer.h"
#include "presentation.h"
#include "command_processor.h"
#include <settings/config.h>
#include <settings/menu.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <hid/hid.h>
#include <debug/battle_menu.h>

#include <SDL.h>
#include <SDL_syswm.h>

#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#ifdef _WIN32
#include <plume_d3d12.h>
#endif
#endif

#include <vector>
#include <future>
#include <thread>
#include <atomic>

#ifdef LO_GPU_PLUME
namespace plume
{
    // Defined in plume_d3d12.cpp / plume_vulkan.cpp but not exported by a header.
    std::unique_ptr<RenderInterface> CreateD3D12Interface();
    std::unique_ptr<RenderInterface> CreateVulkanInterface();
}
#endif

namespace gpu::video
{
    namespace
    {
        constexpr uint32_t kMaxWidth = 1920;
        constexpr uint32_t kMaxHeight = 1080;

        SDL_Window* g_window = nullptr;
        std::atomic<uint64_t> g_shaderProgress{0};
        constexpr uint64_t kProgressMask = (1ull << 28) - 1;
        const wchar_t* PreparationTitle(PreparationStage stage) {
            switch (stage) {
            case PreparationStage::CacheValidation: return L"Validating shader cache";
            case PreparationStage::IndexedExtraction: return L"Extracting indexed shaders";
            case PreparationStage::FallbackScan: return L"Scanning game resources";
            case PreparationStage::Pipelines: return L"Preparing pipelines";
            default: return L"Compiling shaders";
            }
        }
        const char* PreparationTitleNarrow(PreparationStage stage) {
            switch (stage) {
            case PreparationStage::CacheValidation: return "Validating shader cache";
            case PreparationStage::IndexedExtraction: return "Extracting indexed shaders";
            case PreparationStage::FallbackScan: return "Scanning game resources";
            case PreparationStage::Pipelines: return "Preparing pipelines";
            default: return "Compiling shaders";
            }
        }
        const wchar_t* PreparationSuffix(PreparationUnit unit) {
            switch (unit) {
            case PreparationUnit::Files: return L" files";
            case PreparationUnit::MiB: return L" MiB";
            case PreparationUnit::Entries: return L" entries";
            case PreparationUnit::Pipelines: return L" pipelines";
            default: return L" shaders";
            }
        }
        const char* PreparationSuffixNarrow(PreparationUnit unit) {
            switch (unit) {
            case PreparationUnit::Files: return " files";
            case PreparationUnit::MiB: return " MiB";
            case PreparationUnit::Entries: return " entries";
            case PreparationUnit::Pipelines: return " pipelines";
            default: return " shaders";
            }
        }
        std::atomic<int> g_displayMode{-1};
        std::atomic<uint64_t> g_displaySize{0};
        std::atomic<bool> g_displayFailed{false},g_reapplyWindow{false};
        std::vector<uint32_t> g_menuPixels;
        uint64_t g_menuRevision=0;
#ifdef _WIN32
        std::jthread g_windowThread;
        HWND g_nativeWindow = nullptr;
        HWND g_preparationWindow = nullptr;
        LRESULT CALLBACK PreparationWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            if (message == WM_ERASEBKGND) return 1;
            if (message == WM_PAINT || message == WM_PRINTCLIENT) {
                PAINTSTRUCT paint{};
                HDC target=message == WM_PRINTCLIENT ? reinterpret_cast<HDC>(wparam) : BeginPaint(window,&paint);
                RECT bounds{}; GetClientRect(window,&bounds);
                // Compose the complete frame offscreen. Clearing the visible DC
                // first exposes a blank text area between GDI drawing batches.
                HDC dc=CreateCompatibleDC(target);
                HBITMAP bitmap=CreateCompatibleBitmap(target,std::max(1L,bounds.right),std::max(1L,bounds.bottom));
                if (!dc || !bitmap) {
                    if (bitmap) DeleteObject(bitmap);
                    if (dc) DeleteDC(dc);
                    if (message == WM_PAINT) EndPaint(window,&paint);
                    return 0;
                }
                auto oldBitmap=SelectObject(dc,bitmap);
                HBRUSH background=CreateSolidBrush(RGB(20,24,31));
                FillRect(dc,&bounds,background); DeleteObject(background);
                const uint64_t state=g_shaderProgress.load();
                const uint32_t total=uint32_t((state>>28)&kProgressMask), done=uint32_t(state&kProgressMask);
                const auto stage=PreparationStage((state>>56)&15);
                const auto unit=PreparationUnit(state>>60);
                SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(235,238,242));
                HFONT font=CreateFontW(-28,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
                auto old=SelectObject(dc,font);
                RECT title{20,bounds.bottom/2-85,bounds.right-20,bounds.bottom/2-35};
                DrawTextW(dc,PreparationTitle(stage),-1,&title,DT_CENTER|DT_SINGLELINE);
                SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));
                const auto detail=std::to_wstring(done)+L" / "+std::to_wstring(total)+PreparationSuffix(unit);
                RECT count{20,bounds.bottom/2-35,bounds.right-20,bounds.bottom/2};
                DrawTextW(dc,detail.c_str(),-1,&count,DT_CENTER|DT_SINGLELINE);
                const int width=std::min(480,std::max(0,int(bounds.right)-80));
                RECT bar{(bounds.right-width)/2,bounds.bottom/2+8,(bounds.right+width)/2,bounds.bottom/2+14};
                HBRUSH track=CreateSolidBrush(RGB(51,58,70)); FillRect(dc,&bar,track); DeleteObject(track);
                bar.right=bar.left+int(total ? uint64_t(width)*std::min(done,total)/total : 0);
                HBRUSH fill=CreateSolidBrush(RGB(111,177,218)); FillRect(dc,&bar,fill); DeleteObject(fill);
                SetTextColor(dc,RGB(157,168,184));
                RECT hint{20,bounds.bottom/2+45,bounds.right-20,bounds.bottom/2+95};
                DrawTextW(dc,L"The game will continue automatically.\nFuture launches reuse the shader cache.",-1,&hint,DT_CENTER);
                SelectObject(dc,old); DeleteObject(font);
                BitBlt(target,0,0,bounds.right,bounds.bottom,dc,0,0,SRCCOPY);
                SelectObject(dc,oldBitmap); DeleteObject(bitmap); DeleteDC(dc);
                if (message == WM_PAINT) EndPaint(window,&paint);
                return 0;
            }
            return DefWindowProcW(window,message,wparam,lparam);
        }
#endif
        void PumpWindowEvents();
        bool g_initAttempted = false;
        bool g_available = false;

        std::vector<uint32_t> g_pixels;   // last untiled frame, R8G8B8A8
        uint32_t g_frameWidth = 0, g_frameHeight = 0;
        bool g_frameOnGpu = false;        // last frame came straight from a resolved surface
        uint32_t g_frontbufferPhysical = 0;

#ifdef LO_GPU_PLUME
        std::unique_ptr<plume::RenderInterface> g_interface;
        std::unique_ptr<plume::RenderDevice> g_device;
        std::unique_ptr<plume::RenderCommandQueue> g_queue;
        std::unique_ptr<plume::RenderCommandList> g_commandList;
        std::unique_ptr<plume::RenderCommandFence> g_fence;
        std::unique_ptr<plume::RenderCommandSemaphore> g_acquireSemaphore;
        std::unique_ptr<plume::RenderCommandSemaphore> g_releaseSemaphore;
        std::unique_ptr<plume::RenderSwapChain> g_swapChain;
        std::unique_ptr<plume::RenderBuffer> g_uploadBuffer;
        uint64_t g_uploadCapacity = uint64_t(kMaxWidth) * kMaxHeight * 4;
        std::unique_ptr<Presentation> g_presentation;
        std::unique_ptr<plume::RenderTexture> g_cpuFrame;
        uint32_t g_cpuWidth=0,g_cpuHeight=0;
        uint32_t g_lastPresentedImage=0;
        bool g_hasPresentedImage=false;
        bool g_forceSwapResize=false;
        constexpr plume::RenderFormat kSwapChainFormat = plume::RenderFormat::R8G8B8A8_UNORM;
        constexpr uint32_t kSwapChainBuffers = 3;
#endif

        uint32_t GpuSwap(uint32_t value, uint32_t endian)
        {
            switch (endian & 3)
            {
            case 1: return ((value & 0xFF00FF00u) >> 8) | ((value & 0x00FF00FFu) << 8);
            case 2: return ByteSwap(value);
            case 3: return (value >> 16) | (value << 16);
            default: return value;
            }
        }
    }

    plume::RenderDevice* GetDevice()
    {
#ifdef LO_GPU_PLUME
        return g_available ? g_device.get() : nullptr;
#else
        return nullptr;
#endif
    }

    plume::RenderCommandQueue* GetQueue()
    {
#ifdef LO_GPU_PLUME
        return g_available ? g_queue.get() : nullptr;
#else
        return nullptr;
#endif
    }

    uint32_t TiledOffset2D(uint32_t x, uint32_t y, uint32_t pitchBlocks, uint32_t bytesPerBlockLog2)
    {
        // Macro tiles are 32x32 blocks; the pitch is given in blocks.
        const uint32_t pitchMacroTiles = pitchBlocks >> 5;
        const uint32_t outerBlocks = (((y >> 5) * pitchMacroTiles) + (x >> 5)) << 6;
        const uint32_t innerBlocks = (((y >> 1) & 0x7) << 3) | (x & 0x7);
        const uint32_t outerInnerBytes = (outerBlocks | innerBlocks) << bytesPerBlockLog2;
        const uint32_t bank = (y >> 4) & 0x1;
        const uint32_t pipe = ((x >> 3) & 0x3) ^ (((y >> 3) & 0x1) << 1);
        const uint32_t yLsb = y & 1;
        return (yLsb << 4) | (pipe << 6) | (bank << 11) | (outerInnerBytes & 0xF) |
               (((outerInnerBytes >> 4) & 0x1) << 5) |
               (((outerInnerBytes >> 5) & 0x7) << 8) |
               ((outerInnerBytes >> 8) << 12);
    }

    bool Init()
    {
        if (g_initAttempted)
            return g_available;
        g_initAttempted = true;

        if (getenv("LO_HEADLESS"))
        {
            LOG_INFO("video: LO_HEADLESS set, no window");
            return false;
        }

        auto createWindow = [] {
            if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
            {
                LOG_WARNING("video: SDL video init failed: {}", SDL_GetError());
                return false;
            }

            // Background regression runs still render and capture the swap chain,
            // but must never show a window or take focus from the desktop user.
            const bool background = getenv("LO_BACKGROUND") != nullptr;
            const auto config=settings::GetConfig();
            g_window = SDL_CreateWindow(lo_version::WindowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                config.width, config.height, SDL_WINDOW_RESIZABLE | (background ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN));
            if (!g_window)
            {
                LOG_WARNING("video: window creation failed: {}", SDL_GetError());
                return false;
            }
            // This thread owns the SDL event loop from now on; the controller
            // subsystem is initialised here too so its message window (if any)
            // lives on the pumping thread.
            hid::Init();
            hid::SetExternalEventPump(true);
#ifdef _WIN32
            SDL_SysWMinfo info{};
            SDL_VERSION(&info.version);
            if (!SDL_GetWindowWMInfo(g_window, &info))
            {
                LOG_WARNING("video: native window lookup failed: {}", SDL_GetError());
                SDL_DestroyWindow(g_window);
                g_window = nullptr;
                return false;
            }
            g_nativeWindow = info.info.win.window;
#endif
            return true;
        };
#ifdef _WIN32
        // GPU waits, shader compilation and capture I/O must not starve Win32
        // messages. Create, pump and destroy SDL windows on their own thread.
        std::promise<bool> ready;
        auto initialized = ready.get_future();
        g_windowThread = std::jthread([createWindow, ready = std::move(ready)](std::stop_token stop) mutable {
            const bool success = createWindow();
            LOG_INFO("video: window thread {} (independent event pump)", GetCurrentThreadId());
            ready.set_value(success);
            if (!success) return;
            while (!stop.stop_requested())
            {
                PumpWindowEvents();
                // Bound latency even with no SDL events (native Debug Menu).
                MsgWaitForMultipleObjectsEx(0, nullptr, 8, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            }
            SDL_DestroyWindow(g_window);
            g_window = nullptr;
        });
        if (!initialized.get())
        {
            g_windowThread.join();
            return false;
        }
        LOG_INFO("video: render thread {}", GetCurrentThreadId());
#else
        if (!createWindow()) return false;
#endif

#ifdef LO_GPU_PLUME
#ifdef _WIN32
        g_interface = plume::CreateD3D12Interface();
        plume::RenderWindow renderWindow = g_nativeWindow;
#else
        plume::RenderWindow renderWindow{};
#endif
        if (!g_interface)
        {
            LOG_WARNING("video: no render interface available");
            return false;
        }

        g_device = g_interface->createDevice();
        if (!g_device)
        {
            LOG_WARNING("video: device creation failed");
            return false;
        }

        g_queue = g_device->createCommandQueue(plume::RenderCommandListType::DIRECT);
        g_commandList = g_queue->createCommandList();
        g_fence = g_device->createCommandFence();
        g_acquireSemaphore = g_device->createCommandSemaphore();
        g_releaseSemaphore = g_device->createCommandSemaphore();
        g_swapChain = g_queue->createSwapChain(plume::RenderSwapChainDesc(renderWindow, kSwapChainFormat, kSwapChainBuffers));
        g_uploadCapacity = uint64_t(kMaxWidth) * kMaxHeight * 4;
        g_uploadBuffer = g_device->createBuffer(plume::RenderBufferDesc::UploadBuffer(g_uploadCapacity));
        g_presentation=std::make_unique<Presentation>();
        if(!g_presentation->Init(g_device.get())) g_presentation.reset();

        LOG_INFO("video: {} on {}", "D3D12", g_device->getDescription().name);
        g_available = true;
#endif
        return g_available;
    }

    void Shutdown()
    {
#ifdef LO_GPU_PLUME
        if (g_queue && g_fence)
        {
            // Nothing in flight after the last present's fence wait.
        }
        g_cpuFrame.reset();
        g_presentation.reset();
        g_uploadBuffer.reset();
#ifdef _WIN32
        if(g_swapChain) static_cast<plume::D3D12SwapChain*>(g_swapChain.get())->d3d->SetFullscreenState(FALSE,nullptr);
#endif
        g_swapChain.reset();
        g_releaseSemaphore.reset();
        g_acquireSemaphore.reset();
        g_fence.reset();
        g_commandList.reset();
        g_queue.reset();
        g_device.reset();
        g_interface.reset();
#endif
#ifdef _WIN32
        g_windowThread.request_stop();
        if (g_windowThread.joinable()) g_windowThread.join();
#else
        if (g_window)
        {
            SDL_DestroyWindow(g_window);
            g_window = nullptr;
        }
#endif
        g_available = false;
    }

    void PumpEvents()
    {
#ifndef _WIN32
        PumpWindowEvents();
#endif
    }

    void SetShaderPreparationProgress(uint32_t completed, uint32_t total, PreparationStage stage, PreparationUnit unit)
    {
        g_shaderProgress.store((std::min<uint64_t>(total,kProgressMask) << 28) |
            std::min<uint64_t>(completed,kProgressMask) | (uint64_t(stage) << 56) | (uint64_t(unit) << 60));
    }
    bool DisplayModeFailed() { return g_displayFailed.load(); }

    namespace {
    void PumpWindowEvents()
    {
        static uint64_t shownProgress = 0;
        static auto lastProgressPaint = std::chrono::steady_clock::time_point{};
        const uint64_t progress = g_shaderProgress.load();
        const auto now = std::chrono::steady_clock::now();
        // Scanning can publish hundreds of updates per second. Keep UI updates
        // at 10 Hz, but show phase transitions and completion immediately.
        const bool phaseChanged = (progress >> 56) != (shownProgress >> 56);
        const uint32_t total = uint32_t((progress >> 28) & kProgressMask);
        const uint32_t done = uint32_t(progress & kProgressMask);
        if (g_window && progress != shownProgress &&
            (phaseChanged || done == total || now-lastProgressPaint >= std::chrono::milliseconds(100))) {
            const auto stage=PreparationStage((progress>>56)&15);
            const auto unit=PreparationUnit(progress>>60);
            const auto title = total ? fmt::format("Lost Odyssey Recompiled - {} {}/{}{}",
                PreparationTitleNarrow(stage), done, total, PreparationSuffixNarrow(unit))
                                     : std::string("Lost Odyssey Recompiled");
            SDL_SetWindowTitle(g_window, title.c_str());
#ifdef _WIN32
            // The window thread owns the preparation overlay and stays responsive
            // while the command processor scans resources or waits for DXC.
            if (total && g_nativeWindow) {
                if (!g_preparationWindow) {
                    WNDCLASSW cls{}; cls.lpfnWndProc=PreparationWindowProc;
                    cls.hInstance=GetModuleHandleW(nullptr); cls.lpszClassName=L"LOShaderPreparation";
                    cls.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512)); RegisterClassW(&cls);
                    g_preparationWindow=CreateWindowExW(WS_EX_NOACTIVATE,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE,
                        0,0,1,1,g_nativeWindow,nullptr,cls.hInstance,nullptr);
                }
                RECT rect{}; GetClientRect(g_nativeWindow,&rect);
                RECT childRect{}; GetClientRect(g_preparationWindow,&childRect);
                if (childRect.right!=rect.right || childRect.bottom!=rect.bottom)
                    MoveWindow(g_preparationWindow,0,0,rect.right,rect.bottom,FALSE);
                InvalidateRect(g_preparationWindow,nullptr,FALSE);
            } else if (g_preparationWindow) {
                DestroyWindow(g_preparationWindow); g_preparationWindow=nullptr;
            }
#endif
            shownProgress = progress;
            lastProgressPaint = now;
        }
        if (!g_window)
            return;
        static settings::Config applied;
        static bool displayInitialized=false;
        const auto config=settings::GetConfig();
        if(g_reapplyWindow.exchange(false) || !displayInitialized || config.width!=applied.width || config.height!=applied.height || config.windowMode!=applied.windowMode) {
            // SDL operations remain on the message-owning thread. Hidden tests
            // must never change the user's desktop display mode.
            const auto mode=getenv("LO_BACKGROUND")?settings::WindowMode::Windowed:config.windowMode;
            const int result=SDL_SetWindowFullscreen(g_window,mode==settings::WindowMode::Borderless?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
            g_displayFailed=result!=0;
            if(mode!=settings::WindowMode::Borderless) SDL_SetWindowSize(g_window,config.width,config.height);
            g_displaySize.store(uint64_t(config.width)<<32|config.height);
            g_displayMode.store(int(mode));
            applied=config; displayInitialized=true;
        }
        debug_menu::Update();
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
                hid::HandleKeyboardEvent(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                hid::ClearKeyboardState();
            if(event.type==SDL_MOUSEBUTTONDOWN) {
                int w=0,h=0; SDL_GetWindowSize(g_window,&w,&h);
                const float scale=std::min(w/1280.0f,h/720.0f);
                if(scale>0) settings::PointerClick((event.button.x-(w-1280*scale)*0.5f)/scale,
                    (event.button.y-(h-720*scale)*0.5f)/scale,event.button.button==SDL_BUTTON_RIGHT);
            }
            if (event.type == SDL_KEYDOWN && getenv("LO_TRACE_INPUT"))
                LOG_INFO("video key: {} repeat {}", event.key.keysym.sym, event.key.repeat);
            if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym == SDLK_F1)
                debug_menu::Toggle();
            if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED)
                hid::HandleControllerEvent(event.type, event.cdevice.which);
            if (event.type == SDL_QUIT)
            {
                LOG_INFO("video: window closed, exiting");
                fflush(stdout);
                std::_Exit(0);
            }
        }
    }

    } // namespace

    void PresentFrontbuffer(uint32_t physicalAddress, uint32_t width, uint32_t height, uint32_t copyDestInfo)
    {
        if (width == 0 || height == 0 || width > kMaxWidth || height > kMaxHeight)
            return;

#ifdef LO_GPU_PLUME
        if(g_swapChain) {
#ifdef _WIN32
            static int appliedMode=-1;
            static uint64_t appliedSize=0;
            const int mode=g_displayMode.load(); const uint64_t size=g_displaySize.load();
            if(mode>=0 && (mode!=appliedMode || size!=appliedSize)) {
                auto* swap=static_cast<plume::D3D12SwapChain*>(g_swapChain.get());
                HRESULT result=swap->d3d->SetFullscreenState(FALSE,nullptr);
                if(mode==int(settings::WindowMode::Exclusive)) {
                    DXGI_MODE_DESC target{}; target.Width=uint32_t(size>>32); target.Height=uint32_t(size);
                    target.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
                    result=swap->d3d->ResizeTarget(&target);
                    if(SUCCEEDED(result)) result=swap->d3d->SetFullscreenState(TRUE,nullptr);
                }
                BOOL exclusive=FALSE; swap->d3d->GetFullscreenState(&exclusive,nullptr);
                if(FAILED(result) || (mode==int(settings::WindowMode::Exclusive) && !exclusive)) g_displayFailed=true;
                if(appliedMode==int(settings::WindowMode::Exclusive) && mode!=appliedMode) g_reapplyWindow=true;
                LOG_INFO("display mode: requested={} exclusive={} result={:#x}",mode,bool(exclusive),uint32_t(result));
                // Flip-model swap chains require ResizeBuffers after a
                // fullscreen transition even when the dimensions are unchanged.
                g_forceSwapResize=true;
                appliedMode=mode; appliedSize=size;
            }
#endif
        }
        // Resize before rasterizing host text so its glyphs match the actual output.
        if (g_available && (g_forceSwapResize || g_swapChain->needsResize())) {
            if(!g_swapChain->resize()) { if(g_forceSwapResize) g_displayFailed=true; return; }
            g_forceSwapResize=false; g_hasPresentedImage=false;
        }
        if (g_available && g_swapChain->isEmpty()) return;
        const uint32_t menuWidth = g_available ? g_swapChain->getWidth() : 1280;
        const uint32_t menuHeight = g_available ? g_swapChain->getHeight() : 720;
        renderer::SetOutputSize(menuWidth, menuHeight);
        const auto presentationConfig = settings::GetConfig();
        gpu::SetFrameRateTarget(presentationConfig.frameRate);
        const PresentationOptions presentationOptions{
            presentationConfig.antialiasing == 3 ? Antialiasing::SMAA : static_cast<Antialiasing>(presentationConfig.antialiasing),
            presentationConfig.scalingQuality ? ScalingFilter::Bicubic : ScalingFilter::Bilinear};
        const bool menu=settings::DrawMenu(g_menuPixels,g_menuRevision,menuWidth,menuHeight);
        // Fast path: the frontbuffer was resolved on the GPU, copy it straight
        // into the swap chain. LO_PRESENT_CPU=1 forces the untiling path below.
        static const bool cpuPresent = getenv("LO_PRESENT_CPU") != nullptr;
        if (g_available && !cpuPresent && !menu)
        {
            uint32_t rw = 0, rh = 0, rf = 0;
            plume::RenderTexture* source = renderer::AcquireResolvedSurface(physicalAddress & 0x1FFFFFFF, rw, rh, rf);
            if (source && plume::RenderFormat(rf) == kSwapChainFormat)
            {
                uint32_t sourceWidth=width, sourceHeight=height;
                renderer::ScaleResolvedSize(physicalAddress & 0x1FFFFFFF, sourceWidth, sourceHeight);
                sourceWidth=std::min(sourceWidth,rw); sourceHeight=std::min(sourceHeight,rh);
                g_frameWidth = sourceWidth;
                g_frameHeight = sourceHeight;
                g_frontbufferPhysical = physicalAddress & 0x1FFFFFFF;
                g_frameOnGpu = true;
                if (g_forceSwapResize || g_swapChain->needsResize()) {
                    if(!g_swapChain->resize()) { if(g_forceSwapResize) g_displayFailed=true; return; }
                    g_forceSwapResize=false; g_hasPresentedImage=false;
                }
                if (g_swapChain->isEmpty())
                    return;
                uint32_t imageIndex = 0;
                if (!g_swapChain->acquireTexture(g_acquireSemaphore.get(), &imageIndex))
                    return;
                plume::RenderTexture* backBuffer = g_swapChain->getTexture(imageIndex);
                const uint32_t copyWidth = std::min(sourceWidth, g_swapChain->getWidth());
                const uint32_t copyHeight = std::min(sourceHeight, g_swapChain->getHeight());

                g_commandList->begin();
                if(g_presentation) {
                    if(renderer::SceneAAApplied(physicalAddress & 0x1FFFFFFF))
                        g_presentation->DrawComposited(g_commandList.get(),source,backBuffer,sourceWidth,sourceHeight,
                            g_swapChain->getWidth(),g_swapChain->getHeight(),presentationOptions.scalingFilter);
                    else g_presentation->Draw(g_commandList.get(),source,backBuffer,sourceWidth,sourceHeight,
                        g_swapChain->getWidth(),g_swapChain->getHeight(),presentationOptions);
                }
                else {
                    g_commandList->barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(backBuffer, plume::RenderTextureLayout::COPY_DEST));
                    plume::RenderBox box(0, 0, int32_t(copyWidth), int32_t(copyHeight), 0, 1);
                    g_commandList->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(backBuffer),
                        plume::RenderTextureCopyLocation::Subresource(source), 0, 0, 0, &box);
                }
                g_commandList->barriers(plume::RenderBarrierStage::NONE, plume::RenderTextureBarrier(backBuffer, plume::RenderTextureLayout::PRESENT));
                g_commandList->end();

                const plume::RenderCommandList* lists[] = { g_commandList.get() };
                plume::RenderCommandSemaphore* waitSemaphore = g_acquireSemaphore.get();
                plume::RenderCommandSemaphore* signalSemaphore = g_releaseSemaphore.get();
                g_queue->executeCommandLists(lists, 1, &waitSemaphore, 1, &signalSemaphore, 1, g_fence.get());
                g_swapChain->present(imageIndex, &signalSemaphore, 1);
                g_queue->waitForCommandFence(g_fence.get());
                g_lastPresentedImage=imageIndex; g_hasPresentedImage=true;
                return;
            }
        }
#endif
        g_frameOnGpu = false;

        // Untile: 32bpp blocks, pitch rounded up to a 32-block macro tile.
#ifdef LO_GPU_PLUME
        if(menu) { width=menuWidth; height=menuHeight; g_pixels=g_menuPixels; g_frameWidth=width; g_frameHeight=height; }
        else
#endif
        {
        const uint32_t pitchBlocks = (width + 31) & ~31u;
        const uint32_t endian = copyDestInfo & 7;
        const uint8_t* src = static_cast<const uint8_t*>(g_memory.Translate(0xA0000000u + (physicalAddress & 0x1FFFFFFF)));
        g_pixels.resize(size_t(width) * height);
        g_frameWidth = width;
        g_frameHeight = height;
        for (uint32_t y = 0; y < height; y++)
        {
            uint32_t* dst = &g_pixels[size_t(y) * width];
            for (uint32_t x = 0; x < width; x++)
            {
                uint32_t offset = TiledOffset2D(x, y, pitchBlocks, 2);
                uint32_t v;
                memcpy(&v, src + offset, 4);
                dst[x] = GpuSwap(v, endian);
            }
        }
        }

#ifdef LO_GPU_PLUME
        if (!g_available)
            return;

        if (g_forceSwapResize || g_swapChain->needsResize()) {
            if(!g_swapChain->resize()) { if(g_forceSwapResize) g_displayFailed=true; return; }
            g_forceSwapResize=false; g_hasPresentedImage=false;
        }
        if (g_swapChain->isEmpty())
            return;

        // Upload the untiled pixels; rows must be 256-byte aligned for D3D12.
        const uint32_t rowPitch = (width * 4 + 255) & ~255u;
        const uint64_t requiredBytes = uint64_t(rowPitch) * height;
        if (requiredBytes > g_uploadCapacity) {
            // The previous presentation submission was fenced before returning.
            auto upload = g_device->createBuffer(plume::RenderBufferDesc::UploadBuffer(requiredBytes));
            if (!upload) return;
            g_uploadBuffer = std::move(upload);
            g_uploadCapacity = requiredBytes;
        }
        auto* mapped = static_cast<uint8_t*>(g_uploadBuffer->map());
        if (!mapped) return;
        for (uint32_t y = 0; y < height; y++)
            memcpy(mapped + size_t(y) * rowPitch, &g_pixels[size_t(y) * width], size_t(width) * 4);
        g_uploadBuffer->unmap();

        uint32_t imageIndex = 0;
        if (!g_swapChain->acquireTexture(g_acquireSemaphore.get(), &imageIndex))
            return;
        plume::RenderTexture* backBuffer = g_swapChain->getTexture(imageIndex);
        const uint32_t copyWidth = std::min(width, g_swapChain->getWidth());
        const uint32_t copyHeight = std::min(height, g_swapChain->getHeight());

        g_commandList->begin();
        if(!g_cpuFrame || g_cpuWidth!=width || g_cpuHeight!=height) {
            g_cpuFrame=g_device->createTexture(plume::RenderTextureDesc::Texture2D(width,height,1,kSwapChainFormat));
            g_cpuWidth=width; g_cpuHeight=height;
        }
        auto* uploadTarget=g_presentation?g_cpuFrame.get():backBuffer;
        g_commandList->barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(uploadTarget, plume::RenderTextureLayout::COPY_DEST));
        plume::RenderBox box(0, 0, int32_t(copyWidth), int32_t(copyHeight), 0, 1);
        g_commandList->copyTextureRegion(
            plume::RenderTextureCopyLocation::Subresource(uploadTarget),
            plume::RenderTextureCopyLocation::PlacedFootprint(g_uploadBuffer.get(), kSwapChainFormat, width, height, 1, rowPitch / 4),
            0, 0, 0, g_presentation?nullptr:&box);
        // This upload came from guest tiled memory, not the processed GPU resolve.
        // GPU-only scene-AA provenance cannot authorize skipping its legacy AA.
        if(g_presentation) g_presentation->Draw(g_commandList.get(),g_cpuFrame.get(),backBuffer,width,height,
            g_swapChain->getWidth(),g_swapChain->getHeight(),menu ? PresentationOptions{} : presentationOptions);
        g_commandList->barriers(plume::RenderBarrierStage::NONE, plume::RenderTextureBarrier(backBuffer, plume::RenderTextureLayout::PRESENT));
        g_commandList->end();

        const plume::RenderCommandList* lists[] = { g_commandList.get() };
        plume::RenderCommandSemaphore* waitSemaphore = g_acquireSemaphore.get();
        plume::RenderCommandSemaphore* signalSemaphore = g_releaseSemaphore.get();
        g_queue->executeCommandLists(lists, 1, &waitSemaphore, 1, &signalSemaphore, 1, g_fence.get());
        g_swapChain->present(imageIndex, &signalSemaphore, 1);
        g_queue->waitForCommandFence(g_fence.get());
        g_lastPresentedImage=imageIndex; g_hasPresentedImage=true;
#endif
    }

    static bool WritePpm(const char* path, const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height)
    {
        FILE* f = fopen(path, "wb");
        if (!f)
            return false;
        fprintf(f, "P6\n%u %u\n255\n", width, height);
        std::vector<uint8_t> row(size_t(width) * 3);
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                uint32_t p = pixels[size_t(y) * width + x];
                row[x * 3 + 0] = uint8_t(p);
                row[x * 3 + 1] = uint8_t(p >> 8);
                row[x * 3 + 2] = uint8_t(p >> 16);
            }
            fwrite(row.data(), 1, row.size(), f);
        }
        fclose(f);
        return true;
    }

    bool SaveScreenshot(const char* path)
    {
#ifdef LO_GPU_PLUME
        if(getenv("LO_SCREENSHOT_PRESENTED") && g_hasPresentedImage && g_swapChain) {
            const uint32_t w=g_swapChain->getWidth(),h=g_swapChain->getHeight(),pitch=(w*4+255)&~255u;
            if(!w || !h) return false;
            auto readback=g_device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(uint64_t(pitch)*h));
            auto* frame=g_swapChain->getTexture(g_lastPresentedImage);
            g_commandList->begin();
            g_commandList->barriers(plume::RenderBarrierStage::COPY,plume::RenderTextureBarrier(frame,plume::RenderTextureLayout::COPY_SOURCE));
            g_commandList->copyTextureRegion(plume::RenderTextureCopyLocation::PlacedFootprint(readback.get(),kSwapChainFormat,w,h,1,pitch/4),plume::RenderTextureCopyLocation::Subresource(frame));
            g_commandList->barriers(plume::RenderBarrierStage::NONE,plume::RenderTextureBarrier(frame,plume::RenderTextureLayout::PRESENT));
            g_commandList->end(); const plume::RenderCommandList* lists[]={g_commandList.get()};
            g_queue->executeCommandLists(lists,1,nullptr,0,nullptr,0,g_fence.get()); g_queue->waitForCommandFence(g_fence.get());
            std::vector<uint32_t> pixels(size_t(w)*h); const auto* data=static_cast<const uint8_t*>(readback->map());
            for(uint32_t y=0;y<h;y++) memcpy(pixels.data()+size_t(y)*w,data+size_t(y)*pitch,w*4);
            readback->unmap(); return WritePpm(path,pixels,w,h);
        }
#endif
        // LO_SCREENSHOT_RESOLVED=1: also dump every GPU-resolved surface (HDR
        // scene buffers etc.) as <path>_<address>.ppm for renderer debugging.
        static const bool dumpResolved = getenv("LO_SCREENSHOT_RESOLVED") != nullptr;
        if (dumpResolved)
        {
            {
                std::string p = path;
                size_t dot = p.rfind('.');
                renderer::DumpRenderTargets(p.substr(0, dot).c_str());
            }
            std::vector<uint32_t> pixels;
            for (uint32_t address : renderer::GetResolvedAddresses())
            {
                uint32_t w = 0, h = 0;
                if (renderer::ReadbackResolvedSurface(address, pixels, w, h))
                {
                    std::string p = path;
                    size_t dot = p.rfind('.');
                    p = p.substr(0, dot) + fmt::format("_{:x}", address) + (dot == std::string::npos ? "" : p.substr(dot));
                    WritePpm(p.c_str(), pixels, w, h);
                }
            }
        }
        if (g_frameOnGpu && !renderer::ReadbackResolvedSurface(g_frontbufferPhysical, g_pixels, g_frameWidth, g_frameHeight))
            return false;
        if (g_pixels.empty())
            return false;
        FILE* f = fopen(path, "wb");
        if (!f)
            return false;
        fprintf(f, "P6\n%u %u\n255\n", g_frameWidth, g_frameHeight);
        std::vector<uint8_t> row(size_t(g_frameWidth) * 3);
        for (uint32_t y = 0; y < g_frameHeight; y++)
        {
            for (uint32_t x = 0; x < g_frameWidth; x++)
            {
                uint32_t p = g_pixels[size_t(y) * g_frameWidth + x];
                row[x * 3 + 0] = uint8_t(p);
                row[x * 3 + 1] = uint8_t(p >> 8);
                row[x * 3 + 2] = uint8_t(p >> 16);
            }
            fwrite(row.data(), 1, row.size(), f);
        }
        fclose(f);
        return true;
    }
}
