#include "lo_semantics/mesh_attribute_reorder61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::mesh_attribute_reorder61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
struct Reorder {
 GuestMemory& m;GuestServices& guest;Registers& s;
 std::uint64_t& R(unsigned i){return s.r[i];}
 std::uint32_t Word(std::uint64_t a){return m.ReadU32(Address(a));}
 void Compare(std::uint64_t a,std::uint64_t b=0){auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};}
 void Call(unsigned target,GuestAddress lr){s.ctr=R(target);s.lr=lr;guest.CallIndirect(Address(s.ctr)&~3u,m,s);}
 void Enter(){R(12)=s.lr;s.lr=0x82bb3d00u;for(unsigned i=27;i<32;++i)WriteU64(m,Address(R(1)-16u-8u*(31u-i)),R(i));m.WriteU32(Address(R(1)-8u),Address(R(12)));
  auto old=R(1);R(1)-=128u;m.WriteU32(Address(R(1)),Address(old));R(31)=R(3);R(27)=R(4);}
 void Leave(){R(1)+=128u;for(unsigned i=27;i<32;++i)R(i)=ReadU64(m,Address(R(1)-16u-8u*(31u-i)));R(12)=m.ReadU32(Address(R(1)-8u));s.lr=R(12);}
 void Replace(unsigned field,GuestAddress release_lr){
  R(11)=Word(R(31));R(4)=Word(R(11)+field);Compare(R(4));
  if(!s.cr6.eq){R(3)=Word(R(29)-2744u);R(11)=Word(R(3));R(11)=Word(R(11)+20u);Call(11,release_lr);
   R(11)=Word(R(31));m.WriteU32(Address(R(11)+field),Address(R(28)));}
  R(11)=Word(R(31));m.WriteU32(Address(R(11)+field),Address(R(30)));
 }
 void Positions(){
  R(29)=0xffffffff832e0000ull;R(10)=Address(R(11))<<1u;R(5)=262;R(11)+=R(10);R(3)=Word(R(29)-2744u);R(4)=Address(R(11))<<2u;
  R(9)=Word(R(3));R(10)=Word(R(9)+8u);Call(10,0x82bb3d44u);
  R(10)=Word(R(31));R(28)=0;R(30)=R(3);R(9)=0;R(11)=Word(R(10)+4u);Compare(R(11));
  if(!s.cr6.eq){R(11)=R(3);R(8)=R(27);do{
   R(7)=Word(R(8));R(6)=Word(R(10)+12u);R(10)=Address(R(7))<<1u;R(7)+=R(10);R(7)=Address(R(7))<<2u;R(7)+=R(6);
   R(10)=Word(R(7));m.WriteU32(Address(R(11)),Address(R(10)));R(10)=Word(R(7)+4u);m.WriteU32(Address(R(11)+4u),Address(R(10)));R(10)=Word(R(7)+8u);m.WriteU32(Address(R(11)+8u),Address(R(10)));
   R(10)=Word(R(31));R(9)+=1;R(8)+=4;R(11)+=12;R(7)=Word(R(10)+4u);Compare(R(9),R(7));
  }while(s.cr6.lt);}
  Replace(12,0x82bb3dd8u);
 }
 void GatherHalf(unsigned field){
  R(11)=Word(R(31));R(10)=R(28);R(9)=Word(R(11)+4u);Compare(R(9));
  if(!s.cr6.eq){R(8)=R(3);R(9)=R(27);do{R(7)=Word(R(9));R(7)=Address(R(7))<<1u;R(11)=Word(R(11)+field);R(7)=m.ReadU16(Address(R(11)+R(7)));m.WriteU16(Address(R(8)),std::uint16_t(R(7)));
   R(11)=Word(R(31));R(10)+=1;R(9)+=4;R(8)+=2;R(7)=Word(R(11)+4u);Compare(R(10),R(7));
  }while(s.cr6.lt);}
 }
 void OptionalHalf76(){
  R(11)=Word(R(31));R(11)=Word(R(11)+76u);Compare(R(11));if(s.cr6.eq)return;
  R(3)=Word(R(29)-2744u);R(11)=Word(R(31));R(5)=262;R(4)=Word(R(11)+4u);R(10)=Word(R(3));R(4)=Address(R(4))<<1u;R(11)=Word(R(10)+8u);Call(11,0x82bb3e18u);R(30)=R(3);
  GatherHalf(76);Replace(76,0x82bb3e8cu);
 }
 void OriginalIds(){
  R(3)=Word(R(29)-2744u);R(11)=Word(R(31));R(5)=264;R(11)=Word(R(11)+4u);R(10)=Word(R(3));R(4)=Address(R(11))<<2u;R(10)=Word(R(10)+8u);Call(10,0x82bb3ec0u);
  R(10)=Word(R(31));R(30)=R(3);R(9)=R(28);R(11)=Word(R(10)+4u);Compare(R(11));
  if(!s.cr6.eq){R(11)=R(27);R(8)=R(3)-R(27);do{R(10)=Word(R(10)+80u);Compare(R(10));if(!s.cr6.eq){R(7)=Word(R(11));R(7)=Address(R(7))<<2u;R(10)=Word(R(10)+R(7));}else R(10)=Word(R(11));
   m.WriteU32(Address(R(8)+R(11)),Address(R(10)));R(10)=Word(R(31));R(9)+=1;R(11)+=4;R(7)=Word(R(10)+4u);Compare(R(9),R(7));
  }while(s.cr6.lt);}
  Replace(80,0x82bb3f40u);
 }
 void OptionalHalf32(){
  R(11)=Word(R(31));R(11)=Word(R(11)+32u);Compare(R(11));if(s.cr6.eq)return;
  R(3)=Word(R(29)-2744u);R(11)=Word(R(31));R(5)=268;R(11)=Word(R(11)+4u);R(10)=Word(R(3));R(4)=Address(R(11))<<1u;R(10)=Word(R(10)+8u);Call(10,0x82bb3f80u);R(30)=R(3);
  GatherHalf(32);Replace(32,0x82bb3ff4u);
 }
 void Categories(){
  R(11)=Word(R(31));R(10)=Word(R(11)+28u);Compare(R(10),256u);R(10)=Word(R(11)+36u);
  if(s.cr6.lt){Compare(R(10));if(s.cr6.eq)return;
   R(3)=Word(R(29)-2744u);R(4)=Word(R(11)+4u);R(5)=267;R(11)=Word(R(3));R(11)=Word(R(11)+8u);Call(11,0x82bb403cu);
   R(10)=Word(R(31));R(30)=R(3);R(11)=R(28);R(9)=Word(R(10)+4u);Compare(R(9));
   if(!s.cr6.eq){R(9)=R(27);do{R(8)=Word(R(9));R(10)=Word(R(10)+36u);R(10)=m.ReadU8(Address(R(10)+R(8)));m.WriteU8(Address(R(11)+R(30)),std::uint8_t(R(10)));
    R(10)=Word(R(31));R(11)+=1;R(9)+=4;R(8)=Word(R(10)+4u);Compare(R(11),R(8));
   }while(s.cr6.lt);}
  }else{Compare(R(10));if(s.cr6.eq)return;
   R(3)=Word(R(29)-2744u);R(5)=268;R(11)=Word(R(11)+4u);R(10)=Word(R(3));R(4)=Address(R(11))<<1u;R(10)=Word(R(10)+8u);Call(10,0x82bb40acu);R(30)=R(3);GatherHalf(36);
  }
  Replace(36,0x82bb4120u);
 }
 void Run(){Enter();R(11)=Word(R(3));R(11)=Word(R(11)+4u);Compare(R(11));if(!s.cr6.eq){Positions();OptionalHalf76();OriginalIds();OptionalHalf32();Categories();}Leave();}
};
}
bool Apply(GuestAddress entry,GuestMemory& memory,GuestServices& guest,Registers& state){if(entry!=0x82bb3cf8u)return false;Reorder{memory,guest,state}.Run();return true;}
}
