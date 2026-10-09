// Original BB2638 upper with complete shared semantic lowers; no lower-result
// stubs. Small single-triangle source closes the entire ordinary build route.
#include "object_sort_engine61_oracle_fixture.h"
#include "lo_semantics/grid_transform_pipeline61.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/grid_storage_initialize61.h"
#include "lo_semantics/grid_neighbor_update61.h"
#include "lo_semantics/grid_transform_routes61.h"
#include "lo_semantics/grid_transform_support61.h"
#include "lo_semantics/object_grid_transform61.h"
namespace pipeline61_oracle {
using Full=grid_transform_pipeline61::Registers;using VectorState=grid_transform_pipeline61::VectorState;
using Machine=diagnostic_lock61::MachineState;
constexpr GuestAddress Grid=0x30000,Source=0x31000,Tree=0x32000,Mesh=0x33000,Indices=0x34000,Vertices=0x35000;
constexpr GuestAddress Writer=0x36000,Node=0x37000,Data=0x38000,Table=0x39000,Global=0x3a000;
constexpr GuestAddress Tls=0x40000,ThreadRecord=0x41000,RngRecord=0x42000,Getter=0x2a10;
constexpr GuestAddress Allocate=0x2a00,Free=0x2a04;
constexpr std::array<test::Region,9> Regions{{{0,0x120000},{0x82000000,0x10000},{0x82010000,0x10000},
 {0x82030000,0x10000},{0x820a0000,0x10000},{0x820d6000,0x1000},{0x821ba000,0x1000},
 {0x83214000,0x3000},{0x832d3000,0xd000}}};
struct ServicesCase final:manager_release_context61::GuestServices,grid_transform_pipeline61::GuestServices,
 geometry_unbounded_range61::GuestServices,
 crt_close_block_output_context::ErrorOutputServices,crt_narrow_formatter61::GuestServices,
 diagnostic_lock61::SynchronizationServices,crt_random_thread61::Services,
 heap_allocation_context::BoundaryServices,crt_record_allocation_context::HandlerServices,crt_free_context::LowerCalls {
 std::vector<std::array<std::uint64_t,73>> events;unsigned allocations=0;
 [[noreturn]] static void Unexpected(){throw std::runtime_error("unexpected pipeline service");}
 void Trace(GuestAddress target,Full& s){std::array<std::uint64_t,73> e{};auto snap=crt_full_oracle::Snapshot(s);std::copy(snap.begin(),snap.end(),e.begin());e[72]=target;events.push_back(e);}
 void CallIndirect(GuestAddress target,GuestMemory&,Full& s)override{
  Trace(target,s);if(target==Allocate)s.r[3]=0x90000u+0x1000u*allocations++;
  else if(target==Free)s.r[3]=0;else if(target==Getter)s.r[3]=RngRecord;else Unexpected();
  s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[7]^=0x100u;s.cr7.lt^=1u;
 }
 void CallIndirect(GuestAddress t,GuestMemory& m,Full& s,VectorState& v)override {CallIndirect(t,m,s);v.v[8][1]^=0x100u;}
 void CallIndirect(GuestAddress t,GuestMemory& m,Full& s,VectorState& v,Machine& extra)override{
  CallIndirect(t,m,s,v);extra.msr^=0x10000000u;extra.reserved_bits^=0x0100000000000000ull;}
 void CallDirect(GuestAddress,GuestMemory&,Full&)override{Unexpected();}
 void CallDirect(GuestAddress,GuestMemory&,raw_allocation_context::Registers&)override{Unexpected();}
 void CallNative(GuestAddress,GuestMemory&,raw_allocation_context::Registers&)override{Unexpected();}
 void CallNewHandler(GuestAddress,GuestMemory&,Full&)override{Unexpected();}
 void Call(GuestAddress,GuestMemory&,crt_free_context::Registers&)override{Unexpected();}
 void CallOutput(GuestMemory&,Full&)override{Unexpected();}
 void KeTlsGetValue(GuestMemory&,Full& s)override{Trace(0x830da144u,s);s.r[3]=Getter|1u;}
 void KeTlsSetValue(GuestMemory&,Full&)override{Unexpected();}
 void FatalRuntimeError(GuestMemory&,Full&)override{Unexpected();}
 std::uint32_t LoadReservedWord(GuestAddress,GuestMemory&)override{Unexpected();}
 bool CompareExchangeWord(GuestAddress,std::uint32_t,std::uint32_t,GuestMemory&)override{Unexpected();}
 void EnterCriticalSection(GuestMemory&,Full&,Machine&)override{Unexpected();}
 void LeaveCriticalSection(GuestMemory&,Full&,Machine&)override{Unexpected();}
};
struct Environment {
 sort_engine61_oracle::Environment base;ServicesCase services;Machine machine{0x020a8020u,0xcafebabe11223344ull};
 explicit Environment(test::GuestWindow& w):base(w){}
 grid_transform_pipeline61::Dependencies Deps(){auto accepted=base.Deps().sort.accepted;
  crt_close_reader_callers_context::Dependencies reader{services,accepted};
  object_sort_dispatch61::Dependencies writer{{reader,base.fp},services};
  transform_owner_build61::Dependencies spatial{{services,base.fp},{{accepted,services},services,machine}};
  object_grid_probe61::Dependencies probe{{services,base.fp},{{Dependencies(base.stream),services,services},services,services}};
  return{{reader,base.fp},writer,spatial,probe,services};
 }
};
Environment* original=nullptr;
std::array<std::uint32_t*,128> VectorPointers(PPCContext& c){return {{&c.v0.u32[0],&c.v1.u32[0],&c.v2.u32[0],&c.v3.u32[0],&c.v4.u32[0],&c.v5.u32[0],&c.v6.u32[0],&c.v7.u32[0],&c.v8.u32[0],&c.v9.u32[0],&c.v10.u32[0],&c.v11.u32[0],&c.v12.u32[0],&c.v13.u32[0],&c.v14.u32[0],&c.v15.u32[0],&c.v16.u32[0],&c.v17.u32[0],&c.v18.u32[0],&c.v19.u32[0],&c.v20.u32[0],&c.v21.u32[0],&c.v22.u32[0],&c.v23.u32[0],&c.v24.u32[0],&c.v25.u32[0],&c.v26.u32[0],&c.v27.u32[0],&c.v28.u32[0],&c.v29.u32[0],&c.v30.u32[0],&c.v31.u32[0],&c.v32.u32[0],&c.v33.u32[0],&c.v34.u32[0],&c.v35.u32[0],&c.v36.u32[0],&c.v37.u32[0],&c.v38.u32[0],&c.v39.u32[0],&c.v40.u32[0],&c.v41.u32[0],&c.v42.u32[0],&c.v43.u32[0],&c.v44.u32[0],&c.v45.u32[0],&c.v46.u32[0],&c.v47.u32[0],&c.v48.u32[0],&c.v49.u32[0],&c.v50.u32[0],&c.v51.u32[0],&c.v52.u32[0],&c.v53.u32[0],&c.v54.u32[0],&c.v55.u32[0],&c.v56.u32[0],&c.v57.u32[0],&c.v58.u32[0],&c.v59.u32[0],&c.v60.u32[0],&c.v61.u32[0],&c.v62.u32[0],&c.v63.u32[0],&c.v64.u32[0],&c.v65.u32[0],&c.v66.u32[0],&c.v67.u32[0],&c.v68.u32[0],&c.v69.u32[0],&c.v70.u32[0],&c.v71.u32[0],&c.v72.u32[0],&c.v73.u32[0],&c.v74.u32[0],&c.v75.u32[0],&c.v76.u32[0],&c.v77.u32[0],&c.v78.u32[0],&c.v79.u32[0],&c.v80.u32[0],&c.v81.u32[0],&c.v82.u32[0],&c.v83.u32[0],&c.v84.u32[0],&c.v85.u32[0],&c.v86.u32[0],&c.v87.u32[0],&c.v88.u32[0],&c.v89.u32[0],&c.v90.u32[0],&c.v91.u32[0],&c.v92.u32[0],&c.v93.u32[0],&c.v94.u32[0],&c.v95.u32[0],&c.v96.u32[0],&c.v97.u32[0],&c.v98.u32[0],&c.v99.u32[0],&c.v100.u32[0],&c.v101.u32[0],&c.v102.u32[0],&c.v103.u32[0],&c.v104.u32[0],&c.v105.u32[0],&c.v106.u32[0],&c.v107.u32[0],&c.v108.u32[0],&c.v109.u32[0],&c.v110.u32[0],&c.v111.u32[0],&c.v112.u32[0],&c.v113.u32[0],&c.v114.u32[0],&c.v115.u32[0],&c.v116.u32[0],&c.v117.u32[0],&c.v118.u32[0],&c.v119.u32[0],&c.v120.u32[0],&c.v121.u32[0],&c.v122.u32[0],&c.v123.u32[0],&c.v124.u32[0],&c.v125.u32[0],&c.v126.u32[0],&c.v127.u32[0]}};}
VectorState FromVectors(PPCContext& c){VectorState v;auto p=VectorPointers(c);for(unsigned i=0;i<128;++i)for(unsigned j=0;j<4;++j)v.v[i][j]=p[i][j];return v;}
void ToVectors(PPCContext& c,const VectorState& v){auto p=VectorPointers(c);for(unsigned i=0;i<128;++i)for(unsigned j=0;j<4;++j)p[i][j]=v.v[i][j];}
void Float(GuestMemory& m,GuestAddress p,float v){m.WriteU32(p,std::bit_cast<std::uint32_t>(v));}
void Seed(test::GuestWindow& w,unsigned dimension){
 w.Fill(0);auto m=w.Memory();m.WriteU32(Source+4,3);m.WriteU32(Source+8,1);m.WriteU32(Source+12,Vertices);m.WriteU32(Source+16,Indices);
 m.WriteU32(Source+48,Tree);m.WriteU32(Tree+4,Mesh);m.WriteU32(Tree+8,4);
 m.WriteU32(Mesh+8,1);m.WriteU32(Mesh+16,Indices);m.WriteU32(Mesh+20,Vertices);
 for(unsigned i=0;i<3;++i){m.WriteU32(Indices+4*i,i);Float(m,Source+128+4*i,-1.f);Float(m,Source+140+4*i,1.f);}
 constexpr std::array<float,9> vertex{0,0,0,1,0,0,0,1,0};for(unsigned i=0;i<9;++i)Float(m,Vertices+4*i,vertex[i]);
 m.WriteU32(Writer,Node);m.WriteU32(Writer+16,Data);m.WriteU32(Node,Data);m.WriteU32(Node+8,4096);
 m.WriteU32(0x83216624,Table);m.WriteU32(Table,Allocate|1u);m.WriteU32(Table+12,Free|3u);
 m.WriteU32(0x832df548,Global);m.WriteU32(Global,Table+32);m.WriteU32(Table+40,Allocate|1u);m.WriteU32(Table+52,Free|3u);
 Float(m,0x82000b40,1.f/32767.f);Float(m,0x82000e0c,std::bit_cast<float>(0x7f7fffffu));Float(m,0x82000e50,0.f);
 Float(m,0x82007784,1.f);Float(m,0x8201f9f0,.5f);Float(m,0x82035cb8,-.000001f);Float(m,0x820a6b8c,.000001f);Float(m,0x821baa74,2.f);
 WriteU64(m,0x820029c0,std::bit_cast<std::uint64_t>(4503599627370496.0));WriteU64(m,0x82000f28,std::bit_cast<std::uint64_t>(1.0));
 m.WriteU32(Tls+256,ThreadRecord);m.WriteU32(ThreadRecord+352,0x12345678);m.WriteU32(0x83214d74,7);m.WriteU32(0x83214d78,9);m.WriteU32(0x832d3adc,Getter|1u);m.WriteU32(RngRecord+20,1);
 // The original visit constructor leaves distance+44 untouched; provide the
 // same finite incoming guest-frame contents to both executions.
 Float(m,0x80000-640+412,dimension==4?1.5f:.25f);
}
void Lower(GuestAddress entry,Environment& env,Full& s,VectorState& v){
 auto& m=env.base.stream.memory;auto d=env.Deps();switch(entry){
 case 0x82bd09c0u:case 0x82bd09f8u:(void)crt_reader_units61::Apply(entry,m,env.services,s);break;
 case 0x82b9c298u:case 0x82bd18c0u:(void)diagnostic_format_routes61::Apply(entry,m,d.spatial.diagnostics,s);break;
 case 0x82bb1e58u:(void)grid_storage_initialize61::Apply(entry,m,{env.services,env.base.fp},s);break;
 case 0x82bb2098u:(void)object_grid_read61::Apply(entry,m,d.read,s);break;
 case 0x82bb23c0u:(void)grid_neighbor_update61::Apply(entry,m,s);break;
 case 0x82bd7950u:(void)grid_transform_routes61::Apply(entry,m,s);break;
 case 0x82bd7a60u:(void)transform_owner_build61::Apply(entry,m,d.spatial,s);break;
 case 0x82bd7a20u:(void)transform_owner_routes61::Apply(entry,m,d.spatial.owner,s);break;
 case 0x822d3068u:break;
 case 0x82b7bc40u:crt_reader_chain61::ApplySupport_B7BC40(m,d.write.engine.sort.accepted,s);break;
 case 0x82bb03b0u:(void)object_grid_probe61::Apply(entry,m,d.probe,s,v);break;
 case 0x82bb06d8u:(void)object_grid_transform61::Apply(entry,m,env.base.fp,s);break;
 case 0x82bafec0u:(void)object_sort_dispatch61::Apply(entry,m,d.write,s);break;
 default:if(!grid_transform_support61::Apply(entry,m,env.base.fp,s))throw std::runtime_error("unexpected pipeline lower");break;
 }}
void Check(unsigned dimension){
 sort_engine61_oracle::RestoreHost restore;test::GuestWindow before(Regions),after(Regions);Seed(before,dimension);Seed(after,dimension);
 Environment expected(before),actual(after);auto s=sort_engine61_oracle::Initial(dimension);s.r[3]=Grid;s.r[4]=dimension?Source:0;s.r[5]=dimension;
 s.r[6]=0;s.r[7]=Writer;s.r[8]=0;s.r[9]=0;s.r[13]=Tls;VectorState v;for(unsigned i=0;i<128;++i)for(unsigned j=0;j<4;++j)v.v[i][j]=0x3f800000+i*16+j;
 const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);ToVectors(c,v);c.msr=expected.machine.msr;c.reserved.u64=expected.machine.reserved_bits;
 PPCFPSCRRegister{}.setcsr(s.cached_fp_control);original=&expected;__imp__sub_82BB2638(c,before.Bytes());original=nullptr;const auto expected_host=PPCFPSCRRegister{}.getcsr();
 auto m=after.Memory();PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);if(!grid_transform_pipeline61::Apply(0x82bb2638u,m,actual.Deps(),s,v))throw std::runtime_error("missing pipeline");
 auto a=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),b=crt_full_oracle::Snapshot(s);
 if(a!=b||FromVectors(c).v!=v.v||!before.EqualCommitted(after)||expected.services.events!=actual.services.events||c.msr!=actual.machine.msr||c.reserved.u64!=actual.machine.reserved_bits||expected_host!=PPCFPSCRRegister{}.getcsr()){
  std::fprintf(stderr,"dimension%u compare scalar=%d vector=%d ram=%d events=%d msr=%d reserved=%d host=%d (%x/%x) eventcount=%zu/%zu\n",dimension,a==b,FromVectors(c).v==v.v,before.EqualCommitted(after),expected.services.events==actual.services.events,c.msr==actual.machine.msr,c.reserved.u64==actual.machine.reserved_bits,expected_host==PPCFPSCRRegister{}.getcsr(),expected_host,PPCFPSCRRegister{}.getcsr(),expected.services.events.size(),actual.services.events.size());
  const auto common=std::min(expected.services.events.size(),actual.services.events.size());
  unsigned shown=0;for(std::size_t event=0;event<common&&shown<8u;++event)if(expected.services.events[event]!=actual.services.events[event]){++shown;
   for(unsigned j=0;j<73;++j)if(expected.services.events[event][j]!=actual.services.events[event][j])std::fprintf(stderr,"dimension%u event%zu field%u %llx/%llx\n",dimension,event,j,(unsigned long long)expected.services.events[event][j],(unsigned long long)actual.services.events[event][j]);}
  for(unsigned i=0;i<a.size();++i)if(a[i]!=b[i])std::fprintf(stderr,"dimension%u Full[%u] %llx/%llx\n",dimension,i,(unsigned long long)a[i],(unsigned long long)b[i]);
  for(auto region:Regions)for(std::size_t i=0;i<region.size;++i)if(before.Bytes()[region.base+i]!=after.Bytes()[region.base+i]){std::fprintf(stderr,"dimension%u RAM %08llx %02x/%02x\n",dimension,(unsigned long long)(region.base+i),before.Bytes()[region.base+i],after.Bytes()[region.base+i]);break;}
  throw std::runtime_error("pipeline Full/vector/machine/RAM/callback/host mismatch");}
 if(s.r[3]!=(dimension?1u:0u))throw std::runtime_error("pipeline result mismatch");
}
}
void OriginalGridPipeline61Lower(std::uint32_t entry,PPCContext& c,std::uint8_t*){
 auto& env=*pipeline61_oracle::original;auto s=crt_full_oracle::FromPpc(c);auto v=pipeline61_oracle::FromVectors(c);env.machine={c.msr,c.reserved.u64};
 pipeline61_oracle::Lower(entry,env,s,v);crt_full_oracle::ToPpc(c,s);pipeline61_oracle::ToVectors(c,v);c.msr=env.machine.msr;c.reserved.u64=env.machine.reserved_bits;
}
void OriginalGridPipeline61Indirect(std::uint32_t target,PPCContext& c,std::uint8_t*){
 auto& env=*pipeline61_oracle::original;auto s=crt_full_oracle::FromPpc(c);auto v=pipeline61_oracle::FromVectors(c);env.machine={c.msr,c.reserved.u64};
 env.services.CallIndirect(target,env.base.stream.memory,s,v,env.machine);crt_full_oracle::ToPpc(c,s);pipeline61_oracle::ToVectors(c,v);c.msr=env.machine.msr;c.reserved.u64=env.machine.reserved_bits;
}
void OriginalGridPipeline61Save(unsigned first,PPCContext& c,std::uint8_t*){
 auto s=crt_full_oracle::FromPpc(c);auto& m=pipeline61_oracle::original->base.stream.memory;
 for(unsigned i=first;i<32;++i)WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
}
void OriginalGridPipeline61Restore(unsigned first,PPCContext& c,std::uint8_t*){
 auto s=crt_full_oracle::FromPpc(c);auto& m=pipeline61_oracle::original->base.stream.memory;
 for(unsigned i=first;i<32;++i)s.r[i]=ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);
}
void OriginalGridPipeline61SaveFloat(unsigned first,PPCContext& c,std::uint8_t*){
 auto s=crt_full_oracle::FromPpc(c);auto& m=pipeline61_oracle::original->base.stream.memory;
 for(unsigned i=first;i<32;++i)WriteU64(m,Address(s.r[12]-8u*(32u-i)),s.fpr_bits[i]);
}
void OriginalGridPipeline61RestoreFloat(unsigned first,PPCContext& c,std::uint8_t*){
 auto s=crt_full_oracle::FromPpc(c);auto& m=pipeline61_oracle::original->base.stream.memory;
 for(unsigned i=first;i<32;++i)s.fpr_bits[i]=ReadU64(m,Address(s.r[12]-8u*(32u-i)));crt_full_oracle::ToPpc(c,s);
}
int main(){try{for(unsigned dimension:{0u,2u,4u})pipeline61_oracle::Check(dimension);std::puts("PASS grid-transform-pipeline61 3 original-upper/shared-accepted-lower cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
