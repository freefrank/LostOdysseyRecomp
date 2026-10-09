#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_flatten36_61.h"
#include <bit>
namespace flatten_oracle {
using Registers=tree_flatten36_61::Registers;
constexpr GuestAddress Nodes=0x30000u,Ids=0x31000u,Output=0x32000u,Counter=0x33000u;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x8201f000u,0x1000u}}};
GuestMemory* originalMemory=nullptr;
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
void Check(unsigned mode){
 struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
 test::GuestWindow before(Regions),after(Regions);
 const auto seed=[&](test::GuestWindow& w){w.Fill(0xa5u);auto m=w.Memory();
  for(unsigned n=0;n<5;++n){auto node=Nodes+40u*n;constexpr float bounds[]{-4,2,-8,6,10,4};
   for(unsigned axis=0;axis<6;++axis)m.WriteU32(node+4u*axis,std::bit_cast<std::uint32_t>(bounds[axis]));
   m.WriteU32(node+24u,1u);m.WriteU32(node+32u,Ids+4u*n);m.WriteU32(Ids+4u*n,10u+n);
  }
  if(mode)m.WriteU32(Nodes+24u,(Nodes+40u)|1u);
  if(mode==2)m.WriteU32(Nodes+40u+24u,(Nodes+120u)|1u);
  m.WriteU32(Counter,1u);m.WriteU32(0x8201f9f0u,std::bit_cast<std::uint32_t>(0.5f));
 };seed(before);seed(after);
 Registers s{};for(unsigned i=0;i<32;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
 s.r[1]=0x8877665500080000ull;s.r[3]=Output;s.r[4]=0;s.r[5]=Counter;s.r[6]=Nodes;s.lr=0x9988776681234567ull;s.xer_so=1;s.cached_fp_control=0x9fc0u;
 const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);auto om=before.Memory();originalMemory=&om;
 PPCFPSCRRegister{}.setcsr(s.cached_fp_control);__imp__sub_82BDBC18(c,before.Bytes());auto host=PPCFPSCRRegister{}.getcsr();originalMemory=nullptr;
 auto m=after.Memory();Native native;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
 if(!tree_flatten36_61::Apply(0x82bdbc18u,m,native,s)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||!before.EqualCommitted(after)||host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("tree flatten Full72/RAM/host mismatch");
 constexpr float expected[]{1,6,-2,5,4,6};for(unsigned axis=0;axis<6;++axis)if(m.ReadU32(Output+4u*axis)!=std::bit_cast<std::uint32_t>(expected[axis]))throw std::runtime_error("center/extent mismatch");
 if(m.ReadU32(Counter)!=(mode==0?1u:mode==1?3u:5u))throw std::runtime_error("next node counter");
 if(mode==0){if(m.ReadU32(Output+24u)!=0x8000000au||m.ReadU32(Output+28u)!=0xa5a5a5a5u||m.ReadU32(Output+32u)!=0xa5a5a5a5u)throw std::runtime_error("leaf payload or untouched fields");}
 else if(m.ReadU32(Output+24u)!=1u||m.ReadU32(Output+28u)!=(mode==1?2u:4u)||m.ReadU32(Output+32u)!=(mode==1?2u:4u))throw std::runtime_error("depth-first root links");
 if(mode==2&&(m.ReadU32(Output+36u+24u)!=2u||m.ReadU32(Output+36u+28u)!=3u||m.ReadU32(Output+36u+32u)!=2u||m.ReadU32(Output+2u*36u+24u)!=0x8000000du))throw std::runtime_error("nested subtree links");
}
}
void FlattenSave(PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*flatten_oracle::originalMemory;for(unsigned i=27;i<32;++i)WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));}
void FlattenRestore(PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*flatten_oracle::originalMemory;for(unsigned i=27;i<32;++i)s.r[i]=ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);}
int main(){try{for(unsigned mode=0;mode<3;++mode)flatten_oracle::Check(mode);std::puts("PASS tree-flatten36-61 3 recursive original-body cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
