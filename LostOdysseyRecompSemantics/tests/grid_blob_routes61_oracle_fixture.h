// Original BB2638 upper with complete shared semantic lowers; no lower-result
// stubs. Small single-triangle source closes the entire ordinary build route.
#pragma once
#include "object_sort_engine61_oracle_fixture.h"
#include <atomic>
#include <mutex>
#include "lo_semantics/grid_transform_pipeline61.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/grid_storage_initialize61.h"
#include "lo_semantics/grid_neighbor_update61.h"
#include "lo_semantics/grid_transform_routes61.h"
#include "lo_semantics/grid_transform_support61.h"
#include "lo_semantics/object_grid_transform61.h"
namespace grid_blob61_fixture {
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
 test::GuestWindow* window=nullptr;std::recursive_mutex mutex;
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
 static std::uint32_t Swap(std::uint32_t v){return(v<<24)|((v<<8)&0xff0000u)|((v>>8)&0xff00u)|(v>>24);}
 std::uint32_t& Raw(GuestAddress a){return *reinterpret_cast<std::uint32_t*>(window->Bytes()+a);}
 std::uint32_t LoadReservedWord(GuestAddress a,GuestMemory&)override{return Swap(std::atomic_ref<std::uint32_t>(Raw(a)).load());}
 bool CompareExchangeWord(GuestAddress a,std::uint32_t e,std::uint32_t d,GuestMemory&)override{e=Swap(e);return std::atomic_ref<std::uint32_t>(Raw(a)).compare_exchange_strong(e,Swap(d));}
 void EnterCriticalSection(GuestMemory&,Full& s,Machine& machine)override{mutex.lock();Trace(0x822b29a0u,s);s.r[3]=0;machine.msr^=0x10000000u;}
 void LeaveCriticalSection(GuestMemory&,Full& s,Machine& machine)override{Trace(0x822b3438u,s);mutex.unlock();s.r[3]=0;machine.reserved_bits^=0x0100000000000000ull;}
};
struct Environment {
 sort_engine61_oracle::Environment base;ServicesCase services;Machine machine{0x020a8020u,0xcafebabe11223344ull};
 explicit Environment(test::GuestWindow& w):base(w){services.window=&w;}
 grid_transform_pipeline61::Dependencies Deps(){auto accepted=base.Deps().sort.accepted;
  crt_close_reader_callers_context::Dependencies reader{services,accepted};
  object_sort_dispatch61::Dependencies writer{{reader,base.fp},services};
  transform_owner_build61::Dependencies spatial{{services,base.fp},{{accepted,services},services,machine}};
  object_grid_probe61::Dependencies probe{{services,base.fp},{{Dependencies(base.stream),services,services},services,services}};
  return{{reader,base.fp},writer,spatial,probe,services};
 }
};

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
}
