#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/owned_tree_refit61.h"
#include <bit>
#include <limits>
namespace tree_refit61_oracle {
using Full=owned_tree_refit61::Registers;
constexpr GuestAddress Tree=0x30000u,Nodes=0x31000u,Context=0x33000u,Boxes=0x34000u,Indices=0x35000u,Dirty=0x36000u;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x82000000u,0x1000u}}};
constexpr std::array<std::array<float,6>,3> ItemBounds{{{{1,2,3,4,5,6}},{{-3,4,-2,2,9,7}},{{0,-8,1,10,2,12}}}};
struct Native final:owned_tree_refit61::NativeServices {
    std::vector<std::uint32_t> controls;
    void SetHostFpControl(std::uint32_t value)override{controls.push_back(value);PPCFPSCRRegister{}.setcsr(value);}
};
GuestMemory* active_memory=nullptr;
void Float(GuestMemory& m,GuestAddress a,float v){m.WriteU32(a,std::bit_cast<std::uint32_t>(v));}
void Seed(test::GuestWindow& w,unsigned scenario) {
    w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Tree+4u,Nodes);m.WriteU32(Tree+16u,4u);
    m.WriteU32(Tree+8u,Dirty);m.WriteU32(Tree+12u,2u);m.WriteU32(Context+72u,Boxes);
    m.WriteU32(Dirty,7u);m.WriteU32(Dirty+4u,scenario==2u?1u:0u);
    for(unsigned n=0;n<33u;++n){const auto node=Nodes+40u*n;m.WriteU32(node+24u,0u);m.WriteU32(node+32u,Indices);m.WriteU32(node+36u,0u);}
    m.WriteU32(Nodes+24u,(Nodes+40u)|1u);
    m.WriteU32(Nodes+40u+36u,2u);m.WriteU32(Nodes+80u+32u,Indices+8u);m.WriteU32(Nodes+80u+36u,1u);
    m.WriteU32(Nodes+32u*40u+32u,Indices+4u);m.WriteU32(Nodes+32u*40u+36u,1u);
    for(unsigned i=0;i<3u;++i){m.WriteU32(Indices+4u*i,i);for(unsigned axis=0;axis<6u;++axis)Float(m,Boxes+24u*i+4u*axis,ItemBounds[i][axis]);}
    Float(m,0x82000d64u,-std::numeric_limits<float>::max());Float(m,0x82000e0cu,std::numeric_limits<float>::max());
}
void Check(unsigned scenario) {
    struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    test::GuestWindow original(Regions),recovered(Regions);Seed(original,scenario);Seed(recovered,scenario);
    auto expected_memory=original.Memory(),memory=recovered.Memory();Full initial{};
    for(unsigned i=0;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x9988776600080000ull;initial.r[3]=0xaabbccdd00000000ull|Tree;initial.r[4]=0x4455667700000000ull|Context;
    initial.r[5]=0x1122334400000000ull|Boxes;initial.lr=0xabcdef0181234567ull;initial.ctr=0x4455667711223344ull;
    initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;initial.xer_ca=1u;
    PPCContext c{};crt_full_oracle::ToPpc(c,initial);active_memory=&expected_memory;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(scenario==0u)__imp__sub_82BDA248(c,original.Bytes());else __imp__sub_82BDA5F0(c,original.Bytes());
    const auto expected_csr=PPCFPSCRRegister{}.getcsr();active_memory=nullptr;auto state=initial;Native native;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!owned_tree_refit61::Apply(scenario==0u?0x82bda248u:0x82bda5f0u,memory,native,state))throw std::runtime_error("missing refit entry");
    const auto want=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),got=crt_full_oracle::Snapshot(state);
    if(want!=got||!original.EqualCommitted(recovered)||expected_csr!=PPCFPSCRRegister{}.getcsr()){
        std::fprintf(stderr,"refit case %u state=%u RAM=%u CSR=%x/%x\n",scenario,want==got,original.EqualCommitted(recovered),expected_csr,PPCFPSCRRegister{}.getcsr());
        for(unsigned i=0;i<want.size();++i)if(want[i]!=got[i])std::fprintf(stderr," state[%u]=%llx/%llx\n",i,(unsigned long long)want[i],(unsigned long long)got[i]);
        throw std::runtime_error("refit full state differs");
    }
    constexpr std::array<float,6> merged{-3,-8,-2,10,9,12};
    for(unsigned axis=0;axis<6u;++axis)if(memory.ReadU32(Nodes+4u*axis)!=std::bit_cast<std::uint32_t>(merged[axis]))throw std::runtime_error("refit parent union differs");
    if(scenario==0u){
        for(unsigned axis=0;axis<6u;++axis)if(memory.ReadU32(Nodes+120u+4u*axis)!=std::bit_cast<std::uint32_t>((axis<3u?1.0f:-1.0f)*std::numeric_limits<float>::max()))throw std::runtime_error("refit empty leaf bounds differ");
    }else{
        if(memory.ReadU32(Dirty)!=0u||memory.ReadU32(Dirty+4u)!=0u)throw std::runtime_error("refit did not consume dirty bits");
        if(scenario==2u)for(unsigned axis=0;axis<6u;++axis)if(memory.ReadU32(Nodes+32u*40u+4u*axis)!=std::bit_cast<std::uint32_t>(ItemBounds[1][axis]))throw std::runtime_error("refit high-word node differs");
        for(unsigned i=26;i<32u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("refit saved register differs");
    }
}
}
void OriginalTreeRefit61Save(PPCContext& c,std::uint8_t*) {
    using namespace tree_refit61_oracle;const auto s=crt_full_oracle::FromPpc(c);auto& m=*active_memory;
    for(unsigned i=26;i<32u;++i)WriteU64(m,Address(s.r[1]-8u*(33u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
}
void OriginalTreeRefit61Restore(PPCContext& c,std::uint8_t*) {
    using namespace tree_refit61_oracle;auto s=crt_full_oracle::FromPpc(c);auto& m=*active_memory;
    for(unsigned i=26;i<32u;++i)s.r[i]=ReadU64(m,Address(s.r[1]-8u*(33u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);
}
int main(){try{for(unsigned n=0;n<3u;++n)tree_refit61_oracle::Check(n);std::puts("PASS owned-tree-refit61 3 genuine-body Full72/RAM/host-CSR cases");
    std::puts("LIMIT finite ordinary bounds, tagged contiguous child pairs and valid indices; no nonfinite/fault/MMIO/concurrency/runtime acceptance");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
