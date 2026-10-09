#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_attribute_reorder61.h"
#include <set>
namespace mesh_reorder61_oracle {
using Full=mesh_attribute_reorder61::Registers;
constexpr GuestAddress Owner=0x30000,Mesh=0x31000,Positions=0x32000,Half76=0x33000,Ids=0x34000,Half32=0x35000,Categories=0x36000,Indices=0x37000,Service=0x38000,Table=0x39000;
constexpr GuestAddress Allocate=0x2a00,Release=0x2a04;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x832df000u,0x1000u}}};
struct Guest final:mesh_attribute_reorder61::GuestServices {
 unsigned allocations=0;std::set<GuestAddress> live;std::vector<std::array<std::uint64_t,73>> events;
 void CallIndirect(GuestAddress target,GuestMemory&,Full& s)override{
  std::array<std::uint64_t,73> e{};auto snap=crt_full_oracle::Snapshot(s);std::copy(snap.begin(),snap.end(),e.begin());e[72]=target;events.push_back(e);
  if(target==Allocate){if(s.r[4]>4096u)throw std::runtime_error("unexpected mesh reorder allocation size");const auto p=0x50000u+0x1000u*allocations++;live.insert(p);s.r[3]=p;}
  else if(target==Release){if(!live.erase(Address(s.r[4])))throw std::runtime_error("mesh reorder released unowned buffer");s.r[3]=0x1234567800000001ull;}
  else throw std::runtime_error("unexpected mesh reorder service target");
  s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[3]^=0x100u;s.cr7.lt^=1u;
 }
};
Guest* original=nullptr;GuestMemory* original_memory=nullptr;
constexpr std::array<unsigned,4> Gather{2,0,2,1};
void Check(unsigned mode){
 test::GuestWindow before(Regions),after(Regions);
 const auto seed=[&](test::GuestWindow& w){w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Owner,Mesh);m.WriteU32(Mesh+4,mode?4:0);m.WriteU32(Mesh+12,Positions);m.WriteU32(Mesh+76,mode==1?Half76:0);m.WriteU32(Mesh+80,mode==1?Ids:0);m.WriteU32(Mesh+32,Half32);m.WriteU32(Mesh+36,Categories);m.WriteU32(Mesh+28,mode==2?300u:128u);
  for(unsigned i=0;i<4;++i){m.WriteU32(Indices+4*i,Gather[i]);for(unsigned j=0;j<3;++j)m.WriteU32(Positions+12*i+4*j,0x3f800000u+i*0x10000u+j*0x1000u);
   m.WriteU16(Half76+2*i,std::uint16_t(100+i));m.WriteU16(Half32+2*i,std::uint16_t(200+i));m.WriteU32(Ids+4*i,1000+i);
   if(mode==2)m.WriteU16(Categories+2*i,std::uint16_t(260+i));else m.WriteU8(Categories+i,std::uint8_t(10+i));}
  m.WriteU32(0x832df548u,Service);m.WriteU32(Service,Table);m.WriteU32(Table+8,Allocate|1u);m.WriteU32(Table+20,Release|3u);
 };seed(before);seed(after);Guest expected,actual;expected.live=actual.live={Positions,Half32,Categories};if(mode==1){expected.live.insert(Half76);expected.live.insert(Ids);actual.live=expected.live;}
 Full s{};for(unsigned i=0;i<32;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
 s.r[1]=0x8877665500080000ull;s.r[3]=Owner;s.r[4]=Indices;s.lr=0x9988776681234567ull;s.xer_so=1u;
 const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);auto old_memory=before.Memory();original=&expected;original_memory=&old_memory;__imp__sub_82BB3CF8(c,before.Bytes());original=nullptr;original_memory=nullptr;
 auto m=after.Memory();if(!mesh_attribute_reorder61::Apply(0x82bb3cf8u,m,actual,s))throw std::runtime_error("missing mesh reorder entry");
 auto a=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),b=crt_full_oracle::Snapshot(s);
 if(a!=b||!before.EqualCommitted(after)||expected.events!=actual.events||expected.live!=actual.live){
  std::fprintf(stderr,"mesh mode%u Full%d RAM%d events%d ownership%d\n",mode,a==b,before.EqualCommitted(after),expected.events==actual.events,expected.live==actual.live);
  for(unsigned i=0;i<a.size();++i)if(a[i]!=b[i])std::fprintf(stderr,"Full[%u] %llx/%llx\n",i,(unsigned long long)a[i],(unsigned long long)b[i]);
  for(auto region:Regions)for(std::size_t i=0;i<region.size;++i)if(before.Bytes()[region.base+i]!=after.Bytes()[region.base+i]){std::fprintf(stderr,"RAM %08llx %02x/%02x\n",(unsigned long long)(region.base+i),before.Bytes()[region.base+i],after.Bytes()[region.base+i]);break;}
  for(std::size_t i=0;i<std::min(expected.events.size(),actual.events.size());++i)if(expected.events[i]!=actual.events[i]){for(unsigned j=0;j<73;++j)if(expected.events[i][j]!=actual.events[i][j])std::fprintf(stderr,"event%zu field%u %llx/%llx\n",i,j,(unsigned long long)expected.events[i][j],(unsigned long long)actual.events[i][j]);break;}
  throw std::runtime_error("mesh reorder state mismatch");}
 if(s.r[1]!=initial.r[1]||s.lr!=Address(initial.lr)||actual.allocations!=(mode==1?5u:mode==2?4u:0u))throw std::runtime_error("mesh reorder frame/allocation count mismatch");
 if(mode){for(unsigned i=0;i<4;++i){const auto from=Gather[i];for(unsigned j=0;j<3;++j)if(m.ReadU32(m.ReadU32(Mesh+12)+12*i+4*j)!=0x3f800000u+from*0x10000u+j*0x1000u)throw std::runtime_error("mesh position gather mismatch");
  if(m.ReadU32(m.ReadU32(Mesh+80)+4*i)!=(mode==1?1000u+from:from)||m.ReadU16(m.ReadU32(Mesh+32)+2*i)!=200u+from)throw std::runtime_error("mesh identity/half gather mismatch");
  if(mode==1&&(m.ReadU16(m.ReadU32(Mesh+76)+2*i)!=100u+from||m.ReadU8(m.ReadU32(Mesh+36)+i)!=10u+from))throw std::runtime_error("mesh byte attribute gather mismatch");
  if(mode==2&&m.ReadU16(m.ReadU32(Mesh+36)+2*i)!=260u+from)throw std::runtime_error("mesh wide category gather mismatch");}}
 }
}
void OriginalMeshReorder61Indirect(std::uint32_t target,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);mesh_reorder61_oracle::original->CallIndirect(target,*mesh_reorder61_oracle::original_memory,s);crt_full_oracle::ToPpc(c,s);}
void OriginalMeshReorder61Save(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*mesh_reorder61_oracle::original_memory;for(unsigned i=first;i<32;++i)WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));}
void OriginalMeshReorder61Restore(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*mesh_reorder61_oracle::original_memory;for(unsigned i=first;i<32;++i)s.r[i]=ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);}
int main(){try{for(unsigned mode:{0u,1u,2u})mesh_reorder61_oracle::Check(mode);std::puts("PASS mesh-attribute-reorder61 3 original-body cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
