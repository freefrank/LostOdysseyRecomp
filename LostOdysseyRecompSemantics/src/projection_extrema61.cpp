#include "lo_semantics/projection_extrema61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::projection_extrema61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
double Single(double v){return static_cast<float>(v);}
struct Extrema {
 GuestMemory& memory;Registers& state;
 std::uint64_t& R(unsigned i){return state.r[i];}
 double F(unsigned i){return std::bit_cast<double>(state.fpr_bits[i]);}
 void F(unsigned i,double v){state.fpr_bits[i]=std::bit_cast<std::uint64_t>(v);}
 void Load(unsigned i,std::uint64_t address){F(i,std::bit_cast<float>(memory.ReadU32(Address(address))));}
 void Compare(std::uint64_t a,std::uint64_t b){const auto x=Address(a),y=Address(b);state.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),state.xer_so};}
 void CompareFloat(double a,double b){state.cr6={std::uint8_t(a<b),std::uint8_t(a>b),std::uint8_t(a==b),std::uint8_t(std::isnan(a)||std::isnan(b))};}
 void Direction(){Load(12,R(5)+8u);Load(11,R(5)+4u);Load(10,R(5));}
 // The second unrolled point and every tail point sum z, x, y; other
 // unrolled points sum y, x, z. Double multiply/add then single conversion
 // follows the selected generated arithmetic, without substituting std::fma.
 void Project(std::uint64_t point,std::uint64_t index,bool z_first){
  Load(0,point+(z_first?8u:4u));Load(8,point);Load(7,point+(z_first?4u:8u));
  F(0,Single(F(0)*F(z_first?12u:11u)));
  F(0,Single(F(8)*F(10)+F(0)));
  F(0,Single(F(7)*F(z_first?11u:12u)+F(0)));
  CompareFloat(F(0),F(13));if(state.cr6.lt){R(7)=index;F(13,F(0));}
  state.fpr_bits[0]^=0x8000000000000000ull;
  CompareFloat(F(0),F(9));if(state.cr6.lt){R(8)=index;F(9,F(0));}
 }
 void Run(NativeServices& native){
  WriteU64(memory,Address(R(1)-16u),R(30));WriteU64(memory,Address(R(1)-8u),R(31));R(30)=R(3);
  R(11)=memory.ReadU32(Address(R(3)+8u));R(7)=0;R(8)=0;R(10)=0xffffffff82000000ull;
  if(state.cached_fp_control&0x8040u){state.cached_fp_control&=~0x8040u;native.SetHostFpControl(state.cached_fp_control);}
  Load(13,R(10)+3596u);R(11)=memory.ReadU32(Address(R(11)+32u));F(9,F(13));R(10)=R(11);R(11)=0;
  R(31)=memory.ReadU32(Address(R(10)+12u));R(3)=memory.ReadU32(Address(R(10)+16u));
  const auto count=std::bit_cast<std::int32_t>(Address(R(31)));state.cr6={std::uint8_t(count<4),std::uint8_t(count>4),std::uint8_t(count==4),state.xer_so};
  if(!state.cr6.lt){
   Load(12,R(5)+8u);R(6)=R(31)-3u;Load(11,R(5)+4u);R(10)=2;Load(10,R(5));R(9)=R(3)+16u;
   do{Project(R(9)-16u,R(11),false);Project(R(9)-4u,R(10)-1u,true);Project(R(9)+8u,R(10),false);Project(R(9)+20u,R(10)+1u,false);
    R(11)+=4u;R(10)+=4u;R(9)+=48u;Compare(R(11),R(6));
   }while(state.cr6.lt);
  }
  Compare(R(11),R(31));if(state.cr6.lt){
   R(10)=Address(R(11))<<1u;Direction();R(10)=R(11)+R(10);R(10)=Address(R(10))<<2u;R(10)+=R(3);R(10)+=4u;
   do{Project(R(10)-4u,R(11),true);R(11)+=1u;R(10)+=12u;Compare(R(11),R(31));}while(state.cr6.lt);
  }
  R(11)=memory.ReadU32(Address(R(30)+8u));R(3)=1;R(11)=memory.ReadU32(Address(R(11)+24u));memory.WriteU8(Address(R(11)+R(4)),std::uint8_t(R(7)));
  R(11)=memory.ReadU32(Address(R(30)+8u));R(11)=memory.ReadU32(Address(R(11)+28u));memory.WriteU8(Address(R(11)+R(4)),std::uint8_t(R(8)));
  R(30)=ReadU64(memory,Address(R(1)-16u));R(31)=ReadU64(memory,Address(R(1)-8u));
 }
};
}
bool Apply(GuestAddress entry,GuestMemory& memory,NativeServices& native,Registers& state){if(entry!=0x82bb34f8u)return false;Extrema{memory,state}.Run(native);return true;}
}
