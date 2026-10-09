#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/transform_owner_routes61.h"
#include <algorithm>
namespace transform_owner61_oracle {
using Registers=transform_owner_routes61::Registers;
constexpr GuestAddress Owner=0x30000u,Storage=0x31000u,Buffer=0x32000u,Other=0x33000u;
constexpr GuestAddress Manager=0x35000u,ManagerTable=0x36000u,CrtTable=0x37000u,OtherTable=0x38000u;
constexpr GuestAddress ManagerFree=0x2a00u,CrtFree=0x2b00u,OtherDelete=0x2c00u;
constexpr std::array<test::Region,4> Regions{{{0u,0x120000u},{0x83216000u,0x1000u},
    {0x832df000u,0x1000u},{0x8330b000u,0x1000u}}};
struct Guest final:manager_release_context61::GuestServices {
    unsigned mode=0u;std::vector<std::array<std::uint64_t,73>> events;
    void CallDirect(GuestAddress,GuestMemory&,Registers&)override{throw std::runtime_error("unexpected owner manager initialization");}
    void CallIndirect(GuestAddress target,GuestMemory& m,Registers& s)override {
        std::array<std::uint64_t,73> event{};const auto snapshot=crt_full_oracle::Snapshot(s);
        std::copy(snapshot.begin(),snapshot.end(),event.begin());event.back()=target;events.push_back(event);
        if(target==ManagerFree) {
            if(s.r[3]!=Manager || Address(s.r[4])!=Buffer || s.lr!=0x823f3384u)throw std::runtime_error("owner manager release mismatch");
            m.WriteU32(Buffer,0xdeadbeefu);
        } else if(target==CrtFree) {
            if(Address(s.r[4])!=Storage || s.lr!=0x82bd1598u || m.ReadU32(Storage+8u)!=0u || m.ReadU32(Storage+12u)!=0u)
                throw std::runtime_error("owner storage released before cleanup");
            m.WriteU32(Storage,0xdeadbeefu);
        } else if(target==OtherDelete) {
            if(s.r[3]!=Other || s.r[4]!=1u || s.lr!=0x82bd15bcu)throw std::runtime_error("owner virtual deleting destructor mismatch");
            if(mode==1u)s.r[29]=0x1122334400000055ull;
            m.WriteU32(Other+4u,1u);
        } else throw std::runtime_error("unexpected owner cleanup boundary");
        s.r[3]=0x11223344facebeefull;s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[7]^=0x180u;
        s.cr0.lt^=1u;s.cr1.gt^=1u;s.cr7.eq^=1u;s.xer_ca^=1u;
    }
};
struct Native final:float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t)override{throw std::runtime_error("unexpected owner FP path");}
};
Guest* active_guest=nullptr;GuestMemory* active_memory=nullptr;
Registers Initial() {
    Registers s{};for(unsigned i=0u;i<32u;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
    s.r[1]=0x8877665500080000ull;s.r[3]=0xaabbccdd00000000ull|Owner;s.lr=0x9988776681234567ull;
    s.cached_fp_control=0x9fc0u;s.xer_so=1u;return s;
}
void Check(unsigned mode) {
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[&](test::GuestWindow& window) {
        window.Fill(0xa5u);auto m=window.Memory();
        m.WriteU32(Owner+12u,mode==0u?0u:Storage);m.WriteU32(Owner+16u,mode==0u?0u:Other);
        m.WriteU32(Storage,0u);m.WriteU32(Storage+4u,0u);m.WriteU32(Storage+8u,Buffer);m.WriteU32(Storage+24u,0u);
        m.WriteU32(Other,OtherTable);m.WriteU32(OtherTable,OtherDelete|1u);
        m.WriteU32(0x8330b608u,Manager);m.WriteU32(Manager,ManagerTable);m.WriteU32(ManagerTable+12u,ManagerFree|3u);
        m.WriteU32(0x832df554u,0u);m.WriteU32(0x83216624u,CrtTable);m.WriteU32(CrtTable+12u,CrtFree|2u);
    };
    seed(original);seed(recovered);auto om=original.Memory(),rm=recovered.Memory();
    const auto initial=Initial();auto state=initial;PPCContext context{};crt_full_oracle::ToPpc(context,initial);
    Guest expected,actual;expected.mode=actual.mode=mode;Native fp;active_guest=&expected;active_memory=&om;
    if(mode==0u)__imp__sub_82BD1770(context,original.Bytes());
    else if(mode==1u)__imp__sub_82BD1558(context,original.Bytes());
    else __imp__sub_82BD7A20(context,original.Bytes());
    active_guest=nullptr;active_memory=nullptr;
    const GuestAddress entry=mode==0u?0x82bd1770u:mode==1u?0x82bd1558u:0x82bd7a20u;
    if(!transform_owner_routes61::Apply(entry,rm,{actual,fp},state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events!=actual.events)
        throw std::runtime_error("transform owner Full72/RAM/callback mismatch");
    if(state.r[3]!=(mode==1u?0x11223344facebeefull:0u) || state.r[1]!=initial.r[1] || state.lr!=0x81234567u ||
        rm.ReadU32(Owner+12u)!=0u || rm.ReadU32(Owner+16u)!=(mode==1u?0x55u:0u) ||
        actual.events.size()!=(mode==0u?0u:3u) || (mode!=1u && rm.ReadU32(Owner)!=0x820d6bb8u))
        throw std::runtime_error("transform cleanup clear/tail/ownership mismatch");
    for(unsigned i=29u;i<=31u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("transform owner saved registers mismatch");
}
void CheckConstructors() {
    constexpr std::array<GuestAddress,4> entries{{0x82bdb310u,0x82bdb610u,0x82bdbd58u,0x82bdc1d0u}};
    constexpr std::array<std::uint32_t,4> tables{{0x820d6e7cu,0x820d6e9cu,0x820d6ebcu,0x820d6edcu}};
    for(unsigned i=0u;i<entries.size();++i) {
        test::GuestWindow original(Regions),recovered(Regions);original.Fill(0xa5u);recovered.Fill(0xa5u);
        auto memory=recovered.Memory();auto state=Initial();const auto initial=state;
        PPCContext context{};crt_full_oracle::ToPpc(context,state);Guest guest;Native fp;
        switch(i) {
        case 0u:__imp__sub_82BDB310(context,original.Bytes());break;
        case 1u:__imp__sub_82BDB610(context,original.Bytes());break;
        case 2u:__imp__sub_82BDBD58(context,original.Bytes());break;
        default:__imp__sub_82BDC1D0(context,original.Bytes());break;
        }
        if(!transform_owner_routes61::Apply(entries[i],memory,{guest,fp},state) ||
            crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
            !original.EqualCommitted(recovered) || memory.ReadU32(Owner)!=tables[i] ||
            memory.ReadU32(Owner+4u)!=0u || memory.ReadU32(Owner+8u)!=0u || memory.ReadU32(Owner+12u)!=0xa5a5a5a5u ||
            state.r[3]!=initial.r[3] || state.lr!=initial.lr)
            throw std::runtime_error("mapped transform constructor full-state mismatch");
    }
}
}
void OriginalTransformOwner61Save(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=*transform_owner61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalTransformOwner61Restore(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=*transform_owner61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
void OriginalTransformOwner61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*) {
    auto s=crt_full_oracle::FromPpc(c);transform_owner61_oracle::active_guest->CallIndirect(target,*transform_owner61_oracle::active_memory,s);
    crt_full_oracle::ToPpc(c,s);
}
void OriginalTransformOwner61Direct(GuestAddress target,PPCContext& c,std::uint8_t*) {
    auto s=crt_full_oracle::FromPpc(c);transform_owner61_oracle::active_guest->CallDirect(target,*transform_owner61_oracle::active_memory,s);
    crt_full_oracle::ToPpc(c,s);
}
int main(){
    try{for(unsigned mode=0u;mode<3u;++mode)transform_owner61_oracle::Check(mode);transform_owner61_oracle::CheckConstructors();
        std::puts("PASS transform-owner-routes61 3 cleanup cases and 4 mapped constructors");return 0;}
    catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
