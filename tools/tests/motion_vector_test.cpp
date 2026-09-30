#include <gpu/motion_vector.h>
#include <gpu/motion_options.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <new>
#include <stdexcept>
static size_t allocations=0;
void* operator new(size_t n) { ++allocations; if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
using namespace gpu::temporal;
static unsigned checks=0;
static void Require(bool ok,const char* why) {++checks;if(!ok)throw std::runtime_error(why);}
static Matrix Projection() {return {1,0,0,0, 0,2,0,0, 0,0,100./99,1, 0,0,-100./99,0};}
static void ExactStationary() {
    DrawTemporalTracker t; DrawHistoryKey key{}; key.geometrySignature=1;
    std::array<uint32_t,1024> c{}; std::array<uint32_t,52> shared{};
    MotionRasterContract raster{64,64,{0,0,64,64,0,1},true};
    uint64_t frame=0;
    auto collect=[&](const MotionRasterContract* r) {
        t.BeginFrame(++frame); auto m=t.Collect(key,c.data(),shared.data(),false,-1,nullptr,r);
        t.FinalizeFrame(); return m;
    };
    Require(!collect(&raster).exactStationary,"first frame cannot prove stationary");
    Require(collect(&raster).exactStationary,"identical complete inputs prove stationary");
    c[16]^=1; Require(!collect(&raster).exactStationary,"one position-constant bit preserves slow motion");
    Require(collect(&raster).exactStationary,"unchanged new position becomes stationary");
    shared[51]^=4; Require(!collect(&raster).exactStationary,"last shared word participates in proof");
    key.geometrySignature++; Require(!collect(&raster).exactStationary,"changed actual geometry identity cannot be stationary");
    collect(&raster); raster.width=128; raster.viewport[2]=128;
    Require(!collect(&raster).exactStationary,"target and viewport resize cannot prove stationary");
    Require(!collect(nullptr).exactStationary,"missing raster proof fails closed");
    Require(!collect(&raster).exactStationary,"previous raster proof also required");
    raster.geometryVerified=false; Require(!collect(&raster).exactStationary,"unverified geometry fails closed");
    raster.geometryVerified=true;
    std::array<uint32_t,16> original{};
    for(unsigned phase=0;phase<4;++phase) {
        c[16]=0x3f000000u+phase;
        t.BeginFrame(++frame);
        auto m=t.Collect(key,c.data(),shared.data(),false,4,original.data(),&raster);
        Require(phase==0 ? !m.exactStationary : m.exactStationary,"jitter restoration retains exact stationary proof");
        t.FinalizeFrame();
    }
}
static void ConstantUsage() {
    const auto usage=ParseMotionConstantUsage("float4 XeConst(int index) {}\nvoid main(\n) {r0=XeConst(4);r1=XeConst(255);}",false);
    Require(usage.known&&usage.slots[0]==16&&usage.slots[3]==(uint64_t(1)<<63),"literal usage excludes prelude definition and includes last slot");
    for(const char* body:{"XeConst(4+a0)","XeConst(256)","c [4]","XeOtherConst(4)","vk::RawBufferLoad<float4>(x)"})
        Require(!ParseMotionConstantUsage(std::string("void main(\n) {")+body+";}",false).known,"unknown constant access falls back to full bank");
    Require(!ParseMotionConstantUsage("void main(\n) {XeConst(4);}",true).known,"relative metadata forces full bank");
    Require(!ParseMotionConstantUsage("",false).known,"missing source forces full bank");
    DrawTemporalTracker t;DrawHistoryKey key{};std::array<uint32_t,1024> c{};std::array<uint32_t,52> shared{};
    MotionRasterContract raster{64,64,{0,0,64,64,0,1},true};uint64_t frame=0;
    auto next=[&](const MotionConstantUsage* u){t.BeginFrame(++frame);auto m=t.Collect(key,c.data(),shared.data(),false,-1,nullptr,&raster,u);t.FinalizeFrame();return m.exactStationary;};
    next(&usage);c[0]=1;Require(next(&usage),"unread float slot does not block stationary proof");
    c[16]=1;Require(!next(&usage),"one bit in read float slot blocks stationary proof");
    c[1023]=1;Require(!next(&usage),"all components of last read slot participate");
    shared[43]=1;shared[47]=1;shared[51]=1;Require(next(&usage),"unused NDC w and PS-only flags do not block proof");
    shared[51]^=8;Require(!next(&usage),"VS epilogue flags still block proof");
    c[0]++;Require(!next(nullptr),"unknown current usage falls back to full bank");
    c[0]++;Require(!next(&usage),"unknown previous usage falls back to full bank");
}
int main(int argc,char** argv) {try {
    if(argc>1&&std::strcmp(argv[1],"--constant-usage-only")==0){ConstantUsage();printf("PASS: %u constant usage checks\n",checks);return 0;}
    if(argc>1&&std::strcmp(argv[1],"--exact-stationary-only")==0){ExactStationary();printf("PASS: %u exact stationary checks\n",checks);return 0;}
    ConstantUsage();
    ExactStationary();
    const auto hash = MotionHashWord(0xcbf29ce484222325ULL, 7);
    Require(hash != MotionHashWord(0xcbf29ce484222325ULL, 8), "stream data changes geometry identity");
    std::vector<uint32_t> indices{0, 1, 2, 2, 3, 0};
    const auto indexHash = MotionHashIndices(indices);
    Require(indexHash == MotionHashIndices(indices), "cached and direct index signatures agree");
    indices[3] = 4;
    Require(indexHash != MotionHashIndices(indices), "changed index invalidates motion geometry identity");
    Require(MotionHashWord(hash, 1) != MotionHashWord(hash, 0x100000001ULL), "full arena generation participates in geometry identity");
    DrawTemporalTracker t(8);DrawHistoryKey k{};k.vsHash=7;k.sceneAllocation=1;k.geometrySignature=2;
    std::array<uint32_t,1024> constants{};std::array<uint32_t,52> shared{};
    constants[0]=0x80000000;constants[1023]=0x7fc12345;shared[7]=0xabcdef01;shared[39]=0x12345678;
    t.BeginFrame(1,10);auto m=t.Collect(k,constants.data(),shared.data(),true);
    Require(!m.previous,"first frame has no history");t.BeginFrame(1,10);Require(t.ActiveDrawCount()==1,"same frame is idempotent");
    Require(t.FinalizeFrame()[m.tag]==0,"first-frame tag invalid");
    t.BeginFrame(2,10);m=t.Collect(k,constants.data(),shared.data(),true);Require(m.previous,"previous full state exists");
    uint32_t bits;std::memcpy(&bits,&m.previous->vsConstants[1023],4);Require(bits==constants[1023],"exact bitwise full c255 snapshot including NaN payload");
    Require(m.previous->shared[7]==shared[7]&&m.previous->shared[39]==shared[39],"bool and loop banks retained");
    auto duplicate=t.Collect(k,constants.data(),shared.data(),true);Require(!duplicate.previous,"new second instance has no previous occurrence");
    auto tags=t.FinalizeFrame();Require(tags[m.tag]==1,"first ordered instance remains valid");
    Require(tags[duplicate.tag]==0,"extra ordered instance remains reactive");
    Require(t.Stats().matchedPreviousDraws==1,"statistics count finalized ordered matches");
    Require(t.FindPrevious(k),"first ordered instance remains available");
    t.BeginFrame(3,10);m=t.Collect(k,constants.data(),shared.data(),true);duplicate=t.Collect(k,constants.data(),shared.data(),true);
    Require(m.previous&&duplicate.previous,"stable ordered duplicates match prior occurrences");
    tags=t.FinalizeFrame();Require(tags[m.tag]&&tags[duplicate.tag],"both ordered duplicate tags finalize valid");
    t.BeginFrame(4,11);Require(!t.Collect(k,constants.data(),shared.data(),true).previous,"epoch reset invalidates all object history");t.FinalizeFrame();
    t.BeginFrame(6,11);Require(!t.Collect(k,constants.data(),shared.data(),true).previous,"frame gap invalidates history");
    t.BeginFrame(7,11);Require(!t.Collect(k,constants.data(),shared.data(),true).previous,"unfinished frame cannot supply history");t.FinalizeFrame();
    t.BeginFrame(8,11);m=t.Collect(k,constants.data(),shared.data(),true);Require(t.FinalizeFrame()[m.tag]==1,"consecutive unique draw accepted");
    Require(t.Stats().relativeConstantMatches==1,"relative-state diagnostic count after freeze");
    t.Collect(k,constants.data(),shared.data(),true);Require(t.Failed()&&t.FinalizeFrame()[m.tag]==0,"late collection invalidates entire frozen view");
    t.BeginFrame(9,11);Require(!t.Collect(k,constants.data(),shared.data(),true).previous,"failed frame not promoted");t.FinalizeFrame();
    t.BeginFrame(10,11);auto other=k;other.geometrySignature++;Require(!t.Collect(other,constants.data(),shared.data(),false).previous,"changed geometry generation rejects previous");t.FinalizeFrame();
    t.BeginFrame(11,11);Require(!t.Collect(other,nullptr,shared.data(),false).previous,"missing input never marked valid");t.FinalizeFrame();
    DrawTemporalTracker bounded(2);bounded.BeginFrame(1);bounded.Collect(k,constants.data(),shared.data(),false);other.vsHash++;bounded.Collect(other,constants.data(),shared.data(),false);other.vsHash++;
    bounded.Collect(other,constants.data(),shared.data(),false);Require(bounded.Failed()&&bounded.ActiveDrawCount()==2,"capacity overflow bounded, no growth/fake match");
    const auto& validity=bounded.FinalizeFrame();Require(std::all_of(validity.begin(),validity.end(),[](auto x){return x==0;}),"overflow invalidates all tags");
    DrawTemporalTracker restored;
    auto uploaded=constants;std::array<uint32_t,16> original{};
    for(unsigned i=0;i<16;++i){original[i]=0x3f800000u+i;uploaded[252*4+i]=0x40000000u+i;}
    restored.BeginFrame(1);restored.Collect(k,uploaded.data(),shared.data(),true,252,original.data());restored.FinalizeFrame();
    restored.BeginFrame(2);auto recovered=restored.Collect(k,uploaded.data(),shared.data(),true,252,original.data());
    Require(recovered.previous&&std::memcmp(recovered.previous->vsConstants.data()+252*4,original.data(),64)==0,"pre-jitter matrix restored bit-exact at last legal slot");
    Require(std::memcmp(recovered.previous->vsConstants.data(),uploaded.data(),252*16)==0,"full bank outside proven jitter window is preserved");
    Require(uploaded[252*4]==0x40000000u,"snapshot never mutates the uploaded guest constant bank");
    Require(restored.Stats().snapshotBytes==4096+208+64,"snapshot copy bytes count actual full-bank/prefix/restore writes");
    restored.FinalizeFrame();restored.BeginFrame(3);
    Require(!restored.Collect(k,uploaded.data(),shared.data(),true,253,original.data()).previous,"out-of-range matrix window fails closed");
    DrawTemporalTracker steady(128);
    for(uint64_t f=1;f<=3;++f){steady.BeginFrame(f);for(unsigned i=0;i<100;++i){k.vsHash=i;steady.Collect(k,constants.data(),shared.data(),false);}steady.FinalizeFrame();}
    const size_t before=allocations;
    for(uint64_t f=4;f<104;++f){steady.BeginFrame(f);for(unsigned i=0;i<100;++i){k.vsHash=i;steady.Collect(k,constants.data(),shared.data(),false);}steady.FinalizeFrame();}
    Require(allocations==before,"steady-state tracker performs zero heap allocations across 10,000 draws");
    const Viewport vp{0,0,1280,720};auto cam=Camera::Create(Projection(),vp);Require(bool(cam),"reference projection");auto movedMatrix=Projection();movedMatrix[12]=-1;auto previousCam=Camera::Create(movedMatrix,vp);
    auto mv=ReprojectMotionVector({640.5,360.5,1./11},*cam,*previousCam);Require(mv.valid&&std::abs(mv.x+64)<.001,"camera reference correct reverse-Z -64px");
    Require(!ReprojectMotionVector({640.5,360.5,0},*cam,*cam).valid,"invalid depth explicitly distinguishable from stationary zero");
    Require(ReprojectMotionVector({640.5,360.5,1},*cam,*cam).valid,"reverse-Z near endpoint 1 remains valid");
    Require(ReprojectMotionVector({640.5,360.5,.5},*cam,*cam).valid,"valid stationary vector remains valid");
    MotionVectorProducer p({1,1});auto one=Camera::Create(Projection(),{0,0,1,1});float depth=1;std::vector<MotionVectorPixel> grid;
    Require(p.EvaluateGrid(&depth,&*one,&*one,nullptr,grid)&&grid[0].reactiveMask==0,"CPU grid shares depth endpoint contract");
    Require(p.EvaluateGrid(&depth,&*one,nullptr,nullptr,grid)&&grid[0].reactiveMask==1,"no previous camera is invalid, not stationary");
    const auto opts=MotionOptions::Environment();
    const char* motionOverride=std::getenv("LO_MV_ENABLE");
    if(!motionOverride||std::string_view(motionOverride)=="1")
        Require(opts.enabled&&opts.replay&&opts.consume,"TAA motion defaults on");
    else if(std::string_view(motionOverride)=="0")
        Require(!opts.enabled&&!opts.replay&&!opts.consume,"TAA motion comparison switch disables replay");
    printf("PASS: %u motion lifecycle/reference checks; zero steady-state tracking allocations\n",checks);return 0;
} catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
