// Numerical backend-contract and native WSI checks, without the guest or game assets.
#include <gpu/shader/xenos_translator.h>
#include <gpu/shader/dxc_compiler.h>
#include <gpu/presentation.h>
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
#include <windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(); std::unique_ptr<RenderInterface> CreateD3D12Interface(); }
namespace {
void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
bool FrontFaceOnly(bool vulkan, bool depthResolve = false) {
    using namespace plume;
    auto api=vulkan?CreateVulkanInterface():CreateD3D12Interface();auto device=api->createDevice();
    auto queue=device->createCommandQueue(RenderCommandListType::DIRECT);auto commands=queue->createCommandList();auto fence=device->createCommandFence();
    const auto format=vulkan?xenos::ShaderBinaryFormat::Spirv:xenos::ShaderBinaryFormat::Dxil;
    auto vc=xenos::CompileHlsl("float4 main(uint id:SV_VertexID):SV_Position { float2 uv=float2((id<<1)&2,id&2);return float4(uv*float2(2,-2)+float2(-1,1),.5,1);}","main","vs_6_0",format);
    auto pc=xenos::CompileHlsl(depthResolve ?
        "Texture2D<float4> src:register(t0);float4 main(float4 pos:SV_Position):SV_Target { return src.Load(int3(int2(pos.xy),0));}" :
        "float4 main(bool front:SV_IsFrontFace):SV_Target {return float4(front?1:0,0,0,1);}","main","ps_6_0",format);
    Check(vc.ok&&pc.ok,"front face shader compilation");const auto sf=vulkan?RenderShaderFormat::SPIRV:RenderShaderFormat::DXIL;
    auto vs=device->createShader(vc.bytecode.data(),vc.bytecode.size(),"main",sf);auto ps=device->createShader(pc.bytecode.data(),pc.bytecode.size(),"main",sf);
    RenderDescriptorSetBuilder sb;sb.begin();sb.addTexture(0);sb.end();
    RenderPipelineLayoutBuilder lb;lb.begin(false,false);if(depthResolve)lb.addDescriptorSet(sb);lb.end();auto layout=lb.create(device.get());
    auto set=sb.create(device.get());
    auto depth=device->createTexture(RenderTextureDesc::DepthTarget(4,4,RenderFormat::D32_FLOAT_S8_UINT));
    auto depthFb=device->createFramebuffer(RenderFramebufferDesc(nullptr,0,depth.get()));
    auto target=device->createTexture(RenderTextureDesc::Texture2D(4,4,1,RenderFormat::R32G32B32A32_FLOAT,RenderTextureFlag::RENDER_TARGET));
    const RenderTexture* attachments[]{target.get()};auto fb=device->createFramebuffer(RenderFramebufferDesc(attachments,1));
    auto readback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(1024));bool ok=true;
    for(bool clockwise:{false,true}) {
        RenderGraphicsPipelineDesc pd;pd.pipelineLayout=layout.get();pd.vertexShader=vs.get();pd.pixelShader=ps.get();pd.renderTargetCount=1;
        pd.renderTargetFormat[0]=RenderFormat::R32G32B32A32_FLOAT;pd.renderTargetBlend[0]=RenderBlendDesc::Copy();pd.cullMode=RenderCullMode::NONE;
        pd.frontFace=clockwise?RenderFrontFace::CLOCKWISE:RenderFrontFace::COUNTER_CLOCKWISE;auto pipeline=device->createGraphicsPipeline(pd);
        commands->begin();
        if(depthResolve) {
            commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(depth.get(),RenderTextureLayout::DEPTH_WRITE));
            commands->setFramebuffer(depthFb.get());commands->clearDepthStencil(true,true,.375f,91);
            commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(depth.get(),RenderTextureLayout::SHADER_READ));
            set->setTexture(0,depth.get(),RenderTextureLayout::SHADER_READ);
        }
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(target.get(),RenderTextureLayout::COLOR_WRITE));commands->setFramebuffer(fb.get());
        RenderViewport viewport(0,0,4,4);RenderRect scissor(0,0,4,4);commands->setViewports(&viewport,1);commands->setScissors(&scissor,1);
        commands->setGraphicsPipelineLayout(layout.get());commands->setPipeline(pipeline.get());
        if(depthResolve) commands->setGraphicsDescriptorSet(set.get(),0);
        commands->drawInstanced(3,1,0,0);
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(target.get(),RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),RenderFormat::R32G32B32A32_FLOAT,4,4,1,16),RenderTextureCopyLocation::Subresource(target.get()));commands->end();
        const RenderCommandList* lists[]{commands.get()};queue->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get());queue->waitForCommandFence(fence.get());
        const auto* pixels=static_cast<const float*>(readback->map());
        if(depthResolve) {
            for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)ok &= pixels[y*64+x*4]==.375f;
            std::printf("Vulkan depth aspect shader resolve .375: %s\n",ok?"PASS":"FAIL");
        } else {
            std::printf("%s clockwise=%u front=%g expected=%u\n",vulkan?"Vulkan":"D3D12",clockwise,pixels[0],clockwise);
            ok &= pixels[0]==float(clockwise)&&pixels[3]==1;
        }
        readback->unmap();if(depthResolve)break;
    }
    return ok;
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2&&std::strcmp(argv[1],"--front-face-only")==0) {
            const bool dx=FrontFaceOnly(false),vk=FrontFaceOnly(true);return dx&&vk?0:1;
        }
        if(argc==2&&std::strcmp(argv[1],"--depth-resolve-only")==0)return FrontFaceOnly(true,true)?0:1;
        using namespace plume;
        auto api=CreateVulkanInterface();Check(bool(api),"Vulkan interface");
        auto device=api->createDevice();Check(bool(device),"Vulkan device");
        std::printf("Vulkan backend fixture: %s\n",device->getDescription().name.c_str());
        auto queue=device->createCommandQueue(RenderCommandListType::DIRECT);
        auto commands=queue->createCommandList();auto fence=device->createCommandFence();
        auto submit=[&] {commands->end();const RenderCommandList* lists[]{commands.get()};
            queue->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get());queue->waitForCommandFence(fence.get());};
        // Populate addresses using independent byte offsets and literal expected
        // results, then execute the exact production common HLSL on both stages.
        const auto common=std::string(xenos::GetShaderCommonHlsl());
        const auto vsSource=std::string("#define XE_SAMPLE(t,s,uv) t.SampleLevel(s,uv,0)\n")+common+R"(
struct V { float4 position:SV_Position; float4 value:TEXCOORD0; };
V main(uint id:SV_VertexID) {
 V o;float2 uv=float2((id<<1)&2,id&2);o.position=float4(uv*float2(2,-2)+float2(-1,1),.5,1);
 o.value=XeConst(0);return o;
})";
        const auto psSource=std::string("#define XE_PIXEL_SHADER 1\n#define XE_SAMPLE(t,s,uv) t.Sample(s,uv)\n")+common+R"(
Texture2D<float4> lastTexture:register(t31,space1);
Texture3D<float4> volume:register(t31,space2);
TextureCube<float4> cubeTexture:register(t31,space3);
float4 main(float4 position:SV_Position,float4 value:TEXCOORD0):SV_Target {
 switch(uint(position.x)) {
 case 0:return XeConst(0);
 case 1:return value;
 case 2:return float4(XeBool(130),XeLoopConst(31),XeVfetchOffset(95),0);
 case 3:return asfloat(xeVertexArena.Load4(XeVfetchOffset(95)));
 case 4:return lastTexture.SampleLevel(XeSampler(31),float2(.5,.5),0);
 case 5:return float4(XeTextureDimensions(lastTexture,31),0,1);
 case 6:return XeTextureResult(float4(.25,.5,.75,1),31);
 case 7:return volume.SampleLevel(XeSampler(31),float3(.5,.5,.5),0);
 default:return cubeTexture.SampleLevel(XeSampler(31),float3(1,0,0),0);
 }
})";
        auto vc=xenos::CompileHlsl(vsSource,"main","vs_6_0",xenos::ShaderBinaryFormat::Spirv);
        auto pc=xenos::CompileHlsl(psSource,"main","ps_6_0",xenos::ShaderBinaryFormat::Spirv);
        if(!vc.ok||!pc.ok)throw std::runtime_error(vc.errors+pc.errors);
        auto vs=device->createShader(vc.bytecode.data(),vc.bytecode.size(),"main",RenderShaderFormat::SPIRV);
        auto ps=device->createShader(pc.bytecode.data(),pc.bytecode.size(),"main",RenderShaderFormat::SPIRV);
        RenderDescriptorSetBuilder builders[5];
        builders[0].begin();builders[0].addByteAddressBuffer(0);builders[0].end();
        for(unsigned set=1;set<4;++set){builders[set].begin();for(unsigned slot=0;slot<32;++slot)builders[set].addTexture(slot);builders[set].end();}
        builders[4].begin();builders[4].addSampler(0,64);builders[4].end();
        RenderPipelineLayoutBuilder lb;lb.begin(false,false);
        lb.addPushConstant(0,0,24,RenderShaderStageFlag::VERTEX|RenderShaderStageFlag::PIXEL);
        for(auto& builder:builders)lb.addDescriptorSet(builder);lb.end();
        auto layout=lb.create(device.get());
        std::array<std::unique_ptr<RenderDescriptorSet>,5> sets;
        for(unsigned i=0;i<5;++i)sets[i]=builders[i].create(device.get());
        auto constants=device->createBuffer(RenderBufferDesc::UploadBuffer(1024+8192,RenderBufferFlag::DEVICE_ADDRESSABLE|RenderBufferFlag::STORAGE));
        auto* bytes=static_cast<uint8_t*>(constants->map());std::memset(bytes,0,1024+8192);
        auto word=[&](size_t offset,uint32_t value){std::memcpy(bytes+offset,&value,4);};
        word(16,4);word(32+31*4,123);word(256+95*4,64);word(640+31*4,63);
        word(768+31*4,(0x688u<<8)|2);word(896+31*4,17|(19<<16));
        const std::array<float,4> vertexValue{.125f,.25f,.5f,1},pixelValue{2,3,5,7},arenaValue{11,13,17,19};
        std::memcpy(bytes+1024,vertexValue.data(),16);std::memcpy(bytes+5120,pixelValue.data(),16);constants->unmap();
        auto arena=device->createBuffer(RenderBufferDesc::UploadBuffer(256,RenderBufferFlag::STORAGE));
        auto* arenaBytes=static_cast<uint8_t*>(arena->map());std::memset(arenaBytes,0,256);std::memcpy(arenaBytes+64,arenaValue.data(),16);arena->unmap();
        sets[0]->setBuffer(0,arena.get(),256);
        RenderSamplerDesc sd;sd.addressU=sd.addressV=sd.addressW=RenderTextureAddressMode::CLAMP;
        auto sampler=device->createSampler(sd);for(unsigned i=0;i<64;++i)sets[4]->setSampler(i,sampler.get());
        auto texture=device->createTexture(RenderTextureDesc::Texture2D(1,1,1,RenderFormat::R8G8B8A8_UNORM));
        auto volume=device->createTexture(RenderTextureDesc::Texture3D(1,1,1,1,RenderFormat::R8G8B8A8_UNORM));
        auto cube=device->createTexture(RenderTextureDesc::Texture(RenderTextureDimension::TEXTURE_2D,1,1,1,1,6,RenderFormat::R8G8B8A8_UNORM,RenderTextureFlag::CUBE));
        auto texUpload=device->createBuffer(RenderBufferDesc::UploadBuffer(256));
        auto* color=static_cast<uint32_t*>(texUpload->map());*color=0xff804020;texUpload->unmap();
        commands->begin();
        for(auto* input:{texture.get(),volume.get(),cube.get()}) {
            commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(input,RenderTextureLayout::COPY_DEST));
            for(unsigned layer=0;layer<(input==cube.get()?6:1);++layer)
                commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(input,0,layer),RenderTextureCopyLocation::PlacedFootprint(texUpload.get(),RenderFormat::R8G8B8A8_UNORM,1,1,1,64));
            commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(input,RenderTextureLayout::SHADER_READ));
        }
        submit();
        for(unsigned i=0;i<32;++i){sets[1]->setTexture(i,texture.get(),RenderTextureLayout::SHADER_READ);sets[2]->setTexture(i,volume.get(),RenderTextureLayout::SHADER_READ);sets[3]->setTexture(i,cube.get(),RenderTextureLayout::SHADER_READ);}
        constexpr auto format=RenderFormat::R32G32B32A32_FLOAT;
        auto target=device->createTexture(RenderTextureDesc::Texture2D(9,1,1,format,RenderTextureFlag::RENDER_TARGET));
        const RenderTexture* attachments[]{target.get()};auto fb=device->createFramebuffer(RenderFramebufferDesc(attachments,1));
        RenderGraphicsPipelineDesc pd;pd.pipelineLayout=layout.get();pd.vertexShader=vs.get();pd.pixelShader=ps.get();
        pd.renderTargetCount=1;pd.renderTargetFormat[0]=format;pd.renderTargetBlend[0]=RenderBlendDesc::Copy();pd.cullMode=RenderCullMode::NONE;
        auto pipeline=device->createGraphicsPipeline(pd);Check(bool(pipeline),"BDA pipeline");
        auto readback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(256));
        commands->begin();commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(target.get(),RenderTextureLayout::COLOR_WRITE));
        commands->setFramebuffer(fb.get());RenderViewport viewport(0,0,9,1);RenderRect scissor(0,0,9,1);
        commands->setViewports(&viewport,1);commands->setScissors(&scissor,1);commands->setGraphicsPipelineLayout(layout.get());commands->setPipeline(pipeline.get());
        const uint64_t addresses[]{constants->getDeviceAddress()+1024,constants->getDeviceAddress(),constants->getDeviceAddress()+5120};
        Check(addresses[1]!=0,"nonzero buffer device address");commands->setGraphicsPushConstants(0,addresses);
        for(unsigned i=0;i<5;++i)commands->setGraphicsDescriptorSet(sets[i].get(),i);commands->drawInstanced(3,1,0,0);
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(target.get(),RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),format,9,1,1,16),RenderTextureCopyLocation::Subresource(target.get()));submit();
        const std::array<float,4> sample{32/255.f,64/255.f,128/255.f,1};
        const std::array<std::array<float,4>,9> expected{pixelValue,vertexValue,std::array<float,4>{1,123,64,0},arenaValue,sample,std::array<float,4>{17,19,0,1},std::array<float,4>{-.5f,.5f,.75f,1},sample,sample};
        auto* pixels=static_cast<float*>(readback->map());
        for(unsigned x=0;x<9;++x)for(unsigned c=0;c<4;++c)if(std::abs(pixels[x*4+c]-expected[x][c])>1e-5f){std::printf("pixel %u channel %u: %g expected %g\n",x,c,pixels[x*4+c],expected[x][c]);throw std::runtime_error("production shader resource contract");}
        readback->unmap();std::puts("PASS: VS/PS BDA isolation, shared offsets, vertex arena, slot 31 texture dimensions/sign/swizzle, sampler 63, 2D/3D/cube bindings");

        // The production F1 path reads only R32 depth from D32/S8 storage.
        auto depth=device->createTexture(RenderTextureDesc::DepthTarget(8,4,RenderFormat::D32_FLOAT_S8_UINT));
        auto depthFb=device->createFramebuffer(RenderFramebufferDesc(nullptr,0,depth.get()));
        auto depthReadback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(256*4));
        commands->begin();commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(depth.get(),RenderTextureLayout::DEPTH_WRITE));
        commands->setFramebuffer(depthFb.get());commands->clearDepthStencil(true,true,.375f,91);
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(depth.get(),RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(depthReadback.get(),RenderFormat::R32_FLOAT,8,4,1,64),RenderTextureCopyLocation::Subresource(depth.get()));submit();
        auto* depthPixels=static_cast<float*>(depthReadback->map());for(unsigned y=0;y<4;++y)for(unsigned x=0;x<8;++x)Check(depthPixels[y*64+x]==.375f,"depth plane readback with row padding");depthReadback->unmap();
        std::puts("PASS: D32/S8 clear and depth-plane capture with padded rows");

        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"LoVulkanFixture";RegisterClassW(&wc);
        HWND window=CreateWindowW(wc.lpszClassName,L"Vulkan fixture",WS_OVERLAPPEDWINDOW,0,0,96,96,nullptr,nullptr,wc.hInstance,nullptr);Check(window!=nullptr,"hidden native window");
        {
            auto swap=queue->createSwapChain(RenderSwapChainDesc(window,RenderFormat::R8G8B8A8_UNORM,3));Check(bool(swap)&&swap->resize()&&!swap->isEmpty(),"native Vulkan swapchain");
            auto acquired=device->createCommandSemaphore();auto released=device->createCommandSemaphore();
            gpu::Presentation presentation;Check(presentation.Init(device.get()),"WSI presentation");
            for(unsigned frame=0;frame<3;++frame) {
                if(frame){SetWindowPos(window,nullptr,0,0,128+int(frame)*16,128,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOMOVE);Check(swap->resize(),"swapchain resize");}
                uint32_t index=0;Check(swap->acquireTexture(acquired.get(),&index),"swapchain acquire");
                auto* output=swap->getTexture(index);commands->begin();presentation.DrawComposited(commands.get(),texture.get(),output,1,1,swap->getWidth(),swap->getHeight(),gpu::ScalingFilter::Bilinear);
                commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(output,RenderTextureLayout::PRESENT));commands->end();
                const RenderCommandList* lists[]{commands.get()};RenderCommandSemaphore* waits[]{acquired.get()};RenderCommandSemaphore* signals[]{released.get()};
                queue->executeCommandLists(lists,1,waits,1,signals,1,fence.get());Check(swap->present(index,signals,1),"swapchain present");queue->waitForCommandFence(fence.get());
            }
        }
        DestroyWindow(window);std::puts("PASS: hidden HWND acquire/render/present and two swapchain resize/rebuild cycles");
        return 0;
    } catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
