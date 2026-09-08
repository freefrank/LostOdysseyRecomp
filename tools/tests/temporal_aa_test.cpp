#include <gpu/temporal_aa.h>
#include <gpu/temporal_history.h>
#include <plume_render_interface.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
#include "temporal_aa_geometry_fixture.h"
#include "temporal_aa_trace_fixture.h"
namespace plume { std::unique_ptr<RenderInterface> CreateD3D12Interface(); std::unique_ptr<RenderInterface> CreateVulkanInterface(); }
namespace {
void Require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);std::printf("PASS: %s\n",message);}
uint32_t Pixel(unsigned gray,unsigned alpha=117) {return gray|(gray<<8)|(gray<<16)|(alpha<<24);}
}
int main(int argc,char** argv)
{
    try
    {
        using namespace plume;using namespace gpu;
        const bool vulkan=argc>1 && std::string(argv[1])=="--vulkan";
        if(vulkan){--argc;++argv;}
        auto api=vulkan?CreateVulkanInterface():CreateD3D12Interface();Require(bool(api),"render interface");
        auto device=api->createDevice();Require(bool(device),"render device");
        printf("Backend: %s on %s\n",vulkan?"Vulkan":"D3D12",device->getDescription().name.c_str());
        if(argc==5 && std::string(argv[1])=="--replay") {RunTemporalTraceReplay(device.get(),argv[2],argv[3],argv[4]);return 0;}
        auto queue=device->createCommandQueue(RenderCommandListType::DIRECT);
        auto commands=queue->createCommandList();auto fence=device->createCommandFence();
        TemporalAA taa;const bool initialized=taa.Init(device.get());Require(initialized,initialized?"temporal shaders and pipeline initialized":taa.LastError().c_str());
        constexpr unsigned W=8,H=8,N=W*H;
        auto texture=[&](RenderFormat format,bool target=false){return device->createTexture(RenderTextureDesc::Texture2D(W,H,1,format,target?RenderTextureFlag::RENDER_TARGET:RenderTextureFlag::NONE));};
        auto current=texture(RenderFormat::R8G8B8A8_UNORM),history=texture(RenderFormat::R8G8B8A8_UNORM);
        auto depth=texture(RenderFormat::R32_FLOAT),oldDepth=texture(RenderFormat::R32_FLOAT),reactive=texture(RenderFormat::R32_FLOAT);
        auto target=texture(RenderFormat::R8G8B8A8_UNORM,true);
        auto upload=device->createBuffer(RenderBufferDesc::UploadBuffer(256*H));
        auto readback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(256*H));
        auto submit=[&]{commands->end();const RenderCommandList* lists[]={commands.get()};queue->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get());queue->waitForCommandFence(fence.get());taa.ReleaseCompleted();};
        auto fill=[&](RenderTexture* tex,const void* data,RenderFormat format){
            auto* mapped=static_cast<uint8_t*>(upload->map());for(unsigned y=0;y<H;++y)std::memcpy(mapped+y*256,static_cast<const uint8_t*>(data)+y*W*4,W*4);upload->unmap();
            commands->begin();commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(tex,RenderTextureLayout::COPY_DEST));
            commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(tex),RenderTextureCopyLocation::PlacedFootprint(upload.get(),format,W,H,1,64));submit();
        };
        std::array<uint32_t,N> colors{},oldColors{};std::array<float,N> depths{},oldDepths{},masks{};
        for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x){colors[y*W+x]=Pixel(x%2?192:64,x*17+y);oldColors[y*W+x]=Pixel(40+20*x,251);}
        constexpr float D=float((100.0/8.0-1)/99.0);depths.fill(D);oldDepths.fill(D);
        auto uploadAll=[&]{fill(current.get(),colors.data(),RenderFormat::R8G8B8A8_UNORM);fill(history.get(),oldColors.data(),RenderFormat::R8G8B8A8_UNORM);fill(depth.get(),depths.data(),RenderFormat::R32_FLOAT);fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);fill(reactive.get(),masks.data(),RenderFormat::R32_FLOAT);};
        uploadAll();
        temporal::Matrix projection{1,0,0,0, 0,1,0,0, 0,0,100./99,1, 0,0,-100./99,0};
        auto camera=temporal::Camera::Create(projection,{0,0,W,H});Require(bool(camera),"test pinhole camera");
        TemporalAAInputs input;input.currentColor=current.get();input.currentDepth=depth.get();input.historyColor=history.get();input.historyDepth=oldDepth.get();input.output=target.get();input.width=W;input.height=H;input.historyWidth=W;input.historyHeight=H;input.currentCamera=&*camera;input.previousCamera=&*camera;input.historyWeight=.5f;
        auto render=[&](const TemporalAAInputs& options){
            commands->begin();for(auto* tex:{current.get(),depth.get(),history.get(),oldDepth.get(),reactive.get()})commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(tex,RenderTextureLayout::SHADER_READ));
            commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(target.get(),RenderTextureLayout::COLOR_WRITE));
            if(!taa.Resolve(commands.get(),options))throw std::runtime_error(taa.LastError());
            commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(target.get(),RenderTextureLayout::COPY_SOURCE));
            commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),RenderFormat::R8G8B8A8_UNORM,W,H,1,64),RenderTextureCopyLocation::Subresource(target.get()));submit();
            auto* mapped=static_cast<uint8_t*>(readback->map());std::array<uint32_t,N> result{};for(unsigned y=0;y<H;++y)std::memcpy(result.data()+y*W,mapped+y*256,W*4);readback->unmap();return result;
        };
        auto reset=input;reset.currentDepth=nullptr;reset.historyColor=nullptr;reset.historyDepth=nullptr;reset.currentCamera=nullptr;reset.previousCamera=nullptr;
        Require(render(reset)==colors,"history invalid identity including every alpha; absent history/cameras");
        input.historyValid=true;Require(render(input)==colors,"default reject-all history identity");
        input.rejectAllHistory=false;
        constexpr unsigned index=4*W+4;
        auto blended=render(input);Require((blended[index]&255)==92,"valid depth matched history blend (64 and 120 -> 92)");
        bool alpha=true;for(unsigned i=0;i<N;++i)alpha&=(blended[i]>>24)==(colors[i]>>24);Require(alpha,"current alpha preserved under history blending");
        auto previousProjection=projection;previousProjection[12]=-2; // camera translated +2 X at scene Z=8 => history pixel X-1.
        auto moved=temporal::Camera::Create(previousProjection,{0,0,W,H});Require(bool(moved),"translated camera");
        auto movement=input;movement.previousCamera=&*moved;
        auto shifted=render(movement);Require((shifted[index]&255)==82,"analytic one-pixel camera translation and matching depth");
        Require(shifted[4*W]==colors[4*W],"previous out-of-bounds rejects history");
        auto jitter=input;jitter.previousJitterX=1;Require((render(jitter)[index]&255)==102,"previous positive raster jitter applied once");
        jitter=input;jitter.currentJitterX=1;Require((render(jitter)[index]&255)==82,"current positive raster jitter undone once");
        auto stable=input;stable.stableGrid=true;stable.currentJitterX=.25;stable.previousJitterX=-.25;
        Require((render(stable)[index]&255)==92,"stable color history ignores jitter while raw depth retains previous jitter coordinates");
        stable.previousCamera=&*moved;
        Require((render(stable)[index]&255)==82,"stable history analytic camera translation with unequal current/previous jitter");
        auto halfProjection=projection;halfProjection[12]=-1;auto halfCamera=temporal::Camera::Create(halfProjection,{0,0,W,H});Require(bool(halfCamera),"half-pixel camera fixture");
        stable.previousCamera=&*halfCamera;
        Require((render(stable)[index]&255)==87,"stable half-pixel camera motion reconstructs history color between texels");
        oldDepths[index-2]=0;fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);
        Require(render(stable)[index]==colors[index],"stable depth support follows previous raw jitter grid rather than color grid");
        oldDepths.fill(D);fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);
        oldDepths[index+1]=.4f;fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);
        Require(render(stable)[index]==colors[index],"nonzero camera motion rejects third-depth outer signed cubic tap beyond endpoint support");
        oldDepths.fill(D);depths[index-1]=1;oldDepths[index-1]=1;
        fill(depth.get(),depths.data(),RenderFormat::R32_FLOAT);fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);
        stable.previousCamera=&*moved;
        Require(render(stable)[index]==colors[index],"paired near/far depths cannot accept camera parallax beyond fixed support radius");
        depths.fill(D);oldDepths.fill(D);fill(depth.get(),depths.data(),RenderFormat::R32_FLOAT);fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);
        masks[index]=1;fill(reactive.get(),masks.data(),RenderFormat::R32_FLOAT);auto react=input;react.reactiveMask=reactive.get();Require(render(react)[index]==colors[index],"explicit reactive mask rejects history");
        oldDepths.fill(D+.1f);fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);Require(render(input)==colors,"depth disocclusion mismatch rejects all history");
        oldDepths.fill(D);oldDepths[index+1]=0;fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);Require(render(input)[index]==colors[index],"one invalid bilinear footprint depth rejects history");
        oldDepths.fill(D);fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);
        oldDepths[index-1-W]=0;fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);
        Require(render(input)[index]==colors[index],"invalid outer cubic footprint depth rejects history");
        oldDepths.fill(D);fill(oldDepth.get(),oldDepths.data(),RenderFormat::R32_FLOAT);
        for(float bad:{0.f,-.1f,1.1f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
            depths[index]=bad;fill(depth.get(),depths.data(),RenderFormat::R32_FLOAT);Require(render(input)[index]==colors[index],"invalid current depth rejects history");
        }
        depths.fill(D);fill(depth.get(),depths.data(),RenderFormat::R32_FLOAT);
        auto behindProjection=projection;behindProjection[14]=-16*(100./99)-100./99;behindProjection[15]=-16;
        auto behind=temporal::Camera::Create(behindProjection,{0,0,W,H});Require(bool(behind),"behind-camera fixture");
        auto invalidW=input;invalidW.previousCamera=&*behind;Require(render(invalidW)==colors,"negative previous clip W rejects history");
        colors.fill(Pixel(64,93));oldColors.fill(Pixel(255));uploadAll();Require(render(input)==colors,"3x3 neighborhood clamp prevents history overshoot");
        colors.fill(Pixel(64,93));colors[index+1]=Pixel(96,93);oldColors.fill(Pixel(255));uploadAll();
        auto localReject=input;localReject.stableGrid=true;localReject.rejectOutOfNeighborhoodHistory=true;
        Require((render(input)[index]&255)==80,"baseline clamps stale bright history but retains its contribution");
        Require(render(localReject)[index]==colors[index],"local color-reactive policy rejects out-of-range stale bright history");
        localReject.diagnosticAcceptance=true;
        Require(render(localReject)[index]==0xff00ff00u,"diagnostic identifies color-reactive rejection separately");
        oldColors.fill(Pixel(80));uploadAll();
        Require(render(localReject)[index]==0xff0000ffu,"diagnostic confirms in-range history remains accepted");
        localReject.historyValid=false;
        Require(render(localReject)[index]==0xff000000u,"diagnostic reset reports other rejection");
        colors.fill(Pixel(64,93));oldColors.fill(Pixel(255));uploadAll();render(input);
        // Invalid API parameters must not record a resolve draw or alter existing output.
        commands->begin();auto invalid=input;invalid.width=0;Require(!taa.Resolve(commands.get(),invalid),"zero extent rejects before recording");
        invalid=input;invalid.output=invalid.currentColor;Require(!taa.Resolve(commands.get(),invalid),"input output alias rejected");
        invalid=input;invalid.historyWeight=std::numeric_limits<float>::quiet_NaN();Require(!taa.Resolve(commands.get(),invalid),"nonfinite policy rejected");
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),RenderFormat::R8G8B8A8_UNORM,W,H,1,64),RenderTextureCopyLocation::Subresource(target.get()));submit();
        auto* unchanged=static_cast<uint32_t*>(readback->map());bool preserved=true;
        for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x)preserved&=unchanged[y*64+x]==colors[y*W+x];
        readback->unmap();Require(preserved,"invalid API calls leave existing output unchanged (GPU readback)");
        // Two unresolved draws must retain distinct descriptors AND framebuffer attachments.
        // The second draw changes its source, so recycling the first draw's set is detectable.
        auto secondTarget=texture(RenderFormat::R8G8B8A8_UNORM,true);
        auto secondReadback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(256*H));
        auto firstReset=reset,secondReset=reset;secondReset.currentColor=history.get();secondReset.output=secondTarget.get();
        commands->begin();
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(current.get(),RenderTextureLayout::SHADER_READ));
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(history.get(),RenderTextureLayout::SHADER_READ));
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(target.get(),RenderTextureLayout::COLOR_WRITE));
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(secondTarget.get(),RenderTextureLayout::COLOR_WRITE));
        Require(taa.Resolve(commands.get(),firstReset),"record first pending resolve");
        Require(taa.Resolve(commands.get(),secondReset),"record second pending resolve");
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(target.get(),RenderTextureLayout::COPY_SOURCE));
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(secondTarget.get(),RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),RenderFormat::R8G8B8A8_UNORM,W,H,1,64),RenderTextureCopyLocation::Subresource(target.get()));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(secondReadback.get(),RenderFormat::R8G8B8A8_UNORM,W,H,1,64),RenderTextureCopyLocation::Subresource(secondTarget.get()));submit();
        auto* firstPixels=static_cast<uint32_t*>(readback->map());auto* secondPixels=static_cast<uint32_t*>(secondReadback->map());bool distinct=true;
        for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x)distinct&=firstPixels[y*64+x]==colors[y*W+x]&&secondPixels[y*64+x]==oldColors[y*W+x];
        readback->unmap();secondReadback->unmap();Require(distinct,"two pending draws preserve independent descriptor and framebuffer lifetimes");
        for(unsigned i=0;i<N;++i)colors[i]=Pixel(i*3,i*4);
        fill(current.get(),colors.data(),RenderFormat::R8G8B8A8_UNORM);
        commands->begin();
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(current.get(),RenderTextureLayout::SHADER_READ));
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(target.get(),RenderTextureLayout::COLOR_WRITE));
        TemporalDisplayInputs displayInput{current.get(),target.get(),W,H,0,0};
        Require(taa.ReconstructDisplay(commands.get(),displayInput),"record zero-jitter display reconstruction");
        auto invalidDisplay=displayInput;invalidDisplay.output=current.get();
        Require(!taa.ReconstructDisplay(commands.get(),invalidDisplay),"display input/output alias rejected");
        invalidDisplay=displayInput;invalidDisplay.jitterY=std::numeric_limits<double>::quiet_NaN();
        Require(!taa.ReconstructDisplay(commands.get(),invalidDisplay),"display nonfinite jitter rejected");
        invalidDisplay=displayInput;invalidDisplay.width=0;
        Require(!taa.ReconstructDisplay(commands.get(),invalidDisplay),"display zero extent rejected");
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(target.get(),RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),RenderFormat::R8G8B8A8_UNORM,W,H,1,64),RenderTextureCopyLocation::Subresource(target.get()));submit();
        auto* displayPixels=static_cast<uint32_t*>(readback->map());bool displayExact=true;
        for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x)displayExact&=displayPixels[y*64+x]==colors[y*W+x];
        readback->unmap();Require(displayExact,"display zero jitter exact RGBA and invalid calls preserve output");
        temporal::HistoryOwner owner;Require(owner.Init(device.get()),"owned temporal frame ring init");
        auto ownedFrame=[&](uint64_t frame,uint64_t epoch,unsigned value,bool allow) {
            for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x)colors[y*W+x]=Pixel(x%2?192:value,73);
            fill(current.get(),colors.data(),RenderFormat::R8G8B8A8_UNORM);depths.fill(D);fill(depth.get(),depths.data(),RenderFormat::R32_FLOAT);
            temporal::SceneObservation scene;scene.Reset(frame);temporal::SceneAnchor anchor;anchor.depthAllocation=1;anchor.viewport={0,0,W,H};
            for(unsigned i=0;i<16;++i)anchor.vpBits[i]=std::bit_cast<uint32_t>(float(projection[i]));
            scene.ObserveCamera(anchor);scene.ObserveDepth(1,{frame,frame*2+1,0x1000,24,W,H,true});owner.BeginFrame(frame,epoch);
            commands->begin();commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(depth.get(),RenderTextureLayout::COPY_SOURCE));
            Require(owner.CaptureDepth(commands.get(),depth.get(),scene),"copy current depth into owned frame");submit();owner.ReleaseCompleted();
            // Overwrite the external resolve immediately: owned depth must survive.
            depths.fill(0);fill(depth.get(),depths.data(),RenderFormat::R32_FLOAT);
            scene.ObserveColor({frame,frame*2+2,0x2000,6,W,H,true});
            commands->begin();commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(current.get(),RenderTextureLayout::COPY_SOURCE));
            auto* output=owner.ResolveColor(commands.get(),current.get(),scene,0,0,allow);Require(output!=nullptr,"owned pre-UI resolve returns display texture");
            commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(output,RenderTextureLayout::COPY_SOURCE));
            commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),RenderFormat::R8G8B8A8_UNORM,W,H,1,64),RenderTextureCopyLocation::Subresource(output));
            // Restore the documented owner output state after the external readback.
            commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(output,RenderTextureLayout::SHADER_READ));submit();owner.ReleaseCompleted();
            auto* data=static_cast<uint32_t*>(readback->map());uint32_t result=data[4*64+4];readback->unmap();return result;
        };
        Require((ownedFrame(10,1,64,true)&255)==64&&!owner.Reused(),"owned first frame reset identity");
        Require((ownedFrame(11,1,96,true)&255)==96&&owner.Reused(),"owned consecutive frame reuses history with neighborhood constraint");
        Require((ownedFrame(13,1,48,true)&255)==48&&!owner.Reused(),"owned skipped frame resets");
        Require((ownedFrame(14,2,80,true)&255)==80&&!owner.Reused(),"owned scene epoch change resets");
        Require((ownedFrame(15,2,64,true)&255)>64&&owner.Reused(),"owned depth survives source overwrite and accumulated color survives external overwrite");
        owner.Reset();Require((ownedFrame(16,2,32,true)&255)==32&&!owner.Reused(),"owned explicit reset identity");
        RunTemporalGeometryFixture(device.get(),argc>1?std::filesystem::path(argv[1]):std::filesystem::path("out/v0.4.0-temporal/stable-display-geometry"));
        const auto geometryRoot=argc>1?std::filesystem::path(argv[1]):std::filesystem::path("out/v0.4.0-temporal/stable-display-geometry");
        RunTemporalGeometryFixture(device.get(),geometryRoot/"distinct-depth-static",1);
        RunTemporalGeometryFixture(device.get(),geometryRoot/"distinct-depth-moving",2);
        RunTemporalGeometryFixture(device.get(),geometryRoot/"stable-grid-material",0,true);
        RunTemporalGeometryFixture(device.get(),geometryRoot/"stable-grid-depth-static",1,true);
        RunTemporalGeometryFixture(device.get(),geometryRoot/"stable-grid-depth-moving",2,true);
        RunTemporalGeometryFixture(device.get(),geometryRoot/"reactive-grid-material",0,true,true);
        RunTemporalGeometryFixture(device.get(),geometryRoot/"reactive-grid-depth-static",1,true,true);
        RunTemporalGeometryFixture(device.get(),geometryRoot/"reactive-grid-depth-moving",2,true,true);
        std::puts("PASS: temporal resolve GPU fixture; synthetic inputs only, no game/TAA acceptance");
        return 0;
    }
    catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
