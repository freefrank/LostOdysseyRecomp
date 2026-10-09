#include "lo_semantics/tree_triangle_split61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::tree_triangle_split61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
struct Split {
 GuestMemory& m;float_triplet_transfer::NativeServices& fp;Registers& s;
 std::uint64_t& R(unsigned i){return s.r[i];}
 double F(unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);}
 void Single(unsigned i,double v){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(static_cast<float>(v)));}
 std::uint32_t Word(std::uint64_t p){return m.ReadU32(Address(p));}
 void Load(unsigned i,std::uint64_t p){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));}
 std::uint64_t Twice(std::uint64_t v){return(v<<1u)&0xfffffffeu;}
 std::uint64_t Words(std::uint64_t v){return(v<<2u)&0xfffffffcu;}
 std::uint64_t Triplet(std::uint64_t v){return Words(v+Twice(v));}
 void Flush(){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;fp.SetHostFpControl(s.cached_fp_control);}}
 void Compare(std::uint64_t a,std::uint64_t b=0u){const auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};}
 void Batch(){
  // All record reads precede any guest write. Nonvolatile address temporaries
  // are local here; their incoming register values are restored at return.
  const auto selection=R(9)-8u;
  for(unsigned t=0u;t<4u;++t){
   const auto triangle=R(8)+Triplet(Word(selection+4u*t));
   const auto a=R(10)+Triplet(Word(triangle));
   const auto b=R(10)+Triplet(Word(triangle+4u));
   const auto c=R(10)+Triplet(Word(triangle+8u));
   Flush();Load(13,R(11)+a);Single(0,F(13)+F(0));
   Load(13,R(11)+b);
   // The second triangle leaves its third coordinate in f11; other triangles
   // use f12. These observable FP temporaries are not callee-saved.
   const unsigned third=t==1u?11u:12u;Load(third,R(11)+c);
   Single(0,F(0)+F(13));Single(0,F(0)+F(third));R(6)=c;
  }
  --R(31);R(9)+=16u;Compare(R(31));
 }
 void Mean(){
  R(11)=0xffffffff82000000ull;R(22)=0u;
  const auto count=std::bit_cast<std::int32_t>(Address(R(5)));s.cr6={std::uint8_t(count<4),std::uint8_t(count>4),std::uint8_t(count==4),s.xer_so};Flush();Load(0,R(11)+3664u);
  if(!s.cr6.lt){R(9)=R(5)-4u;R(10)=Word(R(3)+72u);R(11)=Words(R(7));R(8)=Address(R(9))>>2u;R(9)=R(4)+8u;R(31)=R(8)+1u;R(8)=Word(R(10)+16u);R(10)=Word(R(10)+20u);R(22)=Words(R(31));do{Batch();}while(!s.cr6.eq);}
  Compare(R(22),R(5));if(s.cr6.lt){R(11)=Word(R(3)+72u);R(10)=Words(R(22));R(8)=Words(R(7));R(9)=R(10)+R(4);R(10)=R(5)-R(22);R(31)=Word(R(11)+16u);R(7)=Word(R(11)+20u);
   do{
    R(11)=Word(R(9));--R(10);R(9)+=4u;R(6)=Twice(R(11));Compare(R(10));R(11)+=R(6);R(11)=Words(R(11));R(11)+=R(31);
    R(6)=Word(R(11));R(4)=Word(R(11)+4u);R(3)=Twice(R(6));R(11)=Word(R(11)+8u);R(6)+=R(3);R(3)=Twice(R(4));R(6)=Words(R(6));R(4)+=R(3);R(6)+=R(7);R(4)=Words(R(4));R(3)=Twice(R(11));R(4)+=R(7);R(11)+=R(3);
    Flush();Load(13,R(8)+R(6));Single(0,F(13)+F(0));R(11)=Words(R(11));Load(13,R(8)+R(4));R(11)+=R(7);Load(12,R(8)+R(11));Single(0,F(0)+F(13));Single(0,F(0)+F(12));
   }while(!s.cr6.eq);
  }
  R(11)=Twice(R(5));R(11)+=R(5);R(11)=Address(R(11));WriteU64(m,Address(R(1)-96u),R(11));Flush();s.fpr_bits[13]=ReadU64(m,Address(R(1)-96u));
  s.fpr_bits[13]=std::bit_cast<std::uint64_t>(double(std::bit_cast<std::int64_t>(s.fpr_bits[13])));Single(13,F(13));Single(1,F(0)/F(13));
 }
 void Run(){
  R(12)=s.lr;s.lr=0x82bd8c00u;for(unsigned i=22u;i<32u;++i)WriteU64(m,Address(R(1)-16u-8u*(31u-i)),R(i));m.WriteU32(Address(R(1)-8u),Address(R(12)));
  R(11)=Word(R(3)+8u);R(11)&=0x20u;Compare(R(11));
  if(!s.cr6.eq)Mean();else{R(11)=R(7)+3u;R(10)=Words(R(7));R(11)=Words(R(11));Flush();Load(0,R(10)+R(6));Load(13,R(11)+R(6));R(11)=0xffffffff82020000ull;Single(13,F(13)+F(0));Load(0,R(11)-1552u);Single(1,F(13)*F(0));}
  for(unsigned i=22u;i<32u;++i)R(i)=ReadU64(m,Address(R(1)-16u-8u*(31u-i)));R(12)=Word(R(1)-8u);s.lr=R(12);
 }
};
}
bool Apply(GuestAddress entry,GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s){if(entry!=0x82bd8bf8u)return false;Split{m,fp,s}.Run();return true;}
}
