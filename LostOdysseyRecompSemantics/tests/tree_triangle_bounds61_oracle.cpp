#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_triangle_bounds61.h"
#include <bit>
namespace tree_triangle_bounds61_oracle {
using Registers=tree_triangle_bounds61::Registers;
constexpr GuestAddress Builder=0x30000u,Mesh=0x31000u,Triangles=0x32000u,Vertices=0x33000u,Selection=0x34000u,Output=0x35000u;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x82000000u,0x1000u}}};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
void Check(unsigned scenario){
 struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
 const GuestAddress entry=scenario<2u?0x82bd88e8u:scenario==2u?0x82bd8ac0u:0x82bd8b40u;
 test::GuestWindow before(Regions),after(Regions);const auto seed=[](test::GuestWindow& w){w.Fill(0xa5u);auto m=w.Memory();
  m.WriteU32(Builder+72u,Mesh);m.WriteU32(Mesh+16u,Triangles);m.WriteU32(Mesh+20u,Vertices);
  constexpr std::array<unsigned,6> ids{0u,1u,2u,2u,3u,1u};for(unsigned i=0u;i<ids.size();++i)m.WriteU32(Triangles+4u*i,ids[i]);
  constexpr std::array<float,12> points{-3.f,6.f,9.f,3.f,-6.f,0.f,6.f,3.f,-9.f,12.f,9.f,6.f};
  for(unsigned i=0u;i<points.size();++i)m.WriteU32(Vertices+4u*i,std::bit_cast<std::uint32_t>(points[i]));
  m.WriteU32(Selection,1u);m.WriteU32(Selection+4u,0u);
  m.WriteU32(0x82000e0cu,0x7f7fffffu);m.WriteU32(0x82000d64u,0xff7fffffu);m.WriteU32(0x82000f20u,std::bit_cast<std::uint32_t>(1.f/3.f));
 };
 seed(before);seed(after);Registers s{};for(unsigned i=0u;i<32u;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
 s.r[1]=0x8877665500080000ull;s.r[3]=0xaabbccdd00000000ull|Builder;s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0u;s.xer_so=1u;
 s.r[4]=scenario<2u?(0x1234567800000000ull|Selection):scenario==2u?1u:0u;s.r[5]=scenario<2u?(scenario==0u?0u:2u):scenario==2u?1u:Output;s.r[6]=Output;
 const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
 switch(entry){case 0x82bd88e8u:__imp__sub_82BD88E8(c,before.Bytes());break;case 0x82bd8ac0u:__imp__sub_82BD8AC0(c,before.Bytes());break;default:__imp__sub_82BD8B40(c,before.Bytes());}
 const auto expected_host=PPCFPSCRRegister{}.getcsr();auto m=after.Memory();Native fp;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
 if(!tree_triangle_bounds61::Apply(entry,m,fp,s)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||!before.EqualCommitted(after)||expected_host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("tree triangle Full72/RAM/host mismatch");
 if(s.r[1]!=initial.r[1]||s.r[31]!=initial.r[31]||s.fpr_bits[31]!=initial.fpr_bits[31]||s.lr!=initial.lr)throw std::runtime_error("tree triangle borrowed save mismatch");
 if(scenario==0u){if(s.r[3]!=0u||m.ReadU32(Output)!=0xa5a5a5a5u)throw std::runtime_error("empty bounds output changed");}
 if(scenario==1u){constexpr std::array<float,6> expected{-3.f,-6.f,-9.f,12.f,9.f,9.f};for(unsigned i=0u;i<6u;++i)if(m.ReadU32(Output+4u*i)!=std::bit_cast<std::uint32_t>(expected[i]))throw std::runtime_error("selected triangle bounds mismatch");if(s.r[3]!=1u)throw std::runtime_error("bounds success mismatch");}
 if(scenario==2u&&std::bit_cast<double>(s.fpr_bits[1])!=2.)throw std::runtime_error("axis centroid mismatch");
 if(scenario==3u){constexpr std::array<float,3> expected{2.f,1.f,0.f};for(unsigned i=0u;i<3u;++i)if(m.ReadU32(Output+4u*i)!=std::bit_cast<std::uint32_t>(expected[i]))throw std::runtime_error("triplet centroid mismatch");}
}
}
int main(){try{for(unsigned i=0u;i<4u;++i)tree_triangle_bounds61_oracle::Check(i);std::puts("PASS tree-triangle-bounds61 4 actual-body cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
