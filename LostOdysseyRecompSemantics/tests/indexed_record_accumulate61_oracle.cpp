#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/indexed_record_accumulate61.h"
#include <bit>
namespace indexed_accumulate61_oracle {
using Registers=indexed_record_accumulate61::Registers;
constexpr GuestAddress Owner=0x30000u,Records=0x31000u,Indices=0x32000u;
constexpr unsigned Stride=128u;
constexpr std::array<test::Region,1> Regions{{{0u,0x120000u}}};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
void Check(unsigned scenario){
    struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    const unsigned count=scenario==0u?0u:scenario==1u?3u:7u,source=scenario==2u?48u:64u;
    constexpr std::array<std::uint16_t,7> indices{6u,1u,4u,0u,5u,2u,3u};
    const auto skipped=[](unsigned i){return i==1u||i==5u;};
    test::GuestWindow before(Regions),after(Regions);const auto seed=[&](test::GuestWindow& w){
        w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Owner+56u,Records);m.WriteU32(Owner+60u,Indices);m.WriteU32(Owner+120u,Stride);m.WriteU32(Owner+124u,count);
        for(unsigned i=0u;i<7u;++i){m.WriteU16(Indices+2u*i,indices[i]);m.WriteU32(Records+Stride*i+92u,skipped(i)?3u:2u);
            for(unsigned a=0u;a<3u;++a){m.WriteU32(Records+Stride*i+32u+4u*a,std::bit_cast<std::uint32_t>(float(20u+i+a)));
                m.WriteU32(Records+Stride*i+48u+4u*a,std::bit_cast<std::uint32_t>(float(2u+i+a)));
                m.WriteU32(Records+Stride*i+64u+4u*a,std::bit_cast<std::uint32_t>(float(4u+i+a)));}}
    };
    seed(before);seed(after);Registers s{};for(unsigned i=0u;i<32u;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
    s.r[1]=0x8877665500080000ull;s.r[4]=0xaabbccdd00000000ull|Owner;s.r[5]=0x1234567800000000ull|source;s.fpr_bits[1]=std::bit_cast<std::uint64_t>(.5);
    s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0u;s.xer_so=1u;const auto initial=s;
    PPCContext c{};crt_full_oracle::ToPpc(c,s);PPCFPSCRRegister{}.setcsr(s.cached_fp_control);__imp__sub_822CD290(c,before.Bytes());
    const auto expected_host=PPCFPSCRRegister{}.getcsr();auto m=after.Memory();Native fp;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!indexed_record_accumulate61::Apply(0x822cd290u,m,fp,s)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||
        !before.EqualCommitted(after)||expected_host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("indexed accumulate Full72/RAM/host mismatch");
    if(s.r[1]!=initial.r[1]||s.r[5]!=initial.r[5]||s.r[31]!=initial.r[31]||s.lr!=initial.lr||s.fpr_bits[1]!=initial.fpr_bits[1])throw std::runtime_error("indexed accumulate borrowed/frame mismatch");
    std::array<bool,7> active{};for(unsigned p=0u;p<count;++p)active[indices[p]]=true;
    if(m.ReadU32(Owner+124u)!=count)throw std::runtime_error("indexed accumulate count changed");
    for(unsigned i=0u;i<7u;++i){if(m.ReadU16(Indices+2u*i)!=indices[i])throw std::runtime_error("indexed accumulate index changed");
        for(unsigned a=0u;a<3u;++a){float first=float(2u+i+a),second=float(20u+i+a);const float input=source==48u?first:float(4u+i+a);
            if(active[i]&&!skipped(i)){first+=input*.5f;second+=(source==48u?first:input)*.5f;}
            if(m.ReadU32(Records+Stride*i+48u+4u*a)!=std::bit_cast<std::uint32_t>(first)||m.ReadU32(Records+Stride*i+32u+4u*a)!=std::bit_cast<std::uint32_t>(second))throw std::runtime_error("indexed accumulate scaled/reloaded values mismatch");}}
}
}
int main(){try{for(unsigned scenario=0u;scenario<3u;++scenario)indexed_accumulate61_oracle::Check(scenario);std::puts("PASS indexed-record-accumulate61 3 actual-body cases");return 0;}
catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
