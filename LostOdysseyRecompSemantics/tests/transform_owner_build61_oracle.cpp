#include "object_sort_engine61_oracle_fixture.h"
#include "lo_semantics/transform_owner_build61.h"
#include <algorithm>
#include <bit>
namespace owner_build61_oracle {
using Registers=transform_owner_build61::Registers;
using Machine=diagnostic_lock61::MachineState;
constexpr GuestAddress Owner=0x30000u,Descriptor=0x31000u,Source=0x32000u,Triplets=0x33000u;
constexpr GuestAddress Tree=0x35000u,Items=0x36000u,Arena=0x37000u,Strategy=0x38000u,AllocatorTable=0x39000u;
constexpr GuestAddress Allocate=0x2a00u,Bounds=0x2b00u,Approve=0x2c00u,Finalize=0x2d00u;
constexpr std::array<test::Region,4> Regions{{{0u,0x120000u},{0x820d6000u,0x1000u},{0x83216000u,0x1000u},{0x832df000u,0x1000u}}};
struct Guest final:manager_release_context61::GuestServices {
    std::vector<std::array<std::uint64_t,73>> events;
    void CallDirect(GuestAddress,GuestMemory&,Registers&)override{throw std::runtime_error("unexpected owner-build direct service");}
    void CallIndirect(GuestAddress target,GuestMemory& m,Registers& s)override {
        std::array<std::uint64_t,73> event{};auto snap=crt_full_oracle::Snapshot(s);std::copy(snap.begin(),snap.end(),event.begin());event.back()=target;events.push_back(event);
        if(target==Allocate) {
            if(s.lr==0x82bd7b1cu){if(s.r[4]!=28u||s.r[5]!=24u)throw std::runtime_error("tree descriptor allocation");s.r[3]=Tree;}
            else if(s.lr==0x82bdad74u){if(s.r[4]!=8u||s.r[5]!=61u)throw std::runtime_error("index allocation");s.r[3]=Items;}
            else if(s.lr==0x82bdae0cu){if(s.r[4]!=124u||s.r[5]!=26u)throw std::runtime_error("node allocation");s.r[3]=Arena;}
            else if(s.lr==0x82bd143cu){if(s.r[4]!=12u||s.r[5]!=0u)throw std::runtime_error("strategy allocation");s.r[3]=Strategy;}
            else throw std::runtime_error("unexpected owner-build allocation callsite");
        } else if(target==Bounds) {
            constexpr std::array<float,6> bounds{{0.f,0.f,0.f,2.f,3.f,4.f}};
            for(unsigned i=0u;i<6u;++i)m.WriteU32(Address(s.r[6])+i*4u,std::bit_cast<std::uint32_t>(bounds[i]));
        } else if(target==Approve)s.r[3]=0u;
        else if(target==Finalize) {
            if(s.r[3]!=Strategy||s.r[4]!=Tree||s.lr!=0x82bd7be0u||m.ReadU32(Tree+4u)!=Arena+4u)
                throw std::runtime_error("strategy bind arguments/order");
            m.WriteU32(Strategy+4u,Tree);s.r[3]=0x1234567800000001ull;
        } else throw std::runtime_error("unexpected owner-build indirect");
        s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[7]^=0x180u;s.cr0.eq^=1u;s.cr7.lt^=1u;
    }
};
struct Native final:float_triplet_transfer::NativeServices {void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
// These cases never take the diagnostic branch. Unexpected synchronization is
// rejected rather than replaced with a no-op; that lower has separate cases.
struct Sync final:diagnostic_lock61::SynchronizationServices {
    std::uint32_t LoadReservedWord(GuestAddress,GuestMemory&)override{throw std::runtime_error("unexpected diagnostic reservation");}
    bool CompareExchangeWord(GuestAddress,std::uint32_t,std::uint32_t,GuestMemory&)override{throw std::runtime_error("unexpected diagnostic CAS");}
    void EnterCriticalSection(GuestMemory&,Registers&,Machine&)override{throw std::runtime_error("unexpected diagnostic lock");}
    void LeaveCriticalSection(GuestMemory&,Registers&,Machine&)override{throw std::runtime_error("unexpected diagnostic unlock");}
};
struct DiagnosticGuest final:crt_narrow_formatter61::GuestServices {
    void CallIndirect(GuestAddress,GuestMemory&,Registers&)override{throw std::runtime_error("unexpected diagnostic output");}
    void CallOutput(GuestMemory&,Registers&)override{throw std::runtime_error("unexpected formatter output");}
};
struct Environment {
    sort_engine61_oracle::Environment accepted;Guest guest;Native fp;Sync sync;DiagnosticGuest diagnostic;
    Machine machine{0x020a8020u,0xcafebabe11223344ull};
    explicit Environment(test::GuestWindow& w):accepted(w){}
    transform_owner_build61::Dependencies Deps(){return{{guest,fp},{{accepted.Deps().sort.accepted,diagnostic},sync,machine}};}
};
Environment* original=nullptr;
void Check(unsigned mode) {
    struct RestoreHost{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~RestoreHost(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    test::GuestWindow before(Regions),after(Regions);
    const auto seed=[&](test::GuestWindow& w) {
        w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Owner+8u,0u);m.WriteU32(Owner+12u,0u);m.WriteU32(Owner+16u,0u);
        m.WriteU32(Descriptor,mode==2u?0u:Source);m.WriteU32(Descriptor+4u,1u);m.WriteU32(Descriptor+8u,0u);
        m.WriteU32(Descriptor+12u,0u);m.WriteU32(Descriptor+16u,0xffffffffu);m.WriteU32(Descriptor+20u,0u);
        m.WriteU8(Descriptor+24u,0u);m.WriteU8(Descriptor+25u,0u);m.WriteU8(Descriptor+26u,1u);
        m.WriteU32(Source+8u,mode==1u?1u:2u);m.WriteU32(Source+12u,1u);m.WriteU32(Source+16u,Triplets);m.WriteU32(Source+20u,0x40000u);
        for(unsigned i=0u;i<6u;++i)m.WriteU32(Triplets+i*4u,i);
        m.WriteU32(0x820d6c54u+4u,Bounds|3u);m.WriteU32(0x820d6c54u+20u,Approve|1u);m.WriteU32(0x820d6ebcu+4u,Finalize|3u);
        m.WriteU32(0x832df554u,0u);m.WriteU32(0x83216624u,AllocatorTable);m.WriteU32(AllocatorTable,Allocate|2u);
    };
    seed(before);seed(after);Environment expected(before),actual(after);auto state=sort_engine61_oracle::Initial(0u);
    state.r[3]=Owner;state.r[4]=Descriptor;const auto initial=state;PPCContext c{};crt_full_oracle::ToPpc(c,state);
    c.msr=expected.machine.msr;c.reserved.u64=expected.machine.reserved_bits;original=&expected;PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    __imp__sub_82BD7A60(c,before.Bytes());const auto host=PPCFPSCRRegister{}.getcsr();original=nullptr;
    auto m=after.Memory();PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!transform_owner_build61::Apply(0x82bd7a60u,m,actual.Deps(),state)||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(state)||!before.EqualCommitted(after)||
        expected.guest.events!=actual.guest.events||host!=PPCFPSCRRegister{}.getcsr()||c.msr!=actual.machine.msr||c.reserved.u64!=actual.machine.reserved_bits)
        throw std::runtime_error("owner build Full72/machine/RAM/callback/host mismatch");
    if(state.r[3]!=(mode==2u?0u:1u)||state.r[1]!=initial.r[1]||state.lr!=Address(initial.lr)||
        actual.guest.events.size()!=(mode==0u?7u:0u))throw std::runtime_error("owner build result/events");
    if(mode==0u&&(m.ReadU32(Owner+12u)!=Tree||m.ReadU32(Owner+16u)!=Strategy||m.ReadU32(Strategy+4u)!=Tree||
        m.ReadU32(Tree)!=Items||m.ReadU32(Tree+4u)!=Arena+4u))throw std::runtime_error("owner build ownership attachments");
    if(mode==1u&&(m.ReadU32(Owner+8u)!=4u||m.ReadU32(Owner+12u)!=0u||m.ReadU32(Owner+16u)!=0u))throw std::runtime_error("single-item fast path");
}
}
void OriginalOwnerBuild61Save(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=owner_build61_oracle::original->accepted.stream.memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalOwnerBuild61Restore(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=owner_build61_oracle::original->accepted.stream.memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
void OriginalOwnerBuild61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*) {
    auto& e=*owner_build61_oracle::original;auto s=crt_full_oracle::FromPpc(c);e.guest.CallIndirect(target,e.accepted.stream.memory,s);crt_full_oracle::ToPpc(c,s);
}
void OriginalOwnerBuild61Lower(GuestAddress entry,PPCContext& c,std::uint8_t*) {
    auto& e=*owner_build61_oracle::original;auto s=crt_full_oracle::FromPpc(c);auto& m=e.accepted.stream.memory;bool handled=false;
    e.machine={c.msr,c.reserved.u64};
    if(entry==0x82bd17f0u||entry==0x82bd1830u||entry==0x82bdac60u)handled=grid_transform_buffer61::Apply(entry,m,s);
    else if(entry==0x82bd1558u)handled=transform_owner_routes61::Apply(entry,m,e.Deps().owner,s);
    else if(entry==0x82bd0798u)handled=crt_close_recursive_buffer_context::Apply(entry,m,e.guest,s);
    else if(entry==0x82bdad18u)handled=owned_tree_construct61::Apply(entry,m,{e.guest,e.fp},s);
    else if(entry==0x82bd12f8u)handled=transform_owner_initialize61::Apply(entry,m,e.Deps().owner,s);
    else if(entry==0x82bdb260u)handled=manager_release_context61::Apply(entry,m,e.Deps().owner,s);
    else if(entry==0x82b9d328u)handled=diagnostic_format_routes61::Apply(entry,m,e.Deps().diagnostics,s);
    if(!handled)throw std::runtime_error("unexpected owner-build lower");
    crt_full_oracle::ToPpc(c,s);c.msr=e.machine.msr;c.reserved.u64=e.machine.reserved_bits;
}
int main(){try{for(unsigned mode=0u;mode<3u;++mode)owner_build61_oracle::Check(mode);std::puts("PASS transform-owner-build61 3 original-upper/shared-lower cases");return 0;}
catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
