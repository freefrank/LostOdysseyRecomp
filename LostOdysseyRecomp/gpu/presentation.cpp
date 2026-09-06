#include "presentation.h"
#include "shader/dxc_compiler.h"
#include <os/logger.h>
#include <stdafx.h>
#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
namespace gpu
{
using namespace plume;
struct Presentation::Impl
{
    RenderDevice *device = nullptr;
    std::unique_ptr<RenderPipelineLayout> layout;
    std::unique_ptr<RenderShader> vs, ps;
    std::unique_ptr<RenderPipeline> pipeline;
    std::unique_ptr<RenderSampler> sampler;
    std::unique_ptr<RenderDescriptorSet> descriptors;
    std::unique_ptr<RenderFramebuffer> framebuffer;
};
Presentation::Presentation() : impl(std::make_unique<Impl>())
{
}
Presentation::~Presentation() = default;
bool Presentation::Init(RenderDevice *device)
{
    auto &p = *impl;
    p.device = device;
    const char *source = R"(
Texture2D<float4> frame : register(t0);
SamplerState linearClamp : register(s0);
cbuffer Parameters : register(b0) { float2 origin; float2 extent; float2 imageSize; uint aa; uint pad; };
float4 vertex(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2,-2) + float2(-1,1),0,1);
}
float3 sampleFrame(float2 pixel) {
    uint w,h; frame.GetDimensions(w,h);
    return frame.SampleLevel(linearClamp, clamp(pixel,0.5,imageSize-0.5)/float2(w,h),0).rgb;
}
float luma(float3 c) { return dot(c,float3(0.299,0.587,0.114)); }
float4 pixel(float4 position : SV_Position) : SV_Target {
    float2 p = (position.xy-origin)/extent*imageSize;
    float3 center = sampleFrame(p);
    if (!aa) return float4(center,1);
    float nw=luma(sampleFrame(p+float2(-1,-1))), ne=luma(sampleFrame(p+float2(1,-1)));
    float sw=luma(sampleFrame(p+float2(-1,1))), se=luma(sampleFrame(p+float2(1,1)));
    float mid=luma(center), lo=min(mid,min(min(nw,ne),min(sw,se))), hi=max(mid,max(max(nw,ne),max(sw,se)));
    if (hi-lo < max(0.0312,hi*0.125)) return float4(center,1);
    float2 direction=float2(-((nw+ne)-(sw+se)),(nw+sw)-(ne+se));
    float reduce=max((nw+ne+sw+se)*0.03125,0.0078125);
    direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),-8,8);
    float3 a=0.5*(sampleFrame(p+direction*(-1.0/6.0))+sampleFrame(p+direction*(1.0/6.0)));
    float3 b=a*0.5+0.25*(sampleFrame(p-direction*0.5)+sampleFrame(p+direction*0.5));
    float lb=luma(b);
    return float4((lb<lo || lb>hi)?a:b,1);
})";
    auto vs = xenos::CompileHlsl(source, "vertex", "vs_6_0");
    auto ps = xenos::CompileHlsl(source, "pixel", "ps_6_0");
    if (!vs.ok || !ps.ok)
    {
        LOG_WARNING("presentation shaders: {} {}", vs.errors, ps.errors);
        return false;
    }
    p.vs = device->createShader(vs.dxil.data(), vs.dxil.size(), "vertex", RenderShaderFormat::DXIL);
    p.ps = device->createShader(ps.dxil.data(), ps.dxil.size(), "pixel", RenderShaderFormat::DXIL);
    RenderDescriptorSetBuilder set;
    set.begin();
    set.addTexture(0);
    set.addSampler(0);
    set.end();
    RenderPipelineLayoutBuilder layout;
    layout.begin(false, false);
    layout.addPushConstant(0, 0, 32, RenderShaderStageFlag::PIXEL);
    layout.addDescriptorSet(set);
    layout.end();
    p.layout = layout.create(device);
    p.descriptors = set.create(device);
    RenderSamplerDesc sampler;
    sampler.addressU = sampler.addressV = sampler.addressW = RenderTextureAddressMode::CLAMP;
    p.sampler = device->createSampler(sampler);
    p.descriptors->setSampler(1, p.sampler.get());
    RenderGraphicsPipelineDesc desc;
    desc.pipelineLayout = p.layout.get();
    desc.vertexShader = p.vs.get();
    desc.pixelShader = p.ps.get();
    desc.renderTargetCount = 1;
    desc.renderTargetFormat[0] = RenderFormat::R8G8B8A8_UNORM;
    desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
    desc.cullMode = RenderCullMode::NONE;
    p.pipeline = device->createGraphicsPipeline(desc);
    return bool(p.pipeline);
}
void Presentation::Draw(RenderCommandList *commands, RenderTexture *source, RenderTexture *target, uint32_t sw,
                        uint32_t sh, uint32_t ow, uint32_t oh, bool antialias)
{
    auto &p = *impl;
    const float scale = std::min(float(ow) / sw, float(oh) / sh);
    const float width = sw * scale, height = sh * scale, x = (ow - width) * 0.5f, y = (oh - height) * 0.5f;
    struct
    {
        float x, y, w, h, sw, sh;
        uint32_t aa, pad;
    } constants{x, y, width, height, float(sw), float(sh), antialias ? 1u : 0u, 0};
    const RenderTexture *attachments[] = {target};
    p.framebuffer = p.device->createFramebuffer(RenderFramebufferDesc(attachments, 1));
    p.descriptors->setTexture(0, source, RenderTextureLayout::SHADER_READ);
    commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(source, RenderTextureLayout::SHADER_READ));
    commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(target, RenderTextureLayout::COLOR_WRITE));
    commands->setFramebuffer(p.framebuffer.get());
    commands->clearColor(0, RenderColor(0, 0, 0, 1));
    RenderViewport viewport(x, y, width, height);
    RenderRect scissor(0, 0, ow, oh);
    commands->setViewports(&viewport, 1);
    commands->setScissors(&scissor, 1);
    commands->setGraphicsPipelineLayout(p.layout.get());
    commands->setPipeline(p.pipeline.get());
    commands->setGraphicsPushConstants(0, &constants);
    commands->setGraphicsDescriptorSet(p.descriptors.get(), 0);
    commands->drawInstanced(3, 1, 0, 0);
    // Return the resolved surface to the state tracked by the guest renderer.
    commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(source, RenderTextureLayout::COPY_SOURCE));
}
} // namespace gpu
#else
namespace gpu
{
struct Presentation::Impl
{
};
Presentation::Presentation() = default;
Presentation::~Presentation() = default;
bool Presentation::Init(plume::RenderDevice *)
{
    return false;
}
void Presentation::Draw(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *, uint32_t, uint32_t,
                        uint32_t, uint32_t, bool)
{
}
} // namespace gpu
#endif
