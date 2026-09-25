#pragma once
#include "temporal_frame_inputs.h"
#ifdef LO_GPU_PLUME
#include <plume_vulkan.h>

namespace gpu::temporal {
// Only the hybrid producer's confidence field is opted into SDK history bias.
// Legacy geometric invalidity is not silently reinterpreted as material alpha.
inline bool ValidSrHybridMask(const TemporalFrameInputs& inputs, const plume::VulkanDevice* device,
    plume::RenderTextureLayout expectedLayout = plume::RenderTextureLayout::SHADER_READ) {
    if (inputs.motionState != MotionState::Hybrid) return true;
    const auto& region = inputs.motionInvalidity;
    if (!region.Complete() || region.x || region.y || region.width != inputs.plan.width ||
        region.height != inputs.plan.height || region.allocation.width != region.width ||
        region.allocation.height != region.height) return false;
    const auto* image = static_cast<const plume::VulkanTexture*>(region.texture);
    return image->device == device && image->vk && image->imageView && image->allocation &&
        image->imageFormat == VK_FORMAT_R8_UNORM && image->desc.format == plume::RenderFormat::R8_UNORM &&
        image->desc.width == region.width && image->desc.height == region.height &&
        image->desc.dimension == plume::RenderTextureDimension::TEXTURE_2D &&
        image->desc.mipLevels == 1 && image->desc.arraySize == 1 &&
        image->imageSubresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT &&
        image->imageSubresourceRange.baseMipLevel == 0 && image->imageSubresourceRange.levelCount == 1 &&
        image->imageSubresourceRange.baseArrayLayer == 0 && image->imageSubresourceRange.layerCount == 1 &&
        image->textureLayout == expectedLayout;
}
} // namespace gpu::temporal
#endif
