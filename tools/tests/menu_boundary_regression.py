#!/usr/bin/env python3
"""Compile current production routines with host fixtures (no game/GPU required).

This is NOT a runtime build or a real SDL/DXGI/Vulkan integration test. The
fixtures replace external services only; the routines under test are extracted
verbatim from the checkout. Extraction fails loudly if their boundaries change.
Run: python tools/tests/menu_boundary_regression.py --cxx clang++ --sanitize
     python tools/tests/menu_boundary_regression.py --cxx g++
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

COMMON = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
using namespace std::chrono_literals;
static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::fflush(stderr); std::_Exit(1); }
}
#define LOG_INFO(...) ((void)0)
#define LOG_WARNING(...) ((void)0)
'''


def between(text: str, start: str, end: str) -> str:
    if text.count(start) != 1 or text.count(end) != 1:
        raise RuntimeError(f"Production boundaries changed: {start!r} / {end!r}")
    a, b = text.index(start), text.index(end)
    if b <= a:
        raise RuntimeError("Reversed production boundaries")
    return text[a:b]


def input_fixture(source: str) -> str:
    # Compile ALL of hid.cpp, replacing only dependency includes with fixtures.
    body = re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
    names = sorted(set(re.findall(r'\bSDL_[A-Z][A-Z_0-9]*\b', body)))
    # Enum identities are only used by the fake SDL implementation. Xbox masks
    # below retain their real wire values so filtering assertions are meaningful.
    names.remove('SDL_NUM_SCANCODES')
    pre = '\nenum { ' + ', '.join(names) + ', SDL_NUM_SCANCODES = 512 };\n'
    return COMMON + pre + r'''
using Uint8 = uint8_t;
using Mutex = std::mutex;
using SDL_GameControllerButton = int;
using SDL_GameControllerAxis = int;
struct SDL_GameController { std::array<int, 256> buttons{}; } pad;
struct SDL_Event { uint32_t type{};
    struct { int which{}; } cdevice;
    struct { struct { int scancode{}; } keysym; } key;
    struct { int event{}; } window;
};
static int SDL_NumJoysticks() { return 0; }
static int SDL_JoystickGetDeviceInstanceID(int i) { return i; }
static int SDL_JoystickInstanceID(SDL_GameController*) { return 0; }
static SDL_GameController* SDL_GameControllerGetJoystick(SDL_GameController* p) { return p; }
static bool SDL_IsGameController(int) { return true; }
static SDL_GameController* SDL_GameControllerOpen(int) { return &pad; }
static const char* SDL_GameControllerName(SDL_GameController*) { return "fixture"; }
static const char* SDL_GetError() { return "fixture"; }
static void SDL_GameControllerClose(SDL_GameController*) {}
static void SDL_SetHint(int, const char*) {}
static int SDL_InitSubSystem(int) { return 0; }
static void SDL_GameControllerUpdate() {}
static bool SDL_GameControllerGetAttached(SDL_GameController*) { return true; }
static int SDL_GameControllerGetButton(SDL_GameController* p, int b) { return p->buttons[b]; }
static int SDL_GameControllerGetAxis(SDL_GameController*, int) { return 0; }
static void SDL_GameControllerRumble(SDL_GameController*, int, int, int) {}
static bool SDL_PollEvent(SDL_Event*) { return false; }
constexpr uint16_t XAMINPUT_GAMEPAD_DPAD_UP=1, XAMINPUT_GAMEPAD_DPAD_DOWN=2,
    XAMINPUT_GAMEPAD_DPAD_LEFT=4, XAMINPUT_GAMEPAD_DPAD_RIGHT=8,
    XAMINPUT_GAMEPAD_START=0x10, XAMINPUT_GAMEPAD_BACK=0x20,
    XAMINPUT_GAMEPAD_LEFT_THUMB=0x40, XAMINPUT_GAMEPAD_RIGHT_THUMB=0x80,
    XAMINPUT_GAMEPAD_LEFT_SHOULDER=0x100, XAMINPUT_GAMEPAD_RIGHT_SHOULDER=0x200,
    XAMINPUT_GAMEPAD_A=0x1000, XAMINPUT_GAMEPAD_B=0x2000,
    XAMINPUT_GAMEPAD_X=0x4000, XAMINPUT_GAMEPAD_Y=0x8000;
constexpr uint32_t ERROR_SUCCESS=0, ERROR_DEVICE_NOT_CONNECTED=1167;
constexpr uint8_t XAMINPUT_DEVTYPE_GAMEPAD=1, XAMINPUT_DEVSUBTYPE_GAMEPAD=1;
struct Gamepad { uint16_t wButtons{}; uint8_t bLeftTrigger{},bRightTrigger{};
    int16_t sThumbLX{},sThumbLY{},sThumbRX{},sThumbRY{}; };
struct XAMINPUT_STATE { uint32_t dwPacketNumber{}; struct Gamepad Gamepad; };
struct XAMINPUT_VIBRATION { uint16_t wLeftMotorSpeed{},wRightMotorSpeed{}; };
struct XAMINPUT_CAPABILITIES { uint8_t Type{},SubType{}; uint16_t Flags{};
    struct Gamepad Gamepad; XAMINPUT_VIBRATION Vibration; };
std::atomic<uint32_t> g_presentedSwaps{0};
namespace os::shaderlog { void CloseForExit() {} }
namespace frame_timing { uint64_t InputTick() { return 0; } }
namespace settings { bool FilterInput(uint16_t&,int16_t,int16_t) { return false; } }
struct TestInputPulse { uint64_t start{},duration{};
    void Set(uint64_t s,uint64_t d) { start=s; duration=d; }
    bool Active(uint64_t) { return false; } bool Pending(uint64_t) { return false; }
};
namespace debug_menu {
    enum class InputAction { Up,Down,Left,Right,Confirm,Cancel,PrevTab,NextTab };
    std::atomic<bool> visible{false};
    bool IsOverlayVisible() { return visible.load(); }
    void ToggleOverlay(); void HandleInput(InputAction);
}
namespace hid {
    void Init(); void SetExternalEventPump(bool); void HandleControllerEvent(uint32_t,int32_t);
    void HandleKeyboardEvent(int32_t,bool); void ClearKeyboardState(); void PumpHostInput();
    void Poll(); uint32_t GetState(uint32_t,XAMINPUT_STATE*);
    uint32_t SetState(uint32_t,XAMINPUT_VIBRATION*);
    uint32_t GetCapabilities(uint32_t,XAMINPUT_CAPABILITIES*);
}
''' + body + r'''
namespace {
std::mutex gate;
std::condition_variable gateCv;
bool pauseCallback=false, entered=false, leave=false;
uint16_t expectedConsumed=0;
void ResumeBoundary() {
    // The actual ProcessHostInput must already have quarantined the closing
    // buttons when a service can hide the overlay and wake guest threads.
    Check((uint16_t(s_quarantinedButtons) & expectedConsumed) == expectedConsumed,
          "resume happened before quarantine publication");
    debug_menu::visible=false;
    std::unique_lock lock(gate);
    if (!pauseCallback) return;
    entered=true; gateCv.notify_all();
    gateCv.wait(lock, [] { return leave; });
}
uint16_t Sample() { XAMINPUT_STATE s{}; hid::GetState(0,&s); return s.Gamepad.wButtons; }
void SetButtons(uint16_t bits) {
    pad.buttons.fill(0);
    pad.buttons[SDL_CONTROLLER_BUTTON_B]=(bits & 0x2000)!=0;
    pad.buttons[SDL_CONTROLLER_BUTTON_LEFTSHOULDER]=(bits & 0x100)!=0;
    pad.buttons[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER]=(bits & 0x200)!=0;
}
}
void debug_menu::ToggleOverlay() { if (visible) ResumeBoundary(); else visible=true; }
void debug_menu::HandleInput(InputAction a) { if (a==InputAction::Cancel) ResumeBoundary(); }
int main() {
    g_controllers.push_back(&pad);
    hid::SetExternalEventPump(true);
    for (int iteration=0; iteration<16; ++iteration) {
        const uint16_t bits=iteration%2 ? 0x0300 : 0x2000;
        SetButtons(0); hid::PumpHostInput();
        debug_menu::visible=true;
        { std::lock_guard lock(gate); pauseCallback=true; entered=leave=false; expectedConsumed=bits; }
        SetButtons(bits);
        auto host=std::async(std::launch::async, [] { hid::PumpHostInput(); });
        { std::unique_lock lock(gate);
          Check(gateCv.wait_for(lock,2s,[]{return entered;}),"host did not reach close boundary"); }
        std::promise<void> started;
        auto startedFuture=started.get_future();
        auto guest=std::async(std::launch::async,[&]{started.set_value(); return Sample();});
        startedFuture.wait();
        Check(guest.wait_for(20ms)==std::future_status::timeout,
              "guest sample crossed an incomplete host input transaction");
        auto event=std::async(std::launch::async, [] {hid::HandleKeyboardEvent(SDL_SCANCODE_Z,true);});
        Check(event.wait_for(1s)==std::future_status::ready,"menu callback retained HID device lock");
        event.get(); hid::ClearKeyboardState();
        { std::lock_guard lock(gate); leave=true; }
        gateCv.notify_all(); host.get();
        Check(guest.get()==0,"closing button leaked on resume");
        Check(Sample()==0,"held closing button escaped quarantine");
        // Even a guest observation of release must not mutate the host's mask.
        SetButtons(0); Check(Sample()==0,"release not neutral");
        Check((uint16_t(s_quarantinedButtons)&bits)==bits,"guest cleared host quarantine");
        hid::PumpHostInput();
        SetButtons(bits);
        Check(Sample()==bits,"fresh press after host-observed release was swallowed");
        SetButtons(0); hid::PumpHostInput();
    }
    { std::lock_guard lock(gate); pauseCallback=false; }
    hid::SetExternalEventPump(false);
    expectedConsumed=0x2000; debug_menu::visible=true;
    SetButtons(0x2000); Check(Sample()==0,"standalone close leaked");
    Check(Sample()==0,"standalone hold leaked");
    SetButtons(0); Sample(); SetButtons(0x2000);
    Check(Sample()==0x2000,"standalone release did not rearm");
    puts("PASS: actual HID routines; 16 gated B/chord closes, release ownership, device-lock freedom, standalone input");
}
'''


def test_input_file_fixture(source: str) -> str:
    # Reuse the actual HID implementation and platform stubs, but run only the
    # changed file-input protocol instead of repeating the menu-boundary suite.
    original = input_fixture(source)
    return original[:original.rindex('int main() {')] + r'''
int main() {
    const char* path="test-input-pulse.txt";
#ifdef _WIN32
    _putenv_s("LO_TEST_INPUT_FILE",path);
#else
    setenv("LO_TEST_INPUT_FILE",path,1);
#endif
    g_controllers.push_back(&pad);
    auto command=[&](const char* value) {
        FILE* f=fopen(path,"w");Check(f,"input file create");fputs(value,f);fclose(f);
        XAMINPUT_STATE state{};
        for(int i=0;i<12;++i) Check(hid::GetState(0,&state)==ERROR_SUCCESS,"sample command");
        return state.Gamepad;
    };
    auto gp=command("1 1000 100 -200 30 12 34 50000 -50000");
    Check(gp.wButtons==0x1000 && gp.sThumbLX==100 && gp.sThumbLY==-200,"9-field buttons/left stick");
    Check(gp.bLeftTrigger==12 && gp.bRightTrigger==34 && gp.sThumbRX==32767 && gp.sThumbRY==-32768,"9-field right clamp");
    gp=command("2 0 0 0 0 0 0 20000 30000");
    Check(gp.sThumbRX==0 && gp.sThumbRY==0,"zero-duration right-stick cancellation");
    gp=command("3 0 0 0 13 0 0 -12345 23456");
    Check(gp.sThumbRX==-12345 && gp.sThumbRY==23456,"signed right stick");
    XAMINPUT_STATE state{};
    for(int i=0;i<14;++i) hid::GetState(0,&state);
    Check(state.Gamepad.sThumbRX==0 && state.Gamepad.sThumbRY==0,"pulse expiry releases right stick");
    gp=command("4 0 0 0 30 22 44");
    Check(gp.bLeftTrigger==22 && gp.bRightTrigger==44 && gp.sThumbRX==0 && gp.sThumbRY==0,"7-field compatibility");
    gp=command("5 0 0 0 30");
    Check(gp.bLeftTrigger==0 && gp.bRightTrigger==0 && gp.sThumbRX==0 && gp.sThumbRY==0,"5-field clears omitted axes/triggers");
    gp=command("6 0 0 0 30 0 0 20000");
    Check(gp.sThumbRX==0 && gp.sThumbRY==0,"incomplete 8-field command rejected");
    std::remove(path);
    puts("PASS: actual HID 5/7/9-field parsing, right-axis clamp, zero cancellation, expiry and malformed command");
}
'''


def overlay_fixture(source: str) -> str:
    body = between(source, 'namespace debug_menu\n{', '    // Render debug overlay') + '\n}\n'
    return COMMON + r'''
#include <host_ui/host_ui.h>
#include <debug/cheat_overlay.h>
namespace settings { struct Config { int debugLanguage=0; };
Config GetConfig() { return {}; } bool SaveDebugLanguage(int) { return true; } }
namespace gpu::renderer { void RequestDebugCapture() {} }
namespace debug_menu {
enum class InputAction { Up,Down,Left,Right,Confirm,Cancel,PrevTab,NextTab };
struct Position { float x{},y{},z{}; };
struct MapPoi { uint64_t id; std::wstring label; Position position; };
struct TeleportSnapshot { bool available=true; Position current{100,200,300};
    uint64_t poiRevision=1; std::vector<MapPoi> pois; std::wstring status=L"ready"; };
TeleportSnapshot serviceSnapshot;
Position submitted;
TeleportSnapshot GetTeleportSnapshot() { return serviceSnapshot; }
bool RequestTeleport(Position p) { submitted=p; return true; }
bool RequestSavePosition() { return true; } bool RequestRestorePosition() { return true; }
bool RequestPoiTeleport(uint64_t) { return true; } bool RequestVictory() { return true; }
void CancelVictory() {} bool SaveAnywhereEnabled() { return false; } void SetSaveAnywhereEnabled(bool) {}
}
''' + body + r'''
int main() {
    using namespace debug_menu;
    ToggleOverlay(); Check(host_ui::IsGamePaused(),"overlay did not pause");
    HandleInput(InputAction::NextTab);
    for(int i=0;i<3;++i) HandleInput(InputAction::Down); // coordinate editor
    HandleInput(InputAction::Confirm); HandleInput(InputAction::Confirm); // Z
    for(int i=0;i<6;++i) HandleInput(InputAction::Right); // Z = 900
    HandleInput(InputAction::Up); HandleInput(InputAction::Confirm); // fill XYZ
    HandleInput(InputAction::Down); HandleInput(InputAction::Down); // teleport
    HandleInput(InputAction::Confirm);
    Check(submitted.x==100 && submitted.y==200 && submitted.z==300,
          "Fill Coordinates failed to restore all XYZ before RequestTeleport");
    serviceSnapshot.current={-10,20,-30};
    HandleInput(InputAction::Up); HandleInput(InputAction::Up);
    HandleInput(InputAction::Confirm);
    HandleInput(InputAction::Down); HandleInput(InputAction::Down); HandleInput(InputAction::Confirm);
    Check(submitted.x==-10 && submitted.y==20 && submitted.z==-30,"new snapshot XYZ not copied");
    HandleInput(InputAction::Cancel); Check(!host_ui::IsGamePaused(),"cancel did not resume");
    puts("PASS: actual overlay controller and pause implementation; edit Z -> fill XYZ -> submitted coordinates");
}
'''


def display_fixture(source: str) -> str:
    state = between(source, '        struct PresentationDisplayState {', '        constexpr plume::RenderFormat kSwapChainFormat')
    prepare = between(source, '    static bool PreparePresentation(', '    static bool UploadAndPresentPixels(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height,\n                                       bool isMenu, uint64_t displayTicket, const PresentationOptions& presentationOptions)\n    {')
    overlay = between(source, '    void PresentHostOverlay()', '    static bool WritePpm(')
    return COMMON + r'''
#include <gpu/display_change.h>
// _WIN32 is deliberately enabled AFTER standard headers, only to compile the
// production DXGI branch against the explicit fake API below. Not a Windows run.
#ifdef TEST_DXGI_BRANCH
#define _WIN32 1
#endif
#define LO_GPU_PLUME 1
using HRESULT=int32_t; using BOOL=int;
constexpr BOOL FALSE=0,TRUE=1;
#define SUCCEEDED(x) ((x)>=0)
constexpr int DXGI_FORMAT_R8G8B8A8_UNORM=28;
struct DXGI_MODE_DESC { uint32_t Width{},Height{}; int Format{}; };
namespace settings { enum class WindowMode { Windowed,Borderless,Exclusive }; }
namespace plume {
struct WindowPixelContext {};
struct FakeDxgi {
    bool exclusive=false, failSet=false, failQuery=false; int sets=0, targets=0;
    HRESULT SetFullscreenState(BOOL value,void*) { ++sets; if(failSet)return -1; exclusive=value; return 0; }
    HRESULT ResizeTarget(DXGI_MODE_DESC*) { ++targets; return 0; }
    HRESULT GetFullscreenState(BOOL* out,void*) { *out=exclusive; return failQuery ? -1 : 0; }
};
struct D3D12SwapChain {
    FakeDxgi api; FakeDxgi* d3d=&api;
    uint32_t width=1280,height=720,nextWidth=1280,nextHeight=720;
    bool resizeNeeded=false, failResize=false; int resizes=0;
    bool isEmpty() const { return !width || !height; }
    bool needsResize() const { return resizeNeeded; }
    bool resize() { ++resizes; width=nextWidth;height=nextHeight;
        if (failResize || isEmpty()) return false; resizeNeeded=false;return true; }
    uint32_t getWidth() const { return width; } uint32_t getHeight() const { return height; }
};
}
namespace host_ui {
struct PixelBuffer { uint32_t width{},height{}; std::vector<uint32_t> pixels;
    void Resize(uint32_t w,uint32_t h){width=w;height=h;pixels.resize(size_t(w)*h);}
    void Clear(uint32_t v){std::fill(pixels.begin(),pixels.end(),v);} };
struct Rasterizer { explicit Rasterizer(PixelBuffer&) {} };
uint32_t MakeColor(int,int,int,int){return 0xff010203;}
bool CompositeScaled(const PixelBuffer&,uint32_t,uint32_t,std::vector<uint32_t>&){return true;}
}
namespace debug_menu { bool visible=false; bool IsOverlayVisible(){return visible;}
void RenderOverlay(host_ui::Rasterizer&){} }
namespace settings {
bool open=true;
uint32_t drawnWidth=0,drawnHeight=0;
void (*duringDraw)()=nullptr;
bool DrawMenu(std::vector<uint32_t>& pixels,uint64_t&,uint32_t w,uint32_t h) {
    drawnWidth=w;drawnHeight=h;pixels.assign(size_t(w)*h,0xffabcdef);
    if(duringDraw)duringDraw(); return open;
}
}
namespace gpu::video {
bool g_available=true, g_vulkan=true, g_forceSwapResize=false, g_hasPresentedImage=true;
std::atomic<bool> g_displayFailed{false},g_reapplyWindow{false},g_windowResizeRequested{false};
std::atomic<int> g_displayMode{0};
std::atomic<uint64_t> g_displaySize{(uint64_t(1280)<<32)|720};
DisplayChangeTracker g_displayChanges;
std::unique_ptr<plume::D3D12SwapChain> g_swapChain;
std::vector<uint32_t> g_pixels,g_menuPixels;
uint32_t g_frameWidth=0,g_frameHeight=0; bool g_frameOnGpu=true;
uint64_t g_menuRevision=0;
int waits=0,uploads=0; uint64_t uploadedTicket=0;
void WaitForPresentGpu(){++waits;} void LogOutputPixels(const char*){}
struct PresentationOptions {};
namespace renderer { uint32_t width=0,height=0;
void SetOutputSize(uint32_t w,uint32_t h){width=w;height=h;} }
bool IsHostOverlayActive(){return settings::open || debug_menu::visible;}
''' + state + prepare + r'''
// Only GPU submission is mocked. Actual preparation, overlay entry, caching,
// and the repository's DisplayChangeTracker are compiled unchanged.
static bool UploadAndPresentPixels(const std::vector<uint32_t>& pixels,uint32_t w,uint32_t h,
                                  bool,uint64_t ticket,const PresentationOptions&) {
    Check(w==g_swapChain->width && h==g_swapChain->height,"rasterized before final resize");
    Check(pixels.size()==size_t(w)*h,"upload dimensions mismatch");
    ++uploads;uploadedTicket=ticket;
    g_displayChanges.Complete(ticket,!g_displayFailed.load());return true;
}
''' + overlay + r'''
}
using namespace gpu::video;
uint64_t newTicket=0;
void ReplaceTicketDuringRaster() {
    newTicket=g_displayChanges.Begin(1920,1080,0);
    g_displayChanges.WindowComplete(newTicket,true);
}
void Reset() {
    g_swapChain=std::make_unique<plume::D3D12SwapChain>();g_presentationDisplay={};
    g_displayChanges.Reset();g_displayFailed=false;g_reapplyWindow=false;
    g_forceSwapResize=false;g_windowResizeRequested=false;g_available=true;g_vulkan=true;
    g_displayMode=0;g_displaySize=(uint64_t(1280)<<32)|720;
    uploads=waits=0;settings::duringDraw=nullptr;settings::open=true;
}
uint64_t Ticket(int mode=0) {
    auto t=g_displayChanges.Begin(1280,720,mode);g_displayChanges.WindowComplete(t,true);return t;
}
int main() {
    Reset(); auto ticket=Ticket();
    g_swapChain->width=g_swapChain->height=0; // minimized then restored drawable
    g_swapChain->nextWidth=1920;g_swapChain->nextHeight=1080;
    PresentHostOverlay();
    Check(uploads==1 && g_swapChain->resizes==1,"empty chain never recovered");
    Check(settings::drawnWidth==1920 && settings::drawnHeight==1080,"old raster dimensions");
    Check(g_displayChanges.Query(ticket)==DisplayChangeResult::Applied,"prepared ticket did not complete");
    Check(!g_frameOnGpu && g_pixels.size()==1920u*1080u && g_frameWidth==1920,
          "CPU overlay retained GPU screenshot provenance or wrong dimensions");
    ticket=Ticket();PresentHostOverlay();
    Check(g_swapChain->resizes==2,"equal-size transaction skipped required resize");
    Check(g_displayChanges.Query(ticket)==DisplayChangeResult::Applied,"repeat transaction stuck");
    Reset();ticket=Ticket();g_swapChain->nextWidth=g_swapChain->nextHeight=0;
    PresentHostOverlay();Check(uploads==0,"zero drawable presented");
    Check(g_displayChanges.Query(ticket)==DisplayChangeResult::Pending,"minimize failed transaction prematurely");
    g_swapChain->nextWidth=1280;g_swapChain->nextHeight=720;PresentHostOverlay();
    Check(uploads==1 && g_displayChanges.Query(ticket)==DisplayChangeResult::Applied,"restore stayed stuck");
    Reset();ticket=Ticket();g_swapChain->failResize=true;PresentHostOverlay();
    Check(!uploads && g_displayChanges.Query(ticket)==DisplayChangeResult::Failed,"resize failure falsely applied");
    Reset();ticket=Ticket();settings::duringDraw=ReplaceTicketDuringRaster;PresentHostOverlay();
    Check(uploadedTicket==ticket && newTicket!=ticket,"late ticket used for earlier preparation");
    Check(g_displayChanges.Query(newTicket)==DisplayChangeResult::Pending,"unprepared newer ticket acknowledged");
    Reset();settings::open=false;PresentHostOverlay();Check(!uploads&&!waits,"hidden overlay prepared GPU");
#ifdef TEST_DXGI_BRANCH
    Reset();g_vulkan=false;g_displayMode=2;ticket=Ticket(2);PresentHostOverlay();
    Check(g_swapChain->api.exclusive && g_swapChain->api.targets==1,"exclusive enter not applied");
    Check(g_displayChanges.Query(ticket)==DisplayChangeResult::Applied,"exclusive ticket stuck");
    g_displayMode=1;ticket=Ticket(1);PresentHostOverlay();
    Check(!g_swapChain->api.exclusive && g_reapplyWindow,"exclusive exit not applied");
    Check(g_displayChanges.Query(ticket)==DisplayChangeResult::Applied,"exit ticket stuck");
    Reset();g_vulkan=false;g_displayMode=2;ticket=Ticket(2);g_swapChain->api.failSet=true;
    PresentHostOverlay();Check(!uploads && g_displayChanges.Query(ticket)==DisplayChangeResult::Failed,"failed DXGI mode reported success");
    Reset();g_vulkan=false;ticket=Ticket();g_swapChain->api.failQuery=true;PresentHostOverlay();
    Check(!uploads && g_displayChanges.Query(ticket)==DisplayChangeResult::Failed,"DXGI query failure ignored");
    puts("PASS: actual display/overlay routines with fake DXGI calls; enter/exit/failure, restore, exact ticket, CPU provenance");
#else
    puts("PASS: actual display/overlay routines with fake swap chain; empty/zero recovery, resize failure, exact ticket, CPU provenance");
#endif
}
'''



def headless_preparation_fixture(source: str) -> str:
    # Compile the actual helper guards and progress prefix without any Plume
    # declarations. A GPU reference leaking into LO_GPU_PLUME=OFF must fail.
    start=source.index("    namespace {\n",source.index("    DisplayChangeResult QueryDisplayChange"))
    end=source.index("        if (settings::restart::Requested())",start)
    body=source[start:end]+"    }\n}\n"
    return COMMON + r'''
struct SDL_Window {} window;
SDL_Window* g_window=&window;
std::atomic<uint64_t> g_shaderProgress{0};
constexpr uint64_t kProgressMask=(1ull<<28)-1;
enum class PreparationStage { CacheValidation };
enum class PreparationUnit { Files };
const char* PreparationTitleNarrow(PreparationStage) { return "fixture"; }
const char* PreparationSuffixNarrow(PreparationUnit) { return "files"; }
namespace fmt { template<class... Args> std::string format(const char*,Args&&...) { return "progress"; } }
unsigned titleUpdates=0;
void SDL_SetWindowTitle(SDL_Window*,const char*) { ++titleUpdates; }
''' + body + r'''
int main() {
    g_shaderProgress=(1ull<<28);
    PumpWindowEvents();
    Check(titleUpdates==1,"headless progress did not update window title");
    g_shaderProgress=0;PumpWindowEvents();
    Check(titleUpdates==2,"headless progress did not clear window title");
    puts("PASS: actual progress prefix with LO_GPU_PLUME=OFF (no GPU declarations)");
}
'''

def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
    parser.add_argument('--out',type=Path)
    parser.add_argument('--cxx',default='clang++')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--only',choices=['input','test-input','overlay','display','dxgi','headless'])
    args=parser.parse_args()
    root=args.root.resolve();runtime=root/'LostOdysseyRecomp'
    out=args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix='lo-menu-boundary-'))
    out.mkdir(parents=True,exist_ok=True)
    inputs={name:(runtime/path).read_text(encoding='utf-8') for name,path in {
        'hid':'hid/hid.cpp','overlay':'debug/menu_overlay.cpp','video':'gpu/video.cpp'}.items()}
    fixtures={
        'headless':(lambda:headless_preparation_fixture(inputs['video']),[]),
        'input':(lambda:input_fixture(inputs['hid']),[]),
        'test-input':(lambda:test_input_file_fixture(inputs['hid']),[]),
        'overlay':(lambda:overlay_fixture(inputs['overlay']),[]),
        'display':(lambda:display_fixture(inputs['video']),[]),
        'dxgi':(lambda:display_fixture(inputs['video']),['-DTEST_DXGI_BRANCH']),
    }
    flags=['-std=c++20','-pthread','-I'+str(runtime)]
    flags+=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if args.sanitize else ['-O2']
    for name,(generate,extra) in fixtures.items():
        if args.only and args.only!=name:continue
        cpp=out/(name+'.cpp');binary=out/name
        cpp.write_text(generate(),encoding='utf-8')
        cmd=[args.cxx,*flags,*extra,str(cpp),'-o',str(binary)]
        print('BUILD', ' '.join(cmd),flush=True)
        build=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
        (out/(name+'.build.log')).write_text(build.stdout,encoding='utf-8')
        if build.returncode:
            print(build.stdout);raise SystemExit(build.returncode)
        environment={key: value for key,value in os.environ.items() if not key.startswith('LO_')}
        environment['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
        run=subprocess.run([str(binary)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env=environment)
        (out/(name+'.run.log')).write_text(run.stdout,encoding='utf-8')
        print(run.stdout,end='',flush=True)
        if run.returncode:raise SystemExit(run.returncode)
    print('Results:',out)

if __name__=='__main__':
    main()
