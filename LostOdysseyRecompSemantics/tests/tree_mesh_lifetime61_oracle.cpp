#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_mesh_lifetime61.h"
#include "lo_semantics/grid_transform_buffer61.h"
namespace mesh_lifetime_oracle {
using Registers=tree_mesh_lifetime61::Registers;
constexpr GuestAddress Owner=0x30000,Mesh=0x31000,Table=0x32000,Raw=0x33000,Map=0x34004,Options=0x35000;
constexpr GuestAddress Release=0x2a00,Build=0x2a04;
constexpr std::array<test::Region,2> Regions{{{0,0x120000},{0x83216000,0xca000}}};
struct Native final:float_triplet_transfer::NativeServices {void SetHostFpControl(std::uint32_t value)override{PPCFPSCRRegister{}.setcsr(value);}};
struct Guest final:manager_release_context61::GuestServices {
 std::vector<std::array<std::uint64_t,73>> events;std::vector<GuestAddress> freed;
 void CallDirect(GuestAddress,GuestMemory&,Registers&)override{throw std::runtime_error("unexpected lifetime direct boundary");}
 void CallIndirect(GuestAddress entry,GuestMemory& m,Registers& s)override{
  std::array<std::uint64_t,73> event{};auto snapshot=crt_full_oracle::Snapshot(s);std::copy(snapshot.begin(),snapshot.end(),event.begin());event.back()=entry;events.push_back(event);
  if(entry==Release)freed.push_back(Address(s.r[4]));
  else if(entry==Build){if(Address(s.r[3])!=Owner||s.r[4]!=Options||m.ReadU32(Owner+4)!=Mesh)throw std::runtime_error("mesh attachment callback");}
  else throw std::runtime_error("unexpected lifetime indirect boundary");
  s.r[3]=0x1234567800000001ull;s.r[8]^=0xaabbccddu;s.cr7.eq^=1u;
 }
};
Guest* guest=nullptr;GuestMemory* memory=nullptr;Native native;
void Lower(GuestAddress entry,GuestMemory& m,Guest& g,Registers& s){
 if(entry==0x82bd17f0u)(void)grid_transform_buffer61::Apply(entry,m,s);
 else if(entry==0x82bd20f0u)(void)owned_tree_reorder_support61::Apply(entry,m,{g,native},s);
 else if(entry==0x82bd1770u)(void)transform_owner_routes61::Apply(entry,m,{g,native},s);
 else if(entry==0x82bd0798u)(void)crt_close_recursive_buffer_context::Apply(entry,m,g,s);
 else throw std::runtime_error("unexpected lifetime lower");
}
void Check(unsigned mode){
 test::GuestWindow before(Regions),after(Regions);const auto seed=[&](test::GuestWindow& w){w.Fill(0);auto m=w.Memory();m.WriteU32(Owner,Table);m.WriteU32(Owner+24,Map);m.WriteU32(Owner+32,Raw);m.WriteU32(Table+28,Build|1);m.WriteU32(Table+12,Release|3);m.WriteU32(0x83216624,Table);for(unsigned i=8;i<=20;i+=4)m.WriteU32(Mesh+i,mode?1:0);};seed(before);seed(after);
 Registers s{};for(unsigned i=0;i<32;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
 s.r[1]=0x8877665500080000ull;s.r[3]=0xaabbccdd00000000ull|Owner;s.r[4]=mode<2?Mesh:mode==4?1u:0u;s.r[5]=Options;s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0;s.xer_so=1;
 auto initial=s;Guest expected,actual;auto om=before.Memory();guest=&expected;memory=&om;PPCContext c{};crt_full_oracle::ToPpc(c,s);
 if(mode<2)__imp__sub_82BD2200(c,before.Bytes());else if(mode==2)__imp__sub_82BD2268(c,before.Bytes());else __imp__sub_82BD27F8(c,before.Bytes());
 guest=nullptr;memory=nullptr;auto m=after.Memory();const auto entry=mode<2?0x82bd2200u:mode==2?0x82bd2268u:0x82bd27f8u;
 if(!tree_mesh_lifetime61::Apply(entry,m,{actual,native},s)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||!before.EqualCommitted(after)||expected.events!=actual.events||expected.freed!=actual.freed)throw std::runtime_error("mesh lifetime Full72/RAM/callback mismatch");
 const std::vector<GuestAddress> empty{},fields{Raw,Map-4},all{Raw,Map-4,Owner};
 if(actual.freed!=(mode==0?empty:mode==4?all:fields))throw std::runtime_error("mesh disposal order");
 if(mode==0){if(s.r[3]!=0||m.ReadU32(Owner+24)!=Map)throw std::runtime_error("invalid mesh changed owner");}
 else if(m.ReadU32(Owner+24)||m.ReadU32(Owner+32))throw std::runtime_error("owner fields not cleared");
 if(mode>=2&&m.ReadU32(Owner)!=0x820d6bb8u)throw std::runtime_error("base table not restored");
 if(mode>=3&&s.r[3]!=initial.r[3])throw std::runtime_error("deleting destructor return pointer");
}
}
void MeshLifetimeLower(std::uint32_t e,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);mesh_lifetime_oracle::Lower(e,*mesh_lifetime_oracle::memory,*mesh_lifetime_oracle::guest,s);crt_full_oracle::ToPpc(c,s);}
void MeshLifetimeIndirect(std::uint32_t e,PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);mesh_lifetime_oracle::guest->CallIndirect(e,*mesh_lifetime_oracle::memory,s);crt_full_oracle::ToPpc(c,s);}
void MeshLifetimeSave(PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*mesh_lifetime_oracle::memory;for(unsigned i=29;i<32;++i)WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));}
void MeshLifetimeRestore(PPCContext& c,std::uint8_t*){auto s=crt_full_oracle::FromPpc(c);auto& m=*mesh_lifetime_oracle::memory;for(unsigned i=29;i<32;++i)s.r[i]=ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);}
int main(){try{for(unsigned mode=0;mode<5;++mode)mesh_lifetime_oracle::Check(mode);std::puts("PASS tree-mesh-lifetime61 5 original-upper/shared-lower cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
