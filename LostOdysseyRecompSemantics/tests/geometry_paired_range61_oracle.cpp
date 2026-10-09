// Genuine BD5910 body and its VMX119 helpers; BD4448 and its scalar lowers are shared accepted semantics.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/geometry_paired_range61.h"
#include <bit>
namespace paired_range61_oracle {
using Registers=geometry_paired_range61::Registers;
using VectorState=geometry_paired_range61::VectorState;
constexpr GuestAddress Query=0x30000u,Tree=0x31000u,Leaves=0x32000u,Mesh=0x34000u,
    Triangles=0x35000u,Vertices=0x36000u,Buffer=0x37000u,Old=0x38000u,Fresh=0x39000u,
    Vtable=0x3a000u,Nodes=0x40004u,Allocate=0x2a00u,Dispose=0x2b00u;
constexpr std::array<test::Region,7> Regions{{{0u,0x120000u},{0x82000000u,0x10000u},
    {0x82010000u,0x10000u},{0x82030000u,0x10000u},{0x820a0000u,0x10000u},
    {0x83216000u,0x1000u},{0x832df000u,0x1000u}}};
struct Native final:float_triplet_transfer::NativeServices{
    void SetHostFpControl(std::uint32_t value)override{PPCFPSCRRegister{}.setcsr(value);}
};
struct Guest final:geometry_unbounded_range61::GuestServices{
    std::vector<std::array<std::uint64_t,72>> scalar_events;
    std::vector<decltype(VectorState::v)> vector_events;
    void CallIndirect(GuestAddress target,GuestMemory&,Registers& s,VectorState& v)override{
        scalar_events.push_back(crt_full_oracle::Snapshot(s));vector_events.push_back(v.v);
        if(target==Allocate)s.r[3]=Fresh;else if(target!=Dispose)throw std::runtime_error("unexpected VMX callback");
        s.r[8]^=0x1234567890abcdefull;v.v[64][2]^=0x01234567u;v.v[3][1]^=0x40u;v.v[125][0]^=0x100u;
        s.cached_fp_control=0x9fc0u;PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    }
};
GuestMemory* active_memory=nullptr;Guest* active_guest=nullptr;Native native;
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
void Float(GuestMemory& m,GuestAddress a,float value){m.WriteU32(a,std::bit_cast<std::uint32_t>(value));}
void Seed(test::GuestWindow& window,unsigned mode){
    window.Fill(0xa5u);auto m=window.Memory();
    m.WriteU32(Query+4u,0u);m.WriteU32(Query+8u,Tree);m.WriteU32(Query+12u,Mesh);
    m.WriteU32(Tree+24u,Leaves);m.WriteU32(Tree+32u,0u);m.WriteU32(Leaves,0u);
    m.WriteU32(Mesh+16u,Triangles);m.WriteU32(Mesh+20u,Vertices);
    for(unsigned i=0;i<3u;++i)m.WriteU32(Triangles+4u*i,i);
    constexpr std::array<float,9> vertices{0,0,0,1,0,0,0,1,0};
    for(unsigned i=0;i<vertices.size();++i)Float(m,Vertices+4u*i,vertices[i]);
    constexpr std::array<float,9> ray{.25f,.25f,1,0,0,-1,0,0,1};
    for(unsigned i=0;i<ray.size();++i)Float(m,Query+16u+4u*i,ray[i]);
    m.WriteU32(Query+92u,mode==1u?Buffer:0u);m.WriteU32(Query+96u,0u);m.WriteU32(Query+100u,0u);m.WriteU32(Query+104u,0u);
    Float(m,Query+132u,4);Float(m,Query+136u,.0001f);m.WriteU8(Query+140u,0);m.WriteU8(Query+141u,1);
    m.WriteU32(Leaves+4u,0u);
    for(unsigned i=0;i<3u;++i){Float(m,Query+108u+4u*i,1);Float(m,Query+120u+4u*i,1);}
    for(unsigned n=0;n<3u;++n){const auto node=Nodes+n*20u;
        m.WriteU16(node,mode==1u?100u:0u);m.WriteU16(node+2u,0);m.WriteU16(node+4u,0);
        m.WriteU16(node+6u,2);m.WriteU16(node+8u,2);m.WriteU16(node+10u,2);
        m.WriteU32(node+12u,0xc0000000u);m.WriteU32(node+16u,0u);
    }
    m.WriteU32(Buffer,0u);m.WriteU32(Buffer+4u,0u);m.WriteU32(Buffer+8u,Old);Float(m,Buffer+12u,2);
    Float(m,0x8201f9f0u,.5f);Float(m,0x82035cb8u,-.000001f);Float(m,0x82007784u,1);Float(m,0x820a6b8cu,.000001f);
    Float(m,0x82000e50u,1);m.WriteU32(0x832df554u,0);m.WriteU32(0x83216624u,Vtable);
    m.WriteU32(Vtable,Allocate|3u);m.WriteU32(Vtable+12u,Dispose|3u);
}
void Check(unsigned mode){
    struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    test::GuestWindow expected_window(Regions),actual_window(Regions);Seed(expected_window,mode);Seed(actual_window,mode);
    auto expected_memory=expected_window.Memory(),memory=actual_window.Memory();
    Registers initial{};VectorState initial_vectors;
    for(unsigned n=0;n<32u;++n){initial.r[n]=0x1122334400000000ull+n;initial.fpr_bits[n]=0x3ff0000000000000ull+n;}
    for(unsigned n=0;n<128u;++n)for(unsigned i=0;i<4u;++i)initial_vectors.v[n][i]=0x3f800000u+n*16u+i;
    initial.r[1]=0x8877665500080000ull;initial.r[3]=0xaabbccdd00000000ull|Query;
    initial.r[4]=0x778899aa00000000ull|Nodes;initial.r[5]=Nodes+(mode==0u?0u:60u);
    initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x1f80u;initial.xer_so=1u;
    PPCContext c{};crt_full_oracle::ToPpc(c,initial);ToVectors(c,initial_vectors);Guest expected,actual;
    active_memory=&expected_memory;active_guest=&expected;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BD5910(c,expected_window.Bytes());const auto expected_csr=PPCFPSCRRegister{}.getcsr();
    active_memory=nullptr;active_guest=nullptr;auto state=initial;auto vectors=initial_vectors;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!geometry_paired_range61::Apply(0x82bd5910u,memory,{actual,native},state,vectors))throw std::runtime_error("missing paired entry");
    const auto want=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),got=crt_full_oracle::Snapshot(state);
    for(unsigned n=0;n<want.size();++n)if(want[n]!=got[n]){std::fprintf(stderr,"paired mode %u Full72 %u expected %016llx actual %016llx\n",mode,n,(unsigned long long)want[n],(unsigned long long)got[n]);throw std::runtime_error("paired scalar mismatch");}
    const auto want_vectors=FromVectors(c);
    for(unsigned n=0;n<128u;++n)for(unsigned i=0;i<4u;++i)if(want_vectors.v[n][i]!=vectors.v[n][i]){
        std::fprintf(stderr,"paired mode %u v%u lane%u expected %08x actual %08x\n",mode,n,i,want_vectors.v[n][i],vectors.v[n][i]);throw std::runtime_error("paired vector mismatch");}
    if(!expected_window.EqualCommitted(actual_window))throw std::runtime_error("paired RAM mismatch");
    if(expected.scalar_events!=actual.scalar_events||expected.vector_events!=actual.vector_events||expected_csr!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("paired callbacks/CSR mismatch");
    if(memory.ReadU32(Query+96u)!=(mode==0u?0u:3u))throw std::runtime_error("paired traversal count");
    for(unsigned n=119u;n<128u;++n)if(vectors.v[n]!=initial_vectors.v[n])throw std::runtime_error("paired nonvolatile vector restoration");

}
}
void OriginalPairedRange61Lower(PPCContext& c,std::uint8_t*){
    using namespace paired_range61_oracle;auto s=crt_full_oracle::FromPpc(c);auto v=FromVectors(c);
    struct Bridge final:crt_close_recursive_buffer_context::GuestServices{
        VectorState& vectors;explicit Bridge(VectorState& state):vectors(state){}
        void CallIndirect(GuestAddress target,GuestMemory& m,Registers& r)override{active_guest->CallIndirect(target,m,r,vectors);}
    } bridge(v);
    (void)geometry_math61::Apply(0x82bd4448u,*active_memory,{bridge,native},s);crt_full_oracle::ToPpc(c,s);ToVectors(c,v);
}
void OriginalPairedRange61Save(PPCContext& c,std::uint8_t*){
    using namespace paired_range61_oracle;const auto s=crt_full_oracle::FromPpc(c);auto& m=*active_memory;
    for(unsigned n=26u;n<32u;++n)recovery_abi::WriteU64(m,Address(s.r[1]-16u-8u*(31u-n)),s.r[n]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
}
void OriginalPairedRange61Restore(PPCContext& c,std::uint8_t*){
    using namespace paired_range61_oracle;auto s=crt_full_oracle::FromPpc(c);auto& m=*active_memory;
    for(unsigned n=26u;n<32u;++n)s.r[n]=recovery_abi::ReadU64(m,Address(s.r[1]-16u-8u*(31u-n)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);
}
int main(){try{for(unsigned mode=0;mode<3u;++mode)paired_range61_oracle::Check(mode);std::puts("PASS geometry-paired-range61 3 focused Full72+VMX original-upper cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
