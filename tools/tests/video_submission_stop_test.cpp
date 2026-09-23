// Drives production SubmitVulkan and WaitForGpuFence. The fault returns before
// vkQueueSubmit / vkWaitForFences; the stop entry still publishes the snapshot.
#include "gpu/frame_plan.h"
#include "gpu/upscaling_plan.h"
#include "gpu/video.h"

#include <plume_render_interface.h>
#include <plume_vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace gpu::video {
void ConfigureSubmissionProbe(plume::RenderCommandQueue* queue, int32_t submitFault, int32_t waitFault, bool resetStop);
}

namespace {
int checks = 0;
void Require(bool ok, const char* message)
{
    ++checks;
    if (!ok) throw std::runtime_error(message);
}

gpu::frame_plan::DlssEffectSnapshot Describe(const gpu::upscaling::BackendDeviceSnapshot& device)
{
    gpu::upscaling::OutputSizing sizing;
    sizing.key = {device.deviceEpoch, 8, 8};
    sizing.modes[0].state = gpu::upscaling::SizingState::Ready;
    sizing.modes[0].optimal = sizing.modes[0].minimum = sizing.modes[0].maximum = {4, 4};
    gpu::frame_plan::FramePlan plan;
    plan.cpuSerial = 1;
    plan.geometryEpoch = 1;
    plan.deviceEpoch = device.deviceEpoch;
    plan.requestSignature = 0x51;
    plan.requestedUpscaler = gpu::upscaling::Upscaler::Dlss;
    plan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
    plan.dlssQuality = gpu::upscaling::DlssQuality::Quality;
    plan.width = 4;
    plan.height = 4;
    plan.output = {{8, 8}, 0, 0, 8, 8};
    gpu::frame_plan::DlssExecutionObservation execution;
    execution.plan = plan;
    execution.renderFrame = 1;
    execution.submissionSerial = 4;
    execution.outcome = gpu::frame_plan::DlssExecutionOutcome::Submitted;
    execution.reason = gpu::frame_plan::DlssEffectReason::None;
    gpu::frame_plan::PlannerObservation observed;
    observed.hasPlan = true;
    observed.plan = plan;
    observed.execution = execution;
    return gpu::frame_plan::DescribeDlssRuntime(device, observed, &sizing);
}
}

int main()
{
    try {
        auto api = plume::CreateVulkanInterface();
        Require(bool(api), "vulkan interface");
        auto device = api->createDevice();
        Require(bool(device), "vulkan device");
        auto queue = device->createCommandQueue(plume::RenderCommandListType::DIRECT);
        auto list = queue->createCommandList();
        auto fence = device->createCommandFence();
        Require(bool(queue && list && fence), "queue, list, and fence");
        Require(gpu::video::BeginGpuCommands(list.get()) && gpu::video::EndGpuCommands(list.get()), "empty command buffer");

        gpu::video::ConfigureSubmissionProbe(queue.get(), -3, 0, true);
        Require(!gpu::upscaling::PublishedDeviceCapability().gpuWorkStopped, "probe reset publishes not stopped");
        const plume::RenderCommandList* lists[] = {list.get()};
        uint64_t serial = 7;
        int32_t raw = 0;
        Require(!gpu::video::SubmitRendererBatch(lists, 1, fence.get(), serial, raw) && raw == -3,
            "production submit entry must fail before vkQueueSubmit");
        const auto submittedStop = gpu::upscaling::PublishedDeviceCapability();
        Require(submittedStop.gpuWorkStopped && gpu::video::GpuWorkStopped(),
            "SubmitVulkan publishes gpuWorkStopped after VulkanState stops");
        auto eligible = submittedStop;
        eligible.gpuWorkStopped = false;
        eligible.backend = gpu::backend::Backend::Vulkan;
        eligible.deviceReady = true;
        eligible.dlssAvailable = true;
        Require(Describe(eligible).phase == gpu::frame_plan::DlssEffectPhase::Active,
            "the same execution is active when the published stop bit is clear");
        const auto hiddenBySubmit = Describe(submittedStop);
        Require(hiddenBySubmit.phase == gpu::frame_plan::DlssEffectPhase::GpuStopped && !hiddenBySubmit.execution,
            "the snapshot published by SubmitVulkan hides the previous active submission");

        gpu::video::ConfigureSubmissionProbe(queue.get(), 0, -7, true);
        Require(!gpu::upscaling::PublishedDeviceCapability().gpuWorkStopped, "wait case starts from a cleared publish");
        Require(!gpu::video::WaitForGpuFence(fence.get()), "production fence wait entry must fail before vkWaitForFences");
        const auto waitedStop = gpu::upscaling::PublishedDeviceCapability();
        Require(waitedStop.gpuWorkStopped && gpu::video::GpuWorkStopped(),
            "WaitForGpuFence publishes gpuWorkStopped after the wait stops");
        const auto hiddenByWait = Describe(waitedStop);
        Require(hiddenByWait.phase == gpu::frame_plan::DlssEffectPhase::GpuStopped && !hiddenByWait.execution,
            "the snapshot published by WaitForGpuFence hides the previous active submission");
        const auto idle = vkDeviceWaitIdle(static_cast<plume::VulkanDevice*>(device.get())->vk);
        Require(idle == VK_SUCCESS || idle == VK_ERROR_DEVICE_LOST, "device idle");
        std::printf("PASS: %d production Vulkan stop publications (submit and fence wait; no present, NGX, or failed queue submission)\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
