#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/controlled_random_triplet61.h"
#include <bit>
namespace controlled_triplet61_oracle {
using Registers=controlled_random_triplet61::Registers;
constexpr GuestAddress Object=0x30000u,Output=0x31000u,SeedAddress=0x8331367cu;
constexpr std::array<test::Region,5> Regions{{{0u,0x120000u},{0x82000000u,0x1000u},{0x82189000u,0x1000u},{0x821ba000u,0x1000u},{0x83313000u,0x1000u}}};
struct Native final:float_triplet_transfer::NativeServices {void SetHostFpControl(std::uint32_t value)override{PPCFPSCRRegister{}.setcsr(value);}};
void Check(unsigned scenario){
    struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    const bool second=scenario>=2u;const unsigned flags_offset=second?164u:160u,shift=second?6u:2u;
    const GuestAddress output=scenario==3u?Object+flags_offset:Output;
    const std::uint32_t flags=scenario==0u?0x56000000u:scenario==1u?0u:scenario==2u?0x07e00000u:0x07000000u;
    const std::uint32_t seed=scenario==0u?1u:scenario==1u?0xffffffffu:scenario==2u?0x80000000u:0x12345678u;
    test::GuestWindow before(Regions),after(Regions);const auto init=[&](test::GuestWindow& w){w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Object+flags_offset,flags);m.WriteU32(SeedAddress,seed);
        m.WriteU32(0x82000e50u,0u);m.WriteU32(0x8218958cu,std::bit_cast<std::uint32_t>(1.f));m.WriteU32(0x821baa74u,std::bit_cast<std::uint32_t>(2.f));};
    init(before);init(after);Registers s{};for(unsigned i=0u;i<32u;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
    s.r[1]=0x8877665500080000ull;s.r[3]=0xaabbccdd00000000ull|Object;s.r[5]=0x7766554400000000ull|output;
    s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0u;s.xer_so=1u;const auto initial=s;
    PPCContext c{};crt_full_oracle::ToPpc(c,s);PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if(second)__imp__sub_82620F40(c,before.Bytes());else __imp__sub_8261E470(c,before.Bytes());const auto expected_host=PPCFPSCRRegister{}.getcsr();
    auto m=after.Memory();Native fp;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!controlled_random_triplet61::Apply(second?0x82620f40u:0x8261e470u,m,fp,s)||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||!before.EqualCommitted(after)||expected_host!=PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("controlled triplet Full72/RAM/host mismatch");
    if(s.r[1]!=initial.r[1]||s.r[3]!=initial.r[3]||s.r[5]!=initial.r[5]||s.lr!=initial.lr)throw std::runtime_error("controlled triplet borrowed state mismatch");
    std::uint32_t next=seed,live_flags=flags;
    for(unsigned axis=0u;axis<3u;++axis){next=next*196314165u+907633515u;const float sample=float(next&0x7fffffu)/8388608.f;
        const bool positive=(live_flags&(1u<<(32u-shift-axis)))!=0u,negative=(live_flags&(0x08000000u>>(shift-2u+axis)))!=0u;
        const float expected=positive?(negative?sample*2.f-1.f:sample):(negative?-sample:0.f);
        const auto bits=std::bit_cast<std::uint32_t>(expected);if(m.ReadU32(output+axis*4u)!=bits)throw std::runtime_error("controlled triplet axis selection mismatch");
        if(scenario==3u&&axis==0u)live_flags=bits;
    }
    if(m.ReadU32(SeedAddress)!=next)throw std::runtime_error("controlled triplet final seed mismatch");
}
}
int main(){try{for(unsigned scenario=0u;scenario<4u;++scenario)controlled_triplet61_oracle::Check(scenario);std::puts("PASS controlled-random-triplet61 4 actual-body cases");return 0;}
catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
