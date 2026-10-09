// Original BB03B0 upper; RNG, constructors and dispatch are complete shared lowers.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/object_grid_probe61.h"
#include <algorithm>
#include <bit>
namespace grid_probe61_oracle {
using Registers=object_grid_probe61::Registers;using VectorState=object_grid_probe61::VectorState;
constexpr GuestAddress Object=0x30000u,Point=0x31000u,Tree=0x32000u,Mesh=0x33000u,Indices=0x34000u,Vertices=0x35000u;
constexpr GuestAddress Tls=0x40000u,ThreadRecord=0x41000u,RngRecord=0x42000u,Getter=0x2a00u;
constexpr std::array<test::Region,7> Regions{{{0u,0x120000u},{0x82000000u,0x10000u},{0x82010000u,0x10000u},
    {0x82030000u,0x10000u},{0x820a0000u,0x10000u},{0x83214000u,0x3000u},{0x832d3000u,0x1000u}}};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
struct QueryGuest final:geometry_unbounded_range61::GuestServices {
    void CallIndirect(GuestAddress,GuestMemory&,Registers&,VectorState&)override{throw std::runtime_error("unexpected probe query allocation");}
};
struct Unused final:heap_allocation_context::BoundaryServices,crt_record_allocation_context::HandlerServices,crt_free_context::LowerCalls {
    void CallDirect(GuestAddress,GuestMemory&,raw_allocation_context::Registers&)override{throw std::runtime_error("unexpected probe allocation");}
    void CallNative(GuestAddress,GuestMemory&,raw_allocation_context::Registers&)override{throw std::runtime_error("unexpected probe allocation native");}
    void CallNewHandler(GuestAddress,GuestMemory&,Registers&)override{throw std::runtime_error("unexpected probe new handler");}
    void Call(GuestAddress,GuestMemory&,crt_free_context::Registers&)override{throw std::runtime_error("unexpected probe release");}
};
struct Thread final:crt_random_thread61::Services {
    std::vector<std::array<std::uint64_t,73>> events;
    void Observe(GuestAddress target,Registers& s){std::array<std::uint64_t,73> e{};auto snap=crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(),snap.end(),e.begin());e.back()=target;events.push_back(e);
        s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[7]^=0x180u;s.cr7.lt^=1u;s.cached_fp_control=0x9fc0u;PPCFPSCRRegister{}.setcsr(s.cached_fp_control);}
    void KeTlsGetValue(GuestMemory&,Registers& s)override{Observe(0x830da144u,s);s.r[3]=Getter|1u;}
    void KeTlsSetValue(GuestMemory&,Registers& s)override{Observe(0x830da154u,s);s.r[3]=1u;}
    void CallIndirect(GuestAddress target,GuestMemory&,Registers& s)override{Observe(target,s);if(target!=Getter)throw std::runtime_error("unexpected probe TLS target");s.r[3]=RngRecord;}
    void FatalRuntimeError(GuestMemory&,Registers&)override{throw std::runtime_error("unexpected probe fatal path");}
};
struct EnvironmentCase {
    Services stream;QueryGuest query;Native fp;Unused unused;Thread thread;
    explicit EnvironmentCase(test::GuestWindow& w):stream(w,Mode::LockedWrite){}
    object_grid_probe61::Dependencies Deps(){return{{query,fp},{{Dependencies(stream),unused,unused},unused,thread}};}
};
EnvironmentCase* original=nullptr;
std::array<std::uint32_t*,128> VectorPointers(PPCContext& c){return {{
    &c.v0.u32[0],&c.v1.u32[0],&c.v2.u32[0],&c.v3.u32[0],&c.v4.u32[0],&c.v5.u32[0],&c.v6.u32[0],&c.v7.u32[0],
    &c.v8.u32[0],&c.v9.u32[0],&c.v10.u32[0],&c.v11.u32[0],&c.v12.u32[0],&c.v13.u32[0],&c.v14.u32[0],&c.v15.u32[0],
    &c.v16.u32[0],&c.v17.u32[0],&c.v18.u32[0],&c.v19.u32[0],&c.v20.u32[0],&c.v21.u32[0],&c.v22.u32[0],&c.v23.u32[0],
    &c.v24.u32[0],&c.v25.u32[0],&c.v26.u32[0],&c.v27.u32[0],&c.v28.u32[0],&c.v29.u32[0],&c.v30.u32[0],&c.v31.u32[0],
    &c.v32.u32[0],&c.v33.u32[0],&c.v34.u32[0],&c.v35.u32[0],&c.v36.u32[0],&c.v37.u32[0],&c.v38.u32[0],&c.v39.u32[0],
    &c.v40.u32[0],&c.v41.u32[0],&c.v42.u32[0],&c.v43.u32[0],&c.v44.u32[0],&c.v45.u32[0],&c.v46.u32[0],&c.v47.u32[0],
    &c.v48.u32[0],&c.v49.u32[0],&c.v50.u32[0],&c.v51.u32[0],&c.v52.u32[0],&c.v53.u32[0],&c.v54.u32[0],&c.v55.u32[0],
    &c.v56.u32[0],&c.v57.u32[0],&c.v58.u32[0],&c.v59.u32[0],&c.v60.u32[0],&c.v61.u32[0],&c.v62.u32[0],&c.v63.u32[0],
    &c.v64.u32[0],&c.v65.u32[0],&c.v66.u32[0],&c.v67.u32[0],&c.v68.u32[0],&c.v69.u32[0],&c.v70.u32[0],&c.v71.u32[0],
    &c.v72.u32[0],&c.v73.u32[0],&c.v74.u32[0],&c.v75.u32[0],&c.v76.u32[0],&c.v77.u32[0],&c.v78.u32[0],&c.v79.u32[0],
    &c.v80.u32[0],&c.v81.u32[0],&c.v82.u32[0],&c.v83.u32[0],&c.v84.u32[0],&c.v85.u32[0],&c.v86.u32[0],&c.v87.u32[0],
    &c.v88.u32[0],&c.v89.u32[0],&c.v90.u32[0],&c.v91.u32[0],&c.v92.u32[0],&c.v93.u32[0],&c.v94.u32[0],&c.v95.u32[0],
    &c.v96.u32[0],&c.v97.u32[0],&c.v98.u32[0],&c.v99.u32[0],&c.v100.u32[0],&c.v101.u32[0],&c.v102.u32[0],&c.v103.u32[0],
    &c.v104.u32[0],&c.v105.u32[0],&c.v106.u32[0],&c.v107.u32[0],&c.v108.u32[0],&c.v109.u32[0],&c.v110.u32[0],&c.v111.u32[0],
    &c.v112.u32[0],&c.v113.u32[0],&c.v114.u32[0],&c.v115.u32[0],&c.v116.u32[0],&c.v117.u32[0],&c.v118.u32[0],&c.v119.u32[0],
    &c.v120.u32[0],&c.v121.u32[0],&c.v122.u32[0],&c.v123.u32[0],&c.v124.u32[0],&c.v125.u32[0],&c.v126.u32[0],&c.v127.u32[0]
}};}
VectorState FromVectors(PPCContext& c){VectorState result;const auto p=VectorPointers(c);for(unsigned n=0;n<128u;++n)for(unsigned i=0;i<4u;++i)result.v[n][i]=p[n][i];return result;}
void ToVectors(PPCContext& c,const VectorState& v){const auto p=VectorPointers(c);for(unsigned n=0;n<128u;++n)for(unsigned i=0;i<4u;++i)p[n][i]=v.v[n][i];}
void Float(GuestMemory& m,GuestAddress p,float v){m.WriteU32(p,std::bit_cast<std::uint32_t>(v));}
void SeedProbe(test::GuestWindow& w,unsigned mode){
    w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Object+48u,Tree);m.WriteU32(Tree+4u,Mesh);m.WriteU32(Tree+8u,4u);
    m.WriteU32(Mesh+8u,mode==2u?1u:0u);m.WriteU32(Mesh+16u,Indices);m.WriteU32(Mesh+20u,Vertices);
    for(unsigned i=0;i<3u;++i)m.WriteU32(Indices+4u*i,i);
    constexpr std::array<float,9> vertex{0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f};
    for(unsigned i=0;i<vertex.size();++i)Float(m,Vertices+i*4u,vertex[i]);Float(m,Point,.25f);Float(m,Point+4u,.25f);Float(m,Point+8u,-1.f);
    Float(m,0x82000b40u,1.f/32767.f);Float(m,0x82000e0cu,std::bit_cast<float>(0x7f7fffffu));Float(m,0x82000e50u,0.f);
    Float(m,0x82007784u,1.f);Float(m,0x8201f9f0u,.5f);Float(m,0x82035cb8u,-.000001f);Float(m,0x820a6b8cu,.000001f);
    m.WriteU32(Tls+336u,0u);m.WriteU32(Tls+256u,ThreadRecord);m.WriteU32(ThreadRecord+352u,0x12345678u);
    m.WriteU32(0x83214d74u,7u);m.WriteU32(0x83214d78u,9u);m.WriteU32(0x832d3adcu,Getter|1u);m.WriteU32(RngRecord+20u,1u);
}
void Check(unsigned mode){
    struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    test::GuestWindow before(Regions),after(Regions);SeedProbe(before,mode);SeedProbe(after,mode);EnvironmentCase expected(before),actual(after);
    Registers s{};VectorState vectors;for(unsigned i=0;i<32u;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
    for(unsigned i=0;i<128u;++i)for(unsigned j=0;j<4u;++j)vectors.v[i][j]=0x3f800000u+i*16u+j;
    s.r[1]=0x8877665500080000ull;s.r[3]=Object;s.r[4]=Point;s.r[5]=mode;s.r[13]=Tls;s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0u;s.xer_so=1u;
    const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);ToVectors(c,vectors);PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    original=&expected;__imp__sub_82BB03B0(c,before.Bytes());const auto expected_host=PPCFPSCRRegister{}.getcsr();original=nullptr;
    auto m=after.Memory();PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!object_grid_probe61::Apply(0x82bb03b0u,m,actual.Deps(),s,vectors)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||
        FromVectors(c).v!=vectors.v||!before.EqualCommitted(after)||expected.thread.events!=actual.thread.events||expected_host!=PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("grid probe Full72/vector/RAM/TLS/host mismatch");
    if(s.r[3]!=(mode==2u?1u:0u)||s.r[1]!=initial.r[1]||s.lr!=Address(initial.lr))throw std::runtime_error("grid probe majority/restore mismatch");
    std::uint32_t seed=1u;const unsigned samples=mode==2u?0u:mode==3u?3u:9u;
    for(unsigned i=0;i<samples;++i)seed=seed*214013u+2531011u;
    if(m.ReadU32(RngRecord+20u)!=seed||actual.thread.events.size()!=samples*2u)throw std::runtime_error("grid probe RNG sequence/count mismatch");
}
}
void OriginalGridProbe61Save(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=grid_probe61_oracle::original->stream.memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalGridProbe61Restore(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=grid_probe61_oracle::original->stream.memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
void OriginalGridProbe61SaveFpr(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=grid_probe61_oracle::original->stream.memory;const auto f=crt_full_oracle::Fprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r12.u64-8u*(32u-i)),f[i]->u64);
}
void OriginalGridProbe61RestoreFpr(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=grid_probe61_oracle::original->stream.memory;const auto f=crt_full_oracle::Fprs(c);
    for(unsigned i=first;i<=31u;++i)f[i]->u64=ReadU64(m,Address(c.r12.u64-8u*(32u-i)));
}
void OriginalGridProbe61Lower(GuestAddress entry,PPCContext& c,std::uint8_t*){
    auto& e=*grid_probe61_oracle::original;auto s=crt_full_oracle::FromPpc(c);auto v=grid_probe61_oracle::FromVectors(c);auto& m=e.stream.memory;bool ok=false;
    if(entry==0x82bd2c48u)ok=crt_random_thread61::Apply(entry,m,e.Deps().random,s);
    else if(entry==0x82bd43f8u||entry==0x82bd4438u)ok=geometry_support61::Apply(entry,m,e.fp,s);
    else if(entry==0x82bd7258u)ok=geometry_query_dispatch61::Apply(entry,m,e.Deps().query,s,v);
    if(!ok)throw std::runtime_error("unexpected probe lower");crt_full_oracle::ToPpc(c,s);grid_probe61_oracle::ToVectors(c,v);
}
int main(){try{for(unsigned mode:std::array<unsigned,3>{2u,3u,7u})grid_probe61_oracle::Check(mode);std::puts("PASS object-grid-probe61 3 original-upper/shared-lower cases");return 0;}
catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
