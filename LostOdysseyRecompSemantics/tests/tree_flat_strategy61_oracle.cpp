#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_flat_strategy61.h"
#include <bit>
#include <set>
namespace strategy_oracle {
using Registers=tree_flat_strategy61::Registers;
constexpr GuestAddress Owner=0x30000,Tree=0x31000,Nodes=0x32000,Ids=0x33000,Table=0x34000,Old=0x90000,New=0x91000;
constexpr GuestAddress Allocate=0x2000,Free=0x2004;
constexpr std::array<test::Region,3> Regions{{{0,0x120000},{0x8201f000,0x1000},{0x83216000,0xca000}}};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
 std::set<GuestAddress> live{Old};std::vector<std::array<std::uint64_t,73>> events;
 void CallIndirect(GuestAddress target,GuestMemory&,Registers& s)override{
  std::array<std::uint64_t,73> e{};auto snap=crt_full_oracle::Snapshot(s);std::copy(snap.begin(),snap.end(),e.begin());e[72]=target;events.push_back(e);
  if(target==Allocate){if(s.r[4]!=112u||s.r[5]!=30u)throw std::runtime_error("strategy allocation contract");live.insert(New);s.r[3]=New;}
  else if(target==Free){if(!live.erase(Address(s.r[4])))throw std::runtime_error("strategy ownership");s.r[3]=0;}
  else throw std::runtime_error("unexpected strategy target");
  s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[7]^=0x100u;s.cr7.lt^=1u;
 }
};
GuestMemory* originalMemory=nullptr;Guest* originalGuest=nullptr;
void Check(unsigned mode){
 struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
 test::GuestWindow before(Regions),after(Regions);
 const auto seed=[&](test::GuestWindow& w){w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Owner+4,mode==2?3:1);m.WriteU32(Owner+8,Old+4);m.WriteU32(Old,mode==2?3:1);
  m.WriteU32(Tree+4,Nodes);m.WriteU32(Tree+16,3);m.WriteU32(Nodes+36,2);
  for(unsigned n=0;n<3;++n){auto node=Nodes+40*n;constexpr float b[]{-4,2,-8,6,10,4};for(unsigned i=0;i<6;++i)m.WriteU32(node+4*i,std::bit_cast<std::uint32_t>(b[i]));m.WriteU32(node+24,1);m.WriteU32(node+32,Ids+4*n);m.WriteU32(Ids+4*n,10+n);}
  m.WriteU32(Nodes+24,(Nodes+40)|1u);m.WriteU32(0x8201f9f0,std::bit_cast<std::uint32_t>(0.5f));m.WriteU32(0x832df554,0);m.WriteU32(0x83216624,Table);m.WriteU32(Table,Allocate|1);m.WriteU32(Table+12,Free|3);
 };seed(before);seed(after);Guest expected,actual;Registers s{};for(unsigned i=0;i<32;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
 s.r[1]=0x8877665500080000ull;s.r[3]=Owner;s.r[4]=mode?Tree:0;s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0;s.xer_so=1;
 const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);auto om=before.Memory();originalMemory=&om;originalGuest=&expected;PPCFPSCRRegister{}.setcsr(s.cached_fp_control);__imp__sub_82BDBD90(c,before.Bytes());auto host=PPCFPSCRRegister{}.getcsr();originalMemory=nullptr;originalGuest=nullptr;
 auto m=after.Memory();Native native;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
 if(!tree_flat_strategy61::Apply(0x82bdbd90,m,{actual,native},s)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||!before.EqualCommitted(after)||expected.events!=actual.events||expected.live!=actual.live||host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("strategy Full72/RAM/callback/host mismatch");
 if(s.r[3]!=(mode?1u:0u))throw std::runtime_error("strategy result");
 if(mode){auto output=m.ReadU32(Owner+8);if(output!=(mode==1?New+4:Old+4)||m.ReadU32(Owner+4)!=3||m.ReadU32(output+24)!=1||m.ReadU32(output+28)!=2||m.ReadU32(output+32)!=2)throw std::runtime_error("strategy flat layout");}
 if(actual.events.size()!=(mode==1?2u:0u)||actual.live.size()!=1u)throw std::runtime_error("strategy allocation/reuse");
}
}
void StrategySave(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*strategy_oracle::originalMemory;for(unsigned i=first;i<32;++i)WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));}
void StrategyRestore(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*strategy_oracle::originalMemory;for(unsigned i=first;i<32;++i)s.r[i]=ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);}
void StrategyAllocator(PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);(void)crt_close_recursive_buffer_context::Apply(0x82bd0798,*strategy_oracle::originalMemory,*strategy_oracle::originalGuest,s);crt_full_oracle::ToPpc(c,s);}
void StrategyIndirect(std::uint32_t target,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);strategy_oracle::originalGuest->CallIndirect(target,*strategy_oracle::originalMemory,s);crt_full_oracle::ToPpc(c,s);}
int main(){try{for(unsigned mode=0;mode<3;++mode)strategy_oracle::Check(mode);std::puts("PASS tree-flat-strategy61 3 original binder/recursive-lower cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
