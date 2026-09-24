#include "probe_scene.h"
#include <sl_dlss_g.h>
#include <array>
#include <algorithm>
#include <fstream>
#include <cstdio>

namespace probe {
using namespace plume;
Scene::Scene(VulkanDevice& d, uint32_t rw, uint32_t rh, uint32_t ow, uint32_t oh) :
    device(d), renderWidth(rw), renderHeight(rh), outputWidth(ow), outputHeight(oh) {
    auto texture = [&](uint32_t width, uint32_t height, RenderFormat format) {
        return device.createTexture(RenderTextureDesc::Texture2D(width, height, 1, format,
            RenderTextureFlag::RENDER_TARGET | RenderTextureFlag::STORAGE));
    };
    color = texture(rw, rh, RenderFormat::R16G16B16A16_FLOAT);
    depth = texture(rw, rh, RenderFormat::R32_FLOAT);
    motion = texture(rw, rh, RenderFormat::R16G16_FLOAT);
    hudless = texture(ow, oh, RenderFormat::R16G16B16A16_FLOAT);
    ui = texture(ow, oh, RenderFormat::R16G16B16A16_FLOAT);
    readback = device.createBuffer(RenderBufferDesc::ReadbackBuffer(uint64_t(ow) * oh * 8));
    if (!color || !depth || !motion || !hudless || !ui || !readback) return;
    auto fb = [&](const RenderTexture* target) { return device.createFramebuffer(RenderFramebufferDesc(&target, 1)); };
    colorFb = fb(color.get()); depthFb = fb(depth.get()); motionFb = fb(motion.get());
    hudlessFb = fb(hudless.get()); uiFb = fb(ui.get());
    config.renderExtent = {rw, rh}; config.outputExtent = {ow, oh};
    config.quality = gpu::upscaling::DlssQuality::Quality;
    config.colorSpace = gpu::dlss::SrColorSpace::DisplayEncoded;
    config.deviceEpoch = 1;
    config.depthInverted = false;
    config.autoExposure = false;
    inputs.plan.deviceEpoch = 1;
    inputs.plan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
    const auto region = [](RenderTexture* t, uint32_t width, uint32_t height) -> gpu::temporal::TextureRegion {
        return {t, {width, height}, 0, 0, width, height};
    };
    inputs.color = region(color.get(), rw, rh);
    inputs.depth = region(depth.get(), rw, rh);
    inputs.motion = region(motion.get(), rw, rh);
    inputs.motionInvalidity = region(motion.get(), rw, rh);
    inputs.currentInputsComplete = true;
    inputs.colorEncoding = gpu::temporal::ColorEncoding::Sdr;
    inputs.depthConvention = gpu::temporal::DepthConvention::Forward;
}
bool Scene::Ready() const { return colorFb && depthFb && motionFb && hudlessFb && uiFb && readback; }
void Scene::Prefix(RenderCommandList& list, uint32_t frame) {
    list.begin();
    const auto prepare = [&list](RenderTexture* target) {
        list.barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(target, RenderTextureLayout::COLOR_WRITE));
    };
    prepare(color.get()); list.setFramebuffer(colorFb.get());
    // Repeatable moving stripes, displaced two render pixels per real frame.
    // MV is CURRENT -> PREVIOUS (-2 pixels), no jitter and no generated-frame tick.
    list.clearColor(0, RenderColor(.20f, .35f, .45f, 1));
    for (int32_t x = int32_t((frame * 2) % 80) - 80; x < int32_t(renderWidth); x += 80) {
        if (x + 24 <= 0) continue;
        const RenderRect rect(std::max(x, 0), 0, std::min(x + 24, int32_t(renderWidth)), int32_t(renderHeight));
        list.clearColor(0, RenderColor(.7f, .18f, .08f, 1), &rect, 1);
    }
    prepare(depth.get()); list.setFramebuffer(depthFb.get()); list.clearColor(0, RenderColor(.5f, 0, 0, 0));
    prepare(motion.get()); list.setFramebuffer(motionFb.get()); list.clearColor(0, RenderColor(-2.0f, 0, 0, 0));
    prepare(hudless.get()); list.setFramebuffer(hudlessFb.get()); list.clearColor(0, RenderColor(0, 0, 0, 0));
    prepare(ui.get()); list.setFramebuffer(uiFb.get()); list.clearColor(0, RenderColor(0, 0, 0, 0));
    RenderTextureBarrier barriers[] = {{color.get(), RenderTextureLayout::GENERAL}, {depth.get(), RenderTextureLayout::GENERAL},
        {motion.get(), RenderTextureLayout::GENERAL}, {hudless.get(), RenderTextureLayout::GENERAL}, {ui.get(), RenderTextureLayout::GENERAL}};
    list.barriers(RenderBarrierStage::ALL, nullptr, 0, barriers, 5);
    list.end();
    inputs.renderFrameId = frame;
    inputs.resetHistory = frame == 1;
    inputs.motionState = frame == 1 ? gpu::temporal::MotionState::ResetInitialization : gpu::temporal::MotionState::Tracked;
}
void Scene::Continuation(RenderCommandList& list, VkImage swapImage, VkExtent2D extent, bool capture) {
    list.begin();
    list.barriers(RenderBarrierStage::COPY, RenderTextureBarrier(hudless.get(), RenderTextureLayout::COPY_SOURCE));
    if (capture) list.copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(), RenderFormat::R16G16B16A16_FLOAT,
        outputWidth, outputHeight, 1, outputWidth), RenderTextureCopyLocation::Subresource(hudless.get()));
    VkImageMemoryBarrier before{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    before.srcAccessMask = 0;
    before.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    before.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    before.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    before.image = swapImage;
    before.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto cmd = static_cast<VulkanCommandList&>(list).vk;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &before);
    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[1] = {int32_t(outputWidth), int32_t(outputHeight), 1};
    blit.dstOffsets[1] = {int32_t(extent.width), int32_t(extent.height), 1};
    vkCmdBlitImage(cmd, static_cast<VulkanTexture*>(hudless.get())->vk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
    before.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    before.dstAccessMask = 0;
    before.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    before.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &before);
    list.end();
}
void Scene::Tags(sl::Resource (&resources)[4], sl::ResourceTag (&tags)[4]) const {
    const auto fill = [](sl::Resource& resource, RenderTexture* texture, VkImageLayout layout) {
        auto& vkTexture = *static_cast<VulkanTexture*>(texture);
        resource = sl::Resource{sl::ResourceType::eTex2d, reinterpret_cast<void*>(vkTexture.vk),
            reinterpret_cast<void*>(vkTexture.allocationInfo.deviceMemory), reinterpret_cast<void*>(vkTexture.imageView), uint32_t(layout)};
        resource.width = vkTexture.desc.width; resource.height = vkTexture.desc.height;
        resource.nativeFormat = uint32_t(vkTexture.imageFormat); resource.mipLevels = 1; resource.arrayLayers = 1;
        resource.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    };
    fill(resources[0], depth.get(), VK_IMAGE_LAYOUT_GENERAL);
    fill(resources[1], motion.get(), VK_IMAGE_LAYOUT_GENERAL);
    fill(resources[2], hudless.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    fill(resources[3], ui.get(), VK_IMAGE_LAYOUT_GENERAL);
    tags[0] = {&resources[0], sl::kBufferTypeDepth, sl::eValidUntilPresent};
    tags[1] = {&resources[1], sl::kBufferTypeMotionVectors, sl::eValidUntilPresent};
    tags[2] = {&resources[2], sl::kBufferTypeHUDLessColor, sl::eValidUntilPresent};
    tags[3] = {&resources[3], sl::kBufferTypeUIColorAndAlpha, sl::eValidUntilPresent};
    tags[0].extent = {0, 0, renderWidth, renderHeight};
    tags[1].extent = tags[0].extent;
    tags[2].extent = {0, 0, outputWidth, outputHeight};
    tags[3].extent = tags[2].extent;
}
uint64_t Scene::Capture(const std::filesystem::path& path) {
    auto* pixels = static_cast<const uint16_t*>(readback->map());
    if (!pixels) return 0;
    uint64_t hash = 1469598103934665603ull;
    const auto count = size_t(outputWidth) * outputHeight * 4;
    for (size_t i = 0; i < count; ++i) hash = (hash ^ pixels[i]) * 1099511628211ull;
    std::ofstream image(path, std::ios::binary);
    image.write(reinterpret_cast<const char*>(pixels), count * sizeof(uint16_t));
    readback->unmap();
    std::printf("HOST_HUDLESS_RGBA16F=%s size=%ux%u hash=%llu\n", path.string().c_str(), outputWidth, outputHeight,
        static_cast<unsigned long long>(hash));
    return hash;
}
}
