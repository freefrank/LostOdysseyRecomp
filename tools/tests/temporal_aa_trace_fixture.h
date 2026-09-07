#pragma once
#include <filesystem>
#include <fstream>
#include <bit>
// Replay manifest: 16 hex VP words, then decimal frame numbers. Caller has proved
// this one camera applies to every listed source/depth pair; no CPU-time guessing.
inline void RunTemporalTraceReplay(plume::RenderDevice* device,const std::filesystem::path& source,
    const std::filesystem::path& manifest,const std::filesystem::path& output)
{
    using namespace plume;using namespace gpu;
    constexpr unsigned W=1280,H=720,Bytes=W*H*4;
    auto check=[](bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);};
    check(!std::filesystem::exists(output),"replay refuses existing output directory");
    std::ifstream spec(manifest);temporal::Matrix vp{};uint32_t word;
    for(auto& value:vp){check(bool(spec>>std::hex>>word),"replay VP manifest");value=std::bit_cast<float>(word);}
    std::vector<unsigned> frames;unsigned frame;while(spec>>std::dec>>frame)frames.push_back(frame);
    check(frames.size()>1,"replay requires multiple frames");
    for(size_t i=1;i<frames.size();++i)check(frames[i]==frames[i-1]+1,"replay requires consecutive frames");
    auto camera=temporal::Camera::Create(vp,{0,0,W,H,1,1./W,-1./H});check(bool(camera),"replay actual camera accepted");
    auto read=[&](const std::filesystem::path& path){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f)&&f.tellg()==Bytes,"replay input missing or incorrect extent");std::vector<uint8_t>b(Bytes);f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),Bytes);check(bool(f),"replay input read");return b;};
    auto filename=[](unsigned frame,unsigned kind){return "trace_f"+std::to_string(frame)+"_affff000"+std::to_string(kind)+"_n1_1280x720_fmt"+(kind==3?"34":"20")+".bin";};
    // Validate all source inputs before creating any output.
    for(auto f:frames){read(source/filename(f,1));read(source/filename(f,3));}
    std::filesystem::create_directories(output);
    auto radical=[](unsigned n,unsigned base){double f=1,r=0;while(n){f/=base;r+=f*(n%base);n/=base;}return r-.5;};
    for(unsigned policy=0;policy<2;++policy)
    {
        const auto dir=output/(policy?"candidate":"baseline");std::filesystem::create_directory(dir);
        auto queue=device->createCommandQueue(RenderCommandListType::DIRECT);auto commands=queue->createCommandList();auto fence=device->createCommandFence();
        TemporalAA taa;check(taa.Init(device),"trace temporal init");
        auto texture=[&](RenderFormat fmt,bool rt=false){return device->createTexture(RenderTextureDesc::Texture2D(W,H,1,fmt,rt?RenderTextureFlag::RENDER_TARGET:RenderTextureFlag::NONE));};
        auto color=texture(RenderFormat::R8G8B8A8_UNORM),depth=texture(RenderFormat::R32_FLOAT),previousDepth=texture(RenderFormat::R32_FLOAT);
        std::unique_ptr<RenderTexture> history[2]={texture(RenderFormat::R8G8B8A8_UNORM,true),texture(RenderFormat::R8G8B8A8_UNORM,true)};
        auto mask=texture(RenderFormat::R8G8B8A8_UNORM,true);
        auto upload=device->createBuffer(RenderBufferDesc::UploadBuffer(Bytes)),readback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(Bytes));
        auto submit=[&]{commands->end();const RenderCommandList* lists[]={commands.get()};queue->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get());queue->waitForCommandFence(fence.get());taa.ReleaseCompleted();};
        auto fill=[&](RenderTexture* target,const std::vector<uint8_t>& bytes,RenderFormat fmt){auto* p=upload->map();std::memcpy(p,bytes.data(),Bytes);upload->unmap();commands->begin();commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(target,RenderTextureLayout::COPY_DEST));commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(target),RenderTextureCopyLocation::PlacedFootprint(upload.get(),fmt,W,H,1,W));submit();};
        auto save=[&](RenderTexture* target,const std::filesystem::path& path){commands->begin();commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(target,RenderTextureLayout::COPY_SOURCE));commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),RenderFormat::R8G8B8A8_UNORM,W,H,1,W),RenderTextureCopyLocation::Subresource(target));submit();auto* p=readback->map();std::ofstream file(path,std::ios::binary);file.write(static_cast<const char*>(p),Bytes);readback->unmap();check(bool(file),"replay output write");};
        for(size_t i=0;i<frames.size();++i)
        {
            const auto f=frames[i];fill(color.get(),read(source/filename(f,1)),RenderFormat::R8G8B8A8_UNORM);fill(depth.get(),read(source/filename(f,3)),RenderFormat::R32_FLOAT);
            fill(previousDepth.get(),read(source/filename(i?frames[i-1]:f,3)),RenderFormat::R32_FLOAT);
            TemporalAAInputs in;in.stableGrid=true;in.rejectAllHistory=false;in.historyValid=i!=0;in.rejectOutOfNeighborhoodHistory=policy!=0;
            in.currentColor=color.get();in.currentDepth=depth.get();in.historyColor=history[(i+1)%2].get();in.historyDepth=previousDepth.get();in.output=history[i%2].get();
            in.width=in.historyWidth=W;in.height=in.historyHeight=H;in.currentCamera=in.previousCamera=&*camera;
            in.currentJitterX=radical(f%32+1,2);in.currentJitterY=radical(f%32+1,3);
            in.previousJitterX=radical((i?frames[i-1]:f)%32+1,2);in.previousJitterY=radical((i?frames[i-1]:f)%32+1,3);
            commands->begin();for(auto* t:{color.get(),depth.get(),previousDepth.get(),history[(i+1)%2].get()})commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(t,RenderTextureLayout::SHADER_READ));
            commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(in.output,RenderTextureLayout::COLOR_WRITE));check(taa.Resolve(commands.get(),in),"replay color resolve");
            auto diagnostic=in;diagnostic.output=mask.get();diagnostic.diagnosticAcceptance=true;
            commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(mask.get(),RenderTextureLayout::COLOR_WRITE));check(taa.Resolve(commands.get(),diagnostic),"replay acceptance resolve");submit();
            save(in.output,dir/(std::to_string(f)+".rgba"));save(mask.get(),dir/(std::to_string(f)+".acceptance.rgba"));
        }
    }
    std::puts("Replay complete: independent baseline/candidate histories, all frames and acceptance masks retained; not a visual acceptance assertion.");
}
