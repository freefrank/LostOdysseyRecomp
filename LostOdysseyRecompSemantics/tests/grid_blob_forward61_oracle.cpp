#include "grid_blob_routes61_oracle_fixture.h"
#include "lo_semantics/grid_blob_forward61.h"
namespace grid_blob_forward61_oracle {
using namespace grid_blob61_fixture;
grid_blob61_fixture::Environment* original=nullptr;
void Check(unsigned dimension){
 sort_engine61_oracle::RestoreHost restore;test::GuestWindow before(grid_blob61_fixture::Regions),after(grid_blob61_fixture::Regions);
 grid_blob61_fixture::Seed(before,dimension);grid_blob61_fixture::Seed(after,dimension);
 for(auto* w:{&before,&after})w->Memory().WriteU32(0x47008u,Source);
 grid_blob61_fixture::Environment expected(before),actual(after);auto state=sort_engine61_oracle::Initial(dimension);
 state.r[3]=0xaabbccdddeadbeefull;state.r[4]=0x46000u;state.r[5]=0x47000u;state.r[6]=dimension;state.r[7]=0;state.r[13]=Tls;
 VectorState vectors;for(unsigned i=0;i<128;++i)for(unsigned j=0;j<4;++j)vectors.v[i][j]=0x3f800000u+i*16u+j;
 const auto initial=state;PPCContext c{};crt_full_oracle::ToPpc(c,state);ToVectors(c,vectors);c.msr=expected.machine.msr;c.reserved.u64=expected.machine.reserved_bits;
 PPCFPSCRRegister{}.setcsr(state.cached_fp_control);original=&expected;__imp__sub_82B9CBC0(c,before.Bytes());original=nullptr;auto host=PPCFPSCRRegister{}.getcsr();
 auto m=after.Memory();PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
 if(!grid_blob_forward61::Apply(0x82b9cbc0u,m,actual.Deps(),state,vectors)||
 crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(state)||FromVectors(c).v!=vectors.v||
 !before.EqualCommitted(after)||expected.services.events!=actual.services.events||c.msr!=actual.machine.msr||c.reserved.u64!=actual.machine.reserved_bits||host!=PPCFPSCRRegister{}.getcsr())
  throw std::runtime_error("blob forward Full72/vector/machine/RAM/callback/host mismatch");
 if(state.r[3]!=1u||state.r[1]!=initial.r[1]||state.lr!=Address(initial.lr)||m.ReadU32(0x46000u)<8u||m.ReadU32(m.ReadU32(0x46004u))!=0x504d4150u)
  throw std::runtime_error("blob forward output/tail result mismatch");
}
}
void OriginalGridBlobForward61Lower(PPCContext& c,std::uint8_t*){
 auto& env=*grid_blob_forward61_oracle::original;auto s=crt_full_oracle::FromPpc(c);auto v=grid_blob61_fixture::FromVectors(c);env.machine={c.msr,c.reserved.u64};
 if(!grid_blob_routes61::Apply(0x82ba60f8u,env.base.stream.memory,env.Deps(),s,v))throw std::runtime_error("missing accepted serializer");
 crt_full_oracle::ToPpc(c,s);grid_blob61_fixture::ToVectors(c,v);c.msr=env.machine.msr;c.reserved.u64=env.machine.reserved_bits;
}
int main(){try{grid_blob_forward61_oracle::Check(2);grid_blob_forward61_oracle::Check(4);std::puts("PASS grid-blob-forward61 2 original-upper/shared-accepted-lower cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
