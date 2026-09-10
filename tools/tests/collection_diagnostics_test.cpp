#include "../../LostOdysseyRecomp/gpu/collection_diagnostics.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>

using namespace gpu::taa_collection;
namespace d=gpu::taa_collection::diagnostics;
static thread_local bool watch=false;
static unsigned allocations=0,checks=0;
void* operator new(size_t size){if(watch)++allocations;if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](size_t size){return ::operator new(size);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
static void Check(bool value,const char* name){++checks;if(!value){std::cerr<<"FAILED: "<<name<<'\n';std::exit(1);}}
static binding::Record Sample() {
    binding::Record r;r.vs=0xe810cfacc107fd3cull;r.ps=0x5b11f88a8bb293dfull;
    r.width=1280;r.height=720;r.candidates=36;r.flags=19;r.rejection=2;r.guards=31;
    r.position.kind=1;r.position.slot=7;r.position.outputs=30;r.consumer.slot=7;
    for(size_t i=0;i<16;++i)r.consumer.guestVP[i]=r.consumer.uploadedVP[i]=(i%5?0:0x3f800000u);
    r.consumer.viewport={0,0,0x44a00000,0x44340000};r.psC0={0x3f000000,0xbf000000,0x3f000000,0x3f000000};
    auto& t=r.texture;t.kind=binding::TextureKind::Resolved;t.guestFormat=6;t.hostFormat=28;t.dimension=1;t.swizzle=1672;
    t.sampler={2,2,2,1,1,0};t.guestExtent=t.hostExtent=t.parentExtent={1280,720};t.resolveRect={0,0,1280,720};
    t.producerFrameAge=t.resolveFrameAge=t.resolveGap=0;t.producerState=binding::ProducerState::UniformJittered;t.producerDraws=145;t.producer=r.consumer;t.producer.applied=true;
    return r;
}
static summary_collection::Key Key(uint64_t vs=0xe810cfacc107fd3cull){auto r=Sample();return {vs,r.ps,r.width,r.height,r.slot,r.candidates,r.flags,r.rejection,r.position,r.guards};}
int main(int argc,char** argv) {
    if(argc==3&&std::string_view(argv[1])=="--emit-only") {
        d::Window w;w.complete=true;w.frameCount=w.frameSpan=32;w.pairCount=1;w.bindingCount=8;
        w.pairs[0]={Key(),0,31,32};w.counters.source={1,31,0,0,0,0};w.counters.summary={1,31,0,0,0};
        for(uint32_t i=0;i<32;++i){auto& f=w.frames[i];f.offset=i;f.taa=f.ready=f.completed=f.historyCaptured=f.cameraChecks=f.sparseReady=true;f.reused=i>0;f.previousFrameDelta=1;f.sameEpoch=true;f.historyRejection=i?0:1;}
        for(uint32_t i=0;i<8;++i)w.bindings[i]={i,Sample()};
        d::DeliverySnapshot delivery;delivery.streams[size_t(d::Stream::Source)].accepted=2;delivery.sourcePending=3;
        const auto body=d::Request("d3d12","Synthetic GPU","1","0.5.2","unknown",w,delivery);
        std::ofstream out(argv[2],std::ios::binary);out<<body;
        std::cout<<"serializer fixture: "<<body.size()<<" bytes\n";return out&& !body.empty()?0:1;
    }
    auto q=std::make_unique<d::Queue>();d::Window window;const auto now=std::chrono::steady_clock::now();
    watch=true;
    for(uint64_t frame=100;frame<132;++frame) {
        Check(q->Begin(frame,7,now),"32 contiguous frames collect");
        q->Count(d::Stream::Source,frame==100?0:1,7);q->Count(d::Stream::Summary,frame==100?0:1,7);
        q->ObservePair(frame,7,Key());
        if(frame<108)q->ObserveBinding(frame,7,Sample());
        d::Frame f;f.taa=f.ready=f.completed=f.historyCaptured=f.cameraChecks=f.sparseReady=true;
        f.reused=frame>100;f.previousFrameDelta=1;f.sameEpoch=true;f.historyRejection=frame==100?1:0;q->End(frame,7,f);
    }
    Check(!q->Active(),"full window seals");Check(q->Snapshot(window,now),"worker gets window");
    Check(window.complete&&window.frameCount==32&&window.frameSpan==32,"complete requires all contiguous frames");
    Check(window.pairCount==1&&window.pairs[0].count==32&&window.pairs[0].first==0&&window.pairs[0].last==31,"pair occurrences join relative frame window");
    Check(window.bindingCount==8&&window.bindings[7].offset==7,"bindings join exact relative frame");
    Check(window.counters.source[0]==1&&window.counters.source[1]==31,"source return reasons counted");
    Check(!q->Begin(132,7,now+std::chrono::minutes(4)),"pending window never overwritten");
    auto retained=window;q->Acknowledge(window.token,8);Check(q->Snapshot(window,now),"stale consent ack ignored");
    q->Acknowledge(window.token+1,7);Check(q->Snapshot(window,now),"stale window ack ignored");
    q->Acknowledge(window.token,7);Check(!q->Snapshot(window,now),"exact ack releases window");
    Check(!q->Begin(132,7,now),"sampling cooldown enforced");
    q->Reset();Check(q->Begin(500,9,now),"reset starts fresh consent window");
    q->ObservePair(500,8,Key());q->Count(d::Stream::Source,0,8);q->ObserveBinding(500,8,Sample());q->End(500,9,{});
    Check(q->Begin(502,9,now),"nonconsecutive frame remains partial");q->End(502,9,{});
    Check(q->Snapshot(window,now+std::chrono::seconds(11)),"worker seals partial even when rendering stops");
    Check(!window.complete&&window.frameCount==2&&window.frameSpan==3&&window.counters.frameDiscontinuity,"frame gaps explicitly diagnosed");
    Check(window.pairCount==0&&window.bindingCount==0&&window.counters.source[0]==0,"old consent evidence ignored");
    q->Reset();q->Begin(1,10,now);
    for(size_t i=0;i<d::PairCapacity+3;++i)q->ObservePair(1,10,Key(100+i));
    for(size_t i=0;i<d::BindingCapacity+2;++i)q->ObserveBinding(1,10,Sample());
    q->End(1,10,{});q->Snapshot(window,now+std::chrono::seconds(11));
    Check(window.pairCount==24&&window.counters.pairDropped==3,"pair capacity drops reported");
    Check(window.bindingCount==8&&window.counters.bindingDropped==2,"binding capacity drops reported");
    q->Reset();q->Begin(100,11,now);q->End(100,11,{});q->Begin(101,11,now);q->End(101,11,{});
    for(size_t i=0;i<100;++i){q->Begin(100,11,now);q->End(100,11,{});}
    Check(q->Snapshot(window,now)&&window.frameCount==2&&!window.complete&&window.counters.frameDiscontinuity,"regressing frames seal without overrunning fixed storage");
    q->Reset();q->Begin(9,11,now);Check(!q->Snapshot(window,now+std::chrono::seconds(11)),"empty timeout cannot emit invalid record");
    watch=false;Check(allocations==0,"all observed producer paths allocate zero heap bytes");
    const std::string id(64,'a');
    Check(d::AcceptedReceipt("{\"accepted\":1,\"id\":\""+id+"\"}"),"valid exact receipt");
    Check(d::AcceptedReceipt(" { \"id\" : \""+id+"\", \"accepted\" : 1 } \n"),"JSON whitespace and field order allowed");
    for(const auto& invalid:std::array<std::string,7>{"{}","{\"accepted\":10,\"id\":\""+id+"\"}","{\"accepted\":1,\"id\":\""+id+"\",\"extra\":0}","{\"accepted\":1,\"accepted\":1}","{\"accepted\":1,\"id\":\"short\"}","{\"accepted\":1,\"id\":\""+id+"\"}garbage",std::string(513,' ')})
        Check(!d::AcceptedReceipt(invalid),"malformed or overlong receipt retains pending data");
    d::DeliverySnapshot delivery;delivery.streams[size_t(d::Stream::Source)].accepted=2;delivery.sourcePending=3;
    const auto body=d::Request("d3d12","Synthetic GPU","1","0.5.2","unknown",retained,delivery);
    Check(!body.empty()&&body.size()<=32768,"whole request fits 32 KiB");
    Check(body==d::Request("d3d12","Synthetic GPU","1","0.5.2","unknown",retained,delivery),"frozen snapshot serializes byte identically for retry");
    Check(body.find("\"token\"")==std::string::npos&&body.find("\"consentEpoch\"")==std::string::npos&&body.find("\"currentFrame\"")==std::string::npos,"wire excludes local identity");
    Check(d::Request("d3d12","Synthetic GPU","1","0.5.2","unknown",{},delivery).empty(),"empty window not serializable");
    if(argc==2){std::ofstream out(argv[1],std::ios::binary);out<<body;Check(bool(out),"actual serializer fixture written");}
    auto worst=retained;worst.pairCount=d::PairCapacity;
    for(auto& p:worst.pairs){p=retained.pairs[0];p.count=1000000000;}
    for(auto& b:worst.bindings){b.record.consumer.guestVP.fill(UINT32_MAX);b.record.consumer.uploadedVP.fill(UINT32_MAX);b.record.texture.producer=b.record.consumer;}
    const auto bounded=d::Request("d3d12",std::string(100,'G'),std::string(20,'9'),"0.5.2","unknown",worst,delivery);
    Check(!bounded.empty()&&bounded.size()<=32768,"maximum-width numeric payload remains bounded");
    std::cout<<"compact diagnostics: "<<checks<<" checks passed; producer allocations "<<allocations<<"; fixture "<<body.size()<<" bytes; max fixture "<<bounded.size()<<" bytes\n";
}
