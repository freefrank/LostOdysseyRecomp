#include <gpu/display_change.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <vector>

static void Require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static std::vector<std::string> operations;
using HRESULT = int32_t;
using BOOL = int;
[[maybe_unused]] constexpr BOOL FALSE = 0, TRUE = 1;
[[maybe_unused]] constexpr int DXGI_FORMAT_R8G8B8A8_UNORM = 28;
constexpr bool SUCCEEDED(HRESULT result) { return result >= 0; }
constexpr bool FAILED(HRESULT result) { return result < 0; }
struct DXGI_MODE_DESC { uint32_t Width{}, Height{}; int Format{}; };
namespace settings { enum class WindowMode { Windowed, Borderless, Exclusive }; }
namespace plume
{
struct MockDxgi
{
    bool exclusive = false, failSet = false, failQuery = false;
    HRESULT SetFullscreenState(BOOL value, void*)
    {
        operations.push_back(value ? "fullscreen-on" : "fullscreen-off");
        if (failSet) return -1;
        exclusive = value != FALSE;
        return 0;
    }
    HRESULT ResizeTarget(const DXGI_MODE_DESC*) { operations.push_back("target"); return 0; }
    HRESULT GetFullscreenState(BOOL* value, void*)
    {
        operations.push_back("query"); *value = exclusive;
        return failQuery ? -1 : 0;
    }
};
struct WindowPixelContext { WindowPixelContext() {} ~WindowPixelContext() {} };
struct RenderSwapChain
{
    virtual ~RenderSwapChain() = default;
    uint32_t width = 1280, height = 720, nextWidth = 1280, nextHeight = 720;
    bool handle = true, dirty = false, failResize = false;
    unsigned resizeCalls = 0;
    bool isEmpty() const { return !handle || !width || !height; }
    bool needsResize() const { return dirty; }
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    bool resize()
    {
        operations.push_back("resize"); ++resizeCalls;
        width = nextWidth; height = nextHeight;
        if (!width || !height || failResize) return false;
        handle = true; dirty = false; return true;
    }
};
struct D3D12SwapChain : RenderSwapChain
{
    std::unique_ptr<MockDxgi> d3d = std::make_unique<MockDxgi>();
};
}
static bool g_available = true, g_vulkan = true, g_forceSwapResize = false, g_hasPresentedImage = false;
static std::atomic<bool> g_windowResizeRequested{false}, g_displayFailed{false}, g_reapplyWindow{false};
static std::atomic<int> g_displayMode{-1};
static std::atomic<uint64_t> g_displaySize{0};
static std::unique_ptr<plume::RenderSwapChain> g_swapChain;
static gpu::video::DisplayChangeTracker g_displayChanges;
static std::vector<uint32_t> g_pixels;
static uint32_t g_frameWidth = 0, g_frameHeight = 0, g_frontbufferPhysical = 0;
static bool g_frameOnGpu = false;
static void WaitForPresentGpu() { operations.push_back("wait"); }
static void LogOutputPixels(const char*) {}
#define LOG_INFO(...) ((void)0)

// Compile both branches of the exact production helper on either host. These
// shims validate control flow, NOT the Windows ABI or a real DXGI/Vulkan driver.
#ifdef _WIN32
#define LO_RESTORE_WIN32
#undef _WIN32
#endif
#ifdef LO_TEST_WINDOWS_PATH
#define _WIN32 1
#endif
#include "video_prepare.inc"
#undef _WIN32
#ifdef LO_RESTORE_WIN32
#define _WIN32 1
#undef LO_RESTORE_WIN32
#endif
#include "cpu_frame.inc"

static void Reset()
{
    g_displayChanges.Reset(); g_presentationDisplay = {};
    g_available = true; g_vulkan = true; g_forceSwapResize = false;
    g_windowResizeRequested = false; g_displayFailed = false; g_reapplyWindow = false;
    g_displayMode = -1; g_displaySize = 0;
    g_swapChain = std::make_unique<plume::D3D12SwapChain>();
    operations.clear();
}
static uint64_t Begin(uint32_t width, uint32_t height, settings::WindowMode mode)
{
    g_displayMode = int(mode); g_displaySize = uint64_t(width) << 32 | height;
    const auto ticket = g_displayChanges.Begin(width, height, uint32_t(mode));
    g_displayChanges.WindowComplete(ticket, true);
    return ticket;
}

int main()
{
    using gpu::video::DisplayChangeResult;
    using settings::WindowMode;
    Reset();
    g_swapChain->width = g_swapChain->height = 0;
    g_swapChain->nextWidth = g_swapChain->nextHeight = 0;
    const auto ticket = Begin(1920, 1080, WindowMode::Windowed);
    uint64_t prepared = 0;
    for (int i = 0; i < 120; ++i)
        Require(!PreparePresentation(prepared), "zero-size chain became presentable");
    Require(g_swapChain->resizeCalls == 120, "empty chain bypassed recovery");
    Require(!g_displayFailed && g_displayChanges.Query(ticket) == DisplayChangeResult::Pending,
            "minimization failed the display transaction instead of deferring it");
    g_swapChain->nextWidth = 1920; g_swapChain->nextHeight = 1080;
    Require(PreparePresentation(prepared), "restored chain failed to recover");
    Require(prepared == ticket && g_swapChain->getWidth() == 1920, "lost ticket or stale raster dimensions");
    g_displayChanges.Complete(prepared, true); // mock successful queue present
    Require(g_displayChanges.Query(ticket) == DisplayChangeResult::Applied, "present did not complete ticket");
    const auto second = Begin(1280, 720, WindowMode::Borderless);
    Require(second != ticket && PreparePresentation(prepared) && prepared == second, "second display change blocked");
    const auto newer = Begin(1920, 1080, WindowMode::Windowed);
    g_displayChanges.Complete(prepared, true);
    Require(g_displayChanges.Query(newer) == DisplayChangeResult::Pending, "old frame completed a newer request");

    Reset();
    g_swapChain->handle = false; // empty handle with no dirty flag must also retry
    Require(PreparePresentation(prepared) && g_swapChain->resizeCalls == 1, "empty handle was never retried");
    Reset();
    const auto failed = Begin(1920, 1080, WindowMode::Windowed);
    g_swapChain->failResize = true;
    Require(!PreparePresentation(prepared) && g_displayChanges.Query(failed) == DisplayChangeResult::Failed,
            "nonzero-size resize failure was treated as a successful transaction");
    Reset();
    g_windowResizeRequested = true;
    Require(PreparePresentation(prepared) && !g_windowResizeRequested && g_swapChain->resizeCalls == 1,
            "owner-thread resize request was not consumed");

#ifdef LO_TEST_WINDOWS_PATH
    Reset(); g_vulkan = false;
    const auto exclusive = Begin(1920, 1080, WindowMode::Exclusive);
    Require(PreparePresentation(prepared), "exclusive prepare failed");
    const std::vector<std::string> expected{"wait", "fullscreen-off", "target", "fullscreen-on", "query", "wait", "resize"};
    Require(operations == expected, "D3D12 fullscreen steps were skipped or reordered");
    Require(g_displayChanges.Query(exclusive) == DisplayChangeResult::Pending, "ticket completed before queue present");
    g_displayChanges.Complete(prepared, true);
    operations.clear();
    const auto windowed = Begin(1280, 720, WindowMode::Windowed);
    Require(PreparePresentation(prepared) && prepared == windowed, "exclusive exit prepare failed");
    auto* dxgi = static_cast<plume::D3D12SwapChain*>(g_swapChain.get())->d3d.get();
    Require(!dxgi->exclusive && g_reapplyWindow, "exclusive exit was not applied");
    Reset(); g_vulkan = false;
    const auto modeFailure = Begin(1920, 1080, WindowMode::Exclusive);
    static_cast<plume::D3D12SwapChain*>(g_swapChain.get())->d3d->failSet = true;
    Require(!PreparePresentation(prepared) && g_displayChanges.Query(modeFailure) == DisplayChangeResult::Failed,
            "failed fullscreen transition could be reported Applied");
#endif

    for (const auto& [w, h] : {std::pair{1280u, 720u}, {1920u, 1080u}, {3840u, 2160u}})
    {
        const std::vector<uint32_t> frame(size_t(w) * h, 0xFF123456u);
        g_frameOnGpu = true; g_frontbufferPhysical = 0x123400;
        Require(StoreCpuFrame(frame, w, h), "valid menu frame rejected");
        Require(!g_frameOnGpu && !g_frontbufferPhysical && g_frameWidth == w && g_frameHeight == h &&
                g_pixels.size() == size_t(w) * h && g_pixels.back() == 0xFF123456u,
                "CPU frame retained stale GPU provenance or dimensions");
    }
    const auto oldSize = g_pixels.size();
    Require(!StoreCpuFrame({}, 1280, 720) && !StoreCpuFrame({}, 0, 0) && g_pixels.size() == oldSize,
            "invalid frame corrupted the last valid screenshot state");
    std::puts("PASS: shared production prepare, zero-size recovery, ticket ownership/failures, CPU frame provenance and 720p/1080p/4K bounds");
}
