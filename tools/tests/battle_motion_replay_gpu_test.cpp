// Local original microcode is read from ignored evidence directories at test time.
// No captured shaders, game dump, or generated HLSL are distributed with this fixture.
#include <gpu/motion_replay_gpu.h>
#include <gpu/geometry_prepare.h>
#include <gpu/temporal_scene.h>
#include <gpu/shader/dxc_compiler.h>
#include <fstream>
#include <filesystem>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(); }
using namespace plume;
using namespace gpu::temporal;
namespace fs=std::filesystem;
static void Check(bool ok,const std::string& label) {if(!ok)throw std::runtime_error(label);printf("PASS: %s\n",label.c_str());}
static constexpr uint32_t W=64,H=64;
static constexpr uint64_t hashes[3][2]={{0x8d9770d1bd8ba0faull,0xa196904547677608ull},
    {0x4bd8985d84983b83ull,0x8ead384aedf2bb23ull}, {0xf6f074ce5d305448ull,0xa3826242c3338c5full}};
static const char* vsNames[]={"a6f8edc127ae75374e1ac1fd209ef5e5827f5cfc1fe046e8293f4b5473e9208b",
    "bf47df67bbfd857e55ea65815d2f55ef09356818ab8a52fd6d79d6de83220de0",
    "b25fdc9a20ac636263493cdc229dc81291b8b4d2ff95dbba8d1995605d617522"};
static constexpr uint32_t vsBytes[]={432,1500,996},psBytes[]={732,192,972};
static const char* psNames[]={"e3df99cc5e52b1bec72bf1575e3d396ce0a643cba89989ce2d35b1d799c6d67d",
    "ps_8ead384aedf2bb23","ps_a3826242c3338c5f"};
static std::vector<uint32_t> Load(const fs::path& path,size_t bytes) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    Check(in && size_t(in.tellg())==bytes,"local original microcode: "+path.string());
    std::vector<uint32_t> data(bytes/4);in.seekg(0);in.read(reinterpret_cast<char*>(data.data()),bytes);
    Check(bool(in),"original bytecode read");return data;
}
static std::vector<uint32_t> Host(const std::vector<uint32_t>& guest) {
    std::vector<uint32_t> host=guest;
    for(auto& w:host)w=(w>>24)|((w>>8)&0xff00u)|((w<<8)&0xff0000u)|(w<<24);
    return host;
}
static void Write(RenderBuffer* b,const void* data,size_t n) {
    auto* p=b->map();Check(p!=nullptr,"upload map");std::memcpy(p,data,n);b->unmap();
}
struct alignas(16) Shared {
    uint32_t bools[8]{},loops[32]{};
    float ndcScale[4]{1,1,-1,0},ndcOffset[4]{0,0,1,0},halfPixel[2]{};
    uint32_t vtxFmt=4,flags=0;
    float alpha[4]{},colorMax[4]{1,1,1,1};
    uint32_t transfer[4]{},vfetchOffset[96]{},samplerIndex[32]{},textureInfo[32]{},textureSize[32]{};
};
static_assert(sizeof(Shared)==1024);
static float Half(uint16_t h) {
    float m=float(h&1023)/1024.f;
    return (h&0x7c00)==0 ? std::ldexp(m,-14) : std::ldexp(1.f+m,int((h>>10)&31)-15);
}
class Fixture {
    std::unique_ptr<RenderInterface> api;
    std::unique_ptr<RenderDevice> device;
    std::unique_ptr<RenderCommandQueue> queue;
    std::unique_ptr<RenderCommandList> cmd;
    std::unique_ptr<RenderCommandFence> fence;
    std::unique_ptr<RenderBuffer> vertices,vsCB,psCB,sharedCB,mvCB,readback,indices;
    std::unique_ptr<RenderTexture> color,depth,sceneDepth,tex[4];
    std::unique_ptr<RenderFramebuffer> framebuffer;
    RenderDescriptorSetBuilder builders[5];
    std::unique_ptr<RenderDescriptorSet> sets[5];
    std::unique_ptr<RenderPipelineLayout> layout;
    MotionReplayGPU replay;
    DrawTemporalTracker tracker;
    Shared shared;
    std::array<float,1024> prior{},current{},pixel{};
    uint64_t frame=10;
    void Submit() {
        replay.SealTimings(cmd.get());cmd->end();const RenderCommandList* one[]={cmd.get()};
        queue->executeCommandLists(one,1,nullptr,0,nullptr,0,fence.get());queue->waitForCommandFence(fence.get());
        replay.ReleaseCompletedThrough(replay.RecordedSerial());
    }
    std::unique_ptr<RenderShader> Compile(const std::string& source,bool isPixel,xenos::ShaderBinaryFormat format) {
        auto c=xenos::CompileCachedHlsl(source,"main",isPixel?"ps_6_0":"vs_6_0",format);
        Check(c.ok,"current translator/replay generator "+std::string(isPixel?"PS":"VS")+" "+(format==xenos::ShaderBinaryFormat::Dxil?"DXIL":"SPIR-V")+" "+c.errors);
        if(format==xenos::ShaderBinaryFormat::Dxil)return {};
        auto shader=device->createShader(c.bytecode.data(),c.bytecode.size(),"main",RenderShaderFormat::SPIRV);
        Check(bool(shader),"real Vulkan shader module");return shader;
    }
    std::vector<uint8_t> Read(RenderTexture* tex,RenderFormat fmt,unsigned bytes) {
        const unsigned stride=(W*bytes+255)&~255u;
        cmd->begin();cmd->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(tex,RenderTextureLayout::COPY_SOURCE));
        cmd->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),fmt,W,H,1,stride/bytes),RenderTextureCopyLocation::Subresource(tex));
        cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(tex,RenderTextureLayout::SHADER_READ));Submit();
        std::vector<uint8_t> out(W*H*bytes);auto* p=static_cast<const uint8_t*>(readback->map());
        for(unsigned y=0;y<H;++y)std::memcpy(out.data()+y*W*bytes,p+y*stride,W*bytes);
        readback->unmap();return out;
    }
    static void Set(std::array<float,1024>& c,unsigned slot,float x,float y,float z,float w) {
        c[slot*4]=x;c[slot*4+1]=y;c[slot*4+2]=z;c[slot*4+3]=w;
    }
    void Geometry(int test) {
        std::array<uint8_t,1024> v{};
        const float quad[8][2]={{-.8f,-.8f},{-.1f,-.8f},{-.1f,.8f},{-.8f,.8f},
             {.1f,-.8f},{.8f,-.8f},{.8f,.8f},{.1f,.8f}};
        const unsigned first=test==2?3:0;
        const unsigned stride=test==2?64:40;
        auto put=[&](unsigned i,unsigned offset,float value){std::memcpy(v.data()+(first+i)*stride+offset,&value,4);};
        for(unsigned i=0;i<(test==2?8u:4u);++i) {
            float x=test==1 && (i==1||i==2)? .8f:quad[i][0],y=quad[i][1];
            put(i,0,x);put(i,4,y);
            if(test==2) {
                put(i,24,i<4?.30f:.52f);put(i,28,i<4?.22f:.34f);
                put(i,36,(x+1)*.5f);put(i,40,(y+1)*.5f);
                put(i,44,.25f);put(i,48,i&1?1.f:0.f);put(i,52,i>=4?1.f:0.f);
            }else {
                put(i,test==1?24:32,(x+1)*.5f);
                put(i,test==1?28:36,(y+1)*.5f);
            }
            if(test==1) {
                auto* bytes=v.data()+i*stride;
                bytes[32]=i<2?0:9;bytes[33]=i<2?3:6;
                bytes[34]=i<2?6:3;bytes[35]=i<2?9:0;
                bytes[36]=i<2?102:26;bytes[37]=i<2?76:51;
                bytes[38]=i<2?51:76;bytes[39]=i<2?26:102;
            }
        }
        Write(vertices.get(),v.data(),sizeof(v));
        std::vector<uint32_t> expanded,scratch;bool indexed=false;
        if(test==2) {
            gpu::geometry_prepare::ExpandQuadList(expanded,scratch,indexed,8);
            Check(indexed&&expanded==std::vector<uint32_t>({0,1,2,0,2,3,4,5,6,4,6,7}),"production QuadList matches independent literal two-quad indices");
        } else expanded={0,1,2,0,2,3};
        Write(indices.get(),expanded.data(),expanded.size()*4);
    }
    void Constants(int test) {
        prior.fill(0);current.fill(0);pixel.fill(0);shared=Shared{};
        shared.textureInfo[0]=shared.textureInfo[1]=shared.textureInfo[2]=shared.textureInfo[3]=0x68800;
        shared.textureSize[0]=shared.textureSize[1]=shared.textureSize[2]=shared.textureSize[3]=4|(4<<16);
        if(test==0) {
            Set(current,0,0,1,0,0);Set(current,1,0,0,1,0);Set(current,2,1,0,0,0);Set(current,3,0,0,0,1);
            Set(current,8,0,0,0,0);Set(current,9,1,0,0,0);Set(current,10,0,1,0,0);Set(current,11,0,0,.5f,1);
            Set(current,7,1,1,0,0);
        } else if(test==1) {
            // Actual 4-bone branches use c[5+3*bone]..c[7+3*bone]. c[230..233] remain camera.
            Set(current,0,2,0,0,0);Set(current,254,1,0,0,0);Set(current,255,0,0,0,0);
            Set(current,1,0,0,0,0);Set(current,2,0,1,0,0);Set(current,3,0,0,1,0);Set(current,4,1,0,0,0);
            for(unsigned bone=0;bone<4;++bone) {
                Set(current,5+bone*3,0,0,1,bone*.02f);Set(current,6+bone*3,0,1,0,0);
                Set(current,7+bone*3,1,0,0,0);
            }
            Set(current,230,0,0,.5f,1);Set(current,231,0,1,0,0);
            Set(current,232,1,0,0,0);Set(current,233,0,0,0,0);
            Set(pixel,3,1,0,0,0);Set(pixel,4,1,0,0,0);Set(pixel,5,0,0,0,0);
            Set(pixel,255,-.5f,0,0,0);
        } else {
            Set(current,0,0,0,0,0);Set(current,1,.06f,0,0,0);Set(current,2,0,.09f,0,0);
            Set(current,4,1,0,0,0);Set(current,5,0,1,0,0);Set(current,6,0,0,1,0);Set(current,7,0,0,0,1);
            Set(current,8,1,0,0,0);Set(current,9,0,1,0,0);Set(current,10,0,0,0,0);Set(current,11,0,0,.5f,1);
            Set(current,253,.1f,1,.5f,1);Set(current,254,0,.2f,1,0);Set(current,255,1,0,0,1);
        }
        prior=current;
    }
public:
    Fixture() {
        api=CreateVulkanInterface();Check(bool(api),"Vulkan API");device=api->createDevice();Check(bool(device),"Vulkan device");
        printf("Device: %s\n",device->getDescription().name.c_str());
        queue=device->createCommandQueue(RenderCommandListType::DIRECT);cmd=queue->createCommandList();fence=device->createCommandFence();
        auto buffer=[&](size_t n,uint32_t flags){return device->createBuffer(RenderBufferDesc::UploadBuffer(n,flags));};
        const auto cb=RenderBufferFlag::CONSTANT|RenderBufferFlag::DEVICE_ADDRESSABLE;
        vertices=buffer(1024,RenderBufferFlag::STORAGE);indices=buffer(64,RenderBufferFlag::NONE);
        vsCB=buffer(4096,cb);psCB=buffer(4096,cb);sharedCB=buffer(1024,cb);mvCB=buffer(4352,cb);
        readback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(W*H*8));
        color=device->createTexture(RenderTextureDesc::Texture2D(W,H,1,RenderFormat::R8G8B8A8_UNORM,RenderTextureFlag::RENDER_TARGET));
        depth=device->createTexture(RenderTextureDesc::Texture2D(W,H,1,RenderFormat::D32_FLOAT,RenderTextureFlag::DEPTH_TARGET));
        sceneDepth=device->createTexture(RenderTextureDesc::Texture2D(W,H,1,RenderFormat::R32_FLOAT));
        const RenderTexture* targets[]={color.get()};framebuffer=device->createFramebuffer(RenderFramebufferDesc(targets,1,depth.get()));
        for(unsigned s=0;s<5;++s) {builders[s].begin();
            if(s==0)builders[s].addByteAddressBuffer(0);
            else if(s==1)for(unsigned t=0;t<4;++t)builders[s].addTexture(t);
            else if(s==4)builders[s].addSampler(0,64);
            else builders[s].addTexture(0);
            builders[s].end();sets[s]=builders[s].create(device.get());
        }
        sets[0]->setBuffer(0,vertices.get(),1024);
        for(unsigned t=0;t<4;++t) {
            tex[t]=device->createTexture(RenderTextureDesc::Texture2D(4,4,1,RenderFormat::R8G8B8A8_UNORM));
            sets[1]->setTexture(t,tex[t].get(),RenderTextureLayout::SHADER_READ);
        }
        std::array<uint32_t,16> texels{};
        auto upload=device->createBuffer(RenderBufferDesc::UploadBuffer(256,RenderBufferFlag::NONE));
        for(unsigned t=0;t<4;++t) {
            cmd->begin();
            for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)
                texels[y*4+x]=((t==0 && y<2 ? 0u : 255u)<<24) | 0x00ffffffu;
            Write(upload.get(),texels.data(),sizeof(texels));
            cmd->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(tex[t].get(),RenderTextureLayout::COPY_DEST));
            cmd->copyTextureRegion(RenderTextureCopyLocation::Subresource(tex[t].get()),
                RenderTextureCopyLocation::PlacedFootprint(upload.get(),RenderFormat::R8G8B8A8_UNORM,4,4,1,4));
            cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(tex[t].get(),RenderTextureLayout::SHADER_READ));
            Submit();
        }
        RenderPipelineLayoutBuilder b;b.begin(false,false);b.addPushConstant(0,0,24,RenderShaderStageFlag::VERTEX|RenderShaderStageFlag::PIXEL);
        for(auto& builder:builders)b.addDescriptorSet(builder);b.end();layout=b.create(device.get());
        const bool initialized=replay.Init(device.get(),builders,5);
        Check(initialized,"production replay GPU initialization: "+replay.LastError());
    }
    void Test(int test,const fs::path& root) {
        const fs::path external=root/"streamline-fg-p0/fsr-p2-player-feedback-audit/programs";
        const fs::path local=root/"streamline-fg-p0/fsr-p2-battle-diagnostic-01/manual-hypocenter-01/shader-cache/source";
        auto vs=Load(external/"vs"/(std::string(vsNames[test])+".bin"),vsBytes[test]);
        auto ps=Load(test==0?external/"ps"/(std::string(psNames[test])+".bin"):local/(std::string(psNames[test])+".bin"),psBytes[test]);
        auto vh=Host(vs),ph=Host(ps);
        auto v=xenos::TranslateShader(vh.data(),uint32_t(vh.size()),false);
        auto p=xenos::TranslateShader(ph.data(),uint32_t(ph.size()),true);
        Check(v.errors.empty()&&p.errors.empty(),"current translator parses original VS/PS "+std::to_string(test)+" "+v.errors+p.errors);
        Check(!p.writesDepth,"original PS replay retains its pixel coverage");
        auto vm=xenos::motion_replay::Vertex(v),pm=xenos::motion_replay::Pixel(&p);
        Check(!vm.empty()&&!pm.empty(),"current replay generator emits both programs");
        auto dv=Compile(vm,false,xenos::ShaderBinaryFormat::Dxil),dp=Compile(pm,true,xenos::ShaderBinaryFormat::Dxil);
        auto rv=Compile(vm,false,xenos::ShaderBinaryFormat::Spirv),rp=Compile(pm,true,xenos::ShaderBinaryFormat::Spirv);
        RenderGraphicsPipelineDesc d{};d.pipelineLayout=layout.get();d.vertexShader=rv.get();d.pixelShader=rp.get();
        d.renderTargetCount=1;d.renderTargetFormat[0]=RenderFormat::R8G8B8A8_UNORM;d.renderTargetBlend[0]=RenderBlendDesc::Copy();
        d.depthEnabled=d.depthWriteEnabled=true;d.depthFunction=RenderComparisonFunction::GREATER_EQUAL;
        d.depthTargetFormat=RenderFormat::D32_FLOAT;d.cullMode=RenderCullMode::NONE;
        auto originalVS=Compile(v.hlsl,false,xenos::ShaderBinaryFormat::Spirv);
        auto originalPS=Compile(p.hlsl,true,xenos::ShaderBinaryFormat::Spirv);
        d.vertexShader=originalVS.get();d.pixelShader=originalPS.get();
        auto original=device->createGraphicsPipeline(d);Check(bool(original),"original translated pipeline");
        gpu::pipeline_cache::Key key{};key.vs=hashes[test][0];key.ps=hashes[test][1];key.depthControl=0x700766;
        key.prim=test==2?13:4;key.rtFormat=uint32_t(RenderFormat::R8G8B8A8_UNORM);key.depthFormat=uint32_t(RenderFormat::D32_FLOAT);
        auto* motion=replay.PreparePipeline(key,d,vs.data(),uint32_t(vs.size()),ps.data(),uint32_t(ps.size()),true);
        Check(motion!=nullptr,"production replay pipeline from original pair: "+replay.LastError());
        Check(PositionVPSlot(key.vs)==(test==1?230:8),"real production battle position slot lookup");
        Geometry(test);Constants(test);
        // Indexed drawing keeps a nonzero baseVertex for both QuadList quads.
        const int baseVertex=test==2?3:0;const unsigned count=test==2?12:6;
        const MotionRasterContract raster{W,H,{0,0,float(W),float(H),0,1},true};
        DrawHistoryKey draw{};draw.vsHash=key.vs;draw.psHash=key.ps;draw.sceneAllocation=1;
        draw.geometrySignature=777;draw.primitiveType=key.prim;draw.baseVertex=baseVertex;draw.indexCount=count;
        auto drawFrame=[&](float jx,float jy,bool moving) {
            const uint64_t epoch=10;tracker.BeginFrame(++frame,epoch);
            const auto match=tracker.Collect(draw,current.data(),&shared,test==1,-1,nullptr,&raster);
            Check(match.previous!=nullptr,"previous constants/geometry match");
            auto mc=MakeMotionReplayConstants(match,W,H,jx,jy);
            auto jittered=current;unsigned slot=test==1?230:8;
            for(unsigned i=0;i<4;++i) {
                // Jitter applies to raw clip coordinates via the VP's W column;
                // these fixture matrices use c[230] for W on skinned geometry and c[11] otherwise.
                if(i!=(test==1?0u:3u))continue;
                jittered[(slot+i)*4]+=2*jx/W;jittered[(slot+i)*4+1]-=2*jy/H;
            }
            Write(vsCB.get(),jittered.data(),4096);Write(psCB.get(),pixel.data(),4096);
            Write(sharedCB.get(),&shared,sizeof(shared));Write(mvCB.get(),&mc,sizeof(mc));
            RenderViewport vp(0,0,W,H);RenderRect sc(0,0,W,H);
            RenderIndexBufferView indexView(RenderBufferReference(indices.get(),0),count*4,RenderFormat::R32_UINT);
            auto renderOriginal=[&]() {
                cmd->begin();
                cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(color.get(),RenderTextureLayout::COLOR_WRITE));
                cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(depth.get(),RenderTextureLayout::DEPTH_WRITE));
                cmd->setFramebuffer(framebuffer.get());cmd->clearColor(0,RenderColor(0,0,0,0));cmd->clearDepth(true,0);
                cmd->setViewports(&vp,1);cmd->setScissors(&sc,1);
                cmd->setGraphicsPipelineLayout(layout.get());cmd->setPipeline(original.get());
                uint64_t cb[]={vsCB->getDeviceAddress(),sharedCB->getDeviceAddress(),psCB->getDeviceAddress()};
                cmd->setGraphicsPushConstants(0,cb);
                for(unsigned i=0;i<5;++i)cmd->setGraphicsDescriptorSet(sets[i].get(),i);
                cmd->setIndexBuffer(&indexView);
                cmd->drawIndexedInstanced(count,1,0,baseVertex,0);Submit();
            };
            std::vector<uint8_t> uncutDepth;
            if(test==1 && !moving) {
                auto uncut=pixel;uncut[255*4]=-100.f;
                Write(psCB.get(),uncut.data(),4096);
                renderOriginal();uncutDepth=Read(depth.get(),RenderFormat::D32_FLOAT,4);
                Write(psCB.get(),pixel.data(),4096);
            }
            renderOriginal();
            // Upload the actual original depth image, so EQUAL and the validity mask
            // check exactly the fragments produced by the original PS/rasterizer.
            auto originalDepth=Read(depth.get(),RenderFormat::D32_FLOAT,4);
            auto up=device->createBuffer(RenderBufferDesc::UploadBuffer(W*H*4,RenderBufferFlag::NONE));
            Write(up.get(),originalDepth.data(),originalDepth.size());
            cmd->begin();cmd->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(sceneDepth.get(),RenderTextureLayout::COPY_DEST));
            cmd->copyTextureRegion(RenderTextureCopyLocation::Subresource(sceneDepth.get()),
                RenderTextureCopyLocation::PlacedFootprint(up.get(),RenderFormat::R32_FLOAT,W,H,1,W));
            cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(sceneDepth.get(),RenderTextureLayout::SHADER_READ));
            cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(depth.get(),RenderTextureLayout::DEPTH_WRITE));
            replay.BeginFrame(frame,epoch);Check(replay.BeginScene(cmd.get(),1,depth.get(),W,H),"motion target clear");
            RenderBufferReference constants[]={vsCB.get(),sharedCB.get(),psCB.get(),mvCB.get()};
            RenderDescriptorSet* bound[5];for(unsigned i=0;i<5;++i)bound[i]=sets[i].get();
            cmd->setIndexBuffer(&indexView);
            Check(replay.Draw(cmd.get(),motion,constants,bound,5,vp,sc,true,count,baseVertex),"actual indexed motion replay");
            auto output=replay.Finish(cmd.get(),sceneDepth.get(),tracker.FinalizeFrame());Check(output.ready,"motion validity finalized");
            Submit();auto mv=Read(output.velocity,RenderFormat::R16G16_FLOAT,4);
            auto mask=Read(output.reactive,RenderFormat::R8_UNORM,1);
            auto* raw=reinterpret_cast<const float*>(originalDepth.data());
            unsigned valid=0,discard=0;bool goodDepth=true,goodMotion=true;
            float minX=1000,maxX=-1000,minY=1000,maxY=-1000;
            for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x) {
                const unsigned i=y*W+x;
                if(mask[i]==0) {
                    ++valid;goodDepth &= raw[i]>0;
                    uint16_t halves[2];std::memcpy(halves,mv.data()+i*4,4);
                    minX=std::min(minX,Half(halves[0]));maxX=std::max(maxX,Half(halves[0]));
                    minY=std::min(minY,Half(halves[1]));maxY=std::max(maxY,Half(halves[1]));
                    if(test!=1 || !moving) {
                        // Packed world c3.y and local c7.x both move this fixture's
                        // output four pixels left; previous-current is positive X.
                        float expected=moving?4.f:0.f;
                        goodMotion &= std::abs(Half(halves[0])-expected)<.18f&&std::abs(Half(halves[1]))<.18f;
                    }
                } else if(raw[i]>0)++discard;
            }
            unsigned depthPixels=0;for(unsigned i=0;i<W*H;++i)depthPixels+=raw[i]>0;
            printf("pair=%d moving=%d valid=%u depth=%u invalid-with-depth=%u velocityX=[%.3f,%.3f] Y=[%.3f,%.3f]\n",
                test,moving,valid,depthPixels,discard,minX,maxX,minY,maxY);
            Check(goodDepth,"every valid motion pixel has EQUAL-passing source depth");
            Check(goodMotion,moving?"previous/current displacement and one jitter cancellation":"static geometry cancels jitter once");
            Check(valid>64,"source program produced many valid EQUAL-depth pixels");
            Check(mask[0]==255,"uncovered region invalid");
            if(test==1 && !moving) {
                unsigned cut=0,keep=0;
                const auto* uncut=reinterpret_cast<const float*>(uncutDepth.data());
                for(unsigned i=0;i<W*H;++i)if(uncut[i]>0) {
                    if(raw[i]==0 && mask[i]==255)++cut;
                    if(raw[i]>0 && mask[i]==0)++keep;
                }
                printf("original alpha clip: uncut EQUAL-depth eligible, discarded=%u kept=%u\n",cut,keep);
                Check(cut>64&&keep>64,"real three-texture PS discards some EQUAL-depth pixels while preserving others");
            }
            if(test==2) {
                const auto at=[&](unsigned x,unsigned y){return mask[y*W+x];};
                const unsigned gap=moving?36:32;
                printf("quad coverage x16=%u x%u=%u x48=%u at y32\n",at(16,32),gap,at(gap,32),at(48,32));
                Check(at(16,32)==0&&at(gap,32)==255&&at(48,32)==0,
                    "separate quads covered without a cross-quad triangle");
                Check(at(17,33)==0&&at(47,33)==0,
                    "both triangle boundaries of the two quads remain continuous");
            }
            if(test==1 && moving) {
                auto sample=[&](unsigned x,unsigned y){const auto i=y*W+x;uint16_t h[2];std::memcpy(h,mv.data()+i*4,4);
                    printf("skinned sample x%u y%u mask=%u velocity=(%.3f,%.3f)\n",x,y,mask[i],Half(h[0]),Half(h[1]));
                    Check(mask[i]==0,"skinned source coverage");return Half(h[0]);};
                const auto left=sample(10,44),right=sample(54,44);
                Check(left>1.f&&left<2.5f&&right>1.f&&right<2.5f&&maxX-minX>1.5f,
                    "fixed VP/world, distinct vertex weights and changed bone palette vary motion");
            }
        };
        tracker.BeginFrame(++frame,10);tracker.Collect(draw,prior.data(),&shared,test==1,-1,nullptr,&raster);
        tracker.FinalizeFrame();
        drawFrame(.25f,-.25f,false);
        if(test==0)current[3*4+1]+=.125f; // c3.y: packed world X translation, VP unchanged
        else if(test==1) {
            current[7*4]+=.30f;current[16*4]-=.35f; // c7 and c16: opposing bone X changes
        } else current[7*4]+=.125f; // local transform X, true trig/basis and VP unchanged
        drawFrame(-.25f,.25f,true);
    }
};
int main(int argc,char** argv) {
    try {Check(argc==2,"ignored evidence root passed explicitly");Fixture f;
        for(int i=0;i<3;++i)f.Test(i,fs::path(argv[1]));return 0;
    }catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
