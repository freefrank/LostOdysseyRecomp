#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/cube_projection_table61.h"
#include "lo_semantics/projection_extrema61.h"
#include <bit>
#include <cmath>
namespace cube_projection61_oracle {
using Full=cube_projection_table61::Registers;
constexpr GuestAddress Owner=0x30000,Storage=0x31000,View=0x32000,Points=0x33000,AllocatorTable=0x34000,Allocate=0x2a00;
constexpr std::array<test::Region,6> Regions{{{0,0x120000},{0x82000000,0x20000},{0x820d6000,0x1000},{0x82bb3000,0x1000},{0x83216000,0x1000},{0x832df000,0x1000}}};
struct Native final:float_triplet_transfer::NativeServices {void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
struct Environment;
Environment* original=nullptr;
void OriginalPrepare(Full& s);
struct Environment final:crt_close_recursive_buffer_context::GuestServices {
 test::GuestWindow& window;GuestMemory memory;Native fp;bool original_side=false;unsigned allocations=0;
 std::vector<std::array<std::uint64_t,73>> events;std::vector<std::array<std::uint32_t,3>> directions;
 explicit Environment(test::GuestWindow& w):window(w),memory(w.Memory()){}
 cube_projection_table61::Dependencies Deps(){return{*this,fp};}
 void CallIndirect(GuestAddress target,GuestMemory& m,Full& s)override{
  std::array<std::uint64_t,73> e{};auto snap=crt_full_oracle::Snapshot(s);std::copy(snap.begin(),snap.end(),e.begin());e[72]=target;events.push_back(e);
  if(target==0x82bb3430u){if(original_side)OriginalPrepare(s);else (void)cube_projection_table61::Apply(target,m,Deps(),s);}
  else if(target==0x82bb34f8u){directions.push_back({m.ReadU32(Address(s.r[5])),m.ReadU32(Address(s.r[5]+4u)),m.ReadU32(Address(s.r[5]+8u))});(void)projection_extrema61::Apply(target,m,fp,s);}
  else if(target==0x822d3068u){} // Existing mapped empty leaf, preserves state.
  else if(target==Allocate){s.r[3]=0x50000u+0x1000u*allocations++;s.r[8]^=0x123456789abcdef0ull;s.cr7.lt^=1u;}
  else throw std::runtime_error("unexpected cube projection slot");
 }
};
void OriginalPrepare(Full& s){PPCContext c{};crt_full_oracle::ToPpc(c,s);__imp__sub_82BB3430(c,original->window.Bytes());s=crt_full_oracle::FromPpc(c);}
void Check(unsigned resolution){
 struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
 test::GuestWindow before(Regions),after(Regions);
 const auto seed=[](test::GuestWindow& w){w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Owner,0x820d6280u);m.WriteU32(Owner+4,Storage);m.WriteU32(Owner+8,Storage);m.WriteU32(Storage+32,View);m.WriteU32(View+12,3);m.WriteU32(View+16,Points);
  m.WriteU32(0x820d6284,0x82bb3430);m.WriteU32(0x820d6288,0x82bb34f8);m.WriteU32(0x820d628c,0x822d3068);
  for(unsigned i=0;i<6;++i)m.WriteU32(0x82bb3988+4*i,i<2?0x82bb39a0u:i<4?0x82bb39f8u:0x82bb3a50u);
  for(unsigned i=0;i<9;++i)m.WriteU32(Points+4*i,std::bit_cast<std::uint32_t>(i==0||i==4||i==8?1.f:0.f));
  m.WriteU32(0x82000e0c,0x7f7fffffu);m.WriteU32(0x82000e40,std::bit_cast<std::uint32_t>(-1.f));m.WriteU32(0x82000e50,0);
  m.WriteU32(0x82007784,std::bit_cast<std::uint32_t>(1.f));m.WriteU32(0x8201f9f0,std::bit_cast<std::uint32_t>(.5f));
  m.WriteU32(0x832df554,0);m.WriteU32(0x83216624,AllocatorTable);m.WriteU32(AllocatorTable,Allocate|1u);
 };seed(before);seed(after);Environment expected(before),actual(after);expected.original_side=true;
 Full s{};for(unsigned i=0;i<32;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
 s.r[1]=0x8877665500080000ull;s.r[3]=Owner;s.r[4]=resolution;s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0u;s.xer_so=1;
 const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);PPCFPSCRRegister{}.setcsr(s.cached_fp_control);original=&expected;__imp__sub_82BB38A0(c,before.Bytes());original=nullptr;auto host=PPCFPSCRRegister{}.getcsr();
 PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);auto m=after.Memory();if(!cube_projection_table61::Apply(0x82bb38a0u,m,actual.Deps(),s))throw std::runtime_error("missing cube projection entry");
 auto a=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),b=crt_full_oracle::Snapshot(s);
 if(a!=b||!before.EqualCommitted(after)||expected.events!=actual.events||expected.directions!=actual.directions||host!=PPCFPSCRRegister{}.getcsr()){
  std::fprintf(stderr,"resolution%u Full%d RAM%d events%d directions%d host%d\n",resolution,a==b,before.EqualCommitted(after),expected.events==actual.events,expected.directions==actual.directions,host==PPCFPSCRRegister{}.getcsr());
  for(unsigned i=0;i<a.size();++i)if(a[i]!=b[i])std::fprintf(stderr,"Full[%u] %llx/%llx\n",i,(unsigned long long)a[i],(unsigned long long)b[i]);
  for(auto region:Regions)for(std::size_t i=0;i<region.size;++i)if(before.Bytes()[region.base+i]!=after.Bytes()[region.base+i]){std::fprintf(stderr,"RAM %08llx %02x/%02x\n",(unsigned long long)(region.base+i),before.Bytes()[region.base+i],after.Bytes()[region.base+i]);break;}
  for(std::size_t i=0;i<std::min(expected.events.size(),actual.events.size());++i)if(expected.events[i]!=actual.events[i]){for(unsigned j=0;j<73;++j)if(expected.events[i][j]!=actual.events[i][j])std::fprintf(stderr,"event%zu field%u %llx/%llx\n",i,j,(unsigned long long)expected.events[i][j],(unsigned long long)actual.events[i][j]);break;}
  throw std::runtime_error("cube projection state mismatch");}
 if(s.r[3]!=1u||actual.allocations!=2u||actual.directions.size()!=6u*resolution*resolution||m.ReadU32(Storage+4)!=resolution||m.ReadU32(Storage+8)!=6u*resolution*resolution)throw std::runtime_error("cube projection output dimensions");
 for(const auto& direction:actual.directions){double square=0;for(auto bits:direction){auto f=std::bit_cast<float>(bits);square+=double(f)*f;}if(std::abs(square-1.0)>1e-5)throw std::runtime_error("cube projection direction normalization");}
}
}
void OriginalCubeProjection61Indirect(std::uint32_t target,PPCContext& c,std::uint8_t*){auto& env=*cube_projection61_oracle::original;auto s=crt_full_oracle::FromPpc(c);env.CallIndirect(target,env.memory,s);crt_full_oracle::ToPpc(c,s);}
void OriginalCubeProjection61Allocator(PPCContext& c,std::uint8_t*){auto& env=*cube_projection61_oracle::original;auto s=crt_full_oracle::FromPpc(c);(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,env.memory,env,s);crt_full_oracle::ToPpc(c,s);}
void OriginalCubeProjection61Save(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=cube_projection61_oracle::original->memory;for(unsigned i=first;i<32;++i)WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));}
void OriginalCubeProjection61Restore(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=cube_projection61_oracle::original->memory;for(unsigned i=first;i<32;++i)s.r[i]=ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);}
void OriginalCubeProjection61SaveFloat(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=cube_projection61_oracle::original->memory;for(unsigned i=first;i<32;++i)WriteU64(m,Address(s.r[12]-8u*(32u-i)),s.fpr_bits[i]);}
void OriginalCubeProjection61RestoreFloat(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=cube_projection61_oracle::original->memory;for(unsigned i=first;i<32;++i)s.fpr_bits[i]=ReadU64(m,Address(s.r[12]-8u*(32u-i)));crt_full_oracle::ToPpc(c,s);}
int main(){try{for(unsigned resolution:{0u,2u,3u})cube_projection61_oracle::Check(resolution);std::puts("PASS cube-projection-table61 3 original-body/shared-extrema cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
