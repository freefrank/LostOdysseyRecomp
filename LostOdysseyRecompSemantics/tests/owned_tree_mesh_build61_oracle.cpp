// Original upper and original BD1AF8 cleanup; shared accepted direct lowers.
// Mode 3 resolves actual vtable addresses through recovered geometry/strategy
// callbacks; only allocator services remain synthetic. Lower bodies are shared
// with original upper, so this is integration, not independent whole-chain proof.
// Bounds/split/strategy are explicit borrowed services, not stand-ins claimed
// as recovered concrete game vtable targets. Their finite fixture geometry
// exercises upper allocation, snapshot gather, leaf remap and cleanup order.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/owned_tree_mesh_build61.h"
#include "lo_semantics/owned_tree_reorder_support61.h"
#include "lo_semantics/grid_transform_buffer61.h"
#include "lo_semantics/owned_tree_construct61.h"
#include "lo_semantics/transform_owner_initialize61.h"
#include "lo_semantics/record_snapshot_gather61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/tree_mesh_callbacks61.h"
#include <set>
#include <limits>
namespace mesh_build61_oracle {
using Full=owned_tree_mesh_build61::Registers;
constexpr GuestAddress Owner=0x30000,Settings=0x31000,Source=0x32000,Triangles=0x33000,Vertices=0x34000,Table=0x35000;
constexpr GuestAddress Allocate=0x2a00,Release=0x2a04,TriangleBounds=0x2b00,BoxBounds=0x2b04,Split=0x2b08,Bind=0x2b0c;
constexpr std::array<test::Region,6> Regions{{{0,0x180000},{0x82000000,0x10000},{0x8201f000,0x1000},{0x820d6000,0x1000},{0x821ba000,0x1000},{0x83216000,0xca000}}};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
struct Guest final:manager_release_context61::GuestServices {
 unsigned allocations=0;std::set<GuestAddress> live;std::vector<std::array<std::uint64_t,73>> events;
 void CallDirect(GuestAddress,GuestMemory&,Full&)override{throw std::runtime_error("unexpected mesh builder direct boundary");}
 void CallIndirect(GuestAddress target,GuestMemory& m,Full& s)override{
  std::array<std::uint64_t,73> e{};auto snap=crt_full_oracle::Snapshot(s);std::copy(snap.begin(),snap.end(),e.begin());e[72]=target;events.push_back(e);
  Native fp;
  if(tree_mesh_callbacks61::Apply(target,m,{*this,fp},s))return;
  if(target==Allocate){if(s.r[4]>4096u)throw std::runtime_error("oversized mesh fixture allocation");auto p=0x90000u+0x1000u*allocations++;live.insert(p);s.r[3]=p;}
  else if(target==Release){if(!live.erase(Address(s.r[4])))throw std::runtime_error("mesh builder double/unowned release");s.r[3]=0;}
  else if(target==Split)s.r[3]=Address(s.r[5])>1?1:0;
  else if(target==TriangleBounds||target==BoxBounds){
   std::array<float,3> lo;lo.fill(std::numeric_limits<float>::infinity());std::array<float,3> hi;hi.fill(-std::numeric_limits<float>::infinity());
   const auto payload=m.ReadU32(Address(s.r[3])+72);
   for(unsigned i=0;i<Address(s.r[5]);++i){auto index=m.ReadU32(Address(s.r[4])+4*i);
    if(target==TriangleBounds){auto triangles=m.ReadU32(payload+16),vertices=m.ReadU32(payload+20);for(unsigned v=0;v<3;++v){auto vertex=m.ReadU32(triangles+index*12+v*4);for(unsigned axis=0;axis<3;++axis){float x=std::bit_cast<float>(m.ReadU32(vertices+12*vertex+4*axis));lo[axis]=std::min(lo[axis],x);hi[axis]=std::max(hi[axis],x);}}}
    else for(unsigned axis=0;axis<3;++axis){lo[axis]=std::min(lo[axis],std::bit_cast<float>(m.ReadU32(payload+24*index+4*axis)));hi[axis]=std::max(hi[axis],std::bit_cast<float>(m.ReadU32(payload+24*index+12+4*axis)));}
   }
   for(unsigned axis=0;axis<3;++axis){m.WriteU32(Address(s.r[6])+4*axis,std::bit_cast<std::uint32_t>(lo[axis]));m.WriteU32(Address(s.r[6])+12+4*axis,std::bit_cast<std::uint32_t>(hi[axis]));}
  }else if(target==Bind){const auto tree=Address(s.r[4]);m.WriteU32(Address(s.r[3])+4,m.ReadU32(tree+16));m.WriteU32(Address(s.r[3])+8,m.ReadU32(tree+20));s.r[3]=1;}
  else throw std::runtime_error("unexpected mesh builder indirect boundary");
  s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[7]^=0x100u;s.cr7.lt^=1u;
 }
};
Guest* original=nullptr;GuestMemory* original_memory=nullptr;Native fp;
void Lower(GuestAddress e,GuestMemory& m,Guest& guest,Full& s){switch(e){
 case 0x82bd0798u:(void)crt_close_recursive_buffer_context::Apply(e,m,guest,s);break;
 case 0x82bd17f0u:case 0x82bd1830u:case 0x82bdac60u:(void)grid_transform_buffer61::Apply(e,m,s);break;
 case 0x82bd20f0u:case 0x82bdb1c0u:case 0x82bdb208u:(void)owned_tree_reorder_support61::Apply(e,m,{guest,fp},s);break;
 case 0x82bdad18u:(void)owned_tree_construct61::Apply(e,m,{guest,fp},s);break;
 case 0x82bd1900u:(void)record_snapshot_gather61::Apply(e,m,guest,s);break;
 case 0x82b7a0b0u:(void)crt_copy_full_context::Apply(e,m,s);break;
 case 0x82bd2a08u:case 0x82bd2c08u:(void)object_sort_support61::Apply(e,m,{guest,fp},s);break;
 case 0x82bd12f8u:(void)transform_owner_initialize61::Apply(e,m,{guest,fp},s);break;
 case 0x82bdb260u:(void)manager_release_context61::Apply(e,m,{guest,fp},s);break;
 default:throw std::runtime_error("unexpected mesh builder lower");}}
void Check(unsigned mode){
 struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
 test::GuestWindow before(Regions),after(Regions);
 const auto seed=[&](test::GuestWindow& w){w.Fill(0);auto m=w.Memory();m.WriteU32(Settings,mode?Source:0);m.WriteU32(Settings+4,1);m.WriteU32(Settings+8,mode==3?1:16);m.WriteU32(Settings+16,0xffffffffu);m.WriteU8(Settings+26,1);m.WriteU8(Settings+27,1);
  m.WriteU32(Source+8,mode==3?9:mode==2?2:1);m.WriteU32(Source+12,1);m.WriteU32(Source+16,Triangles);m.WriteU32(Source+20,Vertices);
  for(unsigned i=0;i<(mode==3?27u:6u);++i){m.WriteU32(Triangles+4*i,i);for(unsigned j=0;j<3;++j)m.WriteU32(Vertices+12*i+4*j,std::bit_cast<std::uint32_t>(float(i*2+j)));}
  m.WriteU32(0x820d6c54+4,TriangleBounds|1);m.WriteU32(0x820d6c54+20,Split|3);m.WriteU32(0x820d6300+4,BoxBounds|1);m.WriteU32(0x820d6300+20,Split|3);m.WriteU32(0x820d6ebc+4,Bind|1);
  if(mode==3){
   constexpr std::array<GuestAddress,5> triangle{0x82bd88e8u,0x82bd8bf8u,0x82bd8ac0u,0x82bd8b40u,0x82bb3b88u};
   constexpr std::array<GuestAddress,5> boxes{0x82bd8ee0u,0x82bb3b60u,0x82bd8848u,0x82bd8888u,0x82bb3b88u};
   for(unsigned i=0;i<5;++i){m.WriteU32(0x820d6c58+4*i,triangle[i]);m.WriteU32(0x820d6304+4*i,boxes[i]);}
   m.WriteU32(0x820d6ec0,0x82bdbd90u);
   m.WriteU32(0x82000e0cu,std::bit_cast<std::uint32_t>(std::numeric_limits<float>::infinity()));
   m.WriteU32(0x82000d64u,std::bit_cast<std::uint32_t>(-std::numeric_limits<float>::infinity()));
   m.WriteU32(0x82000f20u,std::bit_cast<std::uint32_t>(1.f/3.f));
   m.WriteU32(0x8201f9f0u,std::bit_cast<std::uint32_t>(0.5f));
  }
  m.WriteU32(0x83216624,Table);m.WriteU32(Table,Allocate|1);m.WriteU32(Table+12,Release|3);m.WriteU32(0x821baa74,std::bit_cast<std::uint32_t>(2.f));
 };seed(before);seed(after);Guest expected,actual;Full s{};for(unsigned i=0;i<32;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
 s.r[1]=0x8877665500080000ull;s.r[3]=Owner;s.r[4]=Settings;s.lr=0x9988776681234567ull;s.xer_so=1;s.cached_fp_control=0x9fc0;
 const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);auto om=before.Memory();original=&expected;original_memory=&om;PPCFPSCRRegister{}.setcsr(s.cached_fp_control);__imp__sub_82BD22A8(c,before.Bytes());auto host=PPCFPSCRRegister{}.getcsr();original=nullptr;original_memory=nullptr;
 auto m=after.Memory();PPCFPSCRRegister{}.setcsr(s.cached_fp_control);if(!owned_tree_mesh_build61::Apply(0x82bd22a8u,m,{actual,fp},s))throw std::runtime_error("missing mesh builder");auto a=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),b=crt_full_oracle::Snapshot(s);
 if(a!=b||!before.EqualCommitted(after)||expected.events!=actual.events||expected.live!=actual.live||host!=PPCFPSCRRegister{}.getcsr()){
  std::fprintf(stderr,"mesh build mode%u Full%d RAM%d events%d ownership%d host%d\n",mode,a==b,before.EqualCommitted(after),expected.events==actual.events,expected.live==actual.live,host==PPCFPSCRRegister{}.getcsr());
  for(unsigned i=0;i<a.size();++i)if(a[i]!=b[i])std::fprintf(stderr,"Full[%u] %llx/%llx\n",i,(unsigned long long)a[i],(unsigned long long)b[i]);
  for(auto reg:Regions)for(std::size_t i=0;i<reg.size;++i)if(before.Bytes()[reg.base+i]!=after.Bytes()[reg.base+i]){std::fprintf(stderr,"RAM %08llx %02x/%02x\n",(unsigned long long)(reg.base+i),before.Bytes()[reg.base+i],after.Bytes()[reg.base+i]);break;}
  for(std::size_t i=0;i<std::min(expected.events.size(),actual.events.size());++i)if(expected.events[i]!=actual.events[i]){for(unsigned j=0;j<73;++j)if(expected.events[i][j]!=actual.events[i][j])std::fprintf(stderr,"event%zu field%u %llx/%llx\n",i,j,(unsigned long long)expected.events[i][j],(unsigned long long)actual.events[i][j]);break;}
  throw std::runtime_error("mesh builder state mismatch");}
 if(s.r[1]!=initial.r[1]||s.lr!=Address(initial.lr)||s.r[3]!=(mode?1u:0u))throw std::runtime_error("mesh builder frame/result");
 if(mode&&m.ReadU32(Owner+20)!=(mode>=2?2u:1u))throw std::runtime_error("mesh leaf count");
 // BD2168 packs (index-array byte offset << 2) with (count - 1).
 // These leaves each contain one uint32 index: offsets 0 and 4 bytes
 // therefore encode as 0 and 16, not ordinal leaf IDs 0 and 1.
 constexpr std::uint32_t firstPackedRange = 0u;
 constexpr std::uint32_t secondPackedRange = (sizeof(std::uint32_t) << 2u);
 if(mode==2){auto map=m.ReadU32(Owner+24);if(m.ReadU32(map)!=firstPackedRange||m.ReadU32(map+4)!=secondPackedRange||m.ReadU32(Owner+32)!=0||actual.live.size()!=5u){std::fprintf(stderr,"mesh map %u,%u preserved %08x live %zu\n",m.ReadU32(map),m.ReadU32(map+4),m.ReadU32(Owner+32),actual.live.size());throw std::runtime_error("mesh final map/owned storage");}}
 if(mode==3){
  const auto map=m.ReadU32(Owner+24), strategy=m.ReadU32(Owner+16), flat=m.ReadU32(strategy+8);
  const auto first=m.ReadU32(map),second=m.ReadU32(map+4);
  const auto countA=(first&15u)+1u,countB=(second&15u)+1u;
  if(countA+countB!=9u || (first>>4u)!=0u || (second>>4u)!=countA ||
     m.ReadU32(strategy+4)!=3u || !actual.live.contains(flat-4u))
   throw std::runtime_error("concrete mesh remap/flat owner");
  if(m.ReadU32(flat+24)!=1u || m.ReadU32(flat+28)!=2u || m.ReadU32(flat+32)!=2u)
   throw std::runtime_error("concrete flat root child indices");
 }

}
}
void OriginalMeshBuild61Indirect(std::uint32_t t,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);mesh_build61_oracle::original->CallIndirect(t,*mesh_build61_oracle::original_memory,s);crt_full_oracle::ToPpc(c,s);}
void OriginalMeshBuild61Lower(std::uint32_t t,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);mesh_build61_oracle::Lower(t,*mesh_build61_oracle::original_memory,*mesh_build61_oracle::original,s);crt_full_oracle::ToPpc(c,s);}
void OriginalMeshBuild61Save(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*mesh_build61_oracle::original_memory;for(unsigned i=first;i<32;++i)WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));}
void OriginalMeshBuild61Restore(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*mesh_build61_oracle::original_memory;for(unsigned i=first;i<32;++i)s.r[i]=ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);}
int main(){try{for(unsigned mode:{0u,1u,2u,3u})mesh_build61_oracle::Check(mode);std::puts("PASS owned-tree-mesh-build61 3 borrowed-service cases + 1 concrete nine-triangle integration case");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
