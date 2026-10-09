#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/geometry_box61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace geometry_box61_oracle {
using Registers=geometry_box61::Registers;
constexpr GuestAddress Query=0x30000u,Nodes=0x31000u,Tree=0x32000u,Leaves=0x33000u;
constexpr GuestAddress Mesh=0x34000u,Triangles=0x35000u,Vertices=0x36000u;
constexpr std::array<test::Region,5> Regions{{{0u,0x120000u},{0x82000000u,0x10000u},
    {0x82010000u,0x10000u},{0x82030000u,0x10000u},{0x820a0000u,0x10000u}}};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
struct Guest final:crt_close_recursive_buffer_context::GuestServices{void CallIndirect(GuestAddress,GuestMemory&,Registers&)override{throw std::runtime_error("unexpected box allocator");}};
GuestMemory* active_memory=nullptr;Guest* active_guest=nullptr;Native native;
void Check(unsigned mode) {
    struct Restore{std::uint32_t value=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(value);}}restore;
    const bool paired=mode<2u,hit=(mode%2u)==0u;const unsigned stride=paired?32u:36u;
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[&](test::GuestWindow& window){
        window.Fill(0xa5u);auto m=window.Memory();const auto f=[&](GuestAddress a,float v){m.WriteU32(a,std::bit_cast<std::uint32_t>(v));};
        m.WriteU32(Query+4u,mode==2u?1u:0u);m.WriteU32(Query+8u,Tree);m.WriteU32(Query+12u,Mesh);
        f(Query+16u,.25f);f(Query+20u,.25f);f(Query+24u,1.0f);f(Query+28u,0);f(Query+32u,0);f(Query+36u,-1.0f);
        f(Query+40u,0);f(Query+44u,0);f(Query+48u,.5f);f(Query+52u,0);f(Query+56u,0);f(Query+60u,-.5f);
        f(Query+64u,.25f);f(Query+68u,.25f);f(Query+72u,.5f);
        m.WriteU32(Query+92u,0u);m.WriteU32(Query+96u,0u);m.WriteU32(Query+100u,0u);m.WriteU32(Query+104u,0u);
        f(Query+132u,4.0f);f(Query+136u,.0001f);m.WriteU8(Query+140u,0u);m.WriteU8(Query+141u,1u);
        f(Nodes,hit?.25f:20.0f);f(Nodes+4u,.25f);f(Nodes+8u,.5f);f(Nodes+12u,.5f);f(Nodes+16u,.5f);f(Nodes+20u,1.0f);
        m.WriteU32(Nodes+24u,hit?(paired?0xc0000000u:0x80000000u):0u);
        m.WriteU32(Nodes+(paired?28u:32u),hit?0u:2u);
        m.WriteU32(Tree+24u,Leaves);m.WriteU32(Tree+32u,0u);m.WriteU32(Leaves,0u);m.WriteU32(Leaves+4u,0u);
        m.WriteU32(Mesh+16u,Triangles);m.WriteU32(Mesh+20u,Vertices);
        for(unsigned i=0;i<3u;++i)m.WriteU32(Triangles+4u*i,i);
        constexpr std::array<float,9> xyz{{0,0,0,1,0,0,0,1,0}};for(unsigned i=0;i<xyz.size();++i)f(Vertices+i*4u,xyz[i]);
        f(0x8201f9f0u,.5f);f(0x82035cb8u,-.000001f);f(0x82007784u,1.0f);f(0x820a6b8cu,.000001f);
    };
    seed(original);seed(recovered);auto om=original.Memory(),rm=recovered.Memory();Registers initial{};
    for(unsigned i=0;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=Query;initial.r[4]=Nodes;
    initial.r[5]=Nodes+stride*(hit?(mode==2u?2u:1u):3u);initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    PPCContext context{};crt_full_oracle::ToPpc(context,initial);auto state=initial;Guest guest;
    active_memory=&om;active_guest=&guest;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(paired)__imp__sub_82BD5550(context,original.Bytes());else __imp__sub_82BD5B40(context,original.Bytes());
    const auto host=PPCFPSCRRegister{}.getcsr();active_memory=nullptr;active_guest=nullptr;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!geometry_box61::Apply(paired?0x82bd5550u:0x82bd5b40u,rm,{guest,native},state))throw std::runtime_error("missing geometry box entry");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state)||!original.EqualCommitted(recovered)||host!=PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("geometry box Full72/RAM/FP mismatch");
    if(state.r[1]!=initial.r[1]||state.lr!=0x81234567u||rm.ReadU32(Query+96u)!=1u||rm.ReadU32(Query+104u)!=(hit?1u:0u)||rm.ReadU32(Query+100u)!=(hit?(paired?2u:1u):0u))
        throw std::runtime_error("geometry box prune/pair/stop outcome mismatch");
    for(unsigned i=paired?29u:27u;i<32u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("geometry box saved register mismatch");
}
}
void OriginalGeometryBox61Lower(PPCContext& c,std::uint8_t*){
    using namespace geometry_box61_oracle;auto s=crt_full_oracle::FromPpc(c);
    (void)geometry_triangle_range61::Apply(0x82bd4c40u,*active_memory,{*active_guest,native},s);crt_full_oracle::ToPpc(c,s);
}
void OriginalGeometryBox61Save(unsigned first,PPCContext& c){
    const auto r=crt_full_oracle::Gprs(c);auto& m=*geometry_box61_oracle::active_memory;
    for(unsigned i=first;i<32u;++i)recovery_abi::WriteU64(m,recovery_abi::Address(c.r1.u64-8u*(33u-i)),r[i]->u64);
    m.WriteU32(recovery_abi::Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalGeometryBox61Restore(unsigned first,PPCContext& c){
    const auto r=crt_full_oracle::Gprs(c);auto& m=*geometry_box61_oracle::active_memory;
    for(unsigned i=first;i<32u;++i)r[i]->u64=recovery_abi::ReadU64(m,recovery_abi::Address(c.r1.u64-8u*(33u-i)));
    c.r12.u64=m.ReadU32(recovery_abi::Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
int main(){try{for(unsigned mode=0;mode<4u;++mode)geometry_box61_oracle::Check(mode);std::puts("PASS geometry-box61 4 focused original-upper cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
