#include "dlss_fg_depth.h"
#if defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG))
#include "shader/dxc_compiler.h"
#include <plume_render_interface_builders.h>
#include <cmath>
#include <plume_d3d12.h>
#include <cstring>

namespace gpu::dlss_fg {
namespace {
constexpr const char* kShader = R"HLSL(
Texture2D<float> sourceDepth : register(t0);
#ifdef __spirv__
[[vk::binding(1,0)]]
#endif
cbuffer Parameters : register(b1) {
    float scale;
    float bias;
    uint originX;
    uint originY;
};
float4 vertex(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2,-2) + float2(-1,1), 0, 1);
}
float pixel(float4 position : SV_Position) : SV_Target0 {
    int2 at = int2(position.xy) + int2(originX,originY);
    float depth = sourceDepth.Load(int3(at,0));
    return isfinite(depth) ? saturate(depth * scale + bias) : 0.0;
}
)HLSL";
struct Constants { float scale, bias; uint32_t originX, originY; };
void Describe(plume::RenderDescriptorSetBuilder& b) {
    b.begin(); b.addTexture(0); b.addConstantBuffer(1); b.end();
}
}

bool DepthRemapper::Initialize(plume::RenderDevice* device) {
    if (pipeline_) return device == device_;
    if (!device) return false;
    const auto format=device->getCapabilities().shaderFormat;
    if (format != plume::RenderShaderFormat::SPIRV && format != plume::RenderShaderFormat::DXIL) return false;
    const auto binary=format==plume::RenderShaderFormat::SPIRV ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
    device_ = device;
    plume::RenderDescriptorSetBuilder set; Describe(set);
    plume::RenderPipelineLayoutBuilder builder;
    builder.begin(false,false); builder.addDescriptorSet(set); builder.end();
    layout_ = builder.create(device);
    auto vs = xenos::CompileCachedHlsl(kShader,"vertex","vs_6_0",binary);
    auto ps = xenos::CompileCachedHlsl(kShader,"pixel","ps_6_0",binary);
    if (!layout_ || !vs.ok || !ps.ok) return false;
    vertex_ = device->createShader(vs.bytecode.data(),vs.bytecode.size(),"vertex",format);
    pixel_ = device->createShader(ps.bytecode.data(),ps.bytecode.size(),"pixel",format);
    if (!vertex_ || !pixel_) return false;
    plume::RenderGraphicsPipelineDesc desc{};
    desc.pipelineLayout=layout_.get(); desc.vertexShader=vertex_.get(); desc.pixelShader=pixel_.get();
    desc.renderTargetCount=1; desc.renderTargetFormat[0]=plume::RenderFormat::R32_FLOAT;
    desc.renderTargetBlend[0]=plume::RenderBlendDesc::Copy();
    desc.renderTargetBlend[0].renderTargetWriteMask=0xF;
    desc.depthEnabled=desc.depthWriteEnabled=false;
    desc.cullMode=plume::RenderCullMode::NONE;
    desc.primitiveTopology=plume::RenderPrimitiveTopology::TRIANGLE_LIST;
    pipeline_=device->createGraphicsPipeline(desc);
    return bool(pipeline_);
}

plume::RenderTexture* DepthRemapper::Record(plume::RenderCommandList* commands,
    const temporal::TextureRegion& source, const DepthRemap& mapping) {
    if (!commands || !device_ || !pipeline_ || active_ || !source.Complete() ||
        source.width < 1 || source.height < 1 || !std::isfinite(mapping.scale) ||
        !std::isfinite(mapping.bias) || !(mapping.scale > 0) ||
        !(mapping.nearDistance > 0) || !(mapping.farDistance > mapping.nearDistance)) return nullptr;
    const bool vulkan=device_->getCapabilities().shaderFormat==plume::RenderShaderFormat::SPIRV;
    const plume::RenderTextureDesc* desc=nullptr;
    if (vulkan) {
        const auto& image=*static_cast<const plume::VulkanTexture*>(source.texture);
        if (image.device != device_ || !image.vk || !image.imageView || !image.allocation ||
            image.imageFormat != VK_FORMAT_R32_SFLOAT || image.textureLayout != plume::RenderTextureLayout::SHADER_READ) return nullptr;
        desc=&image.desc;
    } else {
        const auto& image=*static_cast<const plume::D3D12Texture*>(source.texture);
        if (image.device != device_ || !image.d3d ||
            !(image.resourceStates & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)) return nullptr;
        desc=&image.desc;
    }
    if (desc->dimension != plume::RenderTextureDimension::TEXTURE_2D ||
        desc->format != plume::RenderFormat::R32_FLOAT ||
        desc->width != source.allocation.width || desc->height != source.allocation.height ||
        desc->mipLevels != 1 || desc->arraySize != 1) return nullptr;
    std::unique_ptr<Active> next;
    if (available_ && available_->width == source.width && available_->height == source.height) {
        next=std::move(available_);
    } else {
        available_.reset(); // A changed extent needs a matching image and framebuffer.
        next=std::make_unique<Active>();
        next->width=source.width; next->height=source.height;
        next->output=device_->createTexture(plume::RenderTextureDesc::Texture2D(source.width,source.height,1,
            plume::RenderFormat::R32_FLOAT,plume::RenderTextureFlag::RENDER_TARGET));
        next->constants=device_->createBuffer(plume::RenderBufferDesc::UploadBuffer(256,plume::RenderBufferFlag::CONSTANT));
        plume::RenderDescriptorSetBuilder b; Describe(b); next->set=b.create(device_);
        if (!next->output || !next->constants || !next->set) return nullptr;
        const plume::RenderTexture* attachments[]={next->output.get()};
        next->framebuffer=device_->createFramebuffer(plume::RenderFramebufferDesc(attachments,1));
        if (!next->framebuffer) return nullptr;
    }
    auto* mapped=next->constants->map();
    if (!mapped) { available_=std::move(next); return nullptr; }
    const Constants constants{mapping.scale,mapping.bias,source.x,source.y};
    std::memcpy(mapped,&constants,sizeof(constants)); next->constants->unmap();
    next->set->setBuffer(1,next->constants.get(),256); // D3D12 CBV size must be 256-byte aligned.
    next->set->setTexture(0,source.texture,plume::RenderTextureLayout::SHADER_READ);
    // From here command recording can reference the batch even if it fails.
    active_=std::move(next);
    commands->barriers(plume::RenderBarrierStage::ALL,
        plume::RenderTextureBarrier(active_->output.get(),plume::RenderTextureLayout::COLOR_WRITE));
    commands->setFramebuffer(active_->framebuffer.get());
    plume::RenderViewport viewport(0,0,float(source.width),float(source.height));
    plume::RenderRect scissor(0,0,source.width,source.height);
    commands->setViewports(&viewport,1); commands->setScissors(&scissor,1);
    commands->setGraphicsPipelineLayout(layout_.get()); commands->setPipeline(pipeline_.get());
    commands->setGraphicsDescriptorSet(active_->set.get(),0);
    commands->drawInstanced(3,1,0,0);
    commands->barriers(plume::RenderBarrierStage::ALL,
        plume::RenderTextureBarrier(active_->output.get(),plume::RenderTextureLayout::SHADER_READ));
    return active_->output.get();
}

void DepthRemapper::ReleaseAfterInputDrain() {
    if (active_) available_=std::move(active_);
}
} // namespace gpu::dlss_fg
#endif
