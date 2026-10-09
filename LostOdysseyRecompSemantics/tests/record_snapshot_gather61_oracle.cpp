#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/record_snapshot_gather61.h"
namespace snapshot_gather61_oracle {
using Full=record_snapshot_gather61::Registers;
constexpr GuestAddress Descriptor=0x30000,Records=0x31000,Indices=0x32000,Service=0x33000,Table=0x34000,Temporary=0x50000;
constexpr GuestAddress Allocate=0x2a00,Release=0x2a04,Approve=0x2a08;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x832df000u,0x1000u}}};
constexpr std::array<unsigned,4> Gather{2,0,2,1};
struct Guest final:record_snapshot_gather61::GuestServices {
 unsigned allocations=0,releases=0;bool live=false;unsigned mode=0;std::vector<std::array<std::uint64_t,73>> events;
 void CallIndirect(GuestAddress target,GuestMemory&,Full& s)override{
  std::array<std::uint64_t,73> e{};auto snap=crt_full_oracle::Snapshot(s);std::copy(snap.begin(),snap.end(),e.begin());e[72]=target;events.push_back(e);
  if(target==Allocate){if(s.r[4]!=52||s.r[5]!=1||live)throw std::runtime_error("unexpected snapshot allocation");++allocations;live=true;s.r[3]=Temporary;}
  else if(target==Release){if(!live||s.r[4]!=Temporary)throw std::runtime_error("unexpected snapshot release");++releases;live=false;s.r[3]=0x1234567800000001ull;}
  else if(target==Approve){if(s.r[3]!=4||s.r[4]!=Indices||s.r[5]!=0x2468u)throw std::runtime_error("unexpected gather approval arguments");s.r[3]=mode==1?0x100u:1u;}
  else throw std::runtime_error("unexpected snapshot service");
  s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[3]^=0x100u;s.cr7.lt^=1u;
 }
};
Guest* original=nullptr;GuestMemory* original_memory=nullptr;
void Check(unsigned mode){
 test::GuestWindow before(Regions),after(Regions);
 const auto seed=[](test::GuestWindow& w){w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Descriptor,Approve|1u);m.WriteU32(Descriptor+4,0x2468);m.WriteU32(Descriptor+8,4);m.WriteU32(Descriptor+16,Records);
  for(unsigned i=0;i<4;++i){m.WriteU32(Indices+4*i,Gather[i]);for(unsigned j=0;j<3;++j)m.WriteU32(Records+12*i+4*j,0x3f800000u+i*0x10000u+j*0x1000u);}
  m.WriteU32(0x832df554u,Service);m.WriteU32(Service,Table);m.WriteU32(Table,Allocate|1u);m.WriteU32(Table+12,Release|3u);
 };seed(before);seed(after);Guest expected,actual;expected.mode=actual.mode=mode;
 Full s{};for(unsigned i=0;i<32;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
 s.r[1]=0x8877665500080000ull;s.r[3]=Descriptor;s.r[4]=mode==2?3:4;s.r[5]=Indices;s.lr=0x9988776681234567ull;s.xer_so=1;
 const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);auto old_memory=before.Memory();original=&expected;original_memory=&old_memory;__imp__sub_82BD1900(c,before.Bytes());original=nullptr;original_memory=nullptr;
 auto m=after.Memory();if(!record_snapshot_gather61::Apply(0x82bd1900u,m,actual,s))throw std::runtime_error("missing snapshot gather entry");
 auto a=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),b=crt_full_oracle::Snapshot(s);
 if(a!=b||!before.EqualCommitted(after)||expected.events!=actual.events||expected.live!=actual.live){
  std::fprintf(stderr,"snapshot mode%u Full%d RAM%d events%d ownership%d\n",mode,a==b,before.EqualCommitted(after),expected.events==actual.events,expected.live==actual.live);
  for(unsigned i=0;i<a.size();++i)if(a[i]!=b[i])std::fprintf(stderr,"Full[%u] %llx/%llx\n",i,(unsigned long long)a[i],(unsigned long long)b[i]);
  for(std::size_t i=0;i<std::min(expected.events.size(),actual.events.size());++i)if(expected.events[i]!=actual.events[i]){for(unsigned j=0;j<73;++j)if(expected.events[i][j]!=actual.events[i][j])std::fprintf(stderr,"event%zu field%u %llx/%llx\n",i,j,(unsigned long long)expected.events[i][j],(unsigned long long)actual.events[i][j]);break;}
  throw std::runtime_error("snapshot gather state mismatch");}
 if(s.r[1]!=initial.r[1]||s.lr!=Address(initial.lr)||s.r[3]!=(mode==2?0u:1u)||actual.allocations!=(mode==0?1u:0u)||actual.releases!=actual.allocations||actual.live)throw std::runtime_error("snapshot ownership/frame/result mismatch");
 for(unsigned i=0;i<4;++i)for(unsigned j=0;j<3;++j)if(m.ReadU32(Records+12*i+4*j)!=0x3f800000u+(mode==0?Gather[i]:i)*0x10000u+j*0x1000u)throw std::runtime_error("snapshot gathered record mismatch");
 if(mode==0&&m.ReadU32(Temporary)!=4)throw std::runtime_error("snapshot count prefix mismatch");
}
}
void OriginalSnapshotGather61Indirect(std::uint32_t target,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);snapshot_gather61_oracle::original->CallIndirect(target,*snapshot_gather61_oracle::original_memory,s);crt_full_oracle::ToPpc(c,s);}
void OriginalSnapshotGather61Allocator(PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,*snapshot_gather61_oracle::original_memory,*snapshot_gather61_oracle::original,s);crt_full_oracle::ToPpc(c,s);}
void OriginalSnapshotGather61Save(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*snapshot_gather61_oracle::original_memory;for(unsigned i=first;i<32;++i)WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));}
void OriginalSnapshotGather61Restore(unsigned first,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*snapshot_gather61_oracle::original_memory;for(unsigned i=first;i<32;++i)s.r[i]=ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);}
int main(){try{for(unsigned mode:{0u,1u,2u})snapshot_gather61_oracle::Check(mode);std::puts("PASS record-snapshot-gather61 3 original-upper/shared-allocator cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
