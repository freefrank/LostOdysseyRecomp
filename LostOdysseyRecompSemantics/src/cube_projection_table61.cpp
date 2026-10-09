#include "lo_semantics/cube_projection_table61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::cube_projection_table61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
double Single(double v){return static_cast<float>(v);}
std::uint64_t Product(std::uint64_t a,std::uint64_t b){return std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(a)))*std::int64_t(std::bit_cast<std::int32_t>(Address(b))));}
struct Table {
 GuestMemory& m;Dependencies d;Registers& s;
 std::uint64_t& R(unsigned i){return s.r[i];}
 double F(unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);}
 void F(unsigned i,double v){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(v);}
 void Mode(){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;d.fp.SetHostFpControl(s.cached_fp_control);}}
 void Load(unsigned i,std::uint64_t a){Mode();F(i,std::bit_cast<float>(m.ReadU32(Address(a))));}
 void Store(unsigned offset,unsigned i){Mode();m.WriteU32(Address(R(1)+offset),std::bit_cast<std::uint32_t>(static_cast<float>(F(i))));}
 void Compare(std::uint64_t a,std::uint64_t b){auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};}
 void Indirect(GuestAddress lr){s.ctr=R(11);s.lr=lr;d.guest.CallIndirect(Address(s.ctr)&~3u,m,s);}
 void GlobalAllocator(GuestAddress lr){s.lr=lr;(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,m,d.guest,s);}
 void AllocateBytes(GuestAddress lr){R(11)=m.ReadU32(Address(R(3)));R(5)=55;R(4)=m.ReadU32(Address(R(31)+8u));R(4)=m.ReadU32(Address(R(4)+8u));R(11)=m.ReadU32(Address(R(11)));Indirect(lr);}
 void Prepare(){
  R(12)=s.lr;m.WriteU32(Address(R(1)-8u),Address(R(12)));WriteU64(m,Address(R(1)-16u),R(31));auto old=R(1);R(1)-=96u;m.WriteU32(Address(R(1)),Address(old));
  R(31)=R(3);R(11)=m.ReadU32(Address(R(3)+8u));R(11)=m.ReadU32(Address(R(11)+32u));R(11)=m.ReadU32(Address(R(11)+12u));Compare(R(11),255u);
  if(s.cr6.gt){FinishPrepare(0);return;}
  GlobalAllocator(0x82bb3474u);AllocateBytes(0x82bb3490u);
  R(11)=m.ReadU32(Address(R(31)+8u));m.WriteU32(Address(R(11)+24u),Address(R(3)));
  R(11)=m.ReadU32(Address(R(31)+8u));R(11)=m.ReadU32(Address(R(11)+24u));Compare(R(11),0);if(s.cr6.eq){FinishPrepare(0);return;}
  GlobalAllocator(0x82bb34acu);AllocateBytes(0x82bb34c8u);
  R(11)=m.ReadU32(Address(R(31)+8u));m.WriteU32(Address(R(11)+28u),Address(R(3)));
  R(11)=m.ReadU32(Address(R(31)+8u));R(11)=m.ReadU32(Address(R(11)+28u));R(11)=std::countl_zero(Address(R(11)));R(11)=(Address(R(11))>>5)&1u;
  FinishPrepare(R(11)^1u);
 }
 void FinishPrepare(std::uint64_t result){R(3)=result;R(1)+=96u;R(12)=m.ReadU32(Address(R(1)-8u));s.lr=R(12);R(31)=ReadU64(m,Address(R(1)-16u));}
 void Enter(){R(12)=s.lr;s.lr=0x82bb38a8u;for(unsigned i=25;i<32;++i)WriteU64(m,Address(R(1)-16u-8u*(31u-i)),R(i));m.WriteU32(Address(R(1)-8u),Address(R(12)));
  R(12)=R(1)-64u;s.lr=0x82bb38b0u;for(unsigned i=28;i<32;++i)WriteU64(m,Address(R(12)-8u*(32u-i)),s.fpr_bits[i]);
  auto old=R(1);R(1)-=240u;m.WriteU32(Address(R(1)),Address(old));}
 void Leave(unsigned result,GuestAddress lr){R(3)=result;R(1)+=240u;R(12)=R(1)-64u;s.lr=lr;
  for(unsigned i=28;i<32;++i)s.fpr_bits[i]=ReadU64(m,Address(R(12)-8u*(32u-i)));
  for(unsigned i=25;i<32;++i)R(i)=ReadU64(m,Address(R(1)-16u-8u*(31u-i)));R(12)=m.ReadU32(Address(R(1)-8u));s.lr=R(12);}
 void Convert(unsigned spill,unsigned source,unsigned fpr){WriteU64(m,Address(R(1)+spill),R(source));Mode();F(fpr,double(std::bit_cast<std::int64_t>(ReadU64(m,Address(R(1)+spill)))));F(fpr,Single(F(fpr)));}
 void Direction(){
  Compare(R(27),5);if(s.cr6.gt){Load(12,R(1)+128);Load(13,R(1)+132);Load(0,R(1)+136);return;}
  R(11)=0xffffffff82bb0000ull;R(12)=R(11)+14728u;R(11)=Address(R(27))<<2u;R(0)=m.ReadU32(Address(R(12)+R(11)));s.ctr=R(0);
  const auto face=Address(R(27));
  if(face<2){Compare(R(27),0);F(12,s.cr6.eq?F(29):F(31));Store(128,12);
   R(11)=Address(R(31));R(10)=Address(R(30));Convert(80,11,13);Convert(88,10,11);F(0,Single(F(31)/F(30)));
   F(13,Single(-(F(13)*F(0)-F(31))));F(0,Single(-(F(11)*F(0)-F(31))));Store(132,13);Store(136,0);
  }else if(face<4){Compare(R(27),2);F(13,s.cr6.eq?F(29):F(31));Store(132,13);
   R(11)=Address(R(31));R(10)=Address(R(30));Convert(96,11,0);Convert(104,10,11);F(12,Single(F(31)/F(30)));
   F(0,Single(-(F(0)*F(12)-F(31))));F(12,Single(-(F(11)*F(12)-F(31))));Store(136,0);Store(128,12);
  }else{Compare(R(27),4);F(0,s.cr6.eq?F(29):F(31));Store(136,0);
   R(11)=Address(R(31));R(10)=Address(R(30));Convert(112,11,12);Convert(120,10,11);F(13,Single(F(31)/F(30)));
   F(12,Single(-(F(12)*F(13)-F(31))));F(13,Single(-(F(11)*F(13)-F(31))));Store(128,12);Store(132,13);
  }
 }
 void Normalize(){Mode();F(11,Single(F(13)*F(13)));F(11,Single(F(0)*F(0)+F(11)));F(11,Single(F(12)*F(12)+F(11)));
  const double a=F(11),b=F(28);s.cr6={std::uint8_t(a<b),std::uint8_t(a>b),std::uint8_t(a==b),std::uint8_t(std::isnan(a)||std::isnan(b))};
  if(!s.cr6.eq){F(11,Single(std::sqrt(F(11))));F(11,Single(F(31)/F(11)));F(12,Single(F(11)*F(12)));F(13,Single(F(11)*F(13)));F(0,Single(F(11)*F(0)));Store(128,12);Store(132,13);Store(136,0);}
 }
 void SampleCube(){Enter();R(26)=R(3);R(28)=R(4);R(10)=m.ReadU32(Address(R(3)+4u));R(11)=Product(R(4),R(4));R(9)=Address(R(11))<<1u;R(11)+=R(9);R(11)=Address(R(11))<<1u;
  m.WriteU32(Address(R(10)+4u),Address(R(4)));R(9)=m.ReadU32(Address(R(3)+4u));m.WriteU32(Address(R(9)+8u),Address(R(11)));R(11)=m.ReadU32(Address(R(3)));R(11)=m.ReadU32(Address(R(11)+4u));Indirect(0x82bb38ecu);
  R(11)=Address(R(3))&255u;Compare(R(11),0);if(s.cr6.eq){Leave(0,0x82bb3908u);return;}
  R(11)=R(28)-1u;R(11)=Address(R(11));Convert(80,11,13);
  R(11)=0xffffffff82020000ull;R(10)=0xffffffff82000000ull;R(9)=0xffffffff82000000ull;Load(0,R(11)-1552u);
  R(11)=0xffffffff82000000ull;Load(31,R(10)+30596u);Load(29,R(9)+3648u);Load(28,R(11)+3664u);F(30,Single(F(13)*F(0)));R(27)=0;
  do{R(30)=0;Compare(R(28),0);
   if(!s.cr6.eq){R(11)=Product(R(27),R(28));R(25)=Product(R(11),R(28));do{R(29)=R(25)+R(30);R(31)=0;do{
    Direction();Normalize();R(11)=m.ReadU32(Address(R(26)));R(5)=R(1)+128u;R(4)=R(29);R(3)=R(26);R(11)=m.ReadU32(Address(R(11)+8u));Indirect(0x82bb3b04u);
    R(11)=Address(R(3))&255u;Compare(R(11),0);if(s.cr6.eq){Leave(0,0x82bb3908u);return;}
    R(31)+=1;R(29)+=R(28);Compare(R(31),R(28));
   }while(s.cr6.lt);R(30)+=1;Compare(R(30),R(28));}while(s.cr6.lt);}
   R(27)+=1;Compare(R(27),6);
  }while(s.cr6.lt);
  R(11)=m.ReadU32(Address(R(26)));R(3)=R(26);R(11)=m.ReadU32(Address(R(11)+12u));Indirect(0x82bb3b4cu);Leave(1,0x82bb3b5cu);
 }
};
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state){Table t{memory,dependencies,state};if(entry==0x82bb3430u){t.Prepare();return true;}if(entry==0x82bb38a0u){t.SampleCube();return true;}return false;}
}
