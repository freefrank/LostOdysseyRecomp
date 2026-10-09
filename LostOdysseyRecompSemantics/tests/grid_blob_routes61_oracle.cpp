// Two original uppers share accepted complete grid/reader/diagnostic lowers.
// Serialize an ordinary mesh, then deserialize the resulting actual blob.
#include "grid_blob_routes61_oracle_fixture.h"
#include "lo_semantics/grid_blob_routes61.h"
#include "lo_semantics/object_sort_lifecycle61.h"
#include "lo_semantics/crt_close_upper61.h"
#include "lo_semantics/crt_reader_cleanup_callers_context.h"
#include "lo_semantics/crt_close_next61.h"
#include "lo_semantics/crt_reader_object_chain61.h"
#include "lo_semantics/crt_copy_full_context.h"
namespace grid_blob61_oracle {
using namespace grid_blob61_fixture;
grid_blob61_fixture::Environment* original=nullptr;
constexpr GuestAddress Output=0x46000u,Wrapper=0x47000u,Input=0x48000u;
void Lower(GuestAddress entry,grid_blob61_fixture::Environment& env,Full& s,VectorState& v){auto& m=env.base.stream.memory;auto d=env.Deps();switch(entry){
 case 0x82bb25d0u:case 0x82bae1a0u:(void)object_sort_lifecycle61::Apply(entry,m,{env.services,env.base.fp},s);break;
 case 0x82bd0cd0u:(void)crt_close_upper61::Apply(entry,m,d.read.reader,s);break;
 case 0x82bd0a30u:(void)object_grid_read61::Apply(entry,m,d.read,s);break;
 case 0x82bb2638u:(void)grid_transform_pipeline61::Apply(entry,m,d,s,v);break;
 case 0x82b9c298u:(void)diagnostic_format_routes61::Apply(entry,m,d.spatial.diagnostics,s);break;
 case 0x82bd0df0u:(void)crt_reader_cleanup_callers_context::Apply(entry,m,env.services,s);break;
 case 0x82bd0900u:(void)crt_close_next61::Apply(entry,m,env.services,s);break;
 case 0x82bd0ea8u:(void)crt_reader_object_chain61::Apply(entry,m,d.read.reader,s);break;
 case 0x82b7a0b0u:(void)crt_copy_full_context::Apply(entry,m,s);break;
 default:throw std::runtime_error("unexpected blob lower");}}
void Check(){
 sort_engine61_oracle::RestoreHost restore;test::GuestWindow before(grid_blob61_fixture::Regions),after(grid_blob61_fixture::Regions);grid_blob61_fixture::Seed(before,2);grid_blob61_fixture::Seed(after,2);
 for(auto* w:{&before,&after}){auto m=w->Memory();m.WriteU32(Wrapper+8,Source);m.WriteU32(0x832df54c,0x44000u);m.WriteU32(0x44000u+52u,0x45000u);}
 grid_blob61_fixture::Environment expected(before),actual(after);VectorState vectors;for(unsigned i=0;i<128;++i)for(unsigned j=0;j<4;++j)vectors.v[i][j]=0x3f800000u+i*16u+j;
 for(unsigned pass=0;pass<2;++pass){
  auto state=sort_engine61_oracle::Initial(2);state.r[3]=pass?Source:Output;state.r[4]=pass?Input:Wrapper;state.r[5]=2;state.r[6]=0;state.r[13]=Tls;
  if(pass){for(auto* w:{&before,&after}){auto m=w->Memory();m.WriteU32(Input,m.ReadU32(Output));m.WriteU32(Input+4,m.ReadU32(Output+4));}}
  auto initial=state;PPCContext c{};crt_full_oracle::ToPpc(c,state);ToVectors(c,vectors);c.msr=expected.machine.msr;c.reserved.u64=expected.machine.reserved_bits;
  PPCFPSCRRegister{}.setcsr(state.cached_fp_control);original=&expected;
  if(pass)__imp__sub_82B9DD90(c,before.Bytes());else __imp__sub_82BA60F8(c,before.Bytes());
  original=nullptr;auto host=PPCFPSCRRegister{}.getcsr();auto m=after.Memory();PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
  const auto entry=pass?0x82b9dd90u:0x82ba60f8u;if(!grid_blob_routes61::Apply(entry,m,actual.Deps(),state,vectors))throw std::runtime_error("missing blob caller");
  auto a=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),b=crt_full_oracle::Snapshot(state);
  if(a!=b||FromVectors(c).v!=vectors.v||!before.EqualCommitted(after)||expected.services.events!=actual.services.events||c.msr!=actual.machine.msr||c.reserved.u64!=actual.machine.reserved_bits||host!=PPCFPSCRRegister{}.getcsr()){
   std::fprintf(stderr,"blob pass%u scalar%d vector%d RAM%d events%d machine%d host%d\n",pass,a==b,FromVectors(c).v==vectors.v,before.EqualCommitted(after),expected.services.events==actual.services.events,c.msr==actual.machine.msr&&c.reserved.u64==actual.machine.reserved_bits,host==PPCFPSCRRegister{}.getcsr());
   for(unsigned i=0;i<a.size();++i)if(a[i]!=b[i])std::fprintf(stderr,"Full[%u] %llx/%llx\n",i,(unsigned long long)a[i],(unsigned long long)b[i]);
   for(auto region:grid_blob61_fixture::Regions)for(std::size_t i=0;i<region.size;++i)if(before.Bytes()[region.base+i]!=after.Bytes()[region.base+i]){std::fprintf(stderr,"RAM %08llx %02x/%02x\n",(unsigned long long)(region.base+i),before.Bytes()[region.base+i],after.Bytes()[region.base+i]);break;}
   auto count=std::min(expected.services.events.size(),actual.services.events.size());for(std::size_t i=0;i<count;++i)if(expected.services.events[i]!=actual.services.events[i]){for(unsigned j=0;j<73;++j)if(expected.services.events[i][j]!=actual.services.events[i][j])std::fprintf(stderr,"event%zu field%u %llx/%llx\n",i,j,(unsigned long long)expected.services.events[i][j],(unsigned long long)actual.services.events[i][j]);break;}
   throw std::runtime_error("blob route full state mismatch");}
  if(state.r[3]!=1u)throw std::runtime_error("blob route unsuccessful");
  if(!pass){const auto bytes=m.ReadU32(Output);const auto data=m.ReadU32(Output+4);if(bytes<8u||m.ReadU32(data)!=0x504d4150u)throw std::runtime_error("missing serialized PMAP header");}
  else {const auto grid=m.ReadU32(Source+184);if(!grid||m.ReadU32(grid+88)!=2u||m.ReadU32(grid+104)!=8u)throw std::runtime_error("loaded grid shape mismatch");}
 }
}
}
void OriginalGridBlob61Lower(std::uint32_t entry,PPCContext& c,std::uint8_t*){
 auto& env=*grid_blob61_oracle::original;auto s=crt_full_oracle::FromPpc(c);auto v=grid_blob61_fixture::FromVectors(c);env.machine={c.msr,c.reserved.u64};grid_blob61_oracle::Lower(entry,env,s,v);
 crt_full_oracle::ToPpc(c,s);grid_blob61_fixture::ToVectors(c,v);c.msr=env.machine.msr;c.reserved.u64=env.machine.reserved_bits;
}
void OriginalGridBlob61Indirect(std::uint32_t target,PPCContext& c,std::uint8_t*){
 auto& env=*grid_blob61_oracle::original;auto s=crt_full_oracle::FromPpc(c);auto v=grid_blob61_fixture::FromVectors(c);env.machine={c.msr,c.reserved.u64};env.services.CallIndirect(target,env.base.stream.memory,s,v,env.machine);
 crt_full_oracle::ToPpc(c,s);grid_blob61_fixture::ToVectors(c,v);c.msr=env.machine.msr;c.reserved.u64=env.machine.reserved_bits;
}
void OriginalGridBlob61Save(unsigned first,PPCContext& c,std::uint8_t*){
 auto s=crt_full_oracle::FromPpc(c);auto& m=grid_blob61_oracle::original->base.stream.memory;
 for(unsigned i=first;i<32;++i)WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
}
void OriginalGridBlob61Restore(unsigned first,PPCContext& c,std::uint8_t*){
 auto s=crt_full_oracle::FromPpc(c);auto& m=grid_blob61_oracle::original->base.stream.memory;
 for(unsigned i=first;i<32;++i)s.r[i]=ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);
}
int main(){try{grid_blob61_oracle::Check();std::puts("PASS grid-blob-routes61 2 original-upper/shared-accepted-lower cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
