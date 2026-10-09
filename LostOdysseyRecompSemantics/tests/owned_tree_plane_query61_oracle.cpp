#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/owned_tree_plane_query61.h"
#include <algorithm>
#include <bit>
namespace tree_plane_query61_oracle {
using Full=owned_tree_plane_query61::Registers;
constexpr GuestAddress Nodes=0x30000u,Planes=0x31000u,Indices=0x32000u,Visitor=0x2a00u;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x8201f000u,0x1000u}}};
struct Native final:float_triplet_transfer::NativeServices {void SetHostFpControl(std::uint32_t value)override{PPCFPSCRRegister{}.setcsr(value);}};
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t,73>> events;
    void CallIndirect(GuestAddress target,GuestMemory&,Full& s)override{
        if(target!=Visitor)throw std::runtime_error("unexpected plane visitor target");
        std::array<std::uint64_t,73> event{};const auto snap=crt_full_oracle::Snapshot(s);std::copy(snap.begin(),snap.end(),event.begin());event[72]=target;events.push_back(event);
        s.r[3]=0xaabbccddfeedfaceull;s.r[7]^=0x123456789abcdef0ull;s.r[8]^=0x1122334455667788ull;
        s.fpr_bits[15]^=0x100u;s.fpr_bits[31]^=0x1000u;s.cr1={1,0,0,1};s.cr7={0,1,0,0};
        s.cached_fp_control=0x9fc0u;PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    }
};
GuestMemory* active_memory=nullptr;Guest* active_guest=nullptr;
void Float(GuestMemory& m,GuestAddress a,float v){m.WriteU32(a,std::bit_cast<std::uint32_t>(v));}
void Box(GuestMemory& m,GuestAddress node,const std::array<float,6>& values){for(unsigned n=0;n<6u;++n)Float(m,node+4u*n,values[n]);}
void Seed(test::GuestWindow& window,unsigned scenario){
    window.Fill(0xa5u);auto m=window.Memory();
    Box(m,Nodes,scenario==0u?std::array<float,6>{4,-1,-1,6,1,1}:std::array<float,6>{-2,-2,-1,2,2,1});
    Box(m,Nodes+40u,{-1,-1,-1,1,1,1});Box(m,Nodes+80u,{3,-1,-1,5,1,1});
    m.WriteU32(Nodes+24u,(Nodes+40u)|1u);m.WriteU32(Nodes+40u+24u,0u);m.WriteU32(Nodes+80u+24u,0u);
    for(unsigned n=0;n<3u;++n){m.WriteU32(Nodes+n*40u+32u,Indices+n*16u);m.WriteU32(Nodes+n*40u+36u,3u-n);}
    constexpr std::array<std::array<float,4>,3> planes{{{{1,0,0,0}},{{0,0,1,100}},{{0,1,0,0}}}};
    for(unsigned n=0;n<3u;++n)for(unsigned c=0;c<4u;++c)Float(m,Planes+n*16u+c*4u,scenario==1u&&c==3u?-10.0f:planes[n][c]);
    Float(m,0x8201f9f0u,.5f);m.WriteU32(Stack-64u,0x0badc0deu);
}
void Check(unsigned scenario){
    struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    test::GuestWindow original(Regions),recovered(Regions);Seed(original,scenario);Seed(recovered,scenario);
    auto expected_memory=original.Memory(),memory=recovered.Memory();Full initial{};
    for(unsigned i=0;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x9988776600000000ull|Stack;initial.r[3]=0xaabbccdd00000000ull|Nodes;initial.r[4]=0x5566778800000000ull|Planes;
    initial.r[5]=scenario==0u?1u:5u;initial.r[6]=Visitor|3u;initial.r[7]=0x123456789abcdef0ull;
    initial.lr=0xabcdef0181234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;initial.xer_ca=1u;
    Guest expected,actual;Native native;PPCContext c{};crt_full_oracle::ToPpc(c,initial);active_memory=&expected_memory;active_guest=&expected;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);__imp__sub_82BDA8B8(c,original.Bytes());const auto expected_csr=PPCFPSCRRegister{}.getcsr();
    active_memory=nullptr;active_guest=nullptr;auto state=initial;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!owned_tree_plane_query61::Apply(0x82bda8b8u,memory,{actual,native},state))throw std::runtime_error("missing plane query entry");
    const auto want=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),got=crt_full_oracle::Snapshot(state);
    if(want!=got||!original.EqualCommitted(recovered)||expected.events!=actual.events||expected_csr!=PPCFPSCRRegister{}.getcsr()){
        std::fprintf(stderr,"plane query case %u state=%u RAM=%u events=%u CSR=%x/%x\n",scenario,want==got,original.EqualCommitted(recovered),expected.events==actual.events,expected_csr,PPCFPSCRRegister{}.getcsr());
        for(unsigned i=0;i<want.size();++i)if(want[i]!=got[i])std::fprintf(stderr," state[%u]=%llx/%llx\n",i,(unsigned long long)want[i],(unsigned long long)got[i]);
        throw std::runtime_error("plane query Full72/RAM/callback mismatch");
    }
    if(actual.events.size()!=(scenario==0u?0u:1u))throw std::runtime_error("plane pruning or aggregation contract");
    if(scenario!=0u){const auto& event=actual.events[0];if(event[3]!=(scenario==1u?3u:2u)||Address(event[4])!=Indices+(scenario==1u?0u:16u)||event[5]!=(scenario==1u?0u:1u)||event[6]!=initial.r[7])throw std::runtime_error("plane callback classification contract");}
    if(state.r[1]!=initial.r[1]||state.lr!=0x81234567u||state.fpr_bits[31]!=initial.fpr_bits[31])throw std::runtime_error("plane query frame/FP save contract");
    for(unsigned i=27u;i<32u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("plane query nonvolatile contract");
}
}
void OriginalTreePlaneQuery61Guest(std::uint32_t target,PPCContext& c,std::uint8_t*){using namespace tree_plane_query61_oracle;auto s=crt_full_oracle::FromPpc(c);active_guest->CallIndirect(target,*active_memory,s);crt_full_oracle::ToPpc(c,s);}
void OriginalTreePlaneQuery61Save(PPCContext& c,std::uint8_t*){using namespace tree_plane_query61_oracle;const auto s=crt_full_oracle::FromPpc(c);auto& m=*active_memory;for(unsigned i=27u;i<32u;++i)WriteU64(m,Address(s.r[1]-8u*(33u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));}
void OriginalTreePlaneQuery61Restore(PPCContext& c,std::uint8_t*){using namespace tree_plane_query61_oracle;auto s=crt_full_oracle::FromPpc(c);auto& m=*active_memory;for(unsigned i=27u;i<32u;++i)s.r[i]=ReadU64(m,Address(s.r[1]-8u*(33u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);}
int main(){try{for(unsigned n=0;n<3u;++n)tree_plane_query61_oracle::Check(n);std::puts("PASS owned-tree-plane-query61 3 genuine recursive-body cases");std::puts("LIMIT finite small plane masks, borrowed valid child pairs; callback internals, nonfinite, faults/MMIO, concurrency and runtime unverified");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
