#include <gpu/sr_hybrid_motion.h>
#include <gpu/upscaling_plan.h>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace gpu;
namespace {
unsigned checks=0;
void Check(bool ok,const char* reason) { ++checks; if (!ok) throw std::runtime_error(reason); }
uint64_t now=0;
uint64_t Clock() { return now; }
upscaling::OutputSizing Ready(upscaling::SizingKey key) {
    upscaling::OutputSizing out; out.key=key;
    for(auto& mode:out.modes) mode={upscaling::SizingState::Ready,{key.outputWidth,key.outputHeight},
        {key.outputWidth,key.outputHeight},{key.outputWidth,key.outputHeight},1};
    return out;
}
void ProjectionAndIdentity() {
    temporal::Matrix matrix{};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;
    auto camera=temporal::Camera::Create(matrix,{0,0,8,8});Check(bool(camera),"identity camera");
    auto c=temporal::MakeSrHybridConstants(*camera,&*camera,8,8,.25,-.25,false);
    Check(bool(c),"finite jitter accepted");
    Check(c->currentNdc[0]==.25f && c->currentNdc[1]==-.25f,"raster to guest NDC");
    Check(c->previousRaster[0]==4 && c->previousRaster[1]==-4,"backward raster scale");
    Check(c->options[0]==1 && c->options[1]==0,"previous camera and no implicit geometry");
    Check(c->jitterExtent[0]==.25f && c->jitterExtent[1]==-.25f,"current jitter represented once");
    Check(temporal::MakeSrHybridConstants(*camera,nullptr,8,8,0,0,false)->options[0]==0,"missing previous is reset initialization");
    Check(temporal::MakeSrHybridConstants(*camera,&*camera,8,8,0,0,true)->options[0]==0,"explicit reset ignores old motion");
    Check(!temporal::MakeSrHybridConstants(*camera,&*camera,0,8,0,0,false),"zero extent");
    Check(!temporal::MakeSrHybridConstants(*camera,&*camera,16,8,0,0,false),"extent mismatch");
    Check(!temporal::MakeSrHybridConstants(*camera,&*camera,8,8,17,0,false),"unsupported jitter");
    Check(!temporal::MakeSrHybridConstants(*camera,&*camera,8,8,std::numeric_limits<double>::quiet_NaN(),0,false),"NaN jitter");
    auto shifted=temporal::Camera::Create(matrix,{1,0,8,8});
    Check(!temporal::MakeSrHybridConstants(*shifted,&*camera,8,8,0,0,false),"partial viewport remains ineligible");
    auto inverted=temporal::Camera::Create(matrix,{0,0,8,8,-1,.125,-.125});
    c=temporal::MakeSrHybridConstants(*inverted,&*inverted,8,8,0,0,false);
    Check(c && c->currentNdc[1]==.25f && c->previousRaster[1]==4,"Y sign preserved");
    Check(c->currentNdc[2]==-1.125f && c->previousRaster[2]==4.5f,"half-pixel correction preserved");
    auto* image=reinterpret_cast<plume::RenderTexture*>(uintptr_t(1));
    temporal::MotionFrameView view{image,nullptr,image,3,4,5,8,8,true,temporal::MotionState::Tracked};
    auto match=[&](const auto& v){return temporal::SrHybridGeometryMatches(v,3,4,5,8,8);};
    Check(match(view),"exact geometry identity");
    auto bad=view;bad.frame++;Check(!match(bad),"stale frame");
    bad=view;bad.epoch++;Check(!match(bad),"stale epoch");
    bad=view;bad.depthAllocation++;Check(!match(bad),"stale allocation");
    bad=view;bad.width++;Check(!match(bad),"stale extent");
    bad=view;bad.velocity=nullptr;Check(!match(bad),"missing velocity");
    bad=view;bad.reactive=nullptr;Check(!match(bad),"missing confidence");
    bad=view;bad.ready=false;Check(!match(bad),"unready view");
    temporal::TemporalFrameInputs input;
    input.plan.consumer=upscaling::TemporalConsumer::FsrSr; input.plan.requestedUpscaler=upscaling::Upscaler::Fsr;
    input.color=input.depth=input.motion=input.motionInvalidity={image,{8,8},0,0,8,8};
    input.currentInputsComplete=true;input.depthConvention=temporal::DepthConvention::Reversed;
    input.motionState=temporal::MotionState::Hybrid;
    Check(input.CompleteForConsumer(),"hybrid requires actual complete images");
    input.motion.texture=nullptr;Check(!input.CompleteForConsumer(),"hybrid cannot bypass missing image");input.motion=input.depth;
    input.motionInvalidity.texture=nullptr;Check(!input.CompleteForConsumer(),"hybrid cannot bypass missing mask");input.motionInvalidity=input.depth;
    input.depthConvention=temporal::DepthConvention::Unknown;Check(!input.CompleteForConsumer(),"unknown depth remains rejected");
    input.depthConvention=temporal::DepthConvention::Reversed;input.plan.frameGeneration=upscaling::FrameGeneration::Dlss2x;
    Check(!input.CompleteForConsumer(),"SR fallback does not authorize frame generation");input.plan.frameGeneration=upscaling::FrameGeneration::Off;
    input.motionState=static_cast<temporal::MotionState>(99);Check(!input.CompleteForConsumer(),"unknown motion enum rejected");
}
void SizingRecovery() {
    using namespace upscaling;
    now=0;SizingCache cache(Clock);SizingKey key{1,3840,2160};
    Check(cache.LookupOrRequestSizing(key).modes[0].state==SizingState::Pending,"first sizing pending");
    Check(cache.TakeSizingRequest()==key,"initial sizing requested");
    Check(!cache.TakeSizingRequest(),"empty take preserves in-flight request");
    cache.LookupOrRequestSizing(key);Check(!cache.TakeSizingRequest(),"no duplicate request while in flight");
    auto result=Ready(key);result.modes[3].state=SizingState::Error;result.modes[3].optimal={2560,1440};
    cache.PublishSizing(result);
    auto value=cache.LookupOrRequestSizing(key);
    Check(ModeReadyForOutput(value.modes[0],DlssQuality::Quality,{3840,2160}),"DLAA failure preserves working SR mode");
    Check(!ModeReadyForOutput(value.modes[3],DlssQuality::Dlaa,{3840,2160}),"failed DLAA not guessed 1:1");
    Check(!cache.TakeSizingRequest(),"no immediate error loop");
    now=999;cache.LookupOrRequestSizing(key);Check(!cache.TakeSizingRequest(),"first backoff boundary");
    now=1000;cache.LookupOrRequestSizing(key);Check(cache.TakeSizingRequest()==key,"bounded first retry");
    cache.LookupOrRequestSizing(key);Check(!cache.TakeSizingRequest(),"retry single flight");
    cache.PublishSizing(result);
    now=2999;cache.LookupOrRequestSizing(key);Check(!cache.TakeSizingRequest(),"second backoff boundary");
    now=3000;cache.LookupOrRequestSizing(key);Check(cache.TakeSizingRequest()==key,"bounded second retry");
    cache.PublishSizing(result);
    now=999999;cache.LookupOrRequestSizing(key);Check(!cache.TakeSizingRequest(),"persistent failures stop after three attempts");
    cache.ResetSizing(2);SizingKey next{2,3840,2160};
    cache.LookupOrRequestSizing(next);Check(cache.TakeSizingRequest()==next,"new device epoch starts fresh");
    cache.PublishSizing(result);Check(!cache.Peek(key),"old epoch publication ignored");
    cache.PublishSizing(Ready(next));Check(cache.Peek(next)->modes[3].state==SizingState::Ready,"new epoch recovery");
    Check(cache.LookupOrRequestSizing(key).modes[0].state==SizingState::Error,"stale epoch never requeued");
    Check(!cache.TakeSizingRequest(),"stale lookup does not queue work");
    now=0;SizingCache transient(Clock);transient.LookupOrRequestSizing(key);transient.TakeSizingRequest();transient.PublishSizing(result);
    now=1000;transient.LookupOrRequestSizing(key);Check(transient.TakeSizingRequest()==key,"transient retry ready");
    transient.PublishSizing(Ready(key));
    for(unsigned i=0;i<100;++i) {now+=10000;Check(transient.LookupOrRequestSizing(key).modes[3].state==SizingState::Ready && !transient.TakeSizingRequest(),"recovered query remains cached");}
    SizingCache unavailable(Clock);unavailable.LookupOrRequestSizing(key);unavailable.TakeSizingRequest();
    auto unsupported=Ready(key);unsupported.modes[3].state=SizingState::Unavailable;unavailable.PublishSizing(unsupported);
    now+=10000;unavailable.LookupOrRequestSizing(key);Check(!unavailable.TakeSizingRequest(),"unsupported modes are not retried");
    SizingCache fsr(Clock);auto fsrKey=key;fsrKey.provider=Upscaler::Fsr;fsr.LookupOrRequestSizing(fsrKey);fsr.TakeSizingRequest();
    result.key=fsrKey;fsr.PublishSizing(result);now+=10000;fsr.LookupOrRequestSizing(fsrKey);Check(!fsr.TakeSizingRequest(),"FSR sizing is unchanged");
}
}
int main() {
    try {ProjectionAndIdentity();SizingRecovery();std::printf("PASS: %u hybrid/sizing CPU checks\n",checks);return 0;}
    catch(const std::exception& e){std::fprintf(stderr,"FAIL after %u checks: %s\n",checks,e.what());return 1;}
}
