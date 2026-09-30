#include <gpu/scene_copy_promotion_shaders.h>
#include <gpu/shader/dxc_compiler.h>
#include <plume_vulkan.h>
#include <plume_render_interface_builders.h>

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace plume;
namespace {
void Require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
void Checked(VkResult result, const char* operation) {
    Require(result == VK_SUCCESS, std::string(operation) + " raw_vk=" + std::to_string(result));
}
void Write(RenderBuffer* buffer, const void* bytes, size_t size) {
    void* mapped = buffer->map(); Require(mapped != nullptr, "upload map failed");
    std::memcpy(mapped, bytes, size); buffer->unmap();
}
// Vulkan raster/descriptor/readback test. Deliberately no NGX, game assets,
// renderer target-map model, or implied Gate 3 / image-quality acceptance.
class Fixture {
    std::unique_ptr<RenderInterface> api_;
    std::unique_ptr<RenderDevice> device_;
    std::unique_ptr<RenderCommandQueue> queue_;
    std::unique_ptr<RenderCommandList> cmd_;
    std::unique_ptr<RenderCommandFence> fence_;
    std::unique_ptr<RenderBuffer> constants_, upload_, readback_;
    RenderDescriptorSetBuilder builders_[2];
    std::unique_ptr<RenderDescriptorSet> sets_[2];
    std::unique_ptr<RenderPipelineLayout> layout_;
    std::unique_ptr<RenderShader> vertex_, rgba_, rgb_;
    std::unique_ptr<RenderPipeline> rgbaPipeline_, rgbPipeline_;
    static constexpr uint32_t Width = 8, Height = 8, RowPixels = 64;
    std::unique_ptr<RenderShader> Compile(const std::string& source, const char* profile) {
        auto compiled = xenos::CompileHlsl(source, "main", profile, xenos::ShaderBinaryFormat::Spirv);
        Require(compiled.ok, compiled.errors);
        auto shader = device_->createShader(compiled.bytecode.data(), compiled.bytecode.size(), "main", RenderShaderFormat::SPIRV);
        Require(bool(shader), "create shader failed"); return shader;
    }
    void Submit() {
        cmd_->end();
        const auto* device = static_cast<VulkanDevice*>(device_.get());
        const auto* queue = static_cast<VulkanCommandQueue*>(queue_.get());
        const auto* list = static_cast<VulkanCommandList*>(cmd_.get());
        const auto* fence = static_cast<VulkanCommandFence*>(fence_.get());
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1; submit.pCommandBuffers = &list->vk;
        Checked(vkResetFences(device->vk, 1, &fence->vk), "vkResetFences");
        Checked(vkQueueSubmit(queue->queue->vk, 1, &submit, fence->vk), "vkQueueSubmit");
        Checked(vkWaitForFences(device->vk, 1, &fence->vk, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    }
    void Fill(RenderTexture* texture, const std::vector<uint64_t>& pixels, uint32_t width, uint32_t height) {
        std::array<uint64_t, RowPixels * Height> padded{};
        for (uint32_t y = 0; y < height; ++y)
            std::memcpy(padded.data() + y * RowPixels, pixels.data() + y * width, width * 8);
        Write(upload_.get(), padded.data(), sizeof(padded));
        cmd_->begin();
        cmd_->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(texture, RenderTextureLayout::COPY_DEST));
        cmd_->copyTextureRegion(RenderTextureCopyLocation::Subresource(texture),
            RenderTextureCopyLocation::PlacedFootprint(upload_.get(), RenderFormat::R16G16B16A16_FLOAT, width, height, 1, RowPixels));
        cmd_->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(texture, RenderTextureLayout::SHADER_READ));
        Submit();
    }
public:
    Fixture() {
        api_ = CreateVulkanInterface(); Require(bool(api_), "Vulkan interface unavailable");
        device_ = api_->createDevice(); Require(bool(device_), "Vulkan device unavailable");
        std::printf("DEVICE=%s\n", device_->getDescription().name.c_str());
        queue_ = device_->createCommandQueue(RenderCommandListType::DIRECT);
        Require(bool(queue_), "queue unavailable");
        cmd_ = queue_->createCommandList(); fence_ = device_->createCommandFence();
        constants_ = device_->createBuffer(RenderBufferDesc::UploadBuffer(256,
            RenderBufferFlag::CONSTANT | RenderBufferFlag::DEVICE_ADDRESSABLE));
        upload_ = device_->createBuffer(RenderBufferDesc::UploadBuffer(RowPixels * Height * 8));
        readback_ = device_->createBuffer(RenderBufferDesc::ReadbackBuffer(RowPixels * Height * 8));
        Require(cmd_ && fence_ && constants_ && upload_ && readback_, "fixture allocation failed");
        // Keep space1, t0/t1 and the actual shared-address push-constant ABI.
        builders_[0].begin(); builders_[0].addByteAddressBuffer(0); builders_[0].end();
        builders_[1].begin(); builders_[1].addTexture(0); builders_[1].addTexture(1); builders_[1].end();
        for (unsigned i = 0; i < 2; ++i) {
            sets_[i] = builders_[i].create(device_.get()); Require(bool(sets_[i]), "descriptor allocation failed");
        }
        // The unused space0 binding does not participate in either shader.
        RenderPipelineLayoutBuilder builder;
        builder.begin(false, false);
        builder.addPushConstant(0, 0, 24, RenderShaderStageFlag::VERTEX | RenderShaderStageFlag::PIXEL);
        for (auto& set : builders_) builder.addDescriptorSet(set);
        builder.end(); layout_ = builder.create(device_.get()); Require(bool(layout_), "pipeline layout failed");
        vertex_ = Compile(gpu::scene_copy_promotion::VertexShader, "vs_6_0");
        rgba_ = Compile(gpu::scene_copy_promotion::RgbaShader, "ps_6_0");
        rgb_ = Compile(gpu::scene_copy_promotion::RgbShader, "ps_6_0");
        const auto pipeline = [&](RenderShader* ps) {
            RenderGraphicsPipelineDesc desc;
            desc.pipelineLayout = layout_.get(); desc.vertexShader = vertex_.get(); desc.pixelShader = ps;
            desc.renderTargetCount = 1; desc.renderTargetFormat[0] = RenderFormat::R16G16B16A16_FLOAT;
            desc.renderTargetBlend[0] = RenderBlendDesc::Copy(); desc.renderTargetBlend[0].renderTargetWriteMask = 0xF;
            desc.depthEnabled = desc.depthWriteEnabled = false; desc.cullMode = RenderCullMode::NONE;
            desc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
            return device_->createGraphicsPipeline(desc);
        };
        rgbaPipeline_ = pipeline(rgba_.get()); rgbPipeline_ = pipeline(rgb_.get());
        Require(rgbaPipeline_ && rgbPipeline_, "graphics pipeline creation failed");
    }
    void Run(bool composite, uint32_t baseWidth, uint32_t baseHeight, uint32_t srWidth, uint32_t srHeight) {
        const auto texture = [&](uint32_t w, uint32_t h, bool rt) {
            return device_->createTexture(RenderTextureDesc::Texture2D(w, h, 1, RenderFormat::R16G16B16A16_FLOAT,
                rt ? RenderTextureFlag::RENDER_TARGET : RenderTextureFlag::NONE));
        };
        auto base = texture(baseWidth, baseHeight, false), scratch = texture(srWidth, srHeight, false);
        auto output = texture(Width, Height, true);
        Require(base && scratch && output, "texture allocation failed");
        const RenderTexture* attachment[] = {output.get()};
        auto framebuffer = device_->createFramebuffer(RenderFramebufferDesc(attachment, 1));
        Require(bool(framebuffer), "framebuffer failed");
        std::vector<uint64_t> basePixels(baseWidth * baseHeight), srPixels(srWidth * srHeight);
        const auto pack = [](uint64_t r, uint64_t g, uint64_t b, uint64_t a) {
            return r | (g << 16) | (b << 32) | (a << 48);
        };
        constexpr uint16_t alpha[] = {0, 0x3000, 0x3400, 0x3600, 0x3800, 0x3900, 0x3a00, 0x3c00};
        for (unsigned i = 0; i < basePixels.size(); ++i)
            basePixels[i] = pack(0x3400 + i * 4, 0x3000 + i * 4, 0x3a00 - i * 4, alpha[i % 8]);
        for (unsigned i = 0; i < srPixels.size(); ++i)
            srPixels[i] = pack(0x4000 + i * 4, 0x2800 + i * 4, 0x3800 + i * 4, 0x3c00);
        Fill(base.get(), basePixels, baseWidth, baseHeight); Fill(scratch.get(), srPixels, srWidth, srHeight);
        std::array<uint32_t, 64> constants{};
        constants[60] = std::bit_cast<uint32_t>(float(baseWidth) / Width);
        constants[61] = std::bit_cast<uint32_t>(float(baseHeight) / Height);
        Write(constants_.get(), constants.data(), sizeof(constants));
        sets_[1]->setTexture(0, base.get(), RenderTextureLayout::SHADER_READ);
        sets_[1]->setTexture(1, scratch.get(), RenderTextureLayout::SHADER_READ);
        cmd_->begin();
        cmd_->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(output.get(), RenderTextureLayout::COLOR_WRITE));
        cmd_->setFramebuffer(framebuffer.get()); cmd_->clearColor(0, RenderColor(1, 0, 1, 1));
        RenderViewport viewport(0, 0, Width, Height); RenderRect scissor(0, 0, Width, Height);
        cmd_->setViewports(&viewport, 1); cmd_->setScissors(&scissor, 1);
        cmd_->setGraphicsPipelineLayout(layout_.get()); cmd_->setPipeline(composite ? rgbPipeline_.get() : rgbaPipeline_.get());
        const uint64_t addresses[] = {constants_->getDeviceAddress(), constants_->getDeviceAddress(), constants_->getDeviceAddress()};
        cmd_->setGraphicsPushConstants(0, addresses); cmd_->setGraphicsDescriptorSet(sets_[1].get(), 1);
        cmd_->drawInstanced(3, 1, 0, 0);
        cmd_->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(output.get(), RenderTextureLayout::COPY_SOURCE));
        cmd_->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback_.get(), RenderFormat::R16G16B16A16_FLOAT,
            Width, Height, 1, RowPixels), RenderTextureCopyLocation::Subresource(output.get()));
        Submit();
        const auto* pixels = static_cast<const uint64_t*>(readback_->map()); Require(pixels != nullptr, "readback map failed");
        unsigned mismatches = 0;
        for (unsigned y = 0; y < Height; ++y) for (unsigned x = 0; x < Width; ++x) {
            const unsigned bx = unsigned((x + .5f) * baseWidth / Width), by = unsigned((y + .5f) * baseHeight / Height);
            const uint64_t b = basePixels[by * baseWidth + bx];
            const uint64_t expected = composite && x < srWidth && y < srHeight ?
                (srPixels[y * srWidth + x] & 0x0000ffffffffffffull) | (b & 0xffff000000000000ull) : b;
            mismatches += pixels[y * RowPixels + x] != expected;
        }
        readback_->unmap();
        std::printf("%s base=%ux%u scratch=%ux%u output=8x8 fp16 checked=64 mismatches=%u\n",
            composite ? "RGB_ALPHA_PADDING" : "RGBA_RESAMPLE", baseWidth, baseHeight, srWidth, srHeight, mismatches);
        Require(!mismatches, "production shader pixel mismatch");
    }
};
}
int main() {
    try {
        Fixture fixture;
        fixture.Run(false, 4, 4, 8, 8);
        fixture.Run(true, 8, 8, 8, 8);
        fixture.Run(true, 8, 8, 6, 5);
        fixture.Run(true, 4, 4, 5, 7);
        std::puts("PASS: 256 exact RGBA pixel checks; no NGX or renderer mapping execution claimed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what()); return 1;
    }
}
