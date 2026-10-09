#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_triangle_split61.h"
#include <bit>
namespace tree_triangle_split61_oracle {
using Registers=tree_triangle_split61::Registers;
GuestMemory* original;
constexpr GuestAddress Builder=0x30000u,Mesh=0x31000u,Triangles=0x32000u,Vertices=0x33000u,Selection=0x34000u,Output=0x35000u;
constexpr std::array<test::Region,3> Regions{{{0u,0x120000u},{0x82000000u,0x1000u},{0x8201f000u,0x1000u}}};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
void Check(unsigned scenario){
 struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
 const GuestAddress entry=0x82bd8bf8u;
 test::GuestWindow before(Regions),after(Regions);const auto seed=[&](test::GuestWindow& w){w.Fill(0xa5u);auto m=w.Memory();
  m.WriteU32(Builder+8u,scenario==0u?0u:32u);m.WriteU32(Builder+72u,Mesh);m.WriteU32(Mesh+16u,Triangles);m.WriteU32(Mesh+20u,Vertices);
  constexpr std::array<unsigned,6> ids{0u,1u,2u,2u,3u,1u};for(unsigned i=0u;i<ids.size();++i)m.WriteU32(Triangles+4u*i,ids[i]);
  constexpr std::array<float,12> points{-3.f,6.f,9.f,3.f,-6.f,0.f,6.f,3.f,-9.f,12.f,9.f,6.f};
  for(unsigned i=0u;i<points.size();++i)m.WriteU32(Vertices+4u*i,std::bit_cast<std::uint32_t>(points[i]));
  for(unsigned i=0u;i<5u;++i)m.WriteU32(Selection+4u*i,i%2u);
  for(unsigned i=0u;i<6u;++i)m.WriteU32(Output+4u*i,std::bit_cast<std::uint32_t>(float(i+1u)));
  m.WriteU32(0x82000e50u,0u);m.WriteU32(0x8201f9f0u,std::bit_cast<std::uint32_t>(.5f));
  m.WriteU32(0x82000e0cu,0x7f7fffffu);m.WriteU32(0x82000d64u,0xff7fffffu);m.WriteU32(0x82000f20u,std::bit_cast<std::uint32_t>(1.f/3.f));
 };
 seed(before);seed(after);Registers s{};for(unsigned i=0u;i<32u;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
 s.r[1]=0x8877665500080000ull;s.r[3]=0xaabbccdd00000000ull|Builder;s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0u;s.xer_so=1u;
 s.r[4]=Selection;s.r[5]=scenario==2u?5u:2u;s.r[6]=Output;s.r[7]=1u;
 const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
 auto original_memory=before.Memory();original=&original_memory;__imp__sub_82BD8BF8(c,before.Bytes());
 const auto expected_host=PPCFPSCRRegister{}.getcsr();auto m=after.Memory();Native fp;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
 if(!tree_triangle_split61::Apply(entry,m,fp,s)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||!before.EqualCommitted(after)||expected_host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("tree triangle Full72/RAM/host mismatch");
 if(s.r[1]!=initial.r[1]||s.lr!=Address(initial.lr))throw std::runtime_error("triangle split frame mismatch");
 for(unsigned i=22u;i<32u;++i)if(s.r[i]!=initial.r[i])throw std::runtime_error("triangle split saved register mismatch");
 const double expected=scenario==0u?3.5:scenario==1u?1.5:1.4f;
 if(std::bit_cast<double>(s.fpr_bits[1])!=expected)throw std::runtime_error("triangle split mean/midpoint mismatch");

}
}
void OriginalTriangleSplit61Save(PPCContext& c,std::uint8_t*){auto& m=*tree_triangle_split61_oracle::original;const auto r=crt_full_oracle::Gprs(c);for(unsigned i=22u;i<32u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);}
void OriginalTriangleSplit61Restore(PPCContext& c,std::uint8_t*){auto& m=*tree_triangle_split61_oracle::original;const auto r=crt_full_oracle::Gprs(c);for(unsigned i=22u;i<32u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;}
int main(){try{for(unsigned i=0u;i<3u;++i)tree_triangle_split61_oracle::Check(i);std::puts("PASS tree-triangle-split61 3 actual-body cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
