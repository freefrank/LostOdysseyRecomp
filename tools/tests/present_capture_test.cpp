// Hidden swapchain capture through production PresentFrontbuffer.
// No guest, NGX, or full game link.
#include <stdafx.h>
#include <plume_render_interface.h>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <gpu/shader/dxc_compiler.h>
#include <gpu/present_capture.h>
#include <os/capture_archive.h>
#include <settings/config.h>
#include <settings/restart.h>
#include <SDL.h>
#undef main

namespace fixture {
std::mutex mutex;
settings::Config config;
std::deque<std::function<void()>> commands;
std::unique_ptr<plume::RenderTexture> sourceOwner;
plume::RenderTexture* source = nullptr;
uint32_t sourceWidth = 0, sourceHeight = 0, sourceFormat = 0;
bool sceneComposite = false;
void Check(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
template<class F> auto Owner(F fn) {
    using R = std::invoke_result_t<F>;
    auto task = std::make_shared<std::packaged_task<R()>>(std::move(fn));
    auto result = task->get_future();
    { std::lock_guard lock(mutex); commands.push_back([task]{ (*task)(); }); }
    Check(result.wait_for(std::chrono::seconds(20)) == std::future_status::ready, "window owner timeout");
    return result.get();
}
}

#include "../../LostOdysseyRecomp/gpu/video.cpp"

namespace settings {
Config GetConfig() { std::lock_guard lock(fixture::mutex); return fixture::config; }
bool DrawMenu(std::vector<uint32_t>&, uint64_t&, uint32_t, uint32_t) { return false; }
bool IsOpen() { return false; }
void PointerClick(float, float, bool) {}
}
namespace hid {
void Init() {}
void SetExternalEventPump(bool) {}
void PumpHostInput() {}
void HandleControllerEvent(uint32_t, int32_t) {}
void HandleKeyboardEvent(int32_t, bool) {}
void ClearKeyboardState() {}
}
namespace debug_menu {
bool IsOverlayVisible() { return false; }
void Toggle() {}
void RenderOverlay(host_ui::Rasterizer&) {}
void HandleInput(InputAction) {}
void Update() {
    std::deque<std::function<void()>> work;
    { std::lock_guard lock(fixture::mutex); work.swap(fixture::commands); }
    for (auto& command : work) command();
}
}
Memory::Memory() = default;
Memory g_memory;
namespace gpu { bool SetFrameRateTarget(uint32_t) { return true; } }
namespace gpu::renderer {
bool Init() { throw std::runtime_error("renderer init is outside this fixture"); }
void Shutdown() {}
void WaitDebugCaptureArchive() {}
void SetOutputSize(uint32_t, uint32_t) {}
void ScaleResolvedSize(uint32_t, uint32_t& width, uint32_t& height) { width = fixture::sourceWidth; height = fixture::sourceHeight; }
plume::RenderTexture* AcquireResolvedSurface(uint32_t, uint32_t& width, uint32_t& height, uint32_t& format, frame_plan::FramePlan* plan) {
    width = fixture::sourceWidth; height = fixture::sourceHeight; format = fixture::sourceFormat;
    if (plan) { plan->cpuSerial = 1; plan->effectiveAA = 1; plan->scalingQuality = 0; }
    return fixture::source;
}
bool SceneAAApplied(uint32_t) { return fixture::sceneComposite; }
bool SuppressPresent() { return false; }
bool ReadbackResolvedSurface(uint32_t, std::vector<uint32_t>&, uint32_t&, uint32_t&) { return false; }
std::vector<uint32_t> GetResolvedAddresses() { return {}; }
void DumpRenderTargets(const char*) {}
}
namespace gpu::upscaling {
void PublishDeviceCapability(BackendDeviceSnapshot) {}
BackendDeviceSnapshot PublishedDeviceCapability() { return {}; }
OutputSizing SizingService::QueryOutputSizing(dlss::Controller&, const plume::VulkanInterface&, const plume::VulkanDevice&, const SizingKey&) { return {}; }
}
namespace gpu::frame_plan {
void NoteCurrentDlssStatus() {}
void ResetSizing(uint64_t) {}
void PublishSizing(upscaling::OutputSizing) {}
std::optional<upscaling::SizingKey> TakeSizingRequest() { return {}; }
}
namespace gpu::dlss {
Controller::Controller(std::filesystem::path, std::filesystem::path) {}
plume::VulkanExtensionHooks Controller::ExtensionHooks() { return {}; }
void Controller::ProbeOnce(const plume::VulkanInterface&, const plume::VulkanDevice&) {}
void Controller::ReleaseCompletedThrough(uint64_t) {}
void Controller::AbandonUsesAfterDeviceLoss() {}
void Controller::ShutdownAfterGpuDrain() {}
const char* ProbeStateName(ProbeState) { return "stub"; }
}

namespace {
using fixture::Check;
uint32_t RGBA(uint8_t r, uint8_t g, uint8_t b) { return uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | 0xFF000000u; }
uint8_t Channel(uint32_t pixel, int shift) { return uint8_t(pixel >> shift); }
bool Near(uint32_t pixel, uint8_t r, uint8_t g, uint8_t b, int tolerance) {
    return std::abs(int(Channel(pixel, 0)) - r) <= tolerance && std::abs(int(Channel(pixel, 8)) - g) <= tolerance &&
        std::abs(int(Channel(pixel, 16)) - b) <= tolerance;
}
uint32_t At(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t x, uint32_t y) {
    return pixels[size_t(y) * width + x];
}

void FillSource(uint32_t color, bool bar) {
    auto* device = gpu::video::GetDevice();
    auto* queue = gpu::video::GetQueue();
    const uint32_t width = fixture::sourceWidth, height = fixture::sourceHeight;
    const uint32_t pitch = (width + 63u) & ~63u;
    auto upload = device->createBuffer(plume::RenderBufferDesc::UploadBuffer(uint64_t(pitch) * height * 4));
    auto* mapped = static_cast<uint32_t*>(upload->map());
    Check(mapped != nullptr, "source upload map failed");
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            mapped[size_t(y) * pitch + x] = bar && y < height / 6 ? RGBA(255, 255, 255) : color;
    upload->unmap();
    auto commands = queue->createCommandList();
    auto fence = device->createCommandFence();
    commands->begin();
    commands->barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(fixture::source, plume::RenderTextureLayout::COPY_DEST));
    commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(fixture::source),
        plume::RenderTextureCopyLocation::PlacedFootprint(upload.get(), plume::RenderFormat::R8G8B8A8_UNORM, width, height, 1, pitch));
    commands->end();
    const plume::RenderCommandList* lists[]{commands.get()};
    queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
    queue->waitForCommandFence(fence.get());
}

gpu::present_capture::Result PresentMarked(uint32_t frame, uint32_t swap, uint32_t color, bool composite) {
    fixture::sceneComposite = composite;
    FillSource(color, composite);
    gpu::present_capture::Ticket ticket;
    ticket.active = true;
    ticket.rendererFrame = frame;
    ticket.swap = swap;
    ticket.frontbuffer = 0x1000;
    ticket.deviceEpoch = 4;
    gpu::present_capture::Result result;
    gpu::video::PresentFrontbuffer(ticket.frontbuffer, fixture::sourceWidth, fixture::sourceHeight, 0, &ticket, &result);
    Check(result.attempted && result.rendererFrame == frame && result.swap == swap, "ticket identity was not kept");
    return result;
}

void CheckFinal(const gpu::present_capture::Result& result, uint8_t r, uint8_t g, uint8_t b, bool bar) {
    Check(result.available && result.pixels.size() == size_t(result.width) * result.height, "final pixels missing");
    Check(result.width == gpu::video::g_swapChain->getWidth() && result.height == gpu::video::g_swapChain->getHeight(), "final size is not the swapchain");
    Check(result.width != fixture::sourceWidth || result.height != fixture::sourceHeight, "final size matches the guest image");
    const float scale = std::min(float(result.width) / fixture::sourceWidth, float(result.height) / fixture::sourceHeight);
    const float contentW = fixture::sourceWidth * scale;
    const float inset = (float(result.width) - contentW) * 0.5f;
    Check(inset >= 2.0f, "swapchain aspect did not letterbox the guest");
    Check(Near(At(result.pixels, result.width, 1, result.height / 2), 0, 0, 0, 20), "letterbox edge is not empty");
    Check(Near(At(result.pixels, result.width, result.width / 2, result.height / 2), r, g, b, 24), "final center is not this frame");
    if (bar) Check(Near(At(result.pixels, result.width, result.width / 2, 4), 255, 255, 255, 24), "composited UI bar missing from final image");
    else Check(!Near(At(result.pixels, result.width, result.width / 2, 4), 255, 255, 255, 24), "unexpected UI bar");
}

void WriteFrame(const std::filesystem::path& directory, const gpu::present_capture::Ticket& ticket,
    const std::vector<uint32_t>& guest, const gpu::present_capture::Result& finalImage) {
    std::filesystem::create_directories(directory);
    gpu::present_capture::GuestImage guestImage;
    guestImage.available = true;
    guestImage.reason = "readback";
    guestImage.address = ticket.frontbuffer;
    guestImage.width = fixture::sourceWidth;
    guestImage.height = fixture::sourceHeight;
    guestImage.sourceWriteFrame = ticket.rendererFrame;
    guestImage.sourceWriteOrdinal = ticket.swap;
    Check(gpu::present_capture::WriteRgbaBmp(directory / "guest-frontbuffer.bmp", guest, fixture::sourceWidth, fixture::sourceHeight), "guest bmp");
    Check(finalImage.available && gpu::present_capture::WriteRgbaBmp(directory / "screenshot.bmp", finalImage.pixels, finalImage.width, finalImage.height), "final bmp");
    std::ofstream state(directory / "render-state.txt");
    gpu::present_capture::AppendFrameMetadata(state, ticket, guestImage, finalImage);
    state.close();
    Check(bool(state), "metadata");
}
}

int LoPresentCapturePreprocessPlume();
int LoPresentCapturePreprocessOff();
int LoPresentCapturePreprocessUnit();

void RunClose(const std::filesystem::path& output) {
    fixture::Check(LoPresentCapturePreprocessPlume() == 0, "plume preprocess");
    fixture::Check(LoPresentCapturePreprocessOff() == 0, "non-gpu preprocess");
    fixture::Check(LoPresentCapturePreprocessUnit() == 0, "submission-unit preprocess");
    const auto root = output / "captures" / "render-close-capture";
    std::filesystem::remove_all(root);
    std::filesystem::remove(root.wstring() + L".zip");
    std::filesystem::create_directories(root);
    uint32_t stored = 0;
    gpu::present_capture::CaptureClose finalClose;
    for (uint32_t index = 0; index < 3; ++index) {
        const uint32_t ticketFrame = 10 + index;
        const uint32_t liveFrame = ticketFrame + 1;
        const uint32_t swap = index + 1;
        const auto close = gpu::present_capture::BeginCaptureClose(stored, 3, ticketFrame, swap, true);
        fixture::Check(close.attemptFrame == ticketFrame && close.attemptFrame != liveFrame, "close used the live frame");
        fixture::Check(close.completedFrames == index + 1, "completion count advanced before reset");
        const auto frameDir = root / ("frame-0" + std::to_string(index + 1));
        std::filesystem::create_directories(frameDir);
        {
            std::ofstream manifest(root / "capture-info.txt");
            gpu::present_capture::WriteCaptureManifest(manifest, close, 10);
            manifest.close();
            fixture::Check(bool(manifest), "capture-info");
        }
        {
            std::ofstream status(frameDir / "runtime-log-status.txt");
            gpu::present_capture::WriteRuntimeFrameStatus(status, close);
            status << "shader_snapshot_frame=" << close.attemptFrame << '\n';
            status.close();
        }
        std::ofstream(frameDir / "screenshot.bmp", std::ios::binary) << "ticket-frame-" << ticketFrame;
        if (close.continueNext) {
            fixture::Check(!close.publish, "early frame requested a zip");
            stored = close.completedFrames;
            fixture::Check(!std::filesystem::exists(root.wstring() + L".zip"), "zip exists before the third frame");
        } else {
            finalClose = close;
            const uint32_t frozenCompleted = close.completedFrames;
            const bool publish = close.publish;
            stored = 0;
            fixture::Check(frozenCompleted == 3 && publish && stored == 0, "reset swallowed the completion count");
            auto archived = os::StartCaptureArchive(root, [publish](const std::filesystem::path&) {
                if (!publish) throw std::system_error(std::make_error_code(std::errc::io_error));
            }).get();
            fixture::Check(archived.saved, "production archive predicate did not publish the third frame");
        }
    }
    const auto extracted = output / "close-zip";
    std::filesystem::remove_all(extracted);
    const auto zip = root.wstring() + L".zip";
    const std::string unpack = "powershell -NoLogo -NoProfile -NonInteractive -Command \"Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('"
        + (root.string() + ".zip") + "','" + extracted.string() + "')\"";
    fixture::Check(std::system(unpack.c_str()) == 0, "cannot unpack close archive");
    std::ifstream manifest(extracted / "capture-info.txt");
    std::string text((std::istreambuf_iterator<char>(manifest)), {});
    fixture::Check(text.find("completed_frames=3") != std::string::npos, "archived completed count");
    fixture::Check(text.find("last_attempted_frame=12") != std::string::npos, "archived ticket frame");
    fixture::Check(text.find("last_attempted_swap=3") != std::string::npos, "archived ticket swap");
    fixture::Check(text.find("last_attempted_frame=13") == std::string::npos, "archived the incremented live frame");
    std::ifstream runtime(extracted / "frame-03" / "runtime-log-status.txt");
    std::string runtimeText((std::istreambuf_iterator<char>(runtime)), {});
    fixture::Check(runtimeText.find("frame=12") != std::string::npos && runtimeText.find("shader_snapshot_frame=12") != std::string::npos, "runtime status frame");
    (void)zip;
    (void)finalClose;

    const auto failedRoot = output / "captures" / "render-close-fail";
    std::filesystem::remove_all(failedRoot);
    std::filesystem::remove(failedRoot.wstring() + L".zip");
    std::filesystem::create_directories(failedRoot);
    std::ofstream(failedRoot / "keep.txt") << "retain";
    const auto failedClose = gpu::present_capture::BeginCaptureClose(2, 3, 12, 3, false);
    fixture::Check(!failedClose.publish && failedClose.completedFrames == 2 && failedClose.attemptFrame == 12, "failed close");
    {
        std::ofstream manifest(failedRoot / "capture-info.txt");
        gpu::present_capture::WriteCaptureManifest(manifest, failedClose, 10);
    }
    auto failedArchive = os::StartCaptureArchive(failedRoot, [publish = failedClose.publish](const std::filesystem::path&) {
        if (!publish) throw std::system_error(std::make_error_code(std::errc::io_error));
    }).get();
    fixture::Check(!failedArchive.saved && std::filesystem::exists(failedRoot / "keep.txt"), "failed close published a zip or removed the directory");
    fixture::Check(!std::filesystem::exists(failedRoot.wstring() + L".zip"), "failed close left a success zip");

    gpu::present_capture::Result broken;
    broken.available = true;
    broken.width = 2;
    broken.height = 2;
    fixture::Check(!gpu::present_capture::CommitFinalImage(broken, failedRoot / "screenshot.bmp"), "bad pixels were written");
    fixture::Check(!broken.available && broken.reason == "bmp_write_failed", "bmp failure reason");
    fixture::Check(!std::filesystem::exists(failedRoot / "screenshot.bmp"), "partial screenshot remained");
    std::cout << "PASS present capture close\n";
}

int main(int argc, char** argv) {
    std::string backendName = "d3d12", testCase = "three-frame", output = "out/present-capture";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--backend" && i + 1 < argc) backendName = argv[++i];
        else if (arg == "--case" && i + 1 < argc) testCase = argv[++i];
        else if (arg == "--output" && i + 1 < argc) output = argv[++i];
    }
    try {
        if (testCase == "close") {
            RunClose(output);
            return 0;
        }
        const auto cache = std::filesystem::path(output) / "shader-cache";
        std::filesystem::create_directories(cache);
        _putenv_s("LO_BACKGROUND", "1");
        _putenv_s("LO_NO_RENDERER", "1");
        _putenv_s("LO_SHADER_CACHE_DIR", cache.string().c_str());
        _putenv_s("LO_GRAPHICS_API", backendName == "vulkan" ? "vulkan" : "d3d12");
        const auto backend = backendName == "vulkan" ? gpu::backend::Backend::Vulkan : gpu::backend::Backend::D3D12;
        { std::lock_guard lock(fixture::mutex);
            fixture::config.graphicsBackend = backend;
            fixture::config.width = 640; fixture::config.height = 360;
            fixture::config.windowMode = settings::WindowMode::Windowed;
            fixture::config.antialiasing = 1; }
        fixture::Check(gpu::video::Init(), "video init failed");
        fixture::Check(gpu::video::SelectedBackend() == backend, "backend fallback");
        fixture::Check(gpu::video::g_swapChain && !gpu::video::g_swapChain->isEmpty(), "empty swapchain");
        const uint32_t swapW = gpu::video::g_swapChain->getWidth(), swapH = gpu::video::g_swapChain->getHeight();
        fixture::sourceWidth = std::max(32u, swapW / 2);
        fixture::sourceHeight = std::max(32u, fixture::sourceWidth * 3 / 4);
        fixture::sourceFormat = uint32_t(plume::RenderFormat::R8G8B8A8_UNORM);
        fixture::sourceOwner = gpu::video::GetDevice()->createTexture(plume::RenderTextureDesc::Texture2D(
            fixture::sourceWidth, fixture::sourceHeight, 1, plume::RenderFormat::R8G8B8A8_UNORM));
        fixture::source = fixture::sourceOwner.get();
        fixture::Check(fixture::source != nullptr, "source texture");
        std::cout << "swapchain " << swapW << "x" << swapH << " guest " << fixture::sourceWidth << "x" << fixture::sourceHeight
            << " backend " << backendName << "\n";

        if (testCase == "failure") {
            const auto maps = gpu::video::PresentCaptureMapCount();
            const auto allocs = gpu::video::PresentCaptureAllocationCount();
            const auto copies = gpu::video::PresentCaptureCopyCount();
            gpu::present_capture::Result ignored;
            gpu::video::PresentFrontbuffer(0x1000, fixture::sourceWidth, fixture::sourceHeight, 0, nullptr, &ignored);
            fixture::Check(!ignored.attempted, "no request must not record a capture");
            fixture::Check(gpu::video::PresentCaptureAllocationCount() == allocs && gpu::video::PresentCaptureCopyCount() == copies &&
                gpu::video::PresentCaptureMapCount() == maps, "no request allocated, copied, or mapped");

            gpu::present_capture::Ticket stale;
            stale.active = true; stale.rendererFrame = 2; stale.swap = 2;
            gpu::present_capture::Result early;
            gpu::video::PresentFrontbuffer(0x1000, 0, 0, 0, &stale, &early);
            fixture::Check(early.attempted && !early.available && early.pixels.empty() && early.reason == "present_ended_before_capture",
                "early present reused or invented a final image");
            const auto rejected = std::filesystem::path(output) / "rejected";
            std::filesystem::create_directories(rejected);
            std::filesystem::remove(rejected / "screenshot.bmp");
            fixture::Check(!early.available, "rejected frame has pixels");
            fixture::Check(!std::filesystem::exists(rejected / "screenshot.bmp"), "missing final image created screenshot.bmp");

            const auto mapsBefore = gpu::video::PresentCaptureMapCount();
            const auto retainedBefore = gpu::video::PresentCaptureRetainedBuffers();
            gpu::video::SetPresentCaptureCompletionFault(true);
            auto failed = PresentMarked(9, 9, RGBA(255, 0, 0), false);
            gpu::video::SetPresentCaptureCompletionFault(false);
            fixture::Check(!failed.available && failed.pixels.empty() && failed.reason == "completion_unconfirmed", "unconfirmed completion produced pixels");
            fixture::Check(gpu::video::PresentCaptureMapCount() == mapsBefore, "unconfirmed completion mapped the readback");
            fixture::Check(gpu::video::PresentCaptureRetainedBuffers() > retainedBefore, "in-flight capture buffer was released");
            std::cout << "PASS present capture failure " << backendName << "\n";
        } else {
            struct Frame { uint32_t frame, swap; uint8_t r, g, b; bool bar; const char* name; };
            const Frame frames[]{{10, 1, 220, 0, 0, false, "frame-01"}, {11, 2, 0, 220, 0, false, "frame-02"}, {12, 3, 0, 0, 220, true, "frame-03"}};
            std::vector<uint32_t> guest(size_t(fixture::sourceWidth) * fixture::sourceHeight);
            const auto root = std::filesystem::path(output) / "captures" / "render-present-capture";
            std::filesystem::remove_all(root);
            std::filesystem::remove(root.wstring() + L".zip");
            uint32_t previous = 0;
            for (const auto& frame : frames) {
                auto result = PresentMarked(frame.frame, frame.swap, RGBA(frame.r, frame.g, frame.b), frame.bar);
                CheckFinal(result, frame.r, frame.g, frame.b, frame.bar);
                if (previous) fixture::Check(!Near(At(result.pixels, result.width, result.width / 2, result.height / 2),
                    uint8_t(previous), uint8_t(previous >> 8), uint8_t(previous >> 16), 24), "final image contains the previous frame");
                previous = RGBA(frame.r, frame.g, frame.b);
                for (uint32_t y = 0; y < fixture::sourceHeight; ++y)
                    for (uint32_t x = 0; x < fixture::sourceWidth; ++x)
                        guest[size_t(y) * fixture::sourceWidth + x] = frame.bar && y < fixture::sourceHeight / 6 ? RGBA(255, 255, 255) : RGBA(frame.r, frame.g, frame.b);
                gpu::present_capture::Ticket ticket;
                ticket.active = true; ticket.rendererFrame = frame.frame; ticket.swap = frame.swap; ticket.frontbuffer = 0x1000; ticket.deviceEpoch = 4;
                WriteFrame(root / frame.name, ticket, guest, result);
                fixture::Check(!std::filesystem::exists(root.wstring() + L".zip"), "archive started before the third frame");
            }
            fixture::Check(gpu::present_capture::ShouldPublishArchive(3, 3, true), "third frame should publish");
            auto archived = os::StartCaptureArchive(root).get();
            fixture::Check(archived.saved && std::filesystem::exists(archived.archive), "archive was not published");
            const auto extracted = std::filesystem::path(output) / "zip-check";
            std::filesystem::remove_all(extracted);
            const std::string unpack = "powershell -NoLogo -NoProfile -NonInteractive -Command \"Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('"
                + archived.archive.string() + "','" + extracted.string() + "')\"";
            fixture::Check(std::system(unpack.c_str()) == 0, "cannot unpack archive");
            for (const char* name : {"frame-01/guest-frontbuffer.bmp", "frame-01/screenshot.bmp", "frame-02/guest-frontbuffer.bmp",
                "frame-02/screenshot.bmp", "frame-03/guest-frontbuffer.bmp", "frame-03/screenshot.bmp", "frame-03/render-state.txt"})
                fixture::Check(std::filesystem::exists(extracted / name), name);
            std::ifstream state(extracted / "frame-03" / "render-state.txt");
            std::string text((std::istreambuf_iterator<char>(state)), {});
            fixture::Check(text.find("renderer_frame=12") != std::string::npos && text.find("swap=3") != std::string::npos, "zip identity");
            fixture::Check(text.find("source=swapchain-pre-present") != std::string::npos && text.find("guest-frontbuffer.bmp") != std::string::npos, "zip sources");
            std::cout << "PASS present capture three-frame " << backendName << "\n";
        }
        fixture::source = nullptr;
        fixture::sourceOwner.reset();
        gpu::video::Shutdown();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << "\n";
        fixture::source = nullptr;
        fixture::sourceOwner.reset();
        gpu::video::Shutdown();
        return 1;
    }
}
