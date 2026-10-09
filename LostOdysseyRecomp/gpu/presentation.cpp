#include "presentation.h"
#include "display_gamma.h"
#include "shader/dxc_compiler.h"
#include "shader/target_format.h"
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
    hdr::OutputTransform output;
    // Index 1 serves an HDR scene. The scene processor records an SDR and an
    // HDR pass into one command list per frame, so neither may touch the
    // other's descriptors, framebuffers or intermediates.
    SmaaPipeline smaa[2];
    std::unique_ptr<RenderPipelineLayout> layout;
    std::unique_ptr<RenderShader> vs, ps;
    std::unique_ptr<RenderPipeline> pipeline;
    std::unique_ptr<RenderPipeline> hdrPipeline;
    std::unique_ptr<RenderPipeline> presentPipeline;
    std::unique_ptr<RenderShader> gainPs;
    std::unique_ptr<RenderPipeline> gainPipeline;
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
        RenderFormat format = RenderFormat::UNKNOWN;
    };
    std::vector<Pass> passes[2];
    Pass gainPass;
    // Framebuffers of caller-owned targets (rotating swap chain images, scene
    // AA outputs) by texture and size; see ForgetTargets.
    struct TargetFramebuffer
    {
        const RenderTexture *texture = nullptr;
        uint32_t width = 0, height = 0;
        uint64_t lastUse = 0;
        std::unique_ptr<RenderFramebuffer> framebuffer;
    };
    static constexpr size_t kTargetFramebuffers = 8;
    std::vector<TargetFramebuffer> targetFramebuffers;
    uint64_t targetUses = 0;
    RenderFramebuffer *FramebufferFor(const RenderTexture *texture, uint32_t width, uint32_t height);
    // Final-pass lookup tables: the guest's display gamma ramp in .rgb (#179)
    // and the player's brightness/gamma curve in .w. Three 4 KB constant
    // buffers rotate when either changes, so a frame still in flight keeps
    // the copy it was recorded with.
    static constexpr uint32_t kRampBytes = 256 * 16;
    std::unique_ptr<RenderBuffer> rampBuffers[3];
    uint32_t rampIndex = 0, rampGeneration = UINT32_MAX;
    int curveBrightness = 0;
    uint32_t curveGamma = 100;
    bool rampActive = false, curveActive = false;
    // Null keeps the current curve.
    RenderBuffer *UpdateGammaRamp(const PresentationOptions *curve);
    bool EnsureUiPipelines();
};
RenderBuffer *Presentation::Impl::UpdateGammaRamp(const PresentationOptions *curve)
{
    const uint32_t generation = display_gamma::Generation();
    const int brightness = curve ? curve->displayBrightness : curveBrightness;
    const uint32_t gamma = curve ? std::max(curve->displayGamma, 1u) : curveGamma;
    if (generation != rampGeneration || brightness != curveBrightness || gamma != curveGamma) {
        const auto ramp = display_gamma::Snapshot();
        const uint32_t next = (rampIndex + 1) % 3;
        if (auto *mapped = static_cast<float *>(rampBuffers[next]->map())) {
            // Brightness moves the black point (positive lifts black, white
            // stays at 1); gamma then bends the midtones.
            const float black = float(-brightness) / 200.0f, exponent = 100.0f / float(gamma);
            for (uint32_t i = 0; i < 256; ++i) {
                mapped[4 * i + 0] = ramp.values[3 * i + 0];
                mapped[4 * i + 1] = ramp.values[3 * i + 1];
                mapped[4 * i + 2] = ramp.values[3 * i + 2];
                mapped[4 * i + 3] = std::pow(std::clamp((float(i) / 255.0f - black) / (1.0f - black), 0.0f, 1.0f), exponent);
            }
            rampBuffers[next]->unmap();
            rampIndex = next;
            rampGeneration = generation;
            curveBrightness = brightness;
            curveGamma = gamma;
            rampActive = !ramp.identity;
            curveActive = brightness != 0 || gamma != 100;
        }
    }
    return rampBuffers[rampIndex].get();
}
RenderFramebuffer *Presentation::Impl::FramebufferFor(const RenderTexture *texture, uint32_t width, uint32_t height)
{
    TargetFramebuffer *slot = nullptr;
    for (auto &entry : targetFramebuffers)
        if (entry.texture == texture) slot = &entry;
    if (!slot && targetFramebuffers.size() < kTargetFramebuffers) slot = &targetFramebuffers.emplace_back();
    // More entries than swap chain images: the least recently used one is
    // frames old, past the present fence that protects it.
    if (!slot) slot = &*std::min_element(targetFramebuffers.begin(), targetFramebuffers.end(),
        [](const TargetFramebuffer &a, const TargetFramebuffer &b) { return a.lastUse < b.lastUse; });
    slot->lastUse = ++targetUses;
    if (!slot->framebuffer || slot->texture != texture || slot->width != width || slot->height != height) {
        const RenderTexture *attachment[] = {texture};
        slot->framebuffer = device->createFramebuffer(RenderFramebufferDesc(attachment, 1));
        slot->texture = texture;
        slot->width = width;
        slot->height = height;
    }
    return slot->framebuffer.get();
}
void Presentation::ForgetTargets()
{
    impl->targetFramebuffers.clear();
}
struct Presentation::UiCompositionLease
{
    std::unique_ptr<RenderDescriptorSet> descriptors;
    std::unique_ptr<RenderFramebuffer> framebuffer;
};
Presentation::Presentation() : impl(std::make_unique<Impl>())
{
}
Presentation::~Presentation() = default;
void Presentation::SetOutputTransform(const hdr::OutputTransform& transform)
{
    impl->output = transform;
}
bool Presentation::Init(RenderDevice *device)
{
    return Init(device, RenderFormat::R8G8B8A8_UNORM);
}
bool Presentation::Init(RenderDevice *device, RenderFormat swapchainFormat)
{
    auto &p = *impl;
    p.initialized = false;
    p.device = device;
    p.vulkan = gpu::shader::UsesSpirv(device);
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
[[vk::binding(2,0)]]
#endif
Texture2D<float4> calibrationScene : register(t1);
#ifdef __spirv__
[[vk::binding(1,0)]]
#endif
SamplerState linearClamp : register(s0);
#ifdef __spirv__
struct PresentationParameters { float2 origin; float2 extent; float2 imageSize; uint aa; uint filter; uint expandRange; uint outputFlags; float outputScale; float peakRatio; float4 calibrationRect; };
[[vk::push_constant]] ConstantBuffer<PresentationParameters> parameters;
#define origin parameters.origin
#define extent parameters.extent
#define imageSize parameters.imageSize
#define aa parameters.aa
#define filter parameters.filter
#define expandRange parameters.expandRange
#define outputFlags parameters.outputFlags
#define outputScale parameters.outputScale
#define peakRatio parameters.peakRatio
#define calibrationRect parameters.calibrationRect
#else
cbuffer Parameters : register(b0) { float2 origin; float2 extent; float2 imageSize; uint aa; uint filter; uint expandRange; uint outputFlags; float outputScale; float peakRatio; float4 calibrationRect; };
#endif
// Display gamma ramp, one normalized RGB output per 8-bit input (outputFlags 64;
// 128 for the frozen calibration scene). .w holds the player's curve.
#ifdef __spirv__
[[vk::binding(3,0)]]
#endif
cbuffer GammaRamp : register(b1) { float4 gammaRamp[256]; };
float3 applyGammaRamp(float3 color) {
    float3 f=saturate(color)*255.0;
    uint3 i=min(uint3(f),254u);
    float3 t=f-float3(i);
    float3 lo=float3(gammaRamp[i.r].r,gammaRamp[i.g].g,gammaRamp[i.b].b);
    float3 hi=float3(gammaRamp[i.r+1].r,gammaRamp[i.g+1].g,gammaRamp[i.b+1].b);
    // An extended-gamma scene keeps its part above white; the ramp ends at 1.
    return lerp(lo,hi,t)+max(color-1.0,0.0);
}
// The player's brightness/gamma curve, the same table's .w (outputFlags 256 at
// the final pass; 4096 for the calibration and brightness previews).
float3 applyUserCurve(float3 color) {
    float3 f=saturate(color)*255.0;
    uint3 i=min(uint3(f),254u);
    float3 t=f-float3(i);
    float3 lo=float3(gammaRamp[i.r].w,gammaRamp[i.g].w,gammaRamp[i.b].w);
    float3 hi=float3(gammaRamp[i.r+1].w,gammaRamp[i.g+1].w,gammaRamp[i.b+1].w);
    return lerp(lo,hi,t)+max(color-1.0,0.0);
}
// Brightness test pattern in display signal values: a smooth ramp, ten grey
// steps and eight near-black patches on black.
float3 brightnessPattern(float2 tile) {
    float x=(tile.x-0.05)/0.9;
    if (x<0 || x>=1) return 0;
    if (tile.y>=0.06 && tile.y<0.30) return x.xxx;
    if (tile.y>=0.38 && tile.y<0.62) return ((floor(x*10)+1)/10).xxx;
    if (tile.y>=0.70 && tile.y<0.94) {
        float cell=x*8;
        if (frac(cell)<0.12 || frac(cell)>0.88) return 0;
        return ((floor(cell)+1)*2.5/255.0).xxx;
    }
    return 0;
}
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
// Edge decisions on the SDR range only: an extended-gamma scene keeps its
// highlights, but above white the luma contrast is as saturated SDR would see.
float luma(float3 c) { return dot(saturate(c),float3(0.299,0.587,0.114)); }
float3 encodeOutput(float3 color) {
    if ((outputFlags & 8) != 0) {
        color = mul(float3x3(0.627404,0.329283,0.043313,
                            0.069097,0.919540,0.011362,
                            0.016391,0.088013,0.895595), color);
        float3 p=pow(saturate(color/10000.0),2610.0/16384.0);
        color=pow((3424.0/4096.0+(2413.0/128.0)*p)/(1+(2392.0/128.0)*p),2523.0/32.0);
    }
    return color;
}
float3 mapHighlights(float3 color, float peak) {
    color=min(color,65504.0);
    float maximum=max(color.r,max(color.g,color.b));
    if (maximum>1) {
        float range=max(peak-1,0);
        float excess=maximum-1;
        float mapped=1+(range>0 ? range*(excess/(range+excess)) : 0);
        color*=mapped/maximum;
    }
    return color;
}
// Our HDR output conversion of a display signal; extended keeps values above 1.
float4 outputSignal(float3 color, bool extended) {
    if ((outputFlags & 1) != 0) {
        color=max(color,0);
        if (extended) {
            color=mapHighlights(pow(color,2.2),peakRatio);
        } else {
            // Inactive linear output: the compositor re-encodes with the sRGB
            // curve, so decoding with it returns the SDR pixels unchanged.
            color=lerp(pow((color+0.055)/1.055,2.4),color/12.92,step(color,0.04045));
        }
        color*=outputScale;
    }
    return float4(encodeOutput(color),1);
}
float4 finishFrame(float3 color) {
    // Scanout order: the guest's ramp, then the optional RGB range expansion,
    // then the player's brightness/gamma, then the HDR output conversion.
    if ((outputFlags & 64) != 0) color=applyGammaRamp(color);
    if (expandRange != 0) {
        color=max((color-16.0/255.0)*(255.0/219.0),0);
        if ((outputFlags & 2) == 0) color=min(color,1);
    }
    if ((outputFlags & 256) != 0) color=applyUserCurve(color);
    return outputSignal(color,(outputFlags & 2) != 0);
}
// Fit the full frozen scene into one half of calibrationRect without
// stretching or showing padded rows. Both halves sample exactly the same frame.
float2 fitScene(float2 tile) {
    uint sceneWidth,sceneHeight;
    calibrationScene.GetDimensions(sceneWidth,sceneHeight);
    if ((outputFlags & 8192) != 0) sceneWidth/=2;
    float sceneAspect=float(sceneWidth)/float(sceneHeight);
    float2 tileSize=(calibrationRect.zw-calibrationRect.xy)*imageSize*float2(0.5,1);
    float tileAspect=tileSize.x/tileSize.y;
    return (tile-0.5)*float2(max(tileAspect/sceneAspect,1),max(sceneAspect/tileAspect,1))+0.5;
}
float4 pixel(float4 position : SV_Position) : SV_Target {
    float2 uv=(position.xy-origin)/extent;
    if ((outputFlags & 512) != 0 && all(uv>=calibrationRect.xy) && all(uv<calibrationRect.zw)) {
        // Brightness preview: the game image on the left, the player's curve on
        // the right, each through the same output conversion as gameplay.
        float2 local=(uv-calibrationRect.xy)/(calibrationRect.zw-calibrationRect.xy);
        float2 tile=float2(frac(local.x*2),local.y);
        bool extended=(outputFlags & 2048) != 0;
        float3 color;
        if ((outputFlags & 1024) != 0) {
            tile=fitScene(tile);
            if (any(tile<0) || any(tile>1)) return float4(0,0,0,1);
            // Split scene: each tile samples its own half of the texture.
            if ((outputFlags & 8192) != 0) tile.x=tile.x*0.5+(local.x>=0.5 ? 0.5 : 0.0);
            color=calibrationScene.SampleLevel(linearClamp,tile,0).rgb;
            if ((outputFlags & 128) != 0) color=applyGammaRamp(color);
            if ((outputFlags & 32) != 0) {
                color=max((color-16.0/255.0)*(255.0/219.0),0);
                if (!extended) color=min(color,1);
            }
        } else color=brightnessPattern(tile);
        if (local.x>=0.5 && (outputFlags & 4096) != 0) color=applyUserCurve(color);
        return outputSignal(color,extended);
    }
    if ((outputFlags & 4) != 0 && all(uv>=calibrationRect.xy) && all(uv<calibrationRect.zw)) {
        float2 local=(uv-calibrationRect.xy)/(calibrationRect.zw-calibrationRect.xy);
        bool peak=local.x>=0.5;
        float2 tile=float2(frac(local.x*2),local.y);
        if ((outputFlags & 16) != 0) {
            tile=fitScene(tile);
            if (any(tile<0) || any(tile>1)) return float4(0,0,0,1);
            float3 color=calibrationScene.SampleLevel(linearClamp,tile,0).rgb;
            if ((outputFlags & 128) != 0) color=applyGammaRamp(color);
            if ((outputFlags & 32) != 0) color=max((color-16.0/255.0)*(255.0/219.0),0);
            if ((outputFlags & 4096) != 0) color=applyUserCurve(color);
            color=pow(max(color,0),2.2);
            color=peak ? mapHighlights(color,peakRatio) : saturate(color);
            return float4(encodeOutput(color*outputScale),1);
        }
        float level=0;
        if (all(tile>float2(0.08,0.06)) && all(tile<float2(0.92,0.94))) {
            level=peak ? peakRatio : 1;
            // The inner diamond is 90% of its surround. It disappears when
            // both exceed the display's clipping point, without our tone map
            // suppressing the very highlight the user is trying to calibrate.
            if (abs(tile.x-0.5)+abs(tile.y-0.5)<0.20) level*=0.9;
        }
        return float4(encodeOutput((level*outputScale).xxx),1);
    }
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
}
// Highlight gain: frame = final SDR output, calibrationScene = pre-upscale
// extended-gamma scene (valid imageSize inside a calibrationRect.xy allocation).
float4 gainPixel(float4 position : SV_Position) : SV_Target {
    float3 sdr = frame.Load(int3(position.xy, 0)).rgb;
    float2 uv = (position.xy / extent) * (imageSize / calibrationRect.xy);
    float3 hdr = calibrationScene.SampleLevel(linearClamp, uv, 0).rgb;
    float3 gain = max(hdr, 1.0);
    float weight = smoothstep(0.8, 1.0, max(sdr.r, max(sdr.g, sdr.b)));
    return float4(sdr * lerp(1.0.xxx, gain, weight), 1);
})";
    auto vs = xenos::CompileCachedHlsl(source, "vertex", "vs_6_0", binaryFormat);
    auto ps = xenos::CompileCachedHlsl(source, "pixel", "ps_6_0", binaryFormat);
    auto gainPs = xenos::CompileCachedHlsl(source, "gainPixel", "ps_6_0", binaryFormat);
    if (!vs.ok || !ps.ok || !gainPs.ok)
    {
        LOG_WARNING("presentation shaders: {} {} {}", vs.errors, ps.errors, gainPs.errors);
        return false;
    }
    p.vs = device->createShader(vs.bytecode.data(), vs.bytecode.size(), "vertex", renderFormat);
    p.ps = device->createShader(ps.bytecode.data(), ps.bytecode.size(), "pixel", renderFormat);
    p.gainPs = device->createShader(gainPs.bytecode.data(), gainPs.bytecode.size(), "gainPixel", renderFormat);
    if (!p.vs || !p.ps || !p.gainPs) return false;
    RenderDescriptorSetBuilder set;
    set.begin();
    set.addTexture(0);
    set.addSampler(p.vulkan ? 1 : 0);
    set.addTexture(p.vulkan ? 2 : 1);
    set.addConstantBuffer(p.vulkan ? 3 : 1);
    set.end();
    for (auto &buffer : p.rampBuffers) {
        buffer = device->createBuffer(RenderBufferDesc::UploadBuffer(Impl::kRampBytes, RenderBufferFlag::CONSTANT));
        auto *mapped = buffer ? static_cast<float *>(buffer->map()) : nullptr;
        if (!mapped) return false;
        for (uint32_t i = 0; i < 256; ++i)
            mapped[4 * i + 0] = mapped[4 * i + 1] = mapped[4 * i + 2] = mapped[4 * i + 3] = i / 255.0f;
        buffer->unmap();
    }
    p.rampIndex = 0;
    p.rampGeneration = UINT32_MAX;
    p.curveBrightness = 0;
    p.curveGamma = 100;
    p.rampActive = p.curveActive = false;
    RenderPipelineLayoutBuilder layout;
    layout.begin(false, false);
    layout.addPushConstant(0, 0, 64, RenderShaderStageFlag::PIXEL);
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
    // FP16 intermediates serve HDR swap chains and the scene processor's
    // extended-gamma AA pass, so every instance owns the variant.
    desc.renderTargetFormat[0] = RenderFormat::R16G16B16A16_FLOAT;
    p.hdrPipeline = device->createGraphicsPipeline(desc);
    desc.pixelShader = p.gainPs.get();
    p.gainPipeline = device->createGraphicsPipeline(desc);
    desc.pixelShader = p.ps.get();
    desc.renderTargetFormat[0] = swapchainFormat;
    p.presentPipeline = device->createGraphicsPipeline(desc);

    p.initialized = bool(p.pipeline) && bool(p.presentPipeline) && bool(p.hdrPipeline) && bool(p.gainPipeline) &&
        p.smaa[0].Init(device, p.vs.get(), p.sampler.get(), p.vulkan) &&
        p.smaa[1].Init(device, p.vs.get(), p.sampler.get(), p.vulkan);
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
#ifdef __spirv__
struct UiOutputParameters { uint linearOutput; float whiteScale; };
[[vk::push_constant]] ConstantBuffer<UiOutputParameters> uiOutput;
#define linearOutput uiOutput.linearOutput
#define whiteScale uiOutput.whiteScale
#else
cbuffer UiOutputParameters : register(b0) { uint linearOutput; float whiteScale; };
#endif
float4 vertexUi(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2,-2) + float2(-1,1),0,1);
}
float4 pixelUi(float4 position : SV_Position) : SV_Target {
    int2 p = int2(position.xy);
    float4 scene = sceneFrame.Load(int3(p,0));
    float4 overlay = uiFrame.Load(int3(p,0));
    float alpha = saturate(overlay.a);
    float3 color=lerp(scene.rgb, overlay.rgb, alpha);
    if (linearOutput != 0) color=pow(max(color,0),2.2)*whiteScale;
    return float4(color, 1);
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
    builder.begin(false, false); builder.addPushConstant(0, 0, 8, RenderShaderStageFlag::PIXEL);
    builder.addDescriptorSet(set); builder.end();
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
                                     uint32_t width, uint32_t height, Antialiasing antialiasing, bool hdr)
{
    if (!commands || !source || !target || source == target || !width || !height ||
        width > 16384 || height > 16384 || !impl->initialized ||
        (antialiasing != Antialiasing::Off && antialiasing != Antialiasing::FXAA &&
         antialiasing != Antialiasing::SMAA))
        return false;
    // Reuse the tested source-size AA passes, including SMAA's padded crop.
    PresentationOptions options{antialiasing, ScalingFilter::Bilinear};
    options.hdrScene = hdr;
    Draw(commands, source, target, width, height, width, height, options, false);
    commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(source, RenderTextureLayout::SHADER_READ));
    commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(target, RenderTextureLayout::SHADER_READ));
    return true;
}
RenderTexture* Presentation::ComposeHdrGain(RenderCommandList *commands, RenderTexture *sdr, RenderTexture *hdr,
                                            uint32_t width, uint32_t height, uint32_t hdrValidWidth, uint32_t hdrValidHeight,
                                            uint32_t hdrAllocationWidth, uint32_t hdrAllocationHeight)
{
    auto &p = *impl;
    if (!commands || !sdr || !hdr || sdr == hdr || !width || !height || width > 16384 || height > 16384 ||
        !hdrValidWidth || !hdrValidHeight || hdrValidWidth > hdrAllocationWidth || hdrValidHeight > hdrAllocationHeight ||
        !p.initialized || !p.gainPipeline)
        return nullptr;
    auto &pass = p.gainPass;
    if (!pass.descriptors) {
        RenderDescriptorSetBuilder set;
        set.begin(); set.addTexture(0); set.addSampler(p.vulkan ? 1 : 0); set.addTexture(p.vulkan ? 2 : 1);
        set.addConstantBuffer(p.vulkan ? 3 : 1); set.end();
        pass.descriptors = set.create(p.device);
        if (!pass.descriptors) return nullptr;
        pass.descriptors->setSampler(1, p.sampler.get());
    }
    pass.descriptors->setBuffer(3, p.rampBuffers[p.rampIndex].get(), Impl::kRampBytes);
    if (pass.width != width || pass.height != height || !pass.texture) {
        pass.framebuffer.reset();
        pass.texture = p.device->createTexture(RenderTextureDesc::Texture2D(width, height, 1,
            RenderFormat::R16G16B16A16_FLOAT, RenderTextureFlag::RENDER_TARGET));
        if (!pass.texture) { pass.width = pass.height = 0; return nullptr; }
        pass.width = width; pass.height = height; pass.format = RenderFormat::R16G16B16A16_FLOAT;
        const RenderTexture *attachment[] = {pass.texture.get()};
        pass.framebuffer = p.device->createFramebuffer(RenderFramebufferDesc(attachment, 1));
        if (!pass.framebuffer) { pass.texture.reset(); pass.width = pass.height = 0; return nullptr; }
    }
    pass.descriptors->setTexture(0, sdr, RenderTextureLayout::SHADER_READ);
    pass.descriptors->setTexture(2, hdr, RenderTextureLayout::SHADER_READ);
    commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(sdr, RenderTextureLayout::SHADER_READ));
    commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(hdr, RenderTextureLayout::SHADER_READ));
    commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(pass.texture.get(), RenderTextureLayout::COLOR_WRITE));
    commands->setFramebuffer(pass.framebuffer.get());
    RenderViewport viewport(0, 0, float(width), float(height));
    RenderRect scissor(0, 0, width, height);
    commands->setViewports(&viewport, 1);
    commands->setScissors(&scissor, 1);
    struct { float x,y,w,h,sw,sh;uint32_t aa,filter,expandRange,outputFlags;float outputScale,peakRatio;float calibrationRect[4]; }
        constants{0, 0, float(width), float(height), float(hdrValidWidth), float(hdrValidHeight), 0, 0, 0, 0, 1, 1,
                  {float(hdrAllocationWidth), float(hdrAllocationHeight), 0, 0}};
    commands->setGraphicsPipelineLayout(p.layout.get());
    commands->setPipeline(p.gainPipeline.get());
    commands->setGraphicsPushConstants(0, &constants);
    commands->setGraphicsDescriptorSet(pass.descriptors.get(), 0);
    commands->drawInstanced(3, 1, 0, 0);
    commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(pass.texture.get(), RenderTextureLayout::SHADER_READ));
    return pass.texture.get();
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
    struct { uint32_t linear; float scale; } output{toSwapchain && p.output.linear ? 1u : 0u, p.output.scale};
    commands->setGraphicsPushConstants(0, &output);
    commands->setGraphicsDescriptorSet(lease->descriptors.get(), 0);
    commands->drawInstanced(3, 1, 0, 0);
    return lease;
}
void Presentation::DrawComposited(RenderCommandList *commands, RenderTexture *source, RenderTexture *target,
                                  uint32_t sw, uint32_t sh, uint32_t ow, uint32_t oh, ScalingFilter scalingFilter,
                                  bool expandRgbRange, bool displayGammaRamp)
{
    PresentationOptions options{Antialiasing::Off, scalingFilter, expandRgbRange};
    options.displayGammaRamp = displayGammaRamp;
    Draw(commands, source, target, sw, sh, ow, oh, options);
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
    // Off-screen scene processing keeps FP16 regardless of the output transform.
    const bool hdrScene = options.hdrScene && bool(p.hdrPipeline) && (p.output.linear || !toSwapchain);
    const auto intermediateFormat = hdrScene ? RenderFormat::R16G16B16A16_FLOAT : RenderFormat::R8G8B8A8_UNORM;
    auto* intermediatePipeline = hdrScene ? p.hdrPipeline.get() : p.pipeline.get();
    RenderTexture *original = source;
    const float scale = std::min(float(ow) / sw, float(oh) / sh);
    const float width = sw * scale, height = sh * scale;
    // At native size, an odd number of spare pixels belongs to one black bar;
    // placing content on a half pixel would soften an otherwise exact copy.
    const float x = scale == 1.0f ? std::floor((ow-width)*0.5f) : (ow-width)*0.5f;
    const float y = scale == 1.0f ? std::floor((oh-height)*0.5f) : (oh-height)*0.5f;
    size_t passIndex=0;
    auto &passes=p.passes[hdrScene ? 1 : 0];
    RenderBuffer *ramp=p.UpdateGammaRamp(options.displayAdjust || options.brightnessPreview || options.hdrCalibration
        ? &options : nullptr);
    auto render=[&](RenderTexture *input,RenderTexture *output,uint32_t iw,uint32_t ih,
                    uint32_t tw,uint32_t th,float ox,float oy,float ew,float eh,uint32_t aa,uint32_t filter,
                    uint32_t expandRange,bool finalPass,
                    RenderPipeline *pipe) {
        if(passIndex==passes.size()) passes.emplace_back();
        auto &pass=passes[passIndex++];
        if(!pass.descriptors) {
            RenderDescriptorSetBuilder set;
            set.begin();set.addTexture(0);set.addSampler(p.vulkan ? 1 : 0);set.addTexture(p.vulkan ? 2 : 1);
            set.addConstantBuffer(p.vulkan ? 3 : 1);set.end();
            pass.descriptors=set.create(p.device);pass.descriptors->setSampler(1,p.sampler.get());
        }
        pass.descriptors->setBuffer(3,ramp,Impl::kRampBytes);
        // Each recorded pass has distinct descriptors/framebuffers. They and the
        // cached intermediate allocations remain owned until the next present fence.
        // A pass's framebuffer serves its own intermediate; caller targets use
        // the per-texture cache.
        RenderFramebuffer *framebuffer=nullptr;
        if(!output) {
            if(pass.width!=tw || pass.height!=th || pass.format!=intermediateFormat) {
                pass.framebuffer.reset();
                pass.texture=p.device->createTexture(RenderTextureDesc::Texture2D(tw,th,1,intermediateFormat,RenderTextureFlag::RENDER_TARGET));
                pass.width=tw;pass.height=th;
                pass.format=intermediateFormat;
            }
            output=pass.texture.get();
            if(!pass.framebuffer) {
                const RenderTexture *attachment[]={output};
                pass.framebuffer=p.device->createFramebuffer(RenderFramebufferDesc(attachment,1));
            }
            framebuffer=pass.framebuffer.get();
        }
        else framebuffer=p.FramebufferFor(output,tw,th);
        pass.descriptors->setTexture(0,input,RenderTextureLayout::SHADER_READ);
        auto* calibration = options.calibrationScene ? options.calibrationScene : input;
        pass.descriptors->setTexture(2,calibration,RenderTextureLayout::SHADER_READ);
        if (calibration != input)
            commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(calibration,RenderTextureLayout::SHADER_READ));
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(input,RenderTextureLayout::SHADER_READ));
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(output,RenderTextureLayout::COLOR_WRITE));
        // The triangle writes every pixel inside the viewport without blending
        // or discard, so only letterbox bars need the clear.
        const bool covered=framebuffer && framebuffer->getWidth()==tw && framebuffer->getHeight()==th &&
            ox<=0.0f && oy<=0.0f && ox+ew>=float(tw) && oy+eh>=float(th);
        commands->setFramebuffer(framebuffer);
        if(!covered) commands->clearColor(0,RenderColor(0,0,0,1));
        RenderViewport viewport(ox,oy,ew,eh);RenderRect scissor(0,0,tw,th);
        commands->setViewports(&viewport,1);commands->setScissors(&scissor,1);
        const bool linearOutput = toSwapchain && output == target && p.output.linear;
        struct { float x,y,w,h,sw,sh;uint32_t aa,filter,expandRange,outputFlags;float outputScale,peakRatio;float calibrationRect[4]; }
            constants{ox,oy,ew,eh,float(iw),float(ih),aa,filter,expandRange,
                (linearOutput ? 1u : 0u) | (linearOutput && hdrScene ? 2u : 0u) |
                (linearOutput && p.output.active && options.hdrCalibration ? 4u : 0u) |
                (linearOutput && p.output.pq ? 8u : 0u) |
                (linearOutput && p.output.active && options.hdrCalibration && options.calibrationScene ? 16u : 0u) |
                (options.calibrationExpandRgbRange ? 32u : 0u) |
                (finalPass && options.displayGammaRamp && p.rampActive ? 64u : 0u) |
                (options.calibrationDisplayGammaRamp && p.rampActive ? 128u : 0u) |
                (finalPass && options.displayAdjust && p.curveActive ? 256u : 0u) |
                (finalPass && options.brightnessPreview ? 512u : 0u) |
                (finalPass && options.brightnessPreview && options.calibrationScene ? 1024u : 0u) |
                (options.calibrationSceneExtended ? 2048u : 0u) |
                (((options.brightnessPreview && !options.calibrationSplitScene) || options.hdrCalibration) && p.curveActive ? 4096u : 0u) |
                (options.calibrationSplitScene ? 8192u : 0u),
                p.output.scale,p.output.peakRatio,
                {options.calibrationRect[0],options.calibrationRect[1],options.calibrationRect[2],options.calibrationRect[3]}};
        commands->setGraphicsPipelineLayout(p.layout.get());commands->setPipeline(pipe);
        commands->setGraphicsPushConstants(0,&constants);commands->setGraphicsDescriptorSet(pass.descriptors.get(),0);
        commands->drawInstanced(3,1,0,0);
        return output;
    };
    // AA is evaluated once at actual source resolution, independent of scaling.
    // An HDR scene runs the same passes with FP16 intermediates.
    if(options.antialiasing==Antialiasing::SMAA)
        source=p.smaa[hdrScene ? 1 : 0].Draw(commands,source,sw,sh,p.layout.get(),intermediatePipeline,hdrScene,
            ramp,Impl::kRampBytes);
    else if(options.antialiasing==Antialiasing::FXAA) {
        // Scene processing: the final pass would only copy the FXAA image at
        // native size (texel centres, no output conversion), so FXAA writes
        // the target directly.
        if(!toSwapchain && sw==ow && sh==oh && !options.expandRgbRange && !options.displayGammaRamp &&
           !options.displayAdjust && !options.brightnessPreview) {
            render(source,target,sw,sh,ow,oh,0,0,float(sw),float(sh),1,0,0,false,intermediatePipeline);
            commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(original,RenderTextureLayout::COPY_SOURCE));
            return;
        }
        source=render(source,nullptr,sw,sh,sw,sh,0,0,float(sw),float(sh),1,0,0,false,intermediatePipeline);
    }
    // Large reductions use full coverage at each stage. No tap count truncation,
    // and no artificial reduced input presented as a game rendering speedup.
    const uint32_t desiredW=std::max(1u,uint32_t(std::ceil(width)));
    const uint32_t desiredH=std::max(1u,uint32_t(std::ceil(height)));
    while(float(sw)>width*4.0f || float(sh)>height*4.0f) {
        uint32_t nw=std::min(sw,std::max(desiredW,(sw+3)/4));
        uint32_t nh=std::min(sh,std::max(desiredH,(sh+3)/4));
        if(nw==sw && nh==sh) break;
        source=render(source,nullptr,sw,sh,nw,nh,0,0,float(nw),float(nh),0,0,0,false,intermediatePipeline);
        sw=nw;sh=nh;
    }
    render(source,target,sw,sh,ow,oh,x,y,width,height,0,uint32_t(options.scalingFilter),
           options.expandRgbRange ? 1u : 0u,true,
           toSwapchain ? p.presentPipeline.get() : intermediatePipeline);
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
void Presentation::SetOutputTransform(const hdr::OutputTransform&) {}
void Presentation::ForgetTargets() {}
bool Presentation::Init(plume::RenderDevice *)
{
    return false;
}
bool Presentation::Init(plume::RenderDevice *, plume::RenderFormat)
{
    return false;
}
bool Presentation::ProcessSceneColor(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
                                     uint32_t, uint32_t, Antialiasing, bool)
{
    return false;
}
std::shared_ptr<Presentation::UiCompositionLease> Presentation::DrawSeparatedUi(
    plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
    plume::RenderTexture *, uint32_t, uint32_t, bool)
{
    return {};
}
plume::RenderTexture* Presentation::ComposeHdrGain(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
                                                   uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
{
    return nullptr;
}
void Presentation::DrawComposited(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
                                  uint32_t, uint32_t, uint32_t, uint32_t, ScalingFilter, bool, bool)
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
