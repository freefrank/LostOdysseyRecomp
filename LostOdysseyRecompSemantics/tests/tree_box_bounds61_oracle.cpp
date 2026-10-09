#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_box_bounds61.h"
#include <bit>
namespace box_bounds_oracle {
using Registers = tree_box_bounds61::Registers;
constexpr GuestAddress Builder=0x30000u, Boxes=0x31000u, Selection=0x32000u, Output=0x33000u;
constexpr std::array<test::Region,1> Regions{{{0u,0x120000u}}};
GuestMemory* originalMemory=nullptr;
struct Native final:float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t value) override {PPCFPSCRRegister{}.setcsr(value);}
};
void Check(unsigned mode) {
    struct Restore {std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    test::GuestWindow before(Regions),after(Regions);
    const auto seed=[](test::GuestWindow& window){
        window.Fill(0xa5u);auto m=window.Memory();m.WriteU32(Builder+72u,Boxes);
        constexpr std::array<float,12> boxes{-3.f,2.f,-6.f,4.f,5.f,2.f,-8.f,-4.f,1.f,2.f,9.f,12.f};
        for(unsigned i=0;i<boxes.size();++i)m.WriteU32(Boxes+4u*i,std::bit_cast<std::uint32_t>(boxes[i]));
        m.WriteU32(Selection,0u);m.WriteU32(Selection+4u,1u);
    };
    seed(before);seed(after);Registers state{};
    for(unsigned i=0;i<32;++i){state.r[i]=0x1122334400000000ull+i;state.fpr_bits[i]=0x3ff0000000000000ull+i;}
    state.r[1]=0x8877665500080000ull;state.lr=0x9988776681234567ull;state.xer_so=1;state.cached_fp_control=0x9fc0u;
    state.r[3]=mode==3?Boxes:Builder;state.r[4]=mode==3?Boxes+24u:Selection;
    state.r[5]=mode==0?0u:mode==1?1u:2u;state.r[6]=Output;
    const auto initial=state;PPCContext context{};crt_full_oracle::ToPpc(context,state);
    auto om=before.Memory();originalMemory=&om;PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if(mode==3)__imp__sub_82BDDE70(context,before.Bytes());else __imp__sub_82BD8EE0(context,before.Bytes());
    const auto expectedHost=PPCFPSCRRegister{}.getcsr();originalMemory=nullptr;
    auto m=after.Memory();Native native;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!tree_box_bounds61::Apply(mode==3?0x82bdde70u:0x82bd8ee0u,m,native,state)||
       crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state)||
       !before.EqualCommitted(after)||expectedHost!=PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("box bounds Full72/RAM/host mismatch");
    if(mode==0){if(state.r[3]!=0||m.ReadU32(Output)!=0xa5a5a5a5u)throw std::runtime_error("empty bounds wrote output");}
    else {
        constexpr std::array<float,6> first{-3.f,2.f,-6.f,4.f,5.f,2.f},united{-8.f,-4.f,-6.f,4.f,9.f,12.f};
        const auto& expected=mode==1?first:united;const auto output=mode==3?Boxes:Output;
        for(unsigned i=0;i<6;++i)if(m.ReadU32(output+4u*i)!=std::bit_cast<std::uint32_t>(expected[i]))throw std::runtime_error("box bounds finite result");
    }
}
}
void BoxBoundsSave(PPCContext& c,std::uint8_t*) {
    auto s=crt_full_oracle::FromPpc(c);auto& m=*box_bounds_oracle::originalMemory;
    for(unsigned i=28;i<32;++i)WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);
    m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
}
void BoxBoundsRestore(PPCContext& c,std::uint8_t*) {
    auto s=crt_full_oracle::FromPpc(c);auto& m=*box_bounds_oracle::originalMemory;
    for(unsigned i=28;i<32;++i)s.r[i]=ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));
    s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);
}
int main(){try{for(unsigned mode=0;mode<4;++mode)box_bounds_oracle::Check(mode);std::puts("PASS tree-box-bounds61 4 actual-body cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
