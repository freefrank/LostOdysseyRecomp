/* SPDX-License-Identifier: GPL-3.0-or-later */
// Same-device LSFG presentation, adapted from NetherSX2_nx's
// source/lsfg/lsfg_bridge.cpp (GPL-3.0-or-later, f084dc1). Every presenter
// Vulkan handle is borrowed; LSFG owns its private images, pipelines and
// synchronization. See lsfg_frame_gen.h.
#include <stdafx.h>
#include "lsfg_frame_gen.h"

#if LO_PLATFORM_SWITCH && defined(LO_GPU_PLUME)

#include <os/logger.h>
#include <plume_vulkan.h>

#include "lsfg-vk-backend/lsfgvk.hpp"
#include "lsfg-vk-common/vulkan/command_buffer.hpp"
#include "lsfg-vk-common/vulkan/fence.hpp"
#include "lsfg-vk-common/vulkan/semaphore.hpp"
#include "lsfg-vk-common/vulkan/vulkan.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// libnx's default native window (the one the swapchain was made for);
// <switch.h> is not included here (its Mutex/Event typedefs collide).
struct NWindow;
extern "C" NWindow* nwindowGetDefault(void);
extern "C" uint32_t nwindowSetSwapInterval(NWindow* window, uint32_t swapInterval);

namespace
{
VkImageMemoryBarrier ImageBarrier(VkImage image, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess,
                                  VkImageLayout oldLayout, VkImageLayout newLayout)
{
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = sourceAccess,
        .dstAccessMask = destinationAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
}

struct FrameSlot
{
    FrameSlot(const vk::Vulkan& vulkan, size_t generatedFrames)
        : frame(vulkan), restore(vulkan), completion(vulkan), acquired(vulkan), generated(vulkan), original(vulkan)
    {
        // The second and later generated frames each go to a freshly acquired image.
        for (size_t i = 1; i < generatedFrames; ++i)
        {
            extraCopy.emplace_back(vulkan);
            extraAcquired.emplace_back(vulkan);
            extraGenerated.emplace_back(vulkan);
        }
    }

    vk::CommandBuffer frame;
    vk::CommandBuffer restore;
    vk::Fence completion;
    vk::Semaphore acquired;
    vk::Semaphore generated;
    vk::Semaphore original;
    std::vector<vk::CommandBuffer> extraCopy;
    std::vector<vk::Semaphore> extraAcquired;
    std::vector<vk::Semaphore> extraGenerated;
    bool pending{};
};

float EnvFloat(const char* name, float fallback)
{
    const char* value = std::getenv(name);
    return value && *value ? std::strtof(value, nullptr) : fallback;
}

// LO_FRAMEGEN_LOCK=1: show every presented image for two refreshes. Each game
// frame is then a generated + real pair over four refreshes: the game is held
// to 15 fps and the screen gets an evenly paced 30 fps. Mesa's Switch WSI
// queues buffers with the NWindow's swap interval, which it sets to 1 only
// when a swapchain is created.
bool LockRequested()
{
    static const bool requested = [] {
        const char* value = std::getenv("LO_FRAMEGEN_LOCK");
        return value && value[0] == '1';
    }();
    return requested;
}

void SetSwapInterval(uint32_t interval)
{
    NWindow* window = nwindowGetDefault();
    const uint32_t result = window ? nwindowSetSwapInterval(window, interval) : 1;
    if (result != 0) LOG_WARNING("framegen: setting swap interval {} failed ({:#x})", interval, result);
}

// LO_FRAMEGEN_MULTIPLIER=3: two generated frames per game frame. At 60 Hz each
// of the three images is shown for one refresh, so the game is held to 20 fps
// and the screen gets an evenly paced 60 fps. 2 (default): one generated frame.
uint32_t Multiplier()
{
    static const uint32_t multiplier = [] {
        const char* value = std::getenv("LO_FRAMEGEN_MULTIPLIER");
        return value && value[0] == '3' ? 3u : 2u;
    }();
    return multiplier;
}

std::filesystem::path DataRoot()
{
#ifdef LO_SWITCH_DATA_ROOT
    return LO_SWITCH_DATA_ROOT;
#else
    return "/switch/LostOdysseyRecomp";
#endif
}

// One frame-generation session for one swapchain.
class Runtime
{
public:
    Runtime(plume::VulkanSwapChain& swapChain)
        : swapchain_(swapChain.vk), extent_{swapChain.width, swapChain.height}
    {
        auto* queue = swapChain.commandQueue;
        auto* device = queue->device;
        for (auto& texture : swapChain.textures) images_.push_back(texture.vk);
        if (images_.empty() || !extent_.width || !extent_.height) throw std::runtime_error("empty swapchain");
        if (!device->timelineSemaphoresSupported) throw std::runtime_error("timeline semaphores unavailable");

        float flowScale = EnvFloat("LO_FRAMEGEN_FLOW", 0.25f);
        if (!std::isfinite(flowScale) || flowScale < 0.125f || flowScale > 0.5f) flowScale = 0.25f;
        // The flow chain halves the flow image twice and then builds five more
        // levels from that (Beta1), so each flow dimension must be at least 125
        // pixels or the smallest level is 0 pixels and image creation fails
        // (NVK panics instead of returning an error). 720p handheld needs a flow
        // scale of at least ~0.18; use 128 px for margin against rounding.
        const float minFlowScale = 128.0f / static_cast<float>(std::min(extent_.width, extent_.height));
        if (minFlowScale > 1.0f) throw std::runtime_error("swapchain too small for frame generation");
        if (flowScale < minFlowScale)
        {
            LOG_WARNING("framegen: flow scale {} is too small for {}x{}; using {:.3f}", flowScale, extent_.width,
                        extent_.height, minFlowScale);
            flowScale = minFlowScale;
        }
        const char* perfValue = std::getenv("LO_FRAMEGEN_PERF");
        const bool performance = !(perfValue && perfValue[0] == '0');

        const lsfgvk::backend::BorrowedDevice borrowed{
            .instance = device->renderInterface->instance,
            .physicalDevice = device->physicalDevice,
            .device = device->vk,
            .queueFamilyIndex = queue->familyIndex,
            .queue = queue->queue->vk,
            .getInstanceProcAddr = vkGetInstanceProcAddr,
            .pipelineCachePath = DataRoot() / "cache" / "lsfg-vk-pipeline-cache.bin",
        };
        backend_ = std::make_unique<lsfgvk::backend::Instance>(borrowed, DataRoot() / "lsfg" / "Lossless.dll", false);
        generatedFrames_ = Multiplier() - 1;
        context_ = &backend_->openLocalContext(extent_.width, extent_.height, false, 1.0f / flowScale, performance,
                                               generatedFrames_, VK_QUEUE_FAMILY_IGNORED);
        vulkan_ = &backend_->vulkan();
        if (!vulkan_->df().AcquireNextImageKHR || !vulkan_->df().QueuePresentKHR)
            throw std::runtime_error("swapchain entry points are unavailable");
        const size_t slotCount = std::max<size_t>(images_.size(), 3);
        slots_.reserve(slotCount);
        for (size_t i = 0; i < slotCount; ++i) slots_.emplace_back(*vulkan_, generatedFrames_);
        queueMutex_ = queue->queue->mutex.get();
        worker_ = std::thread([this] { WorkerMain(); });
        LOG_NOTICE("framegen: LSFG ready {}x{} ({} swapchain images, flow scale {}, {} mode, x{})", extent_.width,
                   extent_.height, images_.size(), flowScale, performance ? "performance" : "quality",
                   generatedFrames_ + 1);
        if (generatedFrames_ > 1)
            LOG_NOTICE("framegen: x{}: one game frame per {} refreshes, every image shown for one",
                       generatedFrames_ + 1, generatedFrames_ + 1);
        else if (LockRequested())
        {
            SetSwapInterval(2);
            locked_ = true;
            LOG_NOTICE("framegen: locked to 15 game fps, 30 fps shown (swap interval 2)");
        }
    }

    ~Runtime()
    {
        if (worker_.joinable())
        {
            {
                std::lock_guard lock(jobMutex_);
                stopWorker_ = true;
            }
            jobWake_.notify_all();
            worker_.join();
        }
        if (locked_) SetSwapInterval(1);
        if (vulkan_ && vulkan_->df().DeviceWaitIdle) (void)vulkan_->df().DeviceWaitIdle(vulkan_->dev());
    }

    bool Matches(const plume::VulkanSwapChain& swapChain) const
    {
        return swapChain.vk == swapchain_ && swapChain.width == extent_.width && swapChain.height == extent_.height &&
               swapChain.textures.size() == images_.size();
    }

    // Generated frame, then the real one. Returns the real frame's result.
    VkResult Present(VkQueue queue, VkSemaphore wait, uint32_t imageIndex)
    {
        if (imageIndex >= images_.size()) throw std::runtime_error("swapchain image index out of range");
        FrameSlot& slot = PrepareSlot();
        // Source images alternate; the previous frame stays in the other one.
        const size_t sourceIndex = size_t((realFrame_ + 1) & 1);
        const VkImage source = backend_->sourceImage(*context_, sourceIndex);
        std::vector<VkSemaphore> waits;
        if (wait) waits.push_back(wait);

        slot.frame.begin(*vulkan_);
        RecordCapture(slot.frame, images_[imageIndex], source, sourceInitialized_[sourceIndex]);

        VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        info.swapchainCount = 1;
        info.pSwapchains = &swapchain_;

        if (realFrame_ == 0)
        {
            // Nothing to interpolate from yet: keep the frame and show it.
            slot.frame.end(*vulkan_);
            slot.frame.submit(*vulkan_, std::move(waits), VK_NULL_HANDLE, 0, {slot.original.handle()},
                              VK_NULL_HANDLE, 0, slot.completion.handle());
            sourceInitialized_[sourceIndex] = true;
            slot.pending = true;
            ++realFrame_;
            return PresentImage(queue, info, imageIndex, slot.original.handle());
        }

        backend_->recordFrame(*context_, slot.frame);
        // VI allows one acquired image: present the generated frame in this
        // image, reacquire, then copy the kept real frame into the next one.
        RecordCopyToSwapchain(slot.frame, backend_->destinationImage(*context_, 0), images_[imageIndex], true, true);
        slot.frame.end(*vulkan_);
        slot.frame.submit(*vulkan_, std::move(waits), VK_NULL_HANDLE, 0, {slot.generated.handle()}, VK_NULL_HANDLE,
                          0);
        sourceInitialized_[sourceIndex] = true;

        const VkResult generated = PresentImage(queue, info, imageIndex, slot.generated.handle());
        if (generated != VK_SUCCESS && generated != VK_SUBOPTIMAL_KHR) return generated;

        // The other generated frames and the real one are presented by the
        // worker thread: each of them first waits for a free swapchain image,
        // and the command processor goes on with the next frame meanwhile.
        // The caller waits for the worker before its next acquire (WaitWorker).
        slot.pending = true; // the worker submits the slot's fence
        ++realFrame_;
        {
            std::lock_guard lock(jobMutex_);
            job_ = Job{&slot, source, queue};
            jobPending_ = true;
        }
        jobWake_.notify_one();
        return generated;
    }

    // Waits until the worker has presented the previous frame's remaining
    // images. Must not be called with the queue mutex held. Throws on a
    // worker failure; returns the worker's last present result.
    VkResult WaitWorker()
    {
        std::unique_lock lock(jobMutex_);
        jobDone_.wait(lock, [this] { return !jobPending_ && !jobRunning_; });
        if (!workerError_.empty()) throw std::runtime_error(std::exchange(workerError_, {}));
        return std::exchange(workerResult_, VK_SUCCESS);
    }

private:
    struct Job
    {
        FrameSlot* slot{};
        VkImage source{};
        VkQueue queue{};
    };

    void WorkerMain()
    {
        for (;;)
        {
            Job job;
            {
                std::unique_lock lock(jobMutex_);
                jobWake_.wait(lock, [this] { return jobPending_ || stopWorker_; });
                if (stopWorker_) return;
                job = job_;
                jobPending_ = false;
                jobRunning_ = true;
            }
            VkResult result = VK_SUCCESS;
            std::string error;
            try
            {
                result = FinishFrame(job);
            }
            catch (const std::exception& e)
            {
                error = e.what();
            }
            {
                std::lock_guard lock(jobMutex_);
                jobRunning_ = false;
                if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) workerResult_ = result;
                if (!error.empty()) workerError_ = std::move(error);
            }
            jobDone_.notify_all();
        }
    }

    // Worker: the generated frames after the first, then the kept real frame,
    // each into a freshly acquired image. Acquires wait without the queue
    // mutex; submits and presents take it.
    VkResult FinishFrame(const Job& job)
    {
        FrameSlot& slot = *job.slot;
        VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        info.swapchainCount = 1;
        info.pSwapchains = &swapchain_;
        for (size_t i = 1; i < generatedFrames_; ++i)
        {
            uint32_t extraIndex = 0;
            const VkResult extraAcquire = vulkan_->df().AcquireNextImageKHR(
                vulkan_->dev(), swapchain_, UINT64_MAX, slot.extraAcquired[i - 1].handle(), VK_NULL_HANDLE, &extraIndex);
            if (extraAcquire != VK_SUCCESS && extraAcquire != VK_SUBOPTIMAL_KHR) return extraAcquire;
            if (extraIndex >= images_.size()) throw std::runtime_error("reacquired image index out of range");
            auto& copy = slot.extraCopy[i - 1];
            copy.begin(*vulkan_);
            RecordCopyToSwapchain(copy, backend_->destinationImage(*context_, i), images_[extraIndex], true, false);
            copy.end(*vulkan_);
            std::lock_guard lock(*queueMutex_);
            copy.submit(*vulkan_, {slot.extraAcquired[i - 1].handle()}, VK_NULL_HANDLE, 0,
                        {slot.extraGenerated[i - 1].handle()}, VK_NULL_HANDLE, 0);
            const VkResult extraPresent = PresentImage(job.queue, info, extraIndex, slot.extraGenerated[i - 1].handle());
            if (extraPresent != VK_SUCCESS && extraPresent != VK_SUBOPTIMAL_KHR) return extraPresent;
        }

        uint32_t realIndex = 0;
        const VkResult acquire = vulkan_->df().AcquireNextImageKHR(vulkan_->dev(), swapchain_, UINT64_MAX,
                                                                   slot.acquired.handle(), VK_NULL_HANDLE, &realIndex);
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) return acquire;
        if (realIndex >= images_.size()) throw std::runtime_error("reacquired image index out of range");
        slot.restore.begin(*vulkan_);
        RecordCopyToSwapchain(slot.restore, job.source, images_[realIndex], false, false);
        slot.restore.end(*vulkan_);
        std::lock_guard lock(*queueMutex_);
        slot.restore.submit(*vulkan_, {slot.acquired.handle()}, VK_NULL_HANDLE, 0, {slot.original.handle()},
                            VK_NULL_HANDLE, 0, slot.completion.handle());
        return PresentImage(job.queue, info, realIndex, slot.original.handle());
    }

    FrameSlot& PrepareSlot()
    {
        FrameSlot& slot = slots_.at(slotCursor_++ % slots_.size());
        if (slot.pending)
        {
            if (!slot.completion.wait(*vulkan_)) throw std::runtime_error("timeout waiting for an LSFG frame slot");
            slot.pending = false;
        }
        slot.completion.reset(*vulkan_);
        return slot;
    }

    void RecordCapture(const vk::CommandBuffer& command, VkImage swapchainImage, VkImage source,
                       bool sourceInitialized) const
    {
        command.copyImage(*vulkan_,
            {ImageBarrier(swapchainImage, VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
             ImageBarrier(source, sourceInitialized ? VK_ACCESS_SHADER_READ_BIT : 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                          sourceInitialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)},
            {swapchainImage, source}, extent_,
            {ImageBarrier(swapchainImage, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR),
             ImageBarrier(source, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL)});
    }

    // generatedOutput: from an LSFG destination image (written by compute),
    // otherwise from the kept real frame. presentedImage: into the image just
    // captured (in PRESENT_SRC), otherwise into a freshly acquired one.
    void RecordCopyToSwapchain(const vk::CommandBuffer& command, VkImage from, VkImage swapchainImage,
                               bool generatedOutput, bool presentedImage) const
    {
        const VkAccessFlags fromAccess = generatedOutput ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
        command.copyImage(*vulkan_,
            {ImageBarrier(from, fromAccess, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
             ImageBarrier(swapchainImage, presentedImage ? VK_ACCESS_MEMORY_READ_BIT : 0,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          presentedImage ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)},
            {from, swapchainImage}, extent_,
            {ImageBarrier(from, VK_ACCESS_TRANSFER_READ_BIT, fromAccess, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_GENERAL),
             ImageBarrier(swapchainImage, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)});
    }

    VkResult PresentImage(VkQueue queue, VkPresentInfoKHR info, uint32_t imageIndex, VkSemaphore wait) const
    {
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &wait;
        info.pImageIndices = &imageIndex;
        return vulkan_->df().QueuePresentKHR(queue, &info);
    }

    VkSwapchainKHR swapchain_{};
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::unique_ptr<lsfgvk::backend::Instance> backend_;
    lsfgvk::backend::Context* context_{};
    const vk::Vulkan* vulkan_{};
    std::vector<FrameSlot> slots_;
    std::array<bool, 2> sourceInitialized_{false, false};
    uint64_t realFrame_{};
    size_t generatedFrames_{1};
    std::mutex* queueMutex_{};
    std::thread worker_;
    std::mutex jobMutex_;
    std::condition_variable jobWake_, jobDone_;
    Job job_{};
    bool jobPending_{}, jobRunning_{}, stopWorker_{};
    VkResult workerResult_{VK_SUCCESS};
    std::string workerError_;
    size_t slotCursor_{};
    bool locked_{};
};

std::unique_ptr<Runtime> g_runtime;
std::atomic<bool> g_failed{false};
// Presenter-side cost of a frame-generated present, for the periodic log.
uint64_t g_frames = 0;
double g_presentMs = 0, g_maxPresentMs = 0;

void Fail(const char* what, const std::string& detail)
{
    g_runtime.reset();
    g_failed.store(true, std::memory_order_relaxed);
    LOG_ERROR("framegen: {} ({}); frame generation off for this session", what, detail);
}

// Waits for the worker's presents of the previous frame. A swapchain that went
// out of date there is flagged on the next call that has it. False after a
// worker failure (frame generation is then off).
bool g_workerOutOfDate = false;
bool WaitForWorker(plume::VulkanSwapChain* swapChain)
{
    if (!g_runtime) return true;
    try
    {
        if (g_runtime->WaitWorker() == VK_ERROR_OUT_OF_DATE_KHR) g_workerOutOfDate = true;
    }
    catch (const std::exception& error)
    {
        Fail("present failed", error.what());
        return false;
    }
    if (swapChain && std::exchange(g_workerOutOfDate, false)) swapChain->surfaceOutOfDate = true;
    return true;
}
}

namespace gpu::lsfg
{
bool Enabled()
{
    static const bool requested = [] {
        const char* value = std::getenv("LO_FRAMEGEN");
        return value && value[0] == '1';
    }();
    return requested && !g_failed.load(std::memory_order_relaxed);
}

bool Present(plume::RenderSwapChain* swapChain, uint32_t imageIndex, plume::RenderCommandSemaphore* waitSemaphore,
             bool& presented)
{
    if (!Enabled() || !swapChain) return false;
    auto& vulkanSwapChain = *static_cast<plume::VulkanSwapChain*>(swapChain);
    if (vulkanSwapChain.vk == VK_NULL_HANDLE) return false;
    // The generated frame is copied byte for byte: any 4-byte UNORM swapchain works.
    // Swapchain textures leave imageFormat unset; the create info has it.
    const VkFormat format = vulkanSwapChain.createInfo.imageFormat;
    if (format != VK_FORMAT_R8G8B8A8_UNORM && format != VK_FORMAT_B8G8R8A8_UNORM &&
        format != VK_FORMAT_R8G8B8A8_SRGB && format != VK_FORMAT_B8G8R8A8_SRGB)
    {
        Fail("unsupported swapchain format", std::to_string(int(format)));
        return false;
    }
    auto* queue = vulkanSwapChain.commandQueue;
    // The previous frame's remaining presents (worker thread) come first;
    // the worker takes the queue lock, so wait before taking it here.
    if (!WaitForWorker(&vulkanSwapChain)) return false;
    // LSFG submits and presents on the presenter's queue: hold its lock.
    std::scoped_lock lock(*queue->queue->mutex);
    try
    {
        if (g_runtime && !g_runtime->Matches(vulkanSwapChain)) g_runtime.reset();
        if (!g_runtime)
        {
            if (!std::filesystem::exists(DataRoot() / "lsfg" / "Lossless.dll"))
            {
                Fail("Lossless.dll not found", (DataRoot() / "lsfg" / "Lossless.dll").string());
                return false;
            }
            std::error_code ignored;
            std::filesystem::create_directories(DataRoot() / "cache", ignored);
            g_runtime = std::make_unique<Runtime>(vulkanSwapChain);
        }
    }
    catch (const std::exception& error)
    {
        Fail("setup failed", error.what());
        return false; // nothing was presented: the caller presents normally
    }
    try
    {
        const auto start = std::chrono::steady_clock::now();
        VkSemaphore wait = waitSemaphore ? static_cast<plume::VulkanCommandSemaphore*>(waitSemaphore)->vk : VK_NULL_HANDLE;
        const VkResult result = g_runtime->Present(queue->queue->vk, wait, imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) vulkanSwapChain.surfaceOutOfDate = true;
        presented = result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        g_presentMs += ms;
        g_maxPresentMs = std::max(g_maxPresentMs, ms);
        if (++g_frames % 300 == 0)
        {
            LOG_INFO("framegen: {} frames, present+generate {:.2f} ms avg, {:.2f} ms max (CPU wait incl. vsync)",
                     g_frames, g_presentMs / 300, g_maxPresentMs);
            g_presentMs = g_maxPresentMs = 0;
        }
        if (!presented) LOG_WARNING("framegen: present returned {}", int(result));
    }
    catch (const std::exception& error)
    {
        // The image may already be queued: report a failed present rather
        // than letting the caller present it a second time.
        Fail("present failed", error.what());
        presented = false;
    }
    return true;
}

void BeforeAcquire()
{
    (void)WaitForWorker(nullptr);
}

void Reset()
{
    if (!g_runtime) return;
    g_runtime.reset();
}
}

#else

namespace gpu::lsfg
{
bool Enabled() { return false; }
bool Present(plume::RenderSwapChain*, uint32_t, plume::RenderCommandSemaphore*, bool&) { return false; }
void Reset() {}
void BeforeAcquire() {}
}

#endif
