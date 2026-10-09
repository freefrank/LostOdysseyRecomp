#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/owned_tree_visit61.h"
#include <algorithm>
namespace tree_visit61_oracle {
using Full=owned_tree_visit61::Registers;
constexpr GuestAddress Nodes=0x30000u,Maximum=0x31000u,Depth=0x31004u,Callback=0x2a00u;
constexpr std::array<test::Region,1> Regions{{{0u,0x120000u}}};
struct Guest final:owned_tree_visit61::GuestServices {
    unsigned scenario;std::vector<std::array<std::uint64_t,73>> events;std::vector<unsigned> order;
    explicit Guest(unsigned mode):scenario(mode){}
    void CallIndirect(GuestAddress target,GuestMemory& m,Full& s)override{
        if(target!=Callback)throw std::runtime_error("unexpected visitor target");
        std::array<std::uint64_t,73> event{};const auto snap=crt_full_oracle::Snapshot(s);std::copy(snap.begin(),snap.end(),event.begin());event[72]=target;events.push_back(event);
        const auto node=(Address(s.r[3])-Nodes)/40u;order.push_back(node);
        if(scenario!=0u&&node==1u)m.WriteU32(Nodes+24u,(Nodes+3u*40u)|1u);
        s.r[8]^=0x1122334455667788ull;s.r[10]^=0xabcdef1234567890ull;s.fpr_bits[7]^=0x100u;s.cr1={1,0,0,1};s.cr7={0,1,0,0};
        s.cached_fp_control=0x1f80u;PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
        const bool prune=(scenario==1u&&node==1u)||(scenario==2u&&node==3u);
        // A false low byte with nonzero upper bits distinguishes boolean ABI
        // from an ordinary integer/pointer test.
        s.r[3]=prune?0xaabbccdd00000100ull:0xaabbccdd00000101ull;
    }
};
GuestMemory* active_memory=nullptr;Guest* active_guest=nullptr;
void Seed(test::GuestWindow& window) {
    window.Fill(0xa5u);auto m=window.Memory();
    for(unsigned n=0;n<7u;++n)m.WriteU32(Nodes+n*40u+24u,n<3u?((Nodes+(2u*n+1u)*40u)|1u):0u);
    m.WriteU32(Maximum,0u);m.WriteU32(Depth,0u);
}
void Check(unsigned scenario) {
    struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    test::GuestWindow original(Regions),recovered(Regions);Seed(original);Seed(recovered);
    auto expected_memory=original.Memory(),memory=recovered.Memory();Full initial{};
    for(unsigned i=0;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x9988776600080000ull;initial.r[3]=0xaabbccdd00000000ull|Nodes;
    initial.r[4]=scenario==2u?Callback|3u:0x5566778800000000ull|Maximum;
    initial.r[5]=scenario==2u?0x123456789abcdef0ull:0x6677889900000000ull|Depth;
    initial.r[6]=Callback|3u;initial.r[7]=0x123456789abcdef0ull;initial.lr=0xabcdef0181234567ull;
    initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;initial.xer_ca=1u;
    Guest expected(scenario),actual(scenario);PPCContext c{};crt_full_oracle::ToPpc(c,initial);
    active_memory=&expected_memory;active_guest=&expected;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(scenario==2u)__imp__sub_82BDA1A0(c,original.Bytes());else __imp__sub_82BDA0A8(c,original.Bytes());
    const auto expected_csr=PPCFPSCRRegister{}.getcsr();active_memory=nullptr;active_guest=nullptr;
    auto state=initial;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!owned_tree_visit61::Apply(scenario==2u?0x82bda1a0u:0x82bda0a8u,memory,actual,state))throw std::runtime_error("missing visitor entry");
    const auto want=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),got=crt_full_oracle::Snapshot(state);
    if(want!=got||!original.EqualCommitted(recovered)||expected.events!=actual.events||expected_csr!=PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr,"visit case %u state=%u RAM=%u events=%u CSR=%x/%x\n",scenario,want==got,original.EqualCommitted(recovered),expected.events==actual.events,expected_csr,PPCFPSCRRegister{}.getcsr());
        for(unsigned i=0;i<want.size();++i)if(want[i]!=got[i])std::fprintf(stderr," state[%u]=%llx/%llx\n",i,(unsigned long long)want[i],(unsigned long long)got[i]);
        throw std::runtime_error("visitor Full72/RAM/callback mismatch");
    }
    const std::vector<unsigned> order=scenario==0u?std::vector<unsigned>{0,1,3,4,2,5,6}:scenario==1u?std::vector<unsigned>{0,1,4}:std::vector<unsigned>{1,2,3,5,6};
    if(actual.order!=order)throw std::runtime_error("visitor order/prune contract");
    if(scenario!=2u&&(memory.ReadU32(Depth)!=1u||memory.ReadU32(Maximum)!=(scenario==0u?3u:2u)))throw std::runtime_error("visitor live depth contract");
    if(state.r[1]!=initial.r[1]||state.lr!=0x81234567u)throw std::runtime_error("visitor frame contract");
    for(unsigned i=scenario==2u?28u:27u;i<32u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("visitor nonvolatile contract");
}
}
void OriginalTreeVisit61Guest(std::uint32_t target,PPCContext& c,std::uint8_t*) {
    using namespace tree_visit61_oracle;auto s=crt_full_oracle::FromPpc(c);active_guest->CallIndirect(target,*active_memory,s);crt_full_oracle::ToPpc(c,s);
}
void OriginalTreeVisit61Save(unsigned first,PPCContext& c,std::uint8_t*) {
    using namespace tree_visit61_oracle;const auto s=crt_full_oracle::FromPpc(c);auto& m=*active_memory;
    for(unsigned i=first;i<32u;++i)WriteU64(m,Address(s.r[1]-8u*(33u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
}
void OriginalTreeVisit61Restore(unsigned first,PPCContext& c,std::uint8_t*) {
    using namespace tree_visit61_oracle;auto s=crt_full_oracle::FromPpc(c);auto& m=*active_memory;
    for(unsigned i=first;i<32u;++i)s.r[i]=ReadU64(m,Address(s.r[1]-8u*(33u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);
}
int main(){try{for(unsigned n=0;n<3u;++n)tree_visit61_oracle::Check(n);std::puts("PASS owned-tree-visit61 3 genuine recursive-body cases");
    std::puts("LIMIT borrowed acyclic valid child pairs; guest visitor/native internals and exceptional/fault/concurrent traversal remain boundaries");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
