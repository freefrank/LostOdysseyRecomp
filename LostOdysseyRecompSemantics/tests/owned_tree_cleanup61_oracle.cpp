#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/owned_tree_cleanup61.h"
namespace owned_tree61_oracle {
using Registers=owned_tree_cleanup61::Registers;
constexpr GuestAddress Root=0x30004u,Child=0x32004u,Vtable=0x34000u,Target=0x2a00u;
constexpr GuestAddress Descriptor=0x36000u,Payload=0x37000u,Raw=0x38000u;
constexpr std::array<test::Region,4> Regions{{{0u,0x120000u},{0x82000000u,0x1000u},
    {0x83216000u,0x1000u},{0x832df000u,0x1000u}}};
struct Guest final:owned_tree_cleanup61::GuestServices {
    unsigned mode=0u;
    std::vector<std::array<std::uint64_t,72>> events;
    std::vector<GuestAddress> released;
    void CallIndirect(GuestAddress target,GuestMemory& memory,Registers& s) override {
        if(target!=Target || s.ctr!=(Target|3u))throw std::runtime_error("wrong tree disposal table slot");
        events.push_back(crt_full_oracle::Snapshot(s));released.push_back(Address(s.r[4]));
        memory.WriteU32(Address(s.r[4]),0xdeadbeefu);
        if(mode==1u && Address(s.r[4])==Root)s.r[31]+=0x100u;
        if(mode==3u)s.r[28]+=0x100u;
        if(mode==5u && Address(s.r[4])==Raw)s.r[29]=0x1122334400000055ull;
        s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[7]^=0x180u;s.cr1.gt^=1u;s.cr7.eq^=1u;s.xer_ca^=1u;
    }
};
Guest* active_guest=nullptr;GuestMemory* active_memory=nullptr;
void Check(unsigned mode) {
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[&](test::GuestWindow& window) {
        window.Fill(0xa5u);auto m=window.Memory();
        m.WriteU32(0x832df554u,0u);m.WriteU32(0x83216624u,Vtable);m.WriteU32(Vtable+12u,Target|3u);
        m.WriteU32(Root-4u,mode==3u?0u:2u);m.WriteU32(Child-4u,mode==2u?0u:2u);
        m.WriteU32(Root+24u,mode==0u?Child|1u:mode==1u?Child:1u);
        m.WriteU32(Root+40u+24u,Child);
        m.WriteU32(Child+24u,0u);m.WriteU32(Child+40u+24u,0xdead1u);
    };
    seed(original);seed(recovered);auto original_memory=original.Memory(),recovered_memory=recovered.Memory();
    Registers initial{};
    for(unsigned i=0u;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=0xaabbccdd00000000ull|Root;initial.r[4]=mode;
    initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    PPCContext context{};crt_full_oracle::ToPpc(context,initial);auto state=initial;Guest expected,actual;expected.mode=actual.mode=mode;
    active_guest=&expected;active_memory=&original_memory;__imp__sub_82BD9740(context,original.Bytes());
    active_guest=nullptr;active_memory=nullptr;
    if(!owned_tree_cleanup61::Apply(0x82bd9740u,recovered_memory,actual,state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events!=actual.events || expected.released!=actual.released)
        throw std::runtime_error("tree cleanup Full72/RAM/callback mismatch");
    const std::vector<GuestAddress> releases=mode==0u?std::vector<GuestAddress>{}:mode==1u?
        std::vector<GuestAddress>{Child-4u,Root}:mode==2u?std::vector<GuestAddress>{Child-4u}:std::vector<GuestAddress>{Root-4u};
    const auto returned=initial.r[3]-(mode>=2u?4u:0u)+(mode==1u||mode==3u?0x100u:0u);
    if(actual.released!=releases || state.r[3]!=returned || state.r[1]!=initial.r[1] || state.lr!=0x81234567u)
        throw std::runtime_error("tree ownership/order/return mismatch");
    for(unsigned i=27u;i<=31u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("tree saved state mismatch");
    if(mode!=3u && (recovered_memory.ReadU32(Root+32u)!=0u || recovered_memory.ReadU32(Root+36u)!=0u))
        throw std::runtime_error("tree node fields not cleared");
    if(mode==1u)for(unsigned node=0u;node<2u;++node)
        if(recovered_memory.ReadU32(Child+node*40u+32u)!=0u || recovered_memory.ReadU32(Child+node*40u+36u)!=0u)
            throw std::runtime_error("child array fields not cleared");
    if(mode==2u && (recovered_memory.ReadU32(Root+72u)!=0u || recovered_memory.ReadU32(Root+76u)!=0u))
        throw std::runtime_error("reverse array fields not cleared");
}
struct Native final:float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t value)override{PPCFPSCRRegister{}.setcsr(value);}
};
void CheckOwner(bool populated) {
    struct RestoreHost {
        std::uint32_t control=PPCFPSCRRegister{}.getcsr();
        ~RestoreHost(){PPCFPSCRRegister{}.setcsr(control);}
    } restore;
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[&](test::GuestWindow& window) {
        window.Fill(0xa5u);auto m=window.Memory();
        m.WriteU32(0x832df554u,0u);m.WriteU32(0x83216624u,Vtable);m.WriteU32(Vtable+12u,Target|3u);
        m.WriteU32(0x82000e50u,std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(Root,populated?Raw:0u);m.WriteU32(Root+4u,populated?Child:0u);
        m.WriteU32(Root+24u,populated?Descriptor:0u);
        m.WriteU32(Descriptor,2u);m.WriteU32(Descriptor+4u,1u);m.WriteU32(Descriptor+8u,Payload);
        m.WriteU32(Descriptor+12u,std::bit_cast<std::uint32_t>(2.f));
        m.WriteU32(Child-4u,1u);m.WriteU32(Child+24u,1u);
    };
    seed(original);seed(recovered);auto original_memory=original.Memory(),recovered_memory=recovered.Memory();
    Registers initial{};
    for(unsigned i=0u;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=0xaabbccdd00000000ull|Root;
    initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    PPCContext context{};crt_full_oracle::ToPpc(context,initial);auto state=initial;
    Guest expected,actual;expected.mode=actual.mode=5u;Native native;
    active_guest=&expected;active_memory=&original_memory;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BDAC88(context,original.Bytes());const auto expected_host=PPCFPSCRRegister{}.getcsr();
    active_guest=nullptr;active_memory=nullptr;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!owned_tree_cleanup61::Apply(0x82bdac88u,recovered_memory,{actual,native},state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events!=actual.events || expected.released!=actual.released ||
        expected_host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("tree owner Full72/RAM/callback/host-control mismatch");
    const auto releases=populated?std::vector<GuestAddress>{Payload,Descriptor,Child-4u,Raw}:std::vector<GuestAddress>{};
    if(actual.released!=releases || recovered_memory.ReadU32(Root)!=(populated?0x55u:0u) ||
        recovered_memory.ReadU32(Root+4u)!=0u || recovered_memory.ReadU32(Root+24u)!=0u ||
        state.r[1]!=initial.r[1] || state.lr!=0x81234567u)
        throw std::runtime_error("owner release order/live clear mismatch");
    for(unsigned i=29u;i<=31u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("owner save restoration mismatch");
}
}
void OriginalOwnedTree61Save(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=*owned_tree61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalOwnedTree61Restore(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=*owned_tree61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
void OriginalOwnedTree61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*) {
    auto s=crt_full_oracle::FromPpc(c);owned_tree61_oracle::active_guest->CallIndirect(target,*owned_tree61_oracle::active_memory,s);
    crt_full_oracle::ToPpc(c,s);
}
int main(){
    try{for(unsigned mode=0u;mode<4u;++mode)owned_tree61_oracle::Check(mode);
        owned_tree61_oracle::CheckOwner(false);owned_tree61_oracle::CheckOwner(true);
        std::puts("PASS owned-tree-cleanup61 6 focused PPC cases");return 0;}
    catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
