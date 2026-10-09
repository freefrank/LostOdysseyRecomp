#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/geometry_math61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace geometry_math61_oracle {
using Registers=geometry_math61::Registers;
constexpr GuestAddress Object=0x30000u,Hierarchy=0x31000u,Mesh=0x32000u,Leaves=0x33000u;
constexpr GuestAddress TriangleList=0x33100u,Triangles=0x34000u,Vertices=0x35000u;
constexpr GuestAddress Output=0x36000u,Records=0x37000u,Vtable=0x38000u,Allocate=0x2a00u;
constexpr std::array<test::Region,5> Regions{{{0u,0x120000u},{0x82000000u,0x8000u},
    {0x82035000u,0x1000u},{0x820a6000u,0x1000u},{0x83216000u,0x1000u}}};
// Reader growth's default-table global lives separately from the FP constants.
constexpr std::array<test::Region,6> AllRegions{{Regions[0],Regions[1],Regions[2],Regions[3],Regions[4],{0x832df000u,0x1000u}}};
struct RestoreHost {std::uint32_t control=PPCFPSCRRegister{}.getcsr();~RestoreHost(){PPCFPSCRRegister{}.setcsr(control);}};
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t,72>> events;
    void CallIndirect(GuestAddress target,GuestMemory&,Registers& s) override {
        if(target!=Allocate||s.r[4]!=16u||s.r[5]!=64u)throw std::runtime_error("unexpected geometry growth request");
        events.push_back(crt_full_oracle::Snapshot(s));s.r[3]=Records;
        s.r[8]^=0xabcdu;s.fpr_bits[7]^=0x100u;s.cr1.gt^=1u;
    }
};
struct Fp final:float_triplet_transfer::NativeServices {void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
GuestMemory* active_memory=nullptr;Guest* active_guest=nullptr;Fp* active_fp=nullptr;
void Check(unsigned mode) {
    RestoreHost restore;const bool indexed=mode==0u||mode==2u;
    test::GuestWindow original(AllRegions),recovered(AllRegions);
    const auto seed=[&](test::GuestWindow& window) {
        window.Fill(0xa5u);auto m=window.Memory();
        const auto real=[&](GuestAddress a,float v){m.WriteU32(a,std::bit_cast<std::uint32_t>(v));};
        m.WriteU32(Object+4u,0u);m.WriteU32(Object+8u,Hierarchy);m.WriteU32(Object+12u,Mesh);
        real(Object+16u,mode==3u?2.0f:0.25f);real(Object+20u,mode==3u?2.0f:0.25f);real(Object+24u,1.0f);
        real(Object+28u,0.0f);real(Object+32u,0.0f);real(Object+36u,-1.0f);
        m.WriteU32(Object+92u,(mode==1u||mode==2u)?Output:0u);
        m.WriteU32(Object+100u,0u);m.WriteU32(Object+104u,0u);
        real(Object+132u,10.0f);real(Object+136u,0.001f);
        m.WriteU8(Object+140u,mode==2u?1u:0u);m.WriteU8(Object+141u,(mode==0u||mode==3u)?1u:0u);
        m.WriteU32(Hierarchy+24u,Leaves);m.WriteU32(Hierarchy+32u,indexed?TriangleList:0u);m.WriteU32(Leaves,0u);m.WriteU32(TriangleList,0u);
        m.WriteU32(Mesh+16u,Triangles);m.WriteU32(Mesh+20u,Vertices);
        m.WriteU32(Triangles,0u);m.WriteU32(Triangles+4u,1u);m.WriteU32(Triangles+8u,2u);
        constexpr std::array<float,9> points{{0,0,0,1,0,0,0,1,0}};
        for(unsigned i=0;i<points.size();++i)real(Vertices+4u*i,points[i]);
        m.WriteU32(Output,mode==1u?0u:4u);m.WriteU32(Output+4u,mode==2u?4u:0u);
        m.WriteU32(Output+8u,mode==1u?0u:Records);real(Output+12u,2.0f);real(Records+4u,2.0f);
        real(0x82035cb8u,-0.000001f);real(0x82007784u,1.0f);real(0x820a6b8cu,0.000001f);real(0x82000e50u,1.0f);
        m.WriteU32(0x832df554u,0u);m.WriteU32(0x83216624u,Vtable);m.WriteU32(Vtable,Allocate|1u);
    };
    seed(original);seed(recovered);auto om=original.Memory(),rm=recovered.Memory();Registers initial{};
    for(unsigned i=0;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=Object;initial.r[4]=0u;initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    PPCContext context{};crt_full_oracle::ToPpc(context,initial);auto state=initial;Guest expected,actual;Fp efp,afp;
    active_memory=&om;active_guest=&expected;active_fp=&efp;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BD4448(context,original.Bytes());const auto host=PPCFPSCRRegister{}.getcsr();active_memory=nullptr;active_guest=nullptr;active_fp=nullptr;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!geometry_math61::Apply(0x82bd4448u,rm,{actual,afp},state))throw std::runtime_error("missing geometry intersection entry");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state)||!original.EqualCommitted(recovered)||expected.events!=actual.events||host!=PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("geometry intersection Full72/RAM/callback/FP mismatch");
    if(state.r[1]!=initial.r[1]||state.lr!=0x81234567u||rm.ReadU32(Object+100u)!=1u||rm.ReadU32(Object+104u)!=(mode==3u?0u:1u))
        throw std::runtime_error("geometry visit/hit/frame outcome mismatch");
    for(unsigned i=25u;i<=31u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("geometry GPR save mismatch");
    for(unsigned i=27u;i<=31u;++i)if(state.fpr_bits[i]!=initial.fpr_bits[i])throw std::runtime_error("geometry FPR save mismatch");
    if(mode!=3u && rm.ReadU32(Object+80u)!=std::bit_cast<std::uint32_t>(1.0f))throw std::runtime_error("geometry hit distance absent");
    if((mode==1u||mode==2u)&&(rm.ReadU32(Records)!=0u||rm.ReadU32(Records+4u)!=std::bit_cast<std::uint32_t>(1.0f)||rm.ReadU32(Output+4u)!=4u))
        throw std::runtime_error("geometry append/nearest record absent");
    if(actual.events.size()!=(mode==1u?1u:0u))throw std::runtime_error("geometry growth branch absent");
}
}
void OriginalGeometryMath61Lower(std::uint32_t entry,PPCContext& c,std::uint8_t*) {
    using namespace geometry_math61_oracle;auto s=crt_full_oracle::FromPpc(c);
    if(entry==0x82bd2870u)(void)reader_buffer_growth61::Apply(entry,*active_memory,{*active_guest,*active_fp},s);
    else if(entry==0x82b7a0b0u)(void)crt_copy_full_context::Apply(entry,*active_memory,s);
    else throw std::runtime_error("unexpected geometry lower");
    crt_full_oracle::ToPpc(c,s);
}
void OriginalGeometryMath61SaveGpr(PPCContext& c) {
    const auto regs=crt_full_oracle::Gprs(c);auto& m=*geometry_math61_oracle::active_memory;
    for(unsigned i=25;i<=31;++i)recovery_abi::WriteU64(m,recovery_abi::Address(c.r1.u64-8u*(33u-i)),regs[i]->u64);
    m.WriteU32(recovery_abi::Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalGeometryMath61RestoreGpr(PPCContext& c) {
    const auto regs=crt_full_oracle::Gprs(c);auto& m=*geometry_math61_oracle::active_memory;
    for(unsigned i=25;i<=31;++i)regs[i]->u64=recovery_abi::ReadU64(m,recovery_abi::Address(c.r1.u64-8u*(33u-i)));
    c.r12.u64=m.ReadU32(recovery_abi::Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
void OriginalGeometryMath61SaveFpr(PPCContext& c) {
    const auto regs=crt_full_oracle::Fprs(c);auto& m=*geometry_math61_oracle::active_memory;c.fpscr.disableFlushMode();
    for(unsigned i=27;i<=31;++i)recovery_abi::WriteU64(m,recovery_abi::Address(c.r12.u64-8u*(32u-i)),regs[i]->u64);
}
void OriginalGeometryMath61RestoreFpr(PPCContext& c) {
    const auto regs=crt_full_oracle::Fprs(c);auto& m=*geometry_math61_oracle::active_memory;c.fpscr.disableFlushMode();
    for(unsigned i=27;i<=31;++i)regs[i]->u64=recovery_abi::ReadU64(m,recovery_abi::Address(c.r12.u64-8u*(32u-i)));
}
int main(){try{for(unsigned mode=0;mode<4u;++mode)geometry_math61_oracle::Check(mode);std::puts("PASS geometry-math61 4 focused PPC upper cases with accepted lowers");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
