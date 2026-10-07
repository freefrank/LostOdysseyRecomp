#include "ambient_occlusion.h"
#include "fsr_projection.h"
#include "shader/ambient_occlusion_hlsl.h"
#include "shader/dxc_compiler.h"
#include "shader/target_format.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
namespace gpu::ao {
using namespace plume;
namespace {
struct Constants {
    float projection[4], extent[4], raster[4];
    float radius, strength;
    uint32_t mode, debug;
};
static_assert(sizeof(Constants)==64);
void DefineSet(RenderDescriptorSetBuilder& set) {
    set.begin(); set.addTexture(0); set.addTexture(1); set.addTexture(2);
    set.addConstantBuffer(3); set.addTexture(4); set.end();
}
}
struct AmbientOcclusion::Impl {
    RenderDevice* device=nullptr;
    std::string error;
    std::unique_ptr<RenderPipelineLayout> layout;
    std::unique_ptr<RenderShader> vertex, visibility, composite, compositePair;
    std::unique_ptr<RenderPipeline> visibilityPipeline, colorPipeline, pairPipeline;
    struct Pass { std::unique_ptr<RenderDescriptorSet> set; std::unique_ptr<RenderFramebuffer> framebuffer; };
    struct Batch {
        uint64_t serial=0;
        uint32_t width=0,height=0;
        std::unique_ptr<RenderTexture> ao,color,hdr;
        std::unique_ptr<RenderBuffer> constants;
        std::array<Pass,2> passes; // visibility, then composite (RGBA8 + optional HDR)
    };
    std::vector<std::shared_ptr<Batch>> pending, available;
    uint64_t serial=0;
};
AmbientOcclusion::AmbientOcclusion():impl(std::make_unique<Impl>()){}
AmbientOcclusion::~AmbientOcclusion()=default;
const std::string& AmbientOcclusion::LastError()const{return impl->error;}
uint64_t AmbientOcclusion::RecordedSerial()const{return impl->serial;}
void AmbientOcclusion::ReleaseCompletedThrough(uint64_t serial) {
    auto& p=*impl;
    for(auto it=p.pending.begin();it!=p.pending.end();) {
        if((*it)->serial<=serial && it->use_count()==1) {
            if(p.available.size()<4) p.available.push_back(std::move(*it));
            it=p.pending.erase(it);
        } else ++it;
    }
}
bool AmbientOcclusion::Init(RenderDevice* device) {
    auto& p=*impl;
    if(!device||p.device){p.error="AO initialization requires a device and fresh instance";return false;}
    p.device=device;
    const bool spirv=shader::UsesSpirv(device);
    const auto binary=spirv?xenos::ShaderBinaryFormat::Spirv:xenos::ShaderBinaryFormat::Dxil;
    const auto format=spirv?RenderShaderFormat::SPIRV:RenderShaderFormat::DXIL;
    auto vs=xenos::CompileCachedHlsl(Shader,"vertex","vs_6_0",binary);
    auto ao=xenos::CompileCachedHlsl(Shader,"visibility","ps_6_0",binary);
    auto ps=xenos::CompileCachedHlsl(Shader,"composite","ps_6_0",binary);
    auto pair=xenos::CompileCachedHlsl(Shader,"compositePair","ps_6_0",binary);
    if(!vs.ok||!ao.ok||!ps.ok||!pair.ok){p.error=vs.errors+ao.errors+ps.errors+pair.errors;return false;}
    p.vertex=device->createShader(vs.bytecode.data(),vs.bytecode.size(),"vertex",format);
    p.visibility=device->createShader(ao.bytecode.data(),ao.bytecode.size(),"visibility",format);
    p.composite=device->createShader(ps.bytecode.data(),ps.bytecode.size(),"composite",format);
    p.compositePair=device->createShader(pair.bytecode.data(),pair.bytecode.size(),"compositePair",format);
    RenderDescriptorSetBuilder set;DefineSet(set);
    RenderPipelineLayoutBuilder builder;builder.begin(false,false);builder.addDescriptorSet(set);builder.end();
    p.layout=builder.create(device);
    if(!p.vertex||!p.visibility||!p.composite||!p.compositePair||!p.layout){p.error="AO shader/layout creation failed";return false;}
    auto pipeline=[&](RenderShader* pixel,RenderFormat target,RenderFormat second=RenderFormat::UNKNOWN) {
        RenderGraphicsPipelineDesc desc{};
        desc.pipelineLayout=p.layout.get();desc.vertexShader=p.vertex.get();desc.pixelShader=pixel;
        desc.renderTargetCount=second==RenderFormat::UNKNOWN?1:2;
        desc.renderTargetFormat[0]=target;desc.renderTargetFormat[1]=second;
        desc.renderTargetBlend[0]=desc.renderTargetBlend[1]=RenderBlendDesc::Copy();
        desc.depthEnabled=desc.depthWriteEnabled=false;desc.cullMode=RenderCullMode::NONE;
        desc.primitiveTopology=RenderPrimitiveTopology::TRIANGLE_LIST;
        return device->createGraphicsPipeline(desc);
    };
    p.visibilityPipeline=pipeline(p.visibility.get(),RenderFormat::R32G32_FLOAT);
    p.colorPipeline=pipeline(p.composite.get(),RenderFormat::R8G8B8A8_UNORM);
    p.pairPipeline=pipeline(p.compositePair.get(),RenderFormat::R8G8B8A8_UNORM,RenderFormat::R16G16B16A16_FLOAT);
    if(!p.visibilityPipeline||!p.colorPipeline||!p.pairPipeline){p.error="AO pipeline creation failed";return false;}
    return true;
}
Output AmbientOcclusion::Record(RenderCommandList* commands,const Inputs& in) {
    auto& p=*impl;p.error.clear();
    if(in.mode==Mode::Off)return {};
    if(!commands||!p.visibilityPipeline||!p.colorPipeline||!p.pairPipeline||!in.color||!in.depth||!in.camera||
       in.color==in.depth||in.color==in.hdrColor||in.depth==in.hdrColor||
       !in.width||!in.height||in.width>16384||in.height>16384||
       (in.mode!=Mode::Ssao&&in.mode!=Mode::Gtao)||uint32_t(in.debug)>uint32_t(Debug::Depth)||
       !std::isfinite(in.radius)||in.radius<=0||!std::isfinite(in.strength)||in.strength<0||in.strength>1||
       !std::isfinite(in.jitterX)||!std::isfinite(in.jitterY)||std::abs(in.jitterX)>1||std::abs(in.jitterY)>1) {
        p.error="Invalid AO inputs";return {};
    }
    const auto& raster=in.camera->Raster();
    if(raster.x!=0||raster.y!=0||raster.width!=in.width||raster.height!=in.height) {
        p.error="AO requires a full matching camera viewport";return {};
    }
    auto projection=fsr::DeriveProjection(in.camera->VP(),double(in.width)/in.height);
    if(!projection){p.error="AO requires a supported perspective projection";return {};}
    const uint32_t aw=(in.width+1)/2,ah=(in.height+1)/2;
    const float tanY=std::tan(projection->verticalFovRadians*.5f);
    Constants c{{float(projection->nearDistance*(1-projection->pole)),float(projection->pole),float(tanY*projection->aspect),tanY},
        {float(in.width),float(in.height),float(aw),float(ah)},
        {float(-raster.halfPixelNdcX-2*in.jitterX/in.width),float(-raster.halfPixelNdcY+2*in.jitterY/in.height),float(1/raster.ndcYSign),0},
        in.radius,in.strength,uint32_t(in.mode),uint32_t(in.debug)};
    std::shared_ptr<Impl::Batch> batch;
    for(auto it=p.available.begin();it!=p.available.end();++it) {
        if((*it)->width==in.width&&(*it)->height==in.height&&bool((*it)->hdr)==bool(in.hdrColor)) {
            batch=std::move(*it);p.available.erase(it);break;
        }
    }
    if(!batch) {
        // Completed old-size allocations can be destroyed without a queue wait.
        p.available.clear();
        batch=std::make_shared<Impl::Batch>();batch->width=in.width;batch->height=in.height;
        auto texture=[&](uint32_t w,uint32_t h,RenderFormat format) {
            return p.device->createTexture(RenderTextureDesc::Texture2D(w,h,1,format,RenderTextureFlag::RENDER_TARGET));
        };
        batch->ao=texture(aw,ah,RenderFormat::R32G32_FLOAT);
        batch->color=texture(in.width,in.height,RenderFormat::R8G8B8A8_UNORM);
        if(in.hdrColor)batch->hdr=texture(in.width,in.height,RenderFormat::R16G16B16A16_FLOAT);
        batch->constants=p.device->createBuffer(RenderBufferDesc::UploadBuffer(256,RenderBufferFlag::CONSTANT));
        if(!batch->ao||!batch->color||!batch->constants||(in.hdrColor&&!batch->hdr)) {
            p.error="AO texture/constant allocation failed";return {};
        }
        for(uint32_t i=0;i<2;++i) {
            RenderDescriptorSetBuilder set;DefineSet(set);batch->passes[i].set=set.create(p.device);
            const RenderTexture* attachments[]={i==0?batch->ao.get():batch->color.get(),batch->hdr.get()};
            batch->passes[i].framebuffer=p.device->createFramebuffer(RenderFramebufferDesc(attachments,i==1&&batch->hdr?2u:1u));
            if(!batch->passes[i].set||!batch->passes[i].framebuffer){p.error="AO descriptor/framebuffer allocation failed";return {};}
        }
    }
    auto* data=batch->constants->map();
    if(!data){p.error="AO constants map failed";return {};}
    std::memcpy(data,&c,sizeof(c));batch->constants->unmap();
    for(uint32_t i=0;i<2;++i) {
        auto& set=batch->passes[i].set;
        set->setTexture(0,in.color,RenderTextureLayout::SHADER_READ);
        set->setTexture(1,in.depth,RenderTextureLayout::SHADER_READ);
        // Visibility pass does not read the third texture. Never bind its RT as SRV.
        set->setTexture(2,i==0?in.depth:batch->ao.get(),RenderTextureLayout::SHADER_READ);
        set->setBuffer(3,batch->constants.get(),256);
        // Only the paired composite reads the HDR companion; others get a valid unused view.
        set->setTexture(4,in.hdrColor?in.hdrColor:in.color,RenderTextureLayout::SHADER_READ);
    }
    batch->serial=++p.serial;p.pending.push_back(std::move(batch));auto& b=*p.pending.back();
    auto draw=[&](uint32_t pass,RenderTexture* target,RenderTexture* second,RenderPipeline* pipeline,uint32_t w,uint32_t h) {
        const RenderTextureBarrier write[]={RenderTextureBarrier(target,RenderTextureLayout::COLOR_WRITE),
            RenderTextureBarrier(second,RenderTextureLayout::COLOR_WRITE)};
        commands->barriers(RenderBarrierStage::ALL,write,second?2u:1u);
        commands->setFramebuffer(b.passes[pass].framebuffer.get());
        RenderViewport viewport(0,0,float(w),float(h));RenderRect scissor(0,0,w,h);
        commands->setViewports(&viewport,1);commands->setScissors(&scissor,1);
        commands->setGraphicsPipelineLayout(p.layout.get());commands->setPipeline(pipeline);
        commands->setGraphicsDescriptorSet(b.passes[pass].set.get(),0);commands->drawInstanced(3,1,0,0);
        const RenderTextureBarrier read[]={RenderTextureBarrier(target,RenderTextureLayout::SHADER_READ),
            RenderTextureBarrier(second,RenderTextureLayout::SHADER_READ)};
        commands->barriers(RenderBarrierStage::ALL,read,second?2u:1u);
    };
    draw(0,b.ao.get(),nullptr,p.visibilityPipeline.get(),aw,ah);
    draw(1,b.color.get(),b.hdr.get(),b.hdr?p.pairPipeline.get():p.colorPipeline.get(),in.width,in.height);
    return {b.color.get(),b.hdr.get(),p.pending.back()};
}
}
#else
namespace gpu::ao {
struct AmbientOcclusion::Impl {std::string error="AO requires plume";};
AmbientOcclusion::AmbientOcclusion():impl(std::make_unique<Impl>()){} AmbientOcclusion::~AmbientOcclusion()=default;
bool AmbientOcclusion::Init(plume::RenderDevice*){return false;}
Output AmbientOcclusion::Record(plume::RenderCommandList*,const Inputs&){return {};}
uint64_t AmbientOcclusion::RecordedSerial()const{return 0;}
void AmbientOcclusion::ReleaseCompletedThrough(uint64_t){}
const std::string& AmbientOcclusion::LastError()const{return impl->error;}
}
#endif
