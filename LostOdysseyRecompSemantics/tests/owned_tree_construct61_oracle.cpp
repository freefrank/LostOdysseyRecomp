#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/owned_tree_construct61.h"
#include <algorithm>
#include <bit>
namespace tree_construct61_oracle {
using Registers=owned_tree_construct61::Registers;
constexpr GuestAddress Owner=0x30000u,Context=0x31000u,Items=0x32000u,Arena=0x33000u,Table=0x34000u,AllocatorTable=0x35000u;
constexpr GuestAddress Allocate=0x2a00u,Bounds=0x2b00u,Approve=0x2c00u;
constexpr std::array<test::Region,3> Regions{{{0u,0x120000u},{0x83216000u,0x1000u},{0x832df000u,0x1000u}}};
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    unsigned mode=0u;std::vector<std::array<std::uint64_t,73>> events;
    void CallIndirect(GuestAddress target,GuestMemory& m,Registers& s)override {
        std::array<std::uint64_t,73> event{};const auto snapshot=crt_full_oracle::Snapshot(s);
        std::copy(snapshot.begin(),snapshot.end(),event.begin());event.back()=target;events.push_back(event);
        if(target==Allocate) {
            if(s.lr==0x82bdad74u) {
                if(s.r[4]!=8u || s.r[5]!=61u || m.ReadU32(Context+64u)!=1u || m.ReadU32(Context+68u)!=0u)
                    throw std::runtime_error("index allocation arguments/accounting");
                s.r[3]=mode==1u?0x1234567800000000ull:Items;
            } else if(s.lr==0x82bdae0cu) {
                if(s.r[4]!=124u || s.r[5]!=26u || m.ReadU32(Items)!=0u || m.ReadU32(Items+4u)!=1u)
                    throw std::runtime_error("arena sizing/index sequence");
                s.r[3]=0x1234567800000000ull|Arena;
            } else throw std::runtime_error("unexpected tree allocation callsite");
        } else if(target==Bounds) {
            if(s.r[4]!=Items || s.r[5]!=2u || s.r[6]!=Arena+4u)throw std::runtime_error("root bounds arguments");
            constexpr std::array<float,6> bounds{{0.f,0.f,0.f,2.f,3.f,4.f}};
            for(unsigned i=0u;i<6u;++i)m.WriteU32(Address(s.r[6])+4u*i,std::bit_cast<std::uint32_t>(bounds[i]));
        } else if(target==Approve)s.r[3]=0u;
        else throw std::runtime_error("unexpected construct boundary");
        s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[7]^=0x180u;s.cr0.eq^=1u;s.cr7.lt^=1u;
    }
};
struct Native final:float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t value)override{PPCFPSCRRegister{}.setcsr(value);}
};
Guest* active_guest=nullptr;GuestMemory* active_memory=nullptr;
void Check(unsigned mode) {
    struct RestoreHost{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~RestoreHost(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[](test::GuestWindow& w) {
        w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Owner,0u);m.WriteU32(Owner+4u,0u);m.WriteU32(Owner+24u,0u);
        m.WriteU32(Context,Table);m.WriteU32(Context+16u,0xffffffffu);m.WriteU32(Context+20u,0u);
        m.WriteU32(Context+24u,2u);m.WriteU32(Context+60u,0u);m.WriteU32(Table+4u,Bounds|3u);m.WriteU32(Table+20u,Approve|1u);
        m.WriteU32(0x832df554u,0u);m.WriteU32(0x83216624u,AllocatorTable);m.WriteU32(AllocatorTable,Allocate|2u);
    };
    seed(original);seed(recovered);auto om=original.Memory(),rm=recovered.Memory();Registers initial{};
    for(unsigned i=0u;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=Owner;initial.r[4]=mode==2u?0u:Context;
    initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    auto state=initial;PPCContext context{};crt_full_oracle::ToPpc(context,initial);Guest expected,actual;expected.mode=actual.mode=mode;Native fp;
    active_guest=&expected;active_memory=&om;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BDAD18(context,original.Bytes());const auto expected_host=PPCFPSCRRegister{}.getcsr();active_guest=nullptr;active_memory=nullptr;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!owned_tree_construct61::Apply(0x82bdad18u,rm,{actual,fp},state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events!=actual.events || expected_host!=PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("tree construct Full72/RAM/callback/host mismatch");
    if(state.r[3]!=(mode==0u?1u:0u) || state.r[1]!=initial.r[1] || state.lr!=0x81234567u || actual.events.size()!=(mode==0u?4u:mode==1u?1u:0u))
        throw std::runtime_error("tree construct return/events");
    if(mode==0u && (rm.ReadU32(Owner)!=Items || rm.ReadU32(Owner+4u)!=Arena+4u || rm.ReadU32(Arena)!=3u ||
        rm.ReadU32(Owner+16u)!=1u || rm.ReadU32(Owner+20u)!=2u || rm.ReadU32(Context+28u)!=Arena+4u ||
        rm.ReadU32(Arena+4u+32u)!=Items || rm.ReadU32(Arena+4u+36u)!=2u))
        throw std::runtime_error("tree arena ownership/accounting");
    for(unsigned i=26u;i<=31u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("tree construct saved registers");
}
}
void OriginalTreeConstruct61Save(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=*tree_construct61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalTreeConstruct61Restore(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=*tree_construct61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
void OriginalTreeConstruct61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*) {
    auto s=crt_full_oracle::FromPpc(c);tree_construct61_oracle::active_guest->CallIndirect(target,*tree_construct61_oracle::active_memory,s);crt_full_oracle::ToPpc(c,s);
}
// The actual original upper uses complete accepted semantic lowers on both sides.
void OriginalTreeConstruct61Lower(GuestAddress target,PPCContext& c,std::uint8_t*) {
    auto s=crt_full_oracle::FromPpc(c);auto& m=*tree_construct61_oracle::active_memory;auto& guest=*tree_construct61_oracle::active_guest;
    tree_construct61_oracle::Native fp;bool handled=false;
    if(target==0x82bd0798u)handled=crt_close_recursive_buffer_context::Apply(target,m,guest,s);
    else if(target==0x82bdac88u)handled=owned_tree_cleanup61::Apply(target,m,{guest,fp},s);
    else if(target==0x82bdaa48u)handled=owned_tree_expand61::Apply(target,m,{guest,fp},s);
    if(!handled)throw std::runtime_error("unexpected construct lower");crt_full_oracle::ToPpc(c,s);
}
int main(){try{for(unsigned mode=0u;mode<3u;++mode)tree_construct61_oracle::Check(mode);std::puts("PASS owned-tree-construct61 3 cases");return 0;}
catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
