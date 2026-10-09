#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/curve_tangent_update61.h"
#include <bit>
namespace curve_tangent61_oracle {
using Registers=curve_tangent_update61::Registers;
constexpr GuestAddress Curve=0x30000u,Points=0x31000u;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x82189000u,0x1000u}}};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
void Float(GuestMemory& m,GuestAddress p,float v){m.WriteU32(p,std::bit_cast<std::uint32_t>(v));}
void Check(unsigned scenario){
    struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    test::GuestWindow before(Regions),after(Regions);const unsigned count=scenario==0u?0u:scenario==1u?1u:6u;
    const auto seed=[&](test::GuestWindow& w){
        w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Curve,Points);m.WriteU32(Curve+4u,count);
        Float(m,0x8218958cu,1.f);Float(m,0x82189798u,.5f);
        constexpr std::array<float,6> x{0.f,2.f,5.f,9.f,14.f,20.f};constexpr std::array<unsigned,6> modes{1u,1u,2u,1u,3u,1u};
        for(unsigned i=0u;i<6u;++i){const auto p=Points+32u*i;Float(m,p+4u,x[i]);Float(m,p+8u,float(i*i));
            Float(m,p+12u,100.f+float(i));Float(m,p+16u,200.f+float(i));Float(m,p+20u,300.f+float(i));Float(m,p+24u,400.f+float(i));
            m.WriteU8(p+28u,std::uint8_t(scenario==1u?0u:modes[i]));}
    };
    seed(before);seed(after);Registers s{};for(unsigned i=0u;i<32u;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
    s.r[1]=0x8877665500080000ull;s.r[3]=0xaabbccdd00000000ull|Curve;s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0u;s.xer_so=1u;
    s.fpr_bits[1]=std::bit_cast<std::uint64_t>(.25);const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);__imp__sub_8262B498(c,before.Bytes());const auto host=PPCFPSCRRegister{}.getcsr();
    auto m=after.Memory();Native fp;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!curve_tangent_update61::Apply(0x8262b498u,m,fp,s)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||
        !before.EqualCommitted(after)||host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("curve tangent Full72/RAM/host mismatch");
    if(s.r[1]!=initial.r[1]||s.r[3]!=initial.r[3]||s.lr!=initial.lr||s.fpr_bits[1]!=initial.fpr_bits[1])throw std::runtime_error("curve borrowed frame/input mismatch");
    if(count!=0u&&(ReadU64(m,Points+20u)!=0u||m.ReadU32(Points+12u)!=std::bit_cast<std::uint32_t>(100.f)))throw std::runtime_error("curve first/single endpoint mismatch");
    if(scenario==2u){
        const auto blend=Points+32u;for(unsigned offset:std::array<unsigned,2>{12u,20u})
            if(m.ReadU32(blend+offset)!=std::bit_cast<std::uint32_t>(1.875f)||m.ReadU32(blend+offset+4u)!=std::bit_cast<std::uint32_t>(1.5f))
                throw std::runtime_error("curve interior finite tangent mismatch");
        if(ReadU64(m,Points+96u+12u)!=0u||ReadU64(m,Points+96u+20u)!=0u||ReadU64(m,Points+160u+12u)!=0u||
            m.ReadU32(Points+64u+12u)!=std::bit_cast<std::uint32_t>(102.f)||m.ReadU32(Points+128u+20u)!=std::bit_cast<std::uint32_t>(304.f)||
            m.ReadU32(Points+160u+20u)!=std::bit_cast<std::uint32_t>(305.f))throw std::runtime_error("curve mode retention/zeroing mismatch");
    }
}
}
int main(){try{for(unsigned i=0u;i<3u;++i)curve_tangent61_oracle::Check(i);std::puts("PASS curve-tangent-update61 3 actual-body cases");return 0;}
catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
