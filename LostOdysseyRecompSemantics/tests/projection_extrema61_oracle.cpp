#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/projection_extrema61.h"
#include <bit>
namespace projection_extrema61_oracle {
using Full=projection_extrema61::Registers;
constexpr GuestAddress Owner=0x30000,Storage=0x31000,View=0x32000,Points=0x33000,Direction=0x34000,Minimum=0x35000,Maximum=0x36000;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x82000000u,0x1000u}}};
struct Native final:projection_extrema61::NativeServices {void SetHostFpControl(std::uint32_t v)override{PPCFPSCRRegister{}.setcsr(v);}};
void Check(unsigned count){
 struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
 test::GuestWindow before(Regions),after(Regions);
 const auto seed=[&](test::GuestWindow& w){w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Owner+8u,Storage);m.WriteU32(Storage+24u,Minimum);m.WriteU32(Storage+28u,Maximum);m.WriteU32(Storage+32u,View);m.WriteU32(View+12u,count);m.WriteU32(View+16u,Points);
  const std::array<float,3> direction{.5f,-1.f,2.f};
  const std::array<std::array<float,3>,5> points{{{0.f,0.f,0.f},{16777216.f,8388608.f,1.f},{-2.f,0.f,0.f},{16777216.f,8388608.f,1.f},{-2.f,0.f,0.f}}};
  for(unsigned axis=0;axis<3;++axis)m.WriteU32(Direction+4u*axis,std::bit_cast<std::uint32_t>(direction[axis]));
  for(unsigned i=0;i<points.size();++i)for(unsigned axis=0;axis<3;++axis)m.WriteU32(Points+12u*i+4u*axis,std::bit_cast<std::uint32_t>(points[i][axis]));
  m.WriteU32(0x82000e0cu,0x7f7fffffu);
 };seed(before);seed(after);
 Full s{};for(unsigned i=0;i<32;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
 s.r[1]=0x8877665500080000ull;s.r[3]=0xaabbccdd00000000ull|Owner;s.r[4]=0xccdd001100000003ull;s.r[5]=Direction;s.lr=0x9988776681234567ull;s.xer_so=1;s.cached_fp_control=0x9fc0u;
 const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);PPCFPSCRRegister{}.setcsr(s.cached_fp_control);__imp__sub_82BB34F8(c,before.Bytes());auto host=PPCFPSCRRegister{}.getcsr();
 Native native;auto m=after.Memory();PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);if(!projection_extrema61::Apply(0x82bb34f8u,m,native,s))throw std::runtime_error("missing projection extrema entry");
 auto a=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),b=crt_full_oracle::Snapshot(s);
 if(a!=b||!before.EqualCommitted(after)||host!=PPCFPSCRRegister{}.getcsr()){
  for(unsigned i=0;i<a.size();++i)if(a[i]!=b[i])std::fprintf(stderr,"count%u Full[%u] %llx/%llx\n",count,i,(unsigned long long)a[i],(unsigned long long)b[i]);
  throw std::runtime_error("projection extrema Full72/RAM/host mismatch");}
 if(s.r[3]!=1u||s.r[1]!=initial.r[1]||s.lr!=initial.lr||m.ReadU8(Minimum+3u)!=(count?2u:0u)||m.ReadU8(Maximum+3u)!=(count?1u:0u))throw std::runtime_error("projection extrema first-index/tail result mismatch");
}
}
int main(){try{for(unsigned count:{0u,3u,5u})projection_extrema61_oracle::Check(count);std::puts("PASS projection-extrema61 3 original-body cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
