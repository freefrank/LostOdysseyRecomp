#pragma once
#include "temporal_collection.h"
#include "shader/dxc_compiler.h"
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
#include <chrono>

namespace gpu::taa_collection {
// Single recording thread. Readback uses the renderer's already completed fence.
// No extra submission, wait or full-resolution buffer transfer.
class SparseDepthGPU {
    using Device=plume::RenderDevice;
    std::unique_ptr<plume::RenderPipelineLayout> layout;
    std::unique_ptr<plume::RenderShader> vs,ps;
    std::unique_ptr<plume::RenderPipeline> pipeline;
    std::unique_ptr<plume::RenderTexture> target;
    std::unique_ptr<plume::RenderBuffer> readback,constants;
    std::unique_ptr<plume::RenderDescriptorSet> set;
    std::unique_ptr<plume::RenderFramebuffer> framebuffer;
    bool attempted=false,ready=false,pending=false;
    uint64_t lastFrame=~0ull;
    SparseFrame sample;
    static constexpr const char* shader=R"(
Texture2D<float> depth:register(t0);
#ifdef __spirv__
[[vk::binding(1,0)]]
#endif
cbuffer Parameters:register(b0){uint width,height,pad0,pad1;};
float4 vertex(uint id:SV_VertexID):SV_Position {
 float2 uv=float2((id<<1)&2,id&2);return float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
float pixel(float4 p:SV_Position):SV_Target {
 uint2 xy=min(uint2(p.xy*float2(width/32.0,height/18.0)),uint2(width-1,height-1));
 return depth.Load(int3(xy,0));
})";
    bool Init(Device* d) {
        using namespace plume;
        if(attempted)return ready;attempted=true;
        bool vk=d->getCapabilities().shaderFormat==RenderShaderFormat::SPIRV;
        auto binary=vk?xenos::ShaderBinaryFormat::Spirv:xenos::ShaderBinaryFormat::Dxil;
        auto format=vk?RenderShaderFormat::SPIRV:RenderShaderFormat::DXIL;
        auto v=xenos::CompileCachedHlsl(shader,"vertex","vs_6_0",binary),p=xenos::CompileCachedHlsl(shader,"pixel","ps_6_0",binary);
        if(!v.ok||!p.ok)return false;
        vs=d->createShader(v.bytecode.data(),v.bytecode.size(),"vertex",format);ps=d->createShader(p.bytecode.data(),p.bytecode.size(),"pixel",format);
        RenderDescriptorSetBuilder sb;sb.begin();sb.addTexture(0);if(vk)sb.addConstantBuffer(1);sb.end();set=sb.create(d);
        RenderPipelineLayoutBuilder lb;lb.begin(false,false);if(!vk)lb.addPushConstant(0,0,16,RenderShaderStageFlag::PIXEL);lb.addDescriptorSet(sb);lb.end();layout=lb.create(d);
        if(!vs||!ps||!set||!layout)return false;
        RenderGraphicsPipelineDesc desc;desc.pipelineLayout=layout.get();desc.vertexShader=vs.get();desc.pixelShader=ps.get();desc.renderTargetCount=1;desc.renderTargetFormat[0]=RenderFormat::R32_FLOAT;desc.renderTargetBlend[0]=RenderBlendDesc::Copy();desc.cullMode=RenderCullMode::NONE;
        pipeline=d->createGraphicsPipeline(desc);target=d->createTexture(RenderTextureDesc::Texture2D(32,18,1,RenderFormat::R32_FLOAT,RenderTextureFlag::RENDER_TARGET));
        readback=d->createBuffer(RenderBufferDesc::ReadbackBuffer(256*18));
        if(vk)constants=d->createBuffer(RenderBufferDesc::UploadBuffer(16,RenderBufferFlag::CONSTANT));
        if(!pipeline||!target||!readback||(vk&&!constants))return false;
        const RenderTexture* attachments[]={target.get()};framebuffer=d->createFramebuffer(RenderFramebufferDesc(attachments,1));
        return ready=bool(framebuffer);
    }
public:
    void Record(Device* device,plume::RenderCommandList* commands,plume::RenderTexture* depth,SparseFrame frame) {
        using namespace plume;
        if(pending||lastFrame==frame.frame||!WantSparse()||!Init(device))return;
        sample=std::move(frame);sample.consentEpoch=ConsentEpoch();lastFrame=sample.frame;
        sample.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const uint32_t size[]={sample.width,sample.height,0,0};
        set->setTexture(0,depth,RenderTextureLayout::SHADER_READ);
        if(constants){void* dst=constants->map();if(!dst)return;memcpy(dst,size,16);constants->unmap();set->setBuffer(1,constants.get(),16);}
        commands->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(target.get(),RenderTextureLayout::COLOR_WRITE));
        commands->setFramebuffer(framebuffer.get());RenderViewport vp(0,0,32,18);RenderRect sc(0,0,32,18);commands->setViewports(&vp,1);commands->setScissors(&sc,1);
        commands->setGraphicsPipelineLayout(layout.get());commands->setPipeline(pipeline.get());if(!constants)commands->setGraphicsPushConstants(0,size);commands->setGraphicsDescriptorSet(set.get(),0);commands->drawInstanced(3,1,0,0);
        commands->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(target.get(),RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),RenderFormat::R32_FLOAT,32,18,1,64,0),RenderTextureCopyLocation::Subresource(target.get(),0));pending=true;
    }
    void ReleaseCompleted() {
        if(!pending)return;pending=false;
        auto* bytes=static_cast<const char*>(readback->map());if(!bytes)return;
        for(size_t y=0;y<18;++y)memcpy(sample.depth.data()+y*32,bytes+y*256,128);
        readback->unmap();SubmitSparse(std::move(sample));
    }
};
}
