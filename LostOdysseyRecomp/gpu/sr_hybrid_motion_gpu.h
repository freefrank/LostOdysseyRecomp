#pragma once
#include "sr_hybrid_motion.h"
#ifdef LO_GPU_PLUME
#include "shader/dxc_compiler.h"
#include <plume_vulkan.h>
#include <plume_render_interface_builders.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace gpu::temporal {
// No readback, extra queue submission or wait. Each batch owns its descriptors,
// constants and outputs until the existing HistoryOwner serial is completed.
class SrHybridMotionGPU {
    struct Batch {
        uint64_t serial = 0;
        uint32_t width = 0, height = 0;
        std::unique_ptr<plume::RenderTexture> motion, uncertainty;
        std::unique_ptr<plume::RenderBuffer> constants;
        std::unique_ptr<plume::RenderDescriptorSet> set;
        std::unique_ptr<plume::RenderFramebuffer> framebuffer;
    };
    plume::RenderDevice* device_ = nullptr;
    std::unique_ptr<plume::RenderPipelineLayout> layout_;
    std::unique_ptr<plume::RenderShader> vertex_, pixel_;
    std::unique_ptr<plume::RenderPipeline> pipeline_;
    std::vector<std::unique_ptr<Batch>> batches_;
    Batch* active_ = nullptr;
    uint64_t completed_ = 0;
    bool initAttempted_ = false, resourceFailed_ = false;
    static constexpr size_t kMaxBatches = 8;
    static void Describe(plume::RenderDescriptorSetBuilder& b) {
        b.begin(); b.addTexture(0); b.addTexture(1); b.addTexture(2);
        b.addConstantBuffer(3); b.end();
    }
    // Caller first verifies the Vulkan backend. No base RenderTexture exposes
    // allocation metadata; validate the native image, exact subresource and layout.
    static bool ImageMatches(plume::RenderTexture* texture, plume::RenderDevice* device,
        uint32_t width, uint32_t height, plume::RenderFormat format, VkFormat nativeFormat) {
        if (!texture) return false;
        const auto& image = *static_cast<const plume::VulkanTexture*>(texture);
        return image.device == device && image.vk && image.imageView && image.allocation &&
            image.desc.width == width && image.desc.height == height && image.desc.format == format &&
            image.imageFormat == nativeFormat && image.desc.dimension == plume::RenderTextureDimension::TEXTURE_2D &&
            image.desc.mipLevels == 1 && image.desc.arraySize == 1 &&
            image.imageSubresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT &&
            image.imageSubresourceRange.baseMipLevel == 0 && image.imageSubresourceRange.levelCount == 1 &&
            image.imageSubresourceRange.baseArrayLayer == 0 && image.imageSubresourceRange.layerCount == 1 &&
            image.textureLayout == plume::RenderTextureLayout::SHADER_READ;
    }
    bool Init(plume::RenderDevice* device) {
        if (initAttempted_) return device == device_ && bool(pipeline_);
        initAttempted_ = true; device_ = device;
        if (!device || device->getCapabilities().shaderFormat != plume::RenderShaderFormat::SPIRV) return false;
        plume::RenderDescriptorSetBuilder set; Describe(set);
        plume::RenderPipelineLayoutBuilder b;
        b.begin(false,false); b.addDescriptorSet(set); b.end(); layout_ = b.create(device);
        auto vs = xenos::CompileCachedHlsl(kSrHybridMotionShader,"vertex","vs_6_0",xenos::ShaderBinaryFormat::Spirv);
        auto ps = xenos::CompileCachedHlsl(kSrHybridMotionShader,"pixel","ps_6_0",xenos::ShaderBinaryFormat::Spirv);
        if (!layout_ || !vs.ok || !ps.ok) return false;
        vertex_ = device->createShader(vs.bytecode.data(),vs.bytecode.size(),"vertex",plume::RenderShaderFormat::SPIRV);
        pixel_ = device->createShader(ps.bytecode.data(),ps.bytecode.size(),"pixel",plume::RenderShaderFormat::SPIRV);
        if (!vertex_ || !pixel_) return false;
        plume::RenderGraphicsPipelineDesc desc{};
        desc.pipelineLayout = layout_.get(); desc.vertexShader = vertex_.get(); desc.pixelShader = pixel_.get();
        desc.renderTargetCount = 2;
        desc.renderTargetFormat[0] = plume::RenderFormat::R16G16_FLOAT;
        desc.renderTargetFormat[1] = plume::RenderFormat::R8_UNORM;
        for (unsigned i = 0; i < 2; ++i) {
            desc.renderTargetBlend[i] = plume::RenderBlendDesc::Copy();
            desc.renderTargetBlend[i].renderTargetWriteMask = 0xF;
        }
        desc.depthEnabled = desc.depthWriteEnabled = false;
        desc.cullMode = plume::RenderCullMode::NONE;
        desc.primitiveTopology = plume::RenderPrimitiveTopology::TRIANGLE_LIST;
        pipeline_ = device->createGraphicsPipeline(desc);
        batches_.reserve(kMaxBatches);
        return bool(pipeline_);
    }
    std::unique_ptr<Batch> Allocate(uint32_t width, uint32_t height) {
        auto batch = std::make_unique<Batch>(); batch->width = width; batch->height = height;
        batch->motion = device_->createTexture(plume::RenderTextureDesc::Texture2D(width,height,1,
            plume::RenderFormat::R16G16_FLOAT,plume::RenderTextureFlag::RENDER_TARGET));
        batch->uncertainty = device_->createTexture(plume::RenderTextureDesc::Texture2D(width,height,1,
            plume::RenderFormat::R8_UNORM,plume::RenderTextureFlag::RENDER_TARGET));
        batch->constants = device_->createBuffer(plume::RenderBufferDesc::UploadBuffer(
            sizeof(SrHybridConstants),plume::RenderBufferFlag::CONSTANT));
        plume::RenderDescriptorSetBuilder b; Describe(b); batch->set = b.create(device_);
        if (!batch->motion || !batch->uncertainty || !batch->constants || !batch->set) return {};
        const plume::RenderTexture* attachments[] = {batch->motion.get(),batch->uncertainty.get()};
        batch->framebuffer = device_->createFramebuffer(plume::RenderFramebufferDesc(attachments,2));
        if (!batch->framebuffer) return {};
        return batch;
    }
public:
    bool ResourceFailed() const { return resourceFailed_; }
    size_t BatchCount() const { return batches_.size(); }
    void RecordConsumerUse(uint64_t serial) {
        if (active_) active_->serial = std::max(active_->serial,serial);
    }
    void ReleaseCompletedThrough(uint64_t serial) { completed_ = std::max(completed_,serial); }
    MotionFrameView Render(plume::RenderDevice* device, plume::RenderCommandList* commands,
        plume::RenderTexture* depth, const Camera& current, const Camera* previous,
        const MotionFrameView* geometry, uint64_t frame, uint64_t epoch, uint64_t allocation,
        uint32_t width, uint32_t height, double jx, double jy, bool reset, uint64_t serial) {
        resourceFailed_ = false;
        if (!device || !commands || !serial || device->getCapabilities().shaderFormat != plume::RenderShaderFormat::SPIRV ||
            !ImageMatches(depth,device,width,height,plume::RenderFormat::R32_FLOAT,VK_FORMAT_R32_SFLOAT)) return {};
        // A stale ready view is a contract failure, not a coverage gap.
        if (geometry && geometry->ready && !SrHybridGeometryMatches(*geometry,frame,epoch,allocation,width,height)) return {};
        auto constants = MakeSrHybridConstants(current,previous,width,height,jx,jy,reset);
        if (!constants) return {};
        const bool overlay = geometry && SrHybridGeometryMatches(*geometry,frame,epoch,allocation,width,height);
        if (overlay && (!ImageMatches(geometry->velocity,device,width,height,plume::RenderFormat::R16G16_FLOAT,VK_FORMAT_R16G16_SFLOAT) ||
            !ImageMatches(geometry->reactive,device,width,height,plume::RenderFormat::R8_UNORM,VK_FORMAT_R8_UNORM))) return {};
        if (!Init(device)) { resourceFailed_ = true; return {}; }
        // Never recycle the currently exposed result even when its last known
        // fence completed: a later consumer in the same frame may still use it.
        size_t index = 0;
        for (; index < batches_.size(); ++index)
            if (batches_[index].get() != active_ && batches_[index]->serial <= completed_) break;
        if (index == batches_.size() && index == kMaxBatches) return {};
        if (index == batches_.size() || batches_[index]->width != width || batches_[index]->height != height) {
            auto replacement = Allocate(width,height);
            if (!replacement) { resourceFailed_ = true; return {}; }
            if (index == batches_.size()) batches_.push_back(std::move(replacement));
            else batches_[index] = std::move(replacement); // old batch already completed
        }
        auto& batch = *batches_[index];
        constants->options[1] = overlay ? 1u : 0u;
        void* mapped = batch.constants->map();
        if (!mapped) { resourceFailed_ = true; return {}; }
        std::memcpy(mapped,&*constants,sizeof(*constants)); batch.constants->unmap();
        batch.set->setBuffer(3,batch.constants.get(),sizeof(*constants));
        batch.set->setTexture(0,depth,plume::RenderTextureLayout::SHADER_READ);
        batch.set->setTexture(1,overlay ? geometry->velocity : depth,plume::RenderTextureLayout::SHADER_READ);
        batch.set->setTexture(2,overlay ? geometry->reactive : depth,plume::RenderTextureLayout::SHADER_READ);
        commands->barriers(plume::RenderBarrierStage::ALL,
            plume::RenderTextureBarrier(batch.motion.get(),plume::RenderTextureLayout::COLOR_WRITE));
        commands->barriers(plume::RenderBarrierStage::ALL,
            plume::RenderTextureBarrier(batch.uncertainty.get(),plume::RenderTextureLayout::COLOR_WRITE));
        commands->setFramebuffer(batch.framebuffer.get());
        plume::RenderViewport viewport(0,0,float(width),float(height)); plume::RenderRect scissor(0,0,width,height);
        commands->setViewports(&viewport,1); commands->setScissors(&scissor,1);
        commands->setGraphicsPipelineLayout(layout_.get()); commands->setPipeline(pipeline_.get());
        commands->setGraphicsDescriptorSet(batch.set.get(),0); commands->drawInstanced(3,1,0,0);
        commands->barriers(plume::RenderBarrierStage::ALL,
            plume::RenderTextureBarrier(batch.motion.get(),plume::RenderTextureLayout::SHADER_READ));
        commands->barriers(plume::RenderBarrierStage::ALL,
            plume::RenderTextureBarrier(batch.uncertainty.get(),plume::RenderTextureLayout::SHADER_READ));
        batch.serial = serial; active_ = &batch;
        return {batch.motion.get(),nullptr,batch.uncertainty.get(),frame,epoch,allocation,width,height,true,MotionState::Hybrid};
    }
};
} // namespace gpu::temporal
#endif
