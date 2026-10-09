#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/manager_release_context61.h"
#include <algorithm>
namespace manager_release61_oracle {
using Registers=manager_release_context61::Registers;
constexpr GuestAddress Owner=0x30000u,Buffer=0x32000u,Primary=0x35000u,Wrapper=0x36000u;
constexpr GuestAddress PrimaryTable=0x37000u,WrapperTable=0x38000u;
constexpr GuestAddress Query=0x2a00u,Finish=0x2b00u,Free=0x2c00u;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x8330b000u,0x1000u}}};
struct Guest final:manager_release_context61::GuestServices {
    unsigned mode=0u,allocations=0u;
    std::vector<std::array<std::uint64_t,73>> events;
    void Record(GuestAddress target,Registers& s) {
        std::array<std::uint64_t,73> event{};const auto snapshot=crt_full_oracle::Snapshot(s);
        std::copy(snapshot.begin(),snapshot.end(),event.begin());event.back()=target;events.push_back(event);
    }
    void Mutate(Registers& s) {
        s.r[8]^=0x123456789abcdef0ull;s.r[10]^=0xfedcba9876543210ull;
        s.fpr_bits[2]^=0x100u;s.fpr_bits[7]^=0x180u;s.fpr_bits[13]^=0x200u;
        s.cr0.lt^=1u;s.cr1.gt^=1u;s.cr7.eq^=1u;s.xer_ca^=1u;s.xer_so^=1u;
        s.cached_fp_control^=0x40u;
    }
    void CallDirect(GuestAddress target,GuestMemory& memory,Registers& s)override {
        Record(target,s);
        if(target==0x823acbd0u) {
            if(s.r[3]!=(allocations==0u?0x48decu:36u))throw std::runtime_error("manager allocation size mismatch");
            s.r[3]=allocations++==0u?Primary:Wrapper;
        } else if(target==0x827c5970u) {
            if(s.r[3]!=Primary)throw std::runtime_error("primary manager constructor receiver mismatch");
            memory.WriteU32(Primary,PrimaryTable);memory.WriteU32(Primary+4u,0x11223344u);
        } else if(target==0x827c4ed0u) {
            if(s.r[3]!=Wrapper || s.r[4]!=Primary)throw std::runtime_error("manager wrapper constructor arguments mismatch");
            memory.WriteU32(Wrapper,WrapperTable);memory.WriteU32(Wrapper+4u,Primary);
        } else throw std::runtime_error("unexpected manager initialization direct boundary");
        Mutate(s);
    }
    void CallIndirect(GuestAddress target,GuestMemory& memory,Registers& s)override {
        Record(target,s);
        if(target==Query) {
            if(s.r[3]!=Primary || s.lr!=0x827c5f8cu)throw std::runtime_error("manager query receiver mismatch");
            s.r[3]=mode==2u?0u:1u;
        } else if(target==Finish) {
            if(s.r[3]!=(mode==2u?Wrapper:Primary) || s.lr!=0x827c5fd4u)
                throw std::runtime_error("manager initialization finish receiver mismatch");
            s.r[3]=0xaabbccdd99887766ull;
        } else if(target==Free) {
            if(s.r[3]!=(mode==2u?Wrapper:Primary) || Address(s.r[4])!=Buffer || s.lr!=0x823f3384u)
                throw std::runtime_error("manager release receiver/buffer mismatch");
            memory.WriteU32(Buffer,0xdeadbeefu);s.r[3]=0x7766554433221100ull;
        } else throw std::runtime_error("unexpected manager virtual boundary");
        Mutate(s);
    }
};
struct Native final:float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t value)override{PPCFPSCRRegister{}.setcsr(value);}
};
Guest* active_guest=nullptr;GuestMemory* active_memory=nullptr;
void Check(unsigned mode) {
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[&](test::GuestWindow& window) {
        window.Fill(0xa5u);auto m=window.Memory();
        m.WriteU32(0x8330b608u,mode==1u||mode==2u?0u:Primary);
        m.WriteU32(Primary,PrimaryTable);m.WriteU32(Wrapper,WrapperTable);
        for(auto table:{PrimaryTable,WrapperTable}) {
            m.WriteU32(table+12u,Free|3u);m.WriteU32(table+56u,Finish|2u);m.WriteU32(table+60u,Query|1u);
        }
        m.WriteU32(Owner,0u);m.WriteU32(Owner+4u,0u);m.WriteU32(Owner+8u,Buffer);m.WriteU32(Owner+24u,0u);
    };
    seed(original);seed(recovered);auto original_memory=original.Memory(),recovered_memory=recovered.Memory();
    Registers initial{};
    for(unsigned i=0u;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=0xaabbccdd00000000ull|(mode==3u?Owner:Buffer);
    initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    PPCContext context{};crt_full_oracle::ToPpc(context,initial);auto state=initial;
    Guest expected,actual;expected.mode=actual.mode=mode;Native native;
    active_guest=&expected;active_memory=&original_memory;
    if(mode==3u)__imp__sub_82BDB260(context,original.Bytes());
    else if(mode==0u)__imp__sub_82388B58(context,original.Bytes());
    else __imp__sub_823F3340(context,original.Bytes());
    active_guest=nullptr;active_memory=nullptr;
    const auto entry=mode==3u?0x82bdb260u:mode==0u?0x82388b58u:0x823f3340u;
    if(!manager_release_context61::Apply(entry,recovered_memory,{actual,native},state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events!=actual.events)
        throw std::runtime_error("manager release Full72/RAM/callback mismatch");
    if(state.r[3]!=0x7766554433221100ull || state.r[1]!=initial.r[1] || state.lr!=0x81234567u ||
        state.r[30]!=initial.r[30] || state.r[31]!=initial.r[31] ||
        recovered_memory.ReadU32(Buffer)!=0xdeadbeefu ||
        recovered_memory.ReadU32(0x8330b608u)!=(mode==2u?Wrapper:Primary) ||
        actual.events.size()!=(mode==1u?5u:mode==2u?7u:1u))
        throw std::runtime_error("manager release initialization/return mismatch");
    if(mode==3u && (recovered_memory.ReadU32(Owner+8u)!=0u || recovered_memory.ReadU32(Owner+12u)!=0u))
        throw std::runtime_error("destructor buffer fields not cleared");
}
}
void OriginalManagerRelease61Save(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=*manager_release61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalManagerRelease61Restore(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=*manager_release61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
void OriginalManagerRelease61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*) {
    auto s=crt_full_oracle::FromPpc(c);manager_release61_oracle::active_guest->CallIndirect(target,*manager_release61_oracle::active_memory,s);
    crt_full_oracle::ToPpc(c,s);
}
void OriginalManagerRelease61Direct(GuestAddress target,PPCContext& c,std::uint8_t*) {
    auto s=crt_full_oracle::FromPpc(c);manager_release61_oracle::active_guest->CallDirect(target,*manager_release61_oracle::active_memory,s);
    crt_full_oracle::ToPpc(c,s);
}
int main(){
    try{for(unsigned mode=0u;mode<4u;++mode)manager_release61_oracle::Check(mode);
        std::puts("PASS manager-release-context61 4 focused PPC cases");return 0;}
    catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
