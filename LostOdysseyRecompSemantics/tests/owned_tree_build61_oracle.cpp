#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/owned_tree_build61.h"
#include <algorithm>
#include <bit>
namespace tree_build61_oracle {
using Registers=owned_tree_build61::Registers;
constexpr GuestAddress Node=0x30000u,Items=0x31000u,Context=0x32000u,Table=0x33000u;
constexpr GuestAddress Arena=0x35000u,Allocation=0x37000u,AllocatorTable=0x39000u;
constexpr GuestAddress Approve=0x2a00u,Threshold=0x2b00u,Score=0x2c00u,Coordinate=0x2d00u,Allocate=0x2e00u;
constexpr std::array<test::Region,6> Regions{{{0u,0x120000u},{0x82000000u,0x1000u},{0x82007000u,0x1000u},
    {0x8201f000u,0x1000u},{0x83216000u,0x1000u},{0x832df000u,0x1000u}}};
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    unsigned mode=0u;
    std::vector<std::array<std::uint64_t,73>> events;
    void CallIndirect(GuestAddress target,GuestMemory& m,Registers& s)override {
        std::array<std::uint64_t,73> event{};
        const auto snapshot=crt_full_oracle::Snapshot(s);
        std::copy(snapshot.begin(),snapshot.end(),event.begin());
        event.back()=target;
        events.push_back(event);
        if(target==Approve){if(s.r[5]!=4u||Address(s.r[6])!=Node)throw std::runtime_error("split eligibility arguments mismatch");
        s.r[3]=1u;
        }
        else if(target==Threshold){s.fpr_bits[1]=std::bit_cast<std::uint64_t>(1.5);
        }
        else if(target==Score){
            const auto item=Address(s.r[4]),axis=Address(s.r[5]);
            const double value=mode==3u?0.0:axis==0u?0.0:axis==1u?double(item-1u):item==1u?3.0:0.0;
            s.fpr_bits[1]=std::bit_cast<std::uint64_t>(value);
        }else if(target==Coordinate){
            const float item=float(Address(s.r[4]));
            const auto out=Address(s.r[5]);
            m.WriteU32(out,std::bit_cast<std::uint32_t>(item));
            m.WriteU32(out+4u,std::bit_cast<std::uint32_t>(2.f*item));
            m.WriteU32(out+8u,0u);
        }else if(target==Allocate){
            if(s.r[4]!=84u||s.r[5]!=26u||s.lr!=0x82bd9ee0u)throw std::runtime_error("child allocation arguments mismatch");
            s.r[3]=mode==2u?0u:Allocation;
            if(mode==0u)s.r[25]=0x55u;
        }else throw std::runtime_error("unexpected tree-build boundary");
        s.r[8]^=0x123456789abcdef0ull;
        s.fpr_bits[7]^=0x180u;
        s.cr0.eq^=1u;
        s.cr1.gt^=1u;
        s.cr7.lt^=1u;
        s.cached_fp_control=0x9fc0u;
        PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    }
};
struct Native final:float_triplet_transfer::NativeServices{
    void SetHostFpControl(std::uint32_t value)override{PPCFPSCRRegister{}.setcsr(value);
    }
};
Guest* active_guest=nullptr;
GuestMemory* active_memory=nullptr;
void Check(unsigned mode){
    struct RestoreHost{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();
    ~RestoreHost(){PPCFPSCRRegister{}.setcsr(csr);
    }} restore;
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[&](test::GuestWindow& window){
        window.Fill(0xa5u);
        auto m=window.Memory();
        constexpr std::array<float,6> bounds{{0.f,0.f,0.f,2.f,8.f,4.f}};
        for(unsigned i=0u;i<bounds.size();++i)m.WriteU32(Node+4u*i,std::bit_cast<std::uint32_t>(bounds[i]));
        m.WriteU32(Node+24u,0u);
        m.WriteU32(Node+32u,Items);
        m.WriteU32(Node+36u,4u);
        for(unsigned i=0u;i<4u;++i)m.WriteU32(Items+4u*i,i+1u);
        m.WriteU32(Context,Table);
        m.WriteU32(Context+4u,2u);
        m.WriteU32(Context+8u,mode==0u?1u:mode==1u?2u:mode==2u?8u:4u);
        m.WriteU32(Context+28u,mode==1u?Arena:0u);
        m.WriteU32(Context+64u,2u);
        m.WriteU32(Context+68u,0u);
        m.WriteU32(Table+8u,Threshold|3u);
        m.WriteU32(Table+12u,Score|3u);
        m.WriteU32(Table+16u,Coordinate|3u);
        m.WriteU32(Table+20u,Approve|3u);
        m.WriteU32(0x82000e50u,0u);
        m.WriteU32(0x82007784u,std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x8201f9f0u,std::bit_cast<std::uint32_t>(0.5f));
        m.WriteU32(0x832df554u,0u);
        m.WriteU32(0x83216624u,AllocatorTable);
        m.WriteU32(AllocatorTable,Allocate|3u);
    };
    seed(original);
    seed(recovered);
    auto om=original.Memory(),rm=recovered.Memory();
    Registers initial{};
    for(unsigned i=0u;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;
    initial.fpr_bits[i]=0x3ff0000000000000ull+i;
    }
    initial.r[1]=0x8877665500080000ull;
    initial.r[3]=Node;
    initial.r[4]=Context;
    initial.lr=0x9988776681234567ull;
    initial.cached_fp_control=0x9fc0u;
    initial.xer_so=1u;
    PPCContext context{};
    crt_full_oracle::ToPpc(context,initial);
    auto state=initial;
    Guest expected,actual;
    expected.mode=actual.mode=mode;
    Native fp;
    active_guest=&expected;
    active_memory=&om;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BD9928(context,original.Bytes());
    const auto expected_host=PPCFPSCRRegister{}.getcsr();
    active_guest=nullptr;
    active_memory=nullptr;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!owned_tree_build61::Apply(0x82bd9928u,rm,{actual,fp},state)||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state)||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||expected_host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("tree build Full72/RAM/callback/host-control mismatch");
    if(state.r[3]!=(mode==2u?0u:1u)||state.r[1]!=initial.r[1]||state.lr!=0x81234567u||
        rm.ReadU32(Context+64u)!=(mode==2u?2u:4u)||rm.ReadU32(Context+68u)!=(mode==3u?1u:0u))
        throw std::runtime_error("tree build outcome/accounting mismatch");
    if(mode!=2u){
        const auto child=mode==1u?Arena+80u:Allocation+4u;
        if(rm.ReadU32(Node+24u)!=(child|(mode==1u?1u:0u))||rm.ReadU32(child+32u)!=Items||rm.ReadU32(child+36u)!=2u||
            rm.ReadU32(child+72u)!=Items+8u||rm.ReadU32(child+76u)!=2u)throw std::runtime_error("tree split children/borrowed spans mismatch");
        if(mode==0u&&(rm.ReadU32(child+24u)!=0x55u||rm.ReadU32(child+28u)!=0x55u))throw std::runtime_error("child initialization lost live callback fill");
    }
    for(unsigned i=25u;i<=31u;++i)if(state.r[i]!=initial.r[i]||state.fpr_bits[i]!=initial.fpr_bits[i])throw std::runtime_error("tree build saved frame mismatch");
}
}
void OriginalTreeBuild61Save(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=*tree_build61_oracle::active_memory;
    const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalTreeBuild61Restore(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=*tree_build61_oracle::active_memory;
    const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));
    c.lr=c.r12.u64;
}
void OriginalTreeBuild61SaveFpr(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=*tree_build61_oracle::active_memory;
    const auto f=crt_full_oracle::Fprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r12.u64-8u*(32u-i)),f[i]->u64);
}
void OriginalTreeBuild61RestoreFpr(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=*tree_build61_oracle::active_memory;
    const auto f=crt_full_oracle::Fprs(c);
    for(unsigned i=first;i<=31u;++i)f[i]->u64=ReadU64(m,Address(c.r12.u64-8u*(32u-i)));
}
void OriginalTreeBuild61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*){
    auto s=crt_full_oracle::FromPpc(c);
    tree_build61_oracle::active_guest->CallIndirect(target,*tree_build61_oracle::active_memory,s);
    crt_full_oracle::ToPpc(c,s);
}
int main(){try{for(unsigned mode=0u;mode<4u;++mode)tree_build61_oracle::Check(mode);
std::puts("PASS owned-tree-build61 4 focused PPC cases");
return 0;
}
catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());
return 1;
}}
