#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/owned_tree_expand61.h"
#include <algorithm>
#include <bit>
namespace tree_expand61_oracle {
using Registers=owned_tree_expand61::Registers;
constexpr GuestAddress Node=0x30000u,Items=0x31000u,Context=0x32000u,Table=0x33000u,Arena=0x35000u;
constexpr GuestAddress BoundsCall=0x2a00u,Approve=0x2b00u;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x832df000u,0x1000u}}};
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t,73>> events;
    void CallIndirect(GuestAddress target,GuestMemory& m,Registers& s)override{
        std::array<std::uint64_t,73> event{};const auto snapshot=crt_full_oracle::Snapshot(s);
        std::copy(snapshot.begin(),snapshot.end(),event.begin());event.back()=target;events.push_back(event);
        if(target==BoundsCall){
            if(s.r[3]!=Context||s.lr!=0x82bdaa8cu)throw std::runtime_error("bounds callback ABI mismatch");
            constexpr std::array<float,6> bounds{{0.f,0.f,0.f,2.f,4.f,6.f}};
            for(unsigned i=0u;i<bounds.size();++i)m.WriteU32(Address(s.r[6]+i*4u),std::bit_cast<std::uint32_t>(bounds[i]));
            s.r[3]=0x11223344u;
        }else if(target==Approve){
            if(s.r[5]!=2u||Address(s.r[6])!=Node)throw std::runtime_error("recursive split eligibility mismatch");
            s.r[3]=1u;
        }else throw std::runtime_error("unexpected recursive tree boundary");
        s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[7]^=0x180u;s.cr0.eq^=1u;s.cr1.gt^=1u;s.cr7.lt^=1u;
        s.cached_fp_control=0x9fc0u;PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    }
};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t value)override{PPCFPSCRRegister{}.setcsr(value);}};
Guest* active_guest=nullptr;GuestMemory* active_memory=nullptr;
void Check(bool recursive){
    struct RestoreHost{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~RestoreHost(){PPCFPSCRRegister{}.setcsr(csr);}}restore;
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[&](test::GuestWindow& window){
        window.Fill(0xa5u);auto m=window.Memory();m.WriteU32(Node+24u,0u);m.WriteU32(Node+32u,Items);m.WriteU32(Node+36u,recursive?2u:1u);
        m.WriteU32(Items,1u);m.WriteU32(Items+4u,2u);m.WriteU32(Context,Table);m.WriteU32(Context+4u,1u);m.WriteU32(Context+8u,16u);
        m.WriteU32(Context+12u,std::bit_cast<std::uint32_t>(-1.f));m.WriteU32(Context+16u,recursive?0xffffffffu:1u);
        m.WriteU32(Context+20u,recursive?0u:std::bit_cast<std::uint32_t>(0.5f));m.WriteU32(Context+28u,Arena);
        m.WriteU8(Context+56u,recursive?0u:1u);m.WriteU32(Context+60u,0u);m.WriteU32(Context+64u,0u);m.WriteU32(Context+68u,0u);
        m.WriteU32(Arena+24u,0u);m.WriteU32(Arena+64u,0u);m.WriteU32(Table+4u,BoundsCall|3u);m.WriteU32(Table+20u,Approve|3u);
        m.WriteU32(0x832df558u,5u);
    };
    seed(original);seed(recovered);auto om=original.Memory(),rm=recovered.Memory();
    Registers initial{};for(unsigned i=0u;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=Node;initial.r[4]=Context;initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    PPCContext context{};crt_full_oracle::ToPpc(context,initial);auto state=initial;Guest expected,actual;Native fp;
    active_guest=&expected;active_memory=&om;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);__imp__sub_82BDAA48(context,original.Bytes());
    const auto expected_host=PPCFPSCRRegister{}.getcsr();active_guest=nullptr;active_memory=nullptr;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!owned_tree_expand61::Apply(0x82bdaa48u,rm,{actual,fp},state)||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state)||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||expected_host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("tree expand Full72/RAM/callback/host-control mismatch");
    if(state.r[1]!=initial.r[1]||state.lr!=0x81234567u||rm.ReadU32(0x832df558u)!=(recursive?8u:6u)||
        rm.ReadU32(Context+60u)!=(recursive?4u:1u)||actual.events.size()!=(recursive?4u:1u))throw std::runtime_error("tree recursion accounting mismatch");
    if(recursive){
        if(rm.ReadU32(Node+24u)!=(Arena|1u)||rm.ReadU32(Arena+28u)!=Node||rm.ReadU32(Arena+68u)!=Node||
            rm.ReadU32(Arena+36u)!=1u||rm.ReadU32(Arena+76u)!=1u)throw std::runtime_error("recursive child parent/span mismatch");
    }else{
        constexpr std::array<float,6> bounds{{-0.5f,-1.5f,-0.5f,2.5f,4.5f,6.5f}};
        for(unsigned i=0u;i<bounds.size();++i)if(rm.ReadU32(Node+4u*i)!=std::bit_cast<std::uint32_t>(bounds[i]))throw std::runtime_error("plane extension/margin mismatch");
        if(rm.ReadU8(Context+56u)!=0u||rm.ReadU32(Context+36u)!=0u||rm.ReadU32(Context+48u)!=std::bit_cast<std::uint32_t>(4.f))
            throw std::runtime_error("first-bounds snapshot mismatch");
    }
}
}
void OriginalTreeExpand61Save(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=*tree_expand61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalTreeExpand61Restore(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=*tree_expand61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
void OriginalTreeExpand61SaveFpr(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=*tree_expand61_oracle::active_memory;const auto f=crt_full_oracle::Fprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r12.u64-8u*(32u-i)),f[i]->u64);
}
void OriginalTreeExpand61RestoreFpr(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=*tree_expand61_oracle::active_memory;const auto f=crt_full_oracle::Fprs(c);
    for(unsigned i=first;i<=31u;++i)f[i]->u64=ReadU64(m,Address(c.r12.u64-8u*(32u-i)));
}
void OriginalTreeExpand61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*){
    auto s=crt_full_oracle::FromPpc(c);tree_expand61_oracle::active_guest->CallIndirect(target,*tree_expand61_oracle::active_memory,s);crt_full_oracle::ToPpc(c,s);
}
int main(){try{tree_expand61_oracle::Check(false);tree_expand61_oracle::Check(true);std::puts("PASS owned-tree-expand61 2 focused PPC cases");return 0;}
catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
