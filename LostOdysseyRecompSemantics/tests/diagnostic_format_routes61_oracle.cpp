// Four original upper bodies; locks and formatter use shared complete accepted
// implementations. This compares routing, not those lower implementations anew.
#include "object_sort_engine61_oracle_fixture.h"
#include "lo_semantics/diagnostic_format_routes61.h"
#include <atomic>
#include <mutex>
#include <fstream>
namespace diagnostic_format61_oracle {
using Full = diagnostic_format_routes61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Object=0x30000u,Sink=0x31000u,Table=0x32000u,Lock=0x33000u;
constexpr GuestAddress Tls=0x40000u,Thread=0x41000u,Text=0x42000u,Flag=0x43000u;
constexpr std::array<test::Region,4> Regions{{{0u,0x120000u},{0x820d3000u,0x4000u},
 {0x83214000u,0x3000u},{0x832df000u,0x1000u}}};
constexpr std::uint32_t Swap(std::uint32_t v){return(v<<24u)|((v<<8u)&0xff0000u)|((v>>8u)&0xff00u)|(v>>24u);}
struct Sync final:diagnostic_lock61::SynchronizationServices {
 test::GuestWindow& window; std::recursive_mutex mutex;
 explicit Sync(test::GuestWindow& w):window(w){}
 std::uint32_t& Raw(GuestAddress a){return *reinterpret_cast<std::uint32_t*>(window.Bytes()+a);}
 std::uint32_t LoadReservedWord(GuestAddress a,GuestMemory&) override {return Swap(std::atomic_ref<std::uint32_t>(Raw(a)).load());}
 bool CompareExchangeWord(GuestAddress a,std::uint32_t e,std::uint32_t d,GuestMemory&) override {
  e=Swap(e);return std::atomic_ref<std::uint32_t>(Raw(a)).compare_exchange_strong(e,Swap(d));}
 void EnterCriticalSection(GuestMemory&,Full& s,Machine& m) override {
  mutex.lock();s.r[3]=0xabcdu;s.fpr_bits[7]^=0x100u;m.msr^=0x10000000u;m.reserved_bits^=0x0100000000000000ull;}
 void LeaveCriticalSection(GuestMemory&,Full& s,Machine& m) override {
  mutex.unlock();s.r[3]=0xdef0u;s.cr7.eq^=1u;m.msr^=0x01000000u;m.reserved_bits^=0x0200000000000000ull;}
};
struct Guest final:crt_narrow_formatter61::GuestServices {
 unsigned reply=0;std::vector<std::array<std::uint64_t,73>> events;std::string output;
 void CallIndirect(GuestAddress target,GuestMemory& m,Full& s) override {
  std::array<std::uint64_t,73> e{};auto snap=crt_full_oracle::Snapshot(s);
  std::copy(snap.begin(),snap.end(),e.begin());e[72]=target;events.push_back(e);
  if(target==0x2500u||target==0x2508u){
   auto p=Address(s.r[target==0x2500u?5:4]);for(unsigned i=0;i<2048u;++i){auto c=m.ReadU8(p+i);if(!c)break;output+=char(c);}}
  else if(target!=0x2504u)throw std::runtime_error("unexpected diagnostic sink target");
  s.r[3]=reply;s.r[10]^=0x123456789abcdef0ull;s.fpr_bits[3]^=0x80u;s.cr1.gt^=1u;
 }
 void CallOutput(GuestMemory&,Full&) override {throw std::runtime_error("unexpected diagnostic recursive formatter output");}
};
struct Environment {
 sort_engine61_oracle::Environment accepted;Sync sync;Guest guest;Machine machine{0x020a8020u,0xcafebabe11223344ull};
 explicit Environment(test::GuestWindow& w):accepted(w),sync(w){}
 diagnostic_format_routes61::Dependencies Deps(){return{{accepted.Deps().sort.accepted,guest},sync,machine};}
};
Environment* original=nullptr;
void Seed(test::GuestWindow& w){
 w.Fill(0xa5u);auto m=w.Memory();
 std::ifstream image("LostOdysseyRecompLib/private/image_disc1.bin",std::ios::binary);
 if(!image)throw std::runtime_error("missing private diagnostic/formatter image pages");
 image.seekg(0xd3000);std::array<char,0x4000> pages{};image.read(pages.data(),pages.size());
 if(!image)throw std::runtime_error("short private diagnostic image pages");
 for(unsigned i=0;i<pages.size();++i)m.WriteU8(0x820d3000u+i,std::uint8_t(pages[i]));
 m.WriteU32(Object+28u,Sink);m.WriteU32(Object+36u,0u);m.WriteU32(Object+52u,Lock);
 m.WriteU32(Lock+28u,0u);m.WriteU32(Tls+256u,Thread);m.WriteU32(Thread+332u,0x76543210u);
 m.WriteU32(Sink,Table);for(unsigned i=0;i<3u;++i)m.WriteU32(Table+4u*i,0x2500u+4u*i);
 m.WriteU32(0x832df54cu,Object);m.WriteU8(Text,'O');m.WriteU8(Text+1u,'K');m.WriteU8(Text+2u,0u);
 m.WriteU32(0x83215300u,0x70000u);m.WriteU32(0x70000u+188u,0x70100u);
 m.WriteU32(0x70100u,0x70200u);m.WriteU8(0x70200u,'.');
 // Narrow formatting classifies each literal via locale+200.
 m.WriteU32(0x70000u+200u,0x71000u);
 for(unsigned i=0;i<256u;++i)m.WriteU16(0x71000u+2u*i,0u);
}
void Check(unsigned which){
 test::GuestWindow before(Regions),after(Regions);Seed(before);Seed(after);
 Environment expected(before),actual(after);expected.guest.reply=actual.guest.reply=which==2?2u:1u;
 auto s=sort_engine61_oracle::Initial(0u);s.r[3]=Object;s.r[4]=which==3?208u:107u;
 s.r[5]=9u;s.r[6]=10u;s.r[7]=Flag;s.r[8]=Text;s.r[9]=0x45000u;s.r[13]=Tls;
 GuestAddress entry=0x82bc8b78u;
 if(which==0){before.Memory().WriteU32(Object+28u,0u);after.Memory().WriteU32(Object+28u,0u);}
 if(which>=4){entry=0x82bd18c0u;s.r[4]=which==4?123u:0u;s.r[5]=456u;}
 PPCContext c{};crt_full_oracle::ToPpc(c,s);c.msr=expected.machine.msr;c.reserved.u64=expected.machine.reserved_bits;
 original=&expected;if(entry==0x82bd18c0u)__imp__sub_82BD18C0(c,before.Bytes());else __imp__sub_82BC8B78(c,before.Bytes());original=nullptr;
 auto memory=after.Memory();if(!diagnostic_format_routes61::Apply(entry,memory,actual.Deps(),s))throw std::runtime_error("missing route");
 auto a=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),b=crt_full_oracle::Snapshot(s);
 if(a!=b||!before.EqualCommitted(after)||expected.guest.events!=actual.guest.events||expected.guest.output!=actual.guest.output||
 c.msr!=actual.machine.msr||c.reserved.u64!=actual.machine.reserved_bits){
  for(unsigned i=0;i<a.size();++i)if(a[i]!=b[i])std::fprintf(stderr,"case%u Full[%u] %llx/%llx\n",which,i,(unsigned long long)a[i],(unsigned long long)b[i]);
  throw std::runtime_error("diagnostic route Full72/machine/RAM/callback mismatch");}
 if(which==3&&actual.guest.output!="OK")throw std::runtime_error("diagnostic formatted output mismatch");
 if(which==1&&memory.ReadU8(Flag)!=1u)throw std::runtime_error("diagnostic flag mismatch");
}
}
void OriginalDiagnosticFormat61Lower(std::uint32_t entry,PPCContext& c,std::uint8_t*){
 auto& env=*diagnostic_format61_oracle::original;auto s=crt_full_oracle::FromPpc(c);env.machine={c.msr,c.reserved.u64};
 auto& m=env.accepted.stream.memory;bool handled;
 if(entry==0x82b7d260u)handled=crt_narrow_formatter61::Apply(entry,m,env.Deps().formatter,s);
 else handled=diagnostic_lock61::Apply(entry,m,env.sync,s,env.machine);
 if(!handled)throw std::runtime_error("unexpected diagnostic accepted lower");
 crt_full_oracle::ToPpc(c,s);c.msr=env.machine.msr;c.reserved.u64=env.machine.reserved_bits;
}
void OriginalDiagnosticFormat61Indirect(std::uint32_t target,PPCContext& c,std::uint8_t*){
 auto& env=*diagnostic_format61_oracle::original;auto s=crt_full_oracle::FromPpc(c);
 env.guest.CallIndirect(target,env.accepted.stream.memory,s);crt_full_oracle::ToPpc(c,s);
}
void OriginalDiagnosticFormat61Save(unsigned first,PPCContext& c,std::uint8_t*){
 auto s=crt_full_oracle::FromPpc(c);auto& m=diagnostic_format61_oracle::original->accepted.stream.memory;
 for(unsigned i=first;i<32u;++i)recovery_abi::WriteU64(m,Address(s.r[1]-16u-8u*(31u-i)),s.r[i]);
 m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
}
void OriginalDiagnosticFormat61Restore(unsigned first,PPCContext& c,std::uint8_t*){
 auto s=crt_full_oracle::FromPpc(c);auto& m=diagnostic_format61_oracle::original->accepted.stream.memory;
 for(unsigned i=first;i<32u;++i)s.r[i]=recovery_abi::ReadU64(m,Address(s.r[1]-16u-8u*(31u-i)));
 s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);
}
int main(){try{for(unsigned i=0;i<6u;++i)diagnostic_format61_oracle::Check(i);
 std::puts("PASS diagnostic-format-routes61 6 original-upper/shared-accepted-lower cases");return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
