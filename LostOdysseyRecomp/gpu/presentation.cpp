#include "presentation.h"
#include "shader/dxc_compiler.h"
#include <os/logger.h>
#include <stdafx.h>
#include <cmath>
#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
#include "shader/smaa_pipeline.h"
namespace gpu
{
using namespace plume;
struct Presentation::Impl
{
    RenderDevice *device = nullptr;
    bool initialized = false;
    bool vulkan = false;
    RenderFormat swapchainFormat = RenderFormat::R8G8B8A8_UNORM;
    SmaaPipeline smaa;
    std::unique_ptr<RenderPipelineLayout> layout;
    std::unique_ptr<RenderShader> vs, ps;
    std::unique_ptr<RenderPipeline> pipeline;
    std::unique_ptr<RenderPipeline> presentPipeline;
    std::unique_ptr<RenderPipelineLayout> uiLayout;
    std::unique_ptr<RenderShader> uiVs, uiPs;
    std::unique_ptr<RenderPipeline> uiPipeline, uiPresentPipeline;
    std::unique_ptr<RenderSampler> sampler;
    struct Pass
    {
        std::unique_ptr<RenderTexture> texture;
        std::unique_ptr<RenderDescriptorSet> descriptors;
        std::unique_ptr<RenderFramebuffer> framebuffer;
        uint32_t width = 0, height = 0;
    };
    std::vector<Pass> passes;
    bool EnsureUiPipelines();
};
struct Presentation::UiCompositionLease
{
    std::unique_ptr<RenderDescriptorSet> descriptors;
    std::unique_ptr<RenderFramebuffer> framebuffer;
};
Presentation::Presentation() : impl(std::make_unique<Impl>())
{
}
Presentation::~Presentation() = default;
bool Presentation::Init(RenderDevice *device)
{
    return Init(device, RenderFormat::R8G8B8A8_UNORM);
}
bool Presentation::Init(RenderDevice *device, RenderFormat swapchainFormat)
{
    auto &p = *impl;
    p.initialized = false;
    p.device = device;
    p.vulkan = device->getCapabilities().shaderFormat == RenderShaderFormat::SPIRV;
    p.swapchainFormat = swapchainFormat;
    p.uiPresentPipeline.reset();
    p.uiPipeline.reset();
    p.uiVs.reset();
    p.uiPs.reset();
    p.uiLayout.reset();
    const auto binaryFormat = p.vulkan ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
    const auto renderFormat = p.vulkan ? RenderShaderFormat::SPIRV : RenderShaderFormat::DXIL;
    const char *source = R"(
Texture2D<float4> frame : register(t0);
#ifdef __spirv__
[[vk::binding(1,0)]]
#endif
SamplerState linearClamp : register(s0);
#ifdef __spirv__
struct PresentationParameters { float2 origin; float2 extent; float2 imageSize; uint aa; uint filter; uint expandRange; };
[[vk::push_constant]] ConstantBuffer<PresentationParameters> parameters;
#define origin parameters.origin
#define extent parameters.extent
#define imageSize parameters.imageSize
#define aa parameters.aa
#define filter parameters.filter
#define expandRange parameters.expandRange
#else
cbuffer Parameters : register(b0) { float2 origin; float2 extent; float2 imageSize; uint aa; uint filter; uint expandRange; };
#endif
float4 vertex(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2,-2) + float2(-1,1),0,1);
}
float3 sampleFrame(float2 pixel) {
    uint w,h; frame.GetDimensions(w,h);
    return frame.SampleLevel(linearClamp, clamp(pixel,0.5,imageSize-0.5)/float2(w,h),0).rgb;
}
// Catmull-Rom interpolating cubic (B=0,C=1/2), separable 4x4 support.
float cubic(float x) {
    x=abs(x);
    if(x<1) return (1.5*x-2.5)*x*x+1;
    if(x<2) return ((-0.5*x+2.5)*x-4)*x+2;
    return 0;
}
float3 resample(float2 pixel) {
    float2 footprint=imageSize/extent;
    // Native pixel centers retain exact identity, including a padded source.
    if(all(abs(footprint-1)<0.00001)) return sampleFrame(pixel);
    if(any(footprint>1.00001)) {
        // Exact box coverage per source texel, not a sparse sample approximation.
        // CPU stages large reductions so each axis spans at most five texels.
        float2 radius=max(footprint,1.0)*0.5;
        float2 lo=max(pixel-radius,0),hi=min(pixel+radius,imageSize);
        int2 first=int2(floor(lo)),last=int2(ceil(hi));
        float3 sum=0;float total=0;
        [loop] for(int y=first.y;y<last.y;++y) [loop] for(int x=first.x;x<last.x;++x) {
            float2 coverage=max(0,min(hi,float2(x+1,y+1))-max(lo,float2(x,y)));
            float weight=coverage.x*coverage.y;
            sum+=sampleFrame(float2(x+0.5,y+0.5))*weight;total+=weight;
        }
        return sum/max(total,0.000001);
    }
    if(!filter) return sampleFrame(pixel);
    float2 base=floor(pixel-0.5)+0.5;
    float3 sum=0;
    [unroll] for(int y=-1;y<=2;++y) [unroll] for(int x=-1;x<=2;++x) {
        float2 tap=base+float2(x,y);
        sum+=sampleFrame(tap)*cubic(pixel.x-tap.x)*cubic(pixel.y-tap.y);
    }
    // Keep interpolated values within the local 2x2 range to prevent text halos.
    float3 a=sampleFrame(base),b=sampleFrame(base+float2(1,0));
    float3 c=sampleFrame(base+float2(0,1)),d=sampleFrame(base+1);
    return clamp(sum,min(min(a,b),min(c,d)),max(max(a,b),max(c,d)));
}
float luma(float3 c) { return dot(c,float3(0.299,0.587,0.114)); }
float4 finishFrame(float3 color) {
    if (expandRange != 0) color=saturate((color-16.0/255.0)*(255.0/219.0));
    return float4(color,1);
}
float4 pixel(float4 position : SV_Position) : SV_Target {
    float2 p = (position.xy-origin)/extent*imageSize;
    float3 center = sampleFrame(p);
    if (!aa) return finishFrame(resample(p));
    float nw=luma(sampleFrame(p+float2(-1,-1))), ne=luma(sampleFrame(p+float2(1,-1)));
    float sw=luma(sampleFrame(p+float2(-1,1))), se=luma(sampleFrame(p+float2(1,1)));
    float mid=luma(center), lo=min(mid,min(min(nw,ne),min(sw,se))), hi=max(mid,max(max(nw,ne),max(sw,se)));
    if (hi-lo < max(0.0312,hi*0.125)) return finishFrame(center);
    float2 direction=float2(-((nw+ne)-(sw+se)),(nw+sw)-(ne+se));
    float reduce=max((nw+ne+sw+se)*0.03125,0.0078125);
    direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),-8,8);
    float3 a=0.5*(sampleFrame(p+direction*(-1.0/6.0))+sampleFrame(p+direction*(1.0/6.0)));
    float3 b=a*0.5+0.25*(sampleFrame(p-direction*0.5)+sampleFrame(p+direction*0.5));
    float lb=luma(b);
    return finishFrame((lb<lo || lb>hi)?a:b);
})";
    auto vs = xenos::CompileCachedHlsl(source, "vertex", "vs_6_0", binaryFormat);
    auto ps = xenos::CompileCachedHlsl(source, "pixel", "ps_6_0", binaryFormat);
    if (!vs.ok || !ps.ok)
    {
        LOG_WARNING("presentation shaders: {} {}", vs.errors, ps.errors);
        return false;
    }
    p.vs = device->createShader(vs.bytecode.data(), vs.bytecode.size(), "vertex", renderFormat);
    p.ps = device->createShader(ps.bytecode.data(), ps.bytecode.size(), "pixel", renderFormat);
    if (!p.vs || !p.ps) return false;
    RenderDescriptorSetBuilder set;
    set.begin();
    set.addTexture(0);
    set.addSampler(p.vulkan ? 1 : 0);
    set.end();
    RenderPipelineLayoutBuilder layout;
    layout.begin(false, false);
    layout.addPushConstant(0, 0, 36, RenderShaderStageFlag::PIXEL);
    layout.addDescriptorSet(set);
    layout.end();
    p.layout = layout.create(device);
    RenderSamplerDesc sampler;
    sampler.addressU = sampler.addressV = sampler.addressW = RenderTextureAddressMode::CLAMP;
    p.sampler = device->createSampler(sampler);
    if (!p.layout || !p.sampler) return false;
    RenderGraphicsPipelineDesc desc;
    desc.pipelineLayout = p.layout.get();
    desc.vertexShader = p.vs.get();
    desc.pixelShader = p.ps.get();
    desc.renderTargetCount = 1;
    desc.renderTargetFormat[0] = RenderFormat::R8G8B8A8_UNORM;
    desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
    desc.cullMode = RenderCullMode::NONE;
    p.pipeline = device->createGraphicsPipeline(desc);
    desc.renderTargetFormat[0] = swapchainFormat;
    p.presentPipeline = device->createGraphicsPipeline(desc);

    p.initialized = bool(p.pipeline) && bool(p.presentPipeline) &&
        p.smaa.Init(device, p.vs.get(), p.sampler.get(), p.vulkan);
    return p.initialized;
}
bool Presentation::Impl::EnsureUiPipelines()
{
    if (uiPipeline && uiPresentPipeline) return true;
    const char *uiSource = R"(
#ifdef __spirv__
[[vk::binding(0,0)]]
#endif
Texture2D<float4> sceneFrame : register(t0);
#ifdef __spirv__
[[vk::binding(1,0)]]
#endif
Texture2D<float4> uiFrame : register(t1);
float4 vertexUi(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2,-2) + float2(-1,1),0,1);
}
float4 pixelUi(float4 position : SV_Position) : SV_Target {
    int2 p = int2(position.xy);
    float4 scene = sceneFrame.Load(int3(p,0));
    float4 overlay = uiFrame.Load(int3(p,0));
    float alpha = saturate(overlay.a);
    return float4(lerp(scene.rgb, overlay.rgb, alpha), 1);
})";
    const auto binaryFormat = vulkan ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
    const auto renderFormat = vulkan ? RenderShaderFormat::SPIRV : RenderShaderFormat::DXIL;
    auto vsResult = xenos::CompileCachedHlsl(uiSource, "vertexUi", "vs_6_0", binaryFormat);
    auto psResult = xenos::CompileCachedHlsl(uiSource, "pixelUi", "ps_6_0", binaryFormat);
    if (!vsResult.ok || !psResult.ok) {
        LOG_WARNING("presentation UI shaders: {} {}", vsResult.errors, psResult.errors);
        return false;
    }
    auto vertexShader = device->createShader(vsResult.bytecode.data(), vsResult.bytecode.size(),
                                              "vertexUi", renderFormat);
    auto pixelShader = device->createShader(psResult.bytecode.data(), psResult.bytecode.size(),
                                             "pixelUi", renderFormat);
    if (!vertexShader || !pixelShader) return false;
    RenderDescriptorSetBuilder set;
    set.begin(); set.addTexture(0); set.addTexture(1); set.end();
    RenderPipelineLayoutBuilder builder;
    builder.begin(false, false); builder.addDescriptorSet(set); builder.end();
    auto pipelineLayout = builder.create(device);
    if (!pipelineLayout) return false;
    RenderGraphicsPipelineDesc desc;
    desc.pipelineLayout = pipelineLayout.get();
    desc.vertexShader = vertexShader.get();
    desc.pixelShader = pixelShader.get();
    desc.renderTargetCount = 1;
    desc.renderTargetFormat[0] = RenderFormat::R8G8B8A8_UNORM;
    desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
    desc.cullMode = RenderCullMode::NONE;
    auto texturePipeline = device->createGraphicsPipeline(desc);
    desc.renderTargetFormat[0] = swapchainFormat;
    auto swapchainPipeline = device->createGraphicsPipeline(desc);
    if (!texturePipeline || !swapchainPipeline) return false;
    uiVs = std::move(vertexShader);
    uiPs = std::move(pixelShader);
    uiLayout = std::move(pipelineLayout);
    uiPipeline = std::move(texturePipeline);
    uiPresentPipeline = std::move(swapchainPipeline);
    return true;
}
bool Presentation::ProcessSceneColor(RenderCommandList *commands, RenderTexture *source, RenderTexture *target,
                                     uint32_t width, uint32_t height, Antialiasing antialiasing)
{
    if (!commands || !source || !target || source == target || !width || !height ||
        width > 16384 || height > 16384 || !impl->initialized ||
        (antialiasing != Antialiasing::Off && antialiasing != Antialiasing::FXAA &&
         antialiasing != Antialiasing::SMAA))
        return false;
    // Reuse the tested source-size AA passes, including SMAA's padded crop.
    Draw(commands, source, target, width, height, width, height,
         PresentationOptions{antialiasing, ScalingFilter::Bilinear}, false);
    commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(source, RenderTextureLayout::SHADER_READ));
    commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(target, RenderTextureLayout::SHADER_READ));
    return true;
}
std::shared_ptr<Presentation::UiCompositionLease> Presentation::DrawSeparatedUi(
    RenderCommandList *commands, RenderTexture *hudless,
    RenderTexture *uiColorAndAlpha, RenderTexture *target,
    uint32_t width, uint32_t height, bool toSwapchain)
{
    if (!commands || !hudless || !uiColorAndAlpha || !target ||
        hudless == target || uiColorAndAlpha == target || !width || !height ||
        width > 16384 || height > 16384 || !impl->initialized)
        return {};
    auto &p = *impl;
    if (!p.EnsureUiPipelines()) return {};
    auto lease = std::make_shared<UiCompositionLease>();
    RenderDescriptorSetBuilder set;
    set.begin(); set.addTexture(0); set.addTexture(1); set.end();
    lease->descriptors = set.create(p.device);
    if (!lease->descriptors) return {};
    const RenderTexture *attachments[] = {target};
    lease->framebuffer = p.device->createFramebuffer(RenderFramebufferDesc(attachments, 1));
    if (!lease->framebuffer) return {};
    lease->descriptors->setTexture(0, hudless, RenderTextureLayout::SHADER_READ);
    lease->descriptors->setTexture(1, uiColorAndAlpha, RenderTextureLayout::SHADER_READ);
    commands->barriers(RenderBarrierStage::GRAPHICS,
        RenderTextureBarrier(hudless, RenderTextureLayout::SHADER_READ));
    commands->barriers(RenderBarrierStage::GRAPHICS,
        RenderTextureBarrier(uiColorAndAlpha, RenderTextureLayout::SHADER_READ));
    commands->barriers(RenderBarrierStage::GRAPHICS,
        RenderTextureBarrier(target, RenderTextureLayout::COLOR_WRITE));
    commands->setFramebuffer(lease->framebuffer.get());
    RenderViewport viewport(0, 0, float(width), float(height));
    RenderRect scissor(0, 0, width, height);
    commands->setViewports(&viewport, 1);
    commands->setScissors(&scissor, 1);
    commands->setGraphicsPipelineLayout(p.uiLayout.get());
    commands->setPipeline(toSwapchain ? p.uiPresentPipeline.get() : p.uiPipeline.get());
    commands->setGraphicsDescriptorSet(lease->descriptors.get(), 0);
    commands->drawInstanced(3, 1, 0, 0);
    return lease;
}
void Presentation::DrawComposited(RenderCommandList *commands, RenderTexture *source, RenderTexture *target,
                                  uint32_t sw, uint32_t sh, uint32_t ow, uint32_t oh, ScalingFilter scalingFilter,
                                  bool expandRgbRange)
{
    Draw(commands, source, target, sw, sh, ow, oh,
         PresentationOptions{Antialiasing::Off, scalingFilter, expandRgbRange});
}
void Presentation::Draw(RenderCommandList *commands, RenderTexture *source, RenderTexture *target, uint32_t sw,
                        uint32_t sh, uint32_t ow, uint32_t oh, bool antialias)
{
    Draw(commands, source, target, sw, sh, ow, oh, antialias ? Antialiasing::FXAA : Antialiasing::Off);
}
void Presentation::Draw(RenderCommandList *commands, RenderTexture *source, RenderTexture *target, uint32_t sw,
                        uint32_t sh, uint32_t ow, uint32_t oh, Antialiasing antialias)
{
    Draw(commands, source, target, sw, sh, ow, oh, PresentationOptions{antialias, ScalingFilter::Bilinear});
}
void Presentation::Draw(RenderCommandList *commands, RenderTexture *source, RenderTexture *target, uint32_t sw,
                         uint32_t sh, uint32_t ow, uint32_t oh, const PresentationOptions &options, bool toSwapchain)
{
    auto &p = *impl;
    if (!sw || !sh || !ow || !oh) return;
    RenderTexture *original = source;
    const float scale = std::min(float(ow) / sw, float(oh) / sh);
    const float width = sw * scale, height = sh * scale;
    // At native size, an odd number of spare pixels belongs to one black bar;
    // placing content on a half pixel would soften an otherwise exact copy.
    const float x = scale == 1.0f ? std::floor((ow-width)*0.5f) : (ow-width)*0.5f;
    const float y = scale == 1.0f ? std::floor((oh-height)*0.5f) : (oh-height)*0.5f;
    size_t passIndex=0;
    auto render=[&](RenderTexture *input,RenderTexture *output,uint32_t iw,uint32_t ih,
                    uint32_t tw,uint32_t th,float ox,float oy,float ew,float eh,uint32_t aa,uint32_t filter,
                    uint32_t expandRange,
                    RenderPipeline *pipe) {
        if(passIndex==p.passes.size()) p.passes.emplace_back();
        auto &pass=p.passes[passIndex++];
        if(!pass.descriptors) {
            RenderDescriptorSetBuilder set;
            set.begin();set.addTexture(0);set.addSampler(p.vulkan ? 1 : 0);set.end();
            pass.descriptors=set.create(p.device);pass.descriptors->setSampler(1,p.sampler.get());
        }
        // Each recorded pass has distinct descriptors/framebuffers. They and the
        // cached intermediate allocations remain owned until the next present fence.
        if(!output) {
            if(pass.width!=tw || pass.height!=th) {
                pass.framebuffer.reset();
                pass.texture=p.device->createTexture(RenderTextureDesc::Texture2D(tw,th,1,RenderFormat::R8G8B8A8_UNORM,RenderTextureFlag::RENDER_TARGET));
                pass.width=tw;pass.height=th;
            }
            output=pass.texture.get();
        }
        const RenderTexture *attachment[]={output};
        pass.framebuffer=p.device->createFramebuffer(RenderFramebufferDesc(attachment,1));
        pass.descriptors->setTexture(0,input,RenderTextureLayout::SHADER_READ);
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(input,RenderTextureLayout::SHADER_READ));
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(output,RenderTextureLayout::COLOR_WRITE));
        commands->setFramebuffer(pass.framebuffer.get());commands->clearColor(0,RenderColor(0,0,0,1));
        RenderViewport viewport(ox,oy,ew,eh);RenderRect scissor(0,0,tw,th);
        commands->setViewports(&viewport,1);commands->setScissors(&scissor,1);
        struct { float x,y,w,h,sw,sh;uint32_t aa,filter,expandRange; }
            constants{ox,oy,ew,eh,float(iw),float(ih),aa,filter,expandRange};
        commands->setGraphicsPipelineLayout(p.layout.get());commands->setPipeline(pipe);
        commands->setGraphicsPushConstants(0,&constants);commands->setGraphicsDescriptorSet(pass.descriptors.get(),0);
        commands->drawInstanced(3,1,0,0);
        return output;
    };
    // AA is evaluated once at actual source resolution, independent of scaling.
    if(options.antialiasing==Antialiasing::SMAA)
        source=p.smaa.Draw(commands,source,sw,sh,p.layout.get(),p.pipeline.get());
    else if(options.antialiasing==Antialiasing::FXAA)
        source=render(source,nullptr,sw,sh,sw,sh,0,0,float(sw),float(sh),1,0,0,p.pipeline.get());
    // Large reductions use full coverage at each stage. No tap count truncation,
    // and no artificial reduced input presented as a game rendering speedup.
    const uint32_t desiredW=std::max(1u,uint32_t(std::ceil(width)));
    const uint32_t desiredH=std::max(1u,uint32_t(std::ceil(height)));
    while(float(sw)>width*4.0f || float(sh)>height*4.0f) {
        uint32_t nw=std::min(sw,std::max(desiredW,(sw+3)/4));
        uint32_t nh=std::min(sh,std::max(desiredH,(sh+3)/4));
        if(nw==sw && nh==sh) break;
        source=render(source,nullptr,sw,sh,nw,nh,0,0,float(nw),float(nh),0,0,0,p.pipeline.get());
        sw=nw;sh=nh;
    }
    render(source,target,sw,sh,ow,oh,x,y,width,height,0,uint32_t(options.scalingFilter),
           options.expandRgbRange ? 1u : 0u,
           toSwapchain ? p.presentPipeline.get() : p.pipeline.get());
    commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(original,RenderTextureLayout::COPY_SOURCE));
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
bool Presentation::Init(plume::RenderDevice *, plume::RenderFormat)
{
    return false;
}
bool Presentation::ProcessSceneColor(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
                                     uint32_t, uint32_t, Antialiasing)
{
    return false;
}
std::shared_ptr<Presentation::UiCompositionLease> Presentation::DrawSeparatedUi(
    plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
    plume::RenderTexture *, uint32_t, uint32_t, bool)
{
    return {};
}
void Presentation::DrawComposited(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
                                  uint32_t, uint32_t, uint32_t, uint32_t, ScalingFilter, bool)
{
}
void Presentation::Draw(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *, uint32_t, uint32_t,
                        uint32_t, uint32_t, bool)
{
}
void Presentation::Draw(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *, uint32_t, uint32_t,
                        uint32_t, uint32_t, Antialiasing)
{
}
void Presentation::Draw(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *, uint32_t, uint32_t,
                        uint32_t, uint32_t, const PresentationOptions &, bool)
{
}
} // namespace gpu
#endif
