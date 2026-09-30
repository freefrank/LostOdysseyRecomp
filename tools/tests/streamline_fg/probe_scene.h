#pragma once
#include <gpu/dlss_ngx.h>
#include <gpu/temporal_frame_inputs.h>
#include <plume_vulkan.h>
#include <sl_dlss_g.h>
#include <filesystem>
#include <memory>
#include <string>

namespace probe {
struct Scene {
    plume::VulkanDevice& device;
    uint32_t renderWidth{}, renderHeight{}, outputWidth{}, outputHeight{};
    std::unique_ptr<plume::RenderTexture> color, depth, motion, hudless, ui;
    std::unique_ptr<plume::RenderFramebuffer> colorFb, depthFb, motionFb, hudlessFb, uiFb;
    std::unique_ptr<plume::RenderBuffer> readback;
    gpu::dlss::SrConfig config{};
    gpu::temporal::TemporalFrameInputs inputs{};
    Scene(plume::VulkanDevice& device, uint32_t renderWidth, uint32_t renderHeight,
          uint32_t outputWidth, uint32_t outputHeight);
    bool Ready() const;
    void Prefix(plume::RenderCommandList& list, uint32_t frame);
    void Continuation(plume::RenderCommandList& list, VkImage swapchainImage, VkExtent2D extent, bool capture);
    void Tags(sl::Resource (&resources)[4], sl::ResourceTag (&tags)[4]) const;
    uint64_t Capture(const std::filesystem::path& path);
};
}
