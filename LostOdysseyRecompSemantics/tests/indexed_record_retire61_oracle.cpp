#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/indexed_record_retire61.h"
#include <bit>
namespace indexed_retire61_oracle {
using Registers=indexed_record_retire61::Registers;
constexpr GuestAddress Owner=0x30000u,Records=0x31000u,Indices=0x32000u;
constexpr unsigned Stride=64u;
constexpr std::array<test::Region,3> Regions{{{0u,0x120000u},{0x82000000u,0x1000u},{0x82189000u,0x1000u}}};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
void Check(unsigned scenario){
    struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    const unsigned count=scenario==0u?0u:scenario==1u?3u:7u;
    constexpr std::array<float,7> values{2.f,.5f,4.f,.75f,1.f,3.f,-1.f};
    std::array<std::uint16_t,7> indices{0u,1u,2u,3u,4u,5u,6u};if(scenario==2u)indices={6u,1u,4u,0u,5u,2u,3u};
    test::GuestWindow before(Regions),after(Regions);const auto seed=[&](test::GuestWindow& w){
        w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Owner+56u,Records);m.WriteU32(Owner+60u,Indices);m.WriteU32(Owner+112u,32u);
        m.WriteU32(Owner+120u,Stride);m.WriteU32(Owner+124u,count);m.WriteU32(0x82000e50u,0u);m.WriteU32(0x8218958cu,std::bit_cast<std::uint32_t>(1.f));
        for(unsigned i=0u;i<7u;++i){m.WriteU16(Indices+2u*i,indices[i]);m.WriteU32(Records+Stride*i+12u,std::bit_cast<std::uint32_t>(values[i]));}
    };
    seed(before);seed(after);Registers s{};for(unsigned i=0u;i<32u;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
    s.r[1]=0x8877665500080000ull;s.r[3]=0xaabbccdd00000000ull|Owner;s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0u;s.xer_so=1u;
    const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_822CBA60(c,before.Bytes());const auto expected_host=PPCFPSCRRegister{}.getcsr();auto m=after.Memory();Native fp;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!indexed_record_retire61::Apply(0x822cba60u,m,fp,s)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||
        !before.EqualCommitted(after)||expected_host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("indexed retire Full72/RAM/host mismatch");
    if(s.r[1]!=initial.r[1]||s.r[3]!=initial.r[3]||s.r[31]!=initial.r[31]||s.lr!=initial.lr)throw std::runtime_error("indexed retire borrowed/frame mismatch");
    unsigned active=count;std::array<bool,7> retired{};
    for(unsigned remaining=count;remaining!=0u;--remaining){const unsigned position=remaining-1u,id=indices[position];
        if(values[id]>1.f){retired[id]=true;indices[position]=indices[active-1u];indices[active-1u]=std::uint16_t(id);--active;}}
    if(m.ReadU32(Owner+124u)!=active)throw std::runtime_error("indexed retire live count mismatch");
    for(unsigned i=0u;i<7u;++i){if(m.ReadU16(Indices+2u*i)!=indices[i])throw std::runtime_error("indexed retire swapped-last permutation mismatch");
        for(unsigned offset=32u;offset<=48u;offset+=4u)if(m.ReadU32(Records+Stride*i+offset)!=(retired[i]?0u:0xa5a5a5a5u))throw std::runtime_error("indexed retire five-field clear mismatch");
        if(m.ReadU32(Records+Stride*i+52u)!=0xa5a5a5a5u)throw std::runtime_error("indexed retire clear footprint mismatch");}
}
}
int main(){try{for(unsigned scenario=0u;scenario<3u;++scenario)indexed_retire61_oracle::Check(scenario);std::puts("PASS indexed-record-retire61 3 actual-body cases");return 0;}
catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
