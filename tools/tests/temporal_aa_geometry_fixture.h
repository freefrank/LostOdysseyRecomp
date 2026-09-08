#pragma once
#include <gpu/shader/dxc_compiler.h>
#include <plume_render_interface_builders.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>

// Real rasterized geometry, independent analytic pixel-area coverage reference.
// Every Halton phase is retained, including failures; no phase/threshold selection.
inline void RunTemporalGeometryFixture(plume::RenderDevice* device,const std::filesystem::path& directory,unsigned mode=0,bool stableGrid=false,bool colorReactive=false)
{
    using namespace plume;using namespace gpu;
    constexpr unsigned W=128,H=64;
    const unsigned Frames=mode==2?48:32; // Preserve original phases 1-32; retreat exposes background at 33.
    constexpr double Slope=.375,Intercept=40.25;
    std::filesystem::create_directories(directory);
    auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    auto queue=device->createCommandQueue(RenderCommandListType::DIRECT);auto commands=queue->createCommandList();auto fence=device->createCommandFence();
    TemporalAA temporal;check(temporal.Init(device),"geometry temporal init");
    auto submit=[&]{commands->end();const RenderCommandList* lists[]={commands.get()};queue->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get());queue->waitForCommandFence(fence.get());temporal.ReleaseCompleted();};
    const char* geometry=R"(

#ifdef __spirv__
struct SceneParameters{float2 extent;float2 jitter;uint foreground;float backgroundDepth;float2 padding;};
[[vk::push_constant]] ConstantBuffer<SceneParameters> scene;
#define extent scene.extent
#define jitter scene.jitter
#define foreground scene.foreground
#define backgroundDepth scene.backgroundDepth
#else
cbuffer Scene:register(b0){float2 extent;float2 jitter;uint foreground;float backgroundDepth;float2 padding;};
#endif
float4 vertex(uint id:SV_VertexID):SV_Position {
 float2 p;
 if(!foreground){float2 uv=float2((id<<1)&2,id&2);p=uv*extent;}
 else {
  const float2 quad[4]={float2(-256,-256),float2(-55.75,-256),float2(136.25,256),float2(-256,256)};
  const uint ids[6]={0,1,2,0,2,3};p=quad[ids[id]]+jitter;
 }
 return float4(p/extent*float2(2,-2)+float2(-1,1),.5,1);
}
struct Outputs {float4 color:SV_Target0;float depth:SV_Target1;};
Outputs pixel(){Outputs o;o.color=float4(foreground.xxx,.4);o.depth=foreground?.5:backgroundDepth;return o;}
)";
    const auto format=device->getCapabilities().shaderFormat;
    const auto binary=format==RenderShaderFormat::SPIRV?xenos::ShaderBinaryFormat::Spirv:xenos::ShaderBinaryFormat::Dxil;
    auto vb=xenos::CompileHlsl(geometry,"vertex","vs_6_0",binary),pb=xenos::CompileHlsl(geometry,"pixel","ps_6_0",binary);
    check(vb.ok&&pb.ok,"actual geometry shaders compile");
    auto vs=device->createShader(vb.bytecode.data(),vb.bytecode.size(),"vertex",format);
    auto ps=device->createShader(pb.bytecode.data(),pb.bytecode.size(),"pixel",format);
    RenderPipelineLayoutBuilder lb;lb.begin(false,false);lb.addPushConstant(0,0,32,RenderShaderStageFlag::VERTEX|RenderShaderStageFlag::PIXEL);lb.end();auto layout=lb.create(device);
    RenderGraphicsPipelineDesc pipelineDesc;pipelineDesc.pipelineLayout=layout.get();pipelineDesc.vertexShader=vs.get();pipelineDesc.pixelShader=ps.get();pipelineDesc.renderTargetCount=2;
    pipelineDesc.renderTargetFormat[0]=RenderFormat::R8G8B8A8_UNORM;pipelineDesc.renderTargetFormat[1]=RenderFormat::R32_FLOAT;
    pipelineDesc.renderTargetBlend[0]=pipelineDesc.renderTargetBlend[1]=RenderBlendDesc::Copy();pipelineDesc.cullMode=RenderCullMode::NONE;
    auto pipeline=device->createGraphicsPipeline(pipelineDesc);check(bool(pipeline),"geometry pipeline");
    auto make=[&](RenderFormat format){return device->createTexture(RenderTextureDesc::Texture2D(W,H,1,format,RenderTextureFlag::RENDER_TARGET));};
    auto color=make(RenderFormat::R8G8B8A8_UNORM),depth=make(RenderFormat::R32_FLOAT),oldDepth=make(RenderFormat::R32_FLOAT);
    std::array<std::unique_ptr<RenderTexture>,2> history={make(RenderFormat::R8G8B8A8_UNORM),make(RenderFormat::R8G8B8A8_UNORM)};
    auto display=make(RenderFormat::R8G8B8A8_UNORM),baseline=make(RenderFormat::R8G8B8A8_UNORM);
    const RenderTexture* targets[]={color.get(),depth.get()};auto framebuffer=device->createFramebuffer(RenderFramebufferDesc(targets,2));
    auto readback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(W*H*4));
    auto fetch=[&](RenderTexture* texture){commands->begin();commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(texture,RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),RenderFormat::R8G8B8A8_UNORM,W,H,1,W),RenderTextureCopyLocation::Subresource(texture));submit();
        auto* data=static_cast<uint32_t*>(readback->map());std::vector<uint32_t> pixels(data,data+W*H);readback->unmap();return pixels;};
    auto save=[&](const std::vector<uint32_t>& pixels,const std::string& name){std::ofstream file(directory/name,std::ios::binary);file<<"P6\n"<<W<<' '<<H<<"\n255\n";for(uint32_t p:pixels){char rgb[3]={char(p&255),char((p>>8)&255),char((p>>16)&255)};file.write(rgb,3);}};
    auto drawScene=[&](double jx,double jy){commands->begin();commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(color.get(),RenderTextureLayout::COLOR_WRITE));commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(depth.get(),RenderTextureLayout::COLOR_WRITE));
        commands->setFramebuffer(framebuffer.get());RenderViewport viewport(0,0,W,H);RenderRect scissor(0,0,W,H);commands->setViewports(&viewport,1);commands->setScissors(&scissor,1);
        commands->setGraphicsPipelineLayout(layout.get());commands->setPipeline(pipeline.get());
        struct {float width,height,jx,jy;uint32_t foreground;float backgroundDepth;float pad[2];} c{float(W),float(H),float(jx),float(jy),0,mode?.25f:.5f,{0,0}};
        commands->setGraphicsPushConstants(0,&c);commands->drawInstanced(3,1,0,0);c.foreground=1;commands->setGraphicsPushConstants(0,&c);commands->drawInstanced(6,1,0,0);submit();};
    double objectOffset=0;
    auto coverage=[&](unsigned x,unsigned y){
        std::vector<double> cuts={double(y),double(y+1)};
        for(double edge:{double(x),double(x+1)}){double t=(edge-Intercept-objectOffset)/Slope;if(t>y&&t<y+1)cuts.push_back(t);}
        std::sort(cuts.begin(),cuts.end());double area=0;
        for(size_t i=1;i<cuts.size();++i){auto f=[&](double t){return std::clamp(Slope*t+Intercept+objectOffset-x,0.,1.);};area+=(cuts[i]-cuts[i-1])*(f(cuts[i])+f(cuts[i-1]))*.5;}return area;};
    std::vector<double> reference(W*H);std::vector<uint32_t> refPixels(W*H);
    for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x){const auto i=y*W+x;reference[i]=coverage(x,y);unsigned c=unsigned(std::lround(reference[i]*255));refPixels[i]=c|(c<<8)|(c<<16)|0xff000000;}
    save(refPixels,"analytic-coverage.ppm");
    struct Metrics {double mse=0,mae=0,centroid=0,edgeWidth=0;};
    auto measure=[&](const std::vector<uint32_t>& pixels){Metrics result;unsigned count=0;
        for(unsigned y=4;y<H-4;++y){double mass=0;for(unsigned x=0;x<W;++x){double value=(pixels[y*W+x]&255)/255.;mass+=value;
            if(std::abs(x+.5-(Slope*(y+.5)+Intercept+objectOffset))<3){double e=value-reference[y*W+x];result.mse+=e*e;result.mae+=std::abs(e);++count;}}
            result.centroid+=mass-(Slope*(y+.5)+Intercept+objectOffset);
            auto crossing=[&](double threshold){for(unsigned x=0;x<W-1;++x){double a=(pixels[y*W+x]&255)/255.,b=(pixels[y*W+x+1]&255)/255.;if(a>=threshold&&b<threshold)return x+.5+(a-threshold)/(a-b);}return 0.;};
            result.edgeWidth+=crossing(.1)-crossing(.9);
        }result.mse/=count;result.mae/=count;result.centroid/=(H-8);result.edgeWidth/=(H-8);return result;};
    auto referenceMetrics=measure(refPixels);
    drawScene(0,0);auto offPixels=fetch(color.get());save(offPixels,"off-no-jitter.ppm");const auto off=measure(offPixels);
    temporal::Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};auto camera=temporal::Camera::Create(identity,{0,0,W,H});check(bool(camera),"geometry camera");
    auto halton=[](unsigned i,unsigned base){double value=0,f=1;while(i){f/=base;value+=f*(i%base);i/=base;}return value;};
    std::vector<Metrics> rawMetrics(Frames),baselineMetrics(Frames),resolvedMetrics(Frames);
    std::vector<std::vector<uint32_t>> allResolved,allBaseline;
    std::ofstream csv(directory/"phases.csv");csv<<"phase,jx,jy,raw_mse,baseline_mse,resolved_mse,raw_centroid,baseline_centroid,resolved_centroid,baseline_width,resolved_width,object_offset\n"<<std::setprecision(12);
    double previousX=0,previousY=0;
    for(unsigned frame=0;frame<Frames;++frame){const double jx=halton(frame+1,2)-.5,jy=halton(frame+1,3)-.5;
        objectOffset=mode==2&&frame>=16&&frame<32?8:0;
        for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x)reference[y*W+x]=coverage(x,y);
        drawScene(jx+objectOffset,jy);auto raw=fetch(color.get());rawMetrics[frame]=measure(raw);
        commands->begin();for(auto* t:{color.get(),depth.get(),oldDepth.get(),history[(frame+1)%2].get()})commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(t,RenderTextureLayout::SHADER_READ));
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(history[frame%2].get(),RenderTextureLayout::COLOR_WRITE));
        TemporalAAInputs in;in.currentColor=color.get();in.currentDepth=depth.get();in.historyColor=history[(frame+1)%2].get();in.historyDepth=oldDepth.get();in.output=history[frame%2].get();in.width=W;in.height=H;in.historyWidth=W;in.historyHeight=H;in.currentCamera=&*camera;in.previousCamera=&*camera;
        in.rejectOutOfNeighborhoodHistory=colorReactive;in.stableGrid=stableGrid;in.historyValid=frame!=0;in.rejectAllHistory=false;in.currentJitterX=jx;in.currentJitterY=jy;in.previousJitterX=previousX;in.previousJitterY=previousY;
        check(temporal.Resolve(commands.get(),in),"geometry resolve");
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(history[frame%2].get(),RenderTextureLayout::SHADER_READ));
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(display.get(),RenderTextureLayout::COLOR_WRITE));
        commands->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(baseline.get(),RenderTextureLayout::COLOR_WRITE));
        check(temporal.ReconstructDisplay(commands.get(),{history[frame%2].get(),display.get(),W,H,stableGrid?0:jx,stableGrid?0:jy}),"stable display reconstruction");
        check(temporal.ReconstructDisplay(commands.get(),{color.get(),baseline.get(),W,H,jx,jy}),"jitter-only display baseline");
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(depth.get(),RenderTextureLayout::COPY_SOURCE));commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(oldDepth.get(),RenderTextureLayout::COPY_DEST));
        commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(oldDepth.get()),RenderTextureCopyLocation::Subresource(depth.get()));submit();
        auto resolved=fetch(display.get()),base=fetch(baseline.get());resolvedMetrics[frame]=measure(resolved);baselineMetrics[frame]=measure(base);allResolved.push_back(resolved);allBaseline.push_back(base);
        char tag[32];std::snprintf(tag,sizeof(tag),"phase-%02u",frame+1);save(raw,std::string(tag)+"-raw.ppm");save(base,std::string(tag)+"-jitter-only.ppm");save(resolved,std::string(tag)+"-resolved.ppm");
        csv<<frame+1<<','<<jx<<','<<jy<<','<<rawMetrics[frame].mse<<','<<baselineMetrics[frame].mse<<','<<resolvedMetrics[frame].mse<<','<<rawMetrics[frame].centroid<<','<<baselineMetrics[frame].centroid<<','<<resolvedMetrics[frame].centroid<<','<<baselineMetrics[frame].edgeWidth<<','<<resolvedMetrics[frame].edgeWidth<<','<<objectOffset<<'\n';previousX=jx;previousY=jy;
    }
    auto average=[](const auto& series,unsigned begin,unsigned end,auto member){double sum=0;for(unsigned i=begin;i<end;++i)sum+=series[i].*member;return sum/(end-begin);};
    auto spread=[&](const auto& series){double lo=1e9,hi=-1e9;for(unsigned i=8;i<Frames;++i){lo=std::min(lo,series[i].centroid);hi=std::max(hi,series[i].centroid);}return hi-lo;};
    // Variance never spans an object step. Each segment samples its current edge.
    auto variance=[&](const auto& images,unsigned begin,unsigned end,double offset){double total=0;unsigned count=0;for(unsigned y=4;y<H-4;++y)for(unsigned x=0;x<W;++x)if(std::abs(x+.5-(Slope*(y+.5)+Intercept+offset))<3){double mean=0,m2=0;for(unsigned i=begin;i<end;++i){double v=(images[i][y*W+x]&255)/255.;mean+=v;m2+=v*v;}total+=std::max(0.,m2/(end-begin)-std::pow(mean/(end-begin),2));++count;}return total/count;};
    auto stableVariance=[&](const auto& images){return mode==2?(variance(images,8,16,0)+variance(images,24,32,8)+variance(images,40,48,0))/3:variance(images,8,32,0);};
    const double baseMse=average(baselineMetrics,8,32,&Metrics::mse),resolvedMse=average(resolvedMetrics,8,32,&Metrics::mse);
    const double firstMse=average(resolvedMetrics,0,8,&Metrics::mse),lateMse=average(resolvedMetrics,Frames-8,Frames,&Metrics::mse);
    const double baseVariance=stableVariance(allBaseline),resolvedVariance=stableVariance(allResolved);
    std::ofstream report(directory/"summary.json");report<<std::setprecision(12)<<"{\n\"frames\":"<<Frames<<",\n\"variance_contract\":\"per-stationary-segment-current-edge-ROI\",\"off_edge_mse\":"<<off.mse<<",\"jitter_only_mean_mse_phases9_32\":"<<baseMse<<",\"resolved_mean_mse_phases9_32\":"<<resolvedMse<<",\"resolved_first8_mse\":"<<firstMse<<",\"resolved_last8_mse\":"<<lateMse<<",\"raw_centroid_phase_range\":"<<spread(rawMetrics)<<",\"jitter_only_centroid_phase_range\":"<<spread(baselineMetrics)<<",\"resolved_centroid_phase_range\":"<<spread(resolvedMetrics)<<",\"jitter_only_temporal_variance\":"<<baseVariance<<",\"resolved_temporal_variance\":"<<resolvedVariance<<",\"analytic_edge_width\":"<<referenceMetrics.edgeWidth<<",\"resolved_mean_edge_width\":"<<average(resolvedMetrics,8,Frames,&Metrics::edgeWidth)<<"\n}\n";report.close();csv.close();
    std::printf("Geometry: off MSE %.6f, display-only %.6f, resolved %.6f; first8 %.6f last8 %.6f; centroid ranges %.6f/%.6f/%.6f; variance %.6f -> %.6f\n",off.mse,baseMse,resolvedMse,firstMse,lateMse,spread(rawMetrics),spread(baselineMetrics),spread(resolvedMetrics),baseVariance,resolvedVariance);
    std::printf("Geometry artifacts: %s\n",directory.string().c_str());
    if(mode==2 || (mode && !stableGrid)) {std::puts("DIAGNOSTIC: distinct-depth silhouette; coverage/convergence not asserted, inspect phase metrics (mode 2 advances at phase 17 and retreats at 33; run temporal_geometry_analysis.py for segmented convergence and revealed-background residual)");return;}
    check(resolvedMse<off.mse,"resolved subpixel coverage must beat unjittered point sampling");
    check(resolvedMse<baseMse,"history must improve coverage error beyond display-only reconstruction");
    check(lateMse<firstMse,"late coverage error must improve on the first eight phases");
    check(resolvedVariance<baseVariance,"temporal accumulation must reduce variance beyond display-only filtering");
    check(spread(resolvedMetrics)<spread(rawMetrics),"stable output must reduce raw jitter phase centroid drift");
    check(average(resolvedMetrics,8,Frames,&Metrics::edgeWidth)<referenceMetrics.edgeWidth+1.5,"edge reconstruction must not exceed reference width by 1.5 pixels");
    std::puts("PASS: actual Halton geometry coverage, phase stability and bounded edge width; see every phase/convergence metric");
}
