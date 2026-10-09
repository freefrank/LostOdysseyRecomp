#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/transform_owner_initialize61.h"
#include <algorithm>
namespace transform_initialize61_oracle {
using Registers=transform_owner_initialize61::Registers;
constexpr GuestAddress Owner=0x30000u,Old=0x31000u,Fresh=0x32000u,Table=0x33000u,OldTable=0x34000u;
constexpr GuestAddress Allocate=0x2a00u,Delete=0x2b00u;
constexpr std::array<test::Region,3> Regions{{{0u,0x120000u},{0x83216000u,0x1000u},{0x832df000u,0x1000u}}};
struct Guest final:manager_release_context61::GuestServices {
    unsigned mode=0u;std::vector<std::array<std::uint64_t,73>> events;
    void CallDirect(GuestAddress,GuestMemory&,Registers&)override{throw std::runtime_error("unexpected initialize direct call");}
    void CallIndirect(GuestAddress target,GuestMemory& m,Registers& s)override {
        std::array<std::uint64_t,73> event{};const auto snapshot=crt_full_oracle::Snapshot(s);
        std::copy(snapshot.begin(),snapshot.end(),event.begin());event.back()=target;events.push_back(event);
        if(target==Delete) {
            if(s.r[3]!=Old || s.r[4]!=1u || s.lr!=0x82bd1330u)throw std::runtime_error("strategy delete arguments");
            m.WriteU32(Old+4u,0xdeadbeefu);s.r[29]=0x1234000000000000ull;
            s.r[3]=0xfacebeefu;
        } else if(target==Allocate) {
            const unsigned flags=mode==0u?3u:mode==1u?2u:0u;
            if(s.r[4]!=(mode==0u?36u:12u) || s.r[5]!=0u ||
                m.ReadU32(Owner+8u)!=(0xa5a5a5a4u|flags) || m.ReadU32(Owner+16u)!=0u)
                throw std::runtime_error("strategy allocation order/arguments");
            s.r[3]=mode==2u?0x1234567800000000ull:0x1234567800000000ull|Fresh;
        } else throw std::runtime_error("unexpected initialize indirect");
        s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[7]^=0x180u;s.cr0.lt^=1u;s.cr7.eq^=1u;s.xer_ca^=1u;
    }
};
struct Native final:float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t)override{throw std::runtime_error("unexpected initialize FP path");}
};
Guest* active_guest=nullptr;GuestMemory* active_memory=nullptr;
void Check(unsigned mode) {
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[&](test::GuestWindow& w) {
        w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Owner+8u,0xa5a5a5a7u);m.WriteU32(Owner+16u,mode==1u?Old:0u);
        m.WriteU32(Old,OldTable);m.WriteU32(OldTable,Delete|3u);
        m.WriteU32(0x832df554u,0u);m.WriteU32(0x83216624u,Table);m.WriteU32(Table,Allocate|2u);
    };
    seed(original);seed(recovered);auto om=original.Memory(),rm=recovered.Memory();Registers initial{};
    for(unsigned i=0u;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=0xaabbccdd00000000ull|Owner;
    initial.r[4]=mode==2u?0x100u:1u;initial.r[5]=mode==2u?0x100u:1u;
    initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    auto state=initial;PPCContext context{};crt_full_oracle::ToPpc(context,initial);
    Guest expected,actual;expected.mode=actual.mode=mode;Native fp;active_guest=&expected;active_memory=&om;
    __imp__sub_82BD12F8(context,original.Bytes());active_guest=nullptr;active_memory=nullptr;
    if(!transform_owner_initialize61::Apply(0x82bd12f8u,rm,{actual,fp},state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events!=actual.events)
        throw std::runtime_error("transform initialize Full72/RAM/callback mismatch");
    if(state.r[3]!=(mode==2u?0u:1u) || state.r[1]!=initial.r[1] || state.lr!=0x81234567u ||
        rm.ReadU32(Owner+16u)!=(mode==2u?0u:Fresh) || actual.events.size()!=(mode==1u?2u:1u))
        throw std::runtime_error("transform initialize ownership/result mismatch");
    if(mode!=2u && (rm.ReadU32(Fresh)!=(mode==0u?0x820d6e9cu:0x820d6e7cu) ||
        rm.ReadU32(Fresh+4u)!=0u || rm.ReadU32(Fresh+8u)!=0u || rm.ReadU32(Fresh+12u)!=0xa5a5a5a5u))
        throw std::runtime_error("transform initialize constructor mismatch");
    for(unsigned i=29u;i<=31u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("transform initialize saved registers");
}
}
void OriginalTransformInitialize61Save(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=*transform_initialize61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalTransformInitialize61Restore(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=*transform_initialize61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
void OriginalTransformInitialize61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*) {
    auto s=crt_full_oracle::FromPpc(c);transform_initialize61_oracle::active_guest->CallIndirect(target,*transform_initialize61_oracle::active_memory,s);
    crt_full_oracle::ToPpc(c,s);
}
int main() {
    try{for(unsigned mode=0u;mode<3u;++mode)transform_initialize61_oracle::Check(mode);
        std::puts("PASS transform-owner-initialize61 3 cases");return 0;}
    catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
