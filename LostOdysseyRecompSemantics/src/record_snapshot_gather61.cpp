#include "lo_semantics/record_snapshot_gather61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::record_snapshot_gather61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
struct Gather {
 GuestMemory& m;GuestServices& guest;Registers& s;
 std::uint64_t& R(unsigned i){return s.r[i];}
 std::uint32_t Word(std::uint64_t a){return m.ReadU32(Address(a));}
 void Compare(std::uint64_t a,std::uint64_t b=0){auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};}
 void Call(GuestAddress lr){s.ctr=R(11);s.lr=lr;guest.CallIndirect(Address(s.ctr)&~3u,m,s);}
 void Allocator(GuestAddress lr){s.lr=lr;(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,m,guest,s);}
 void Enter(){R(12)=s.lr;s.lr=0x82bd1908u;for(unsigned i=28;i<32;++i)WriteU64(m,Address(R(1)-16u-8u*(31u-i)),R(i));m.WriteU32(Address(R(1)-8u),Address(R(12)));auto old=R(1);R(1)-=128;m.WriteU32(Address(R(1)),Address(old));R(31)=R(3);R(3)=R(4);R(28)=R(5);}
 void Leave(bool success){R(3)=success?1u:0u;R(1)+=128;for(unsigned i=28;i<32;++i)R(i)=ReadU64(m,Address(R(1)-16u-8u*(31u-i)));R(12)=Word(R(1)-8u);s.lr=R(12);}
 void Snapshot(){R(11)=Word(R(31)+8u);R(10)=0;Compare(R(11));if(s.cr6.gt){R(11)=0;do{
   R(9)=Word(R(31)+16u);R(8)=R(11)+R(30);R(10)+=1;R(9)=R(11)+R(9);R(11)+=12;
   R(7)=Word(R(9));m.WriteU32(Address(R(8)),Address(R(7)));R(7)=Word(R(9)+4u);m.WriteU32(Address(R(8)+4u),Address(R(7)));R(9)=Word(R(9)+8u);m.WriteU32(Address(R(8)+8u),Address(R(9)));
   R(9)=Word(R(31)+8u);Compare(R(10),R(9));
  }while(s.cr6.lt);}}
 void Scatter(){R(11)=Word(R(31)+8u);R(8)=0;Compare(R(11));if(s.cr6.gt){R(10)=0;R(11)=R(28);do{
   R(7)=Word(R(31)+16u);R(8)+=1;R(9)=Word(R(11));R(11)+=4;R(6)=R(10)+R(7);R(7)=Address(R(9))<<1u;R(10)+=12;R(9)+=R(7);R(9)=Address(R(9))<<2u;R(9)+=R(30);
   R(7)=Word(R(9));m.WriteU32(Address(R(6)),Address(R(7)));R(7)=Word(R(9)+4u);m.WriteU32(Address(R(6)+4u),Address(R(7)));R(9)=Word(R(9)+8u);m.WriteU32(Address(R(6)+8u),Address(R(9)));
   R(9)=Word(R(31)+8u);Compare(R(8),R(9));
  }while(s.cr6.lt);}}
 bool Run(){
  Compare(R(3));if(s.cr6.eq)return false;Compare(R(5));if(s.cr6.eq)return false;R(11)=Word(R(31)+8u);Compare(R(3),R(11));if(!s.cr6.eq)return false;
  R(11)=Word(R(31));Compare(R(11));if(!s.cr6.eq){R(5)=Word(R(31)+4u);R(4)=R(28);Call(0x82bd1950u);R(11)=Address(R(3))&255u;Compare(R(11));if(s.cr6.eq)return true;}
  R(29)=Word(R(31)+8u);R(11)=0x15555555u;Compare(R(29),R(11));
  if(s.cr6.gt)R(30)=~std::uint64_t(0);else{R(11)=Address(R(29))<<1u;R(10)=std::uint64_t(-5);R(11)+=R(29);R(11)=Address(R(11))<<2u;R(30)=R(11)+4;Compare(R(11),R(10));if(s.cr6.gt)R(30)=~std::uint64_t(0);}
  Allocator(0x82bd1994u);R(11)=Word(R(3));R(5)=1;R(4)=R(30);R(11)=Word(R(11));Call(0x82bd19acu);Compare(R(3));if(s.cr6.eq)return false;
  m.WriteU32(Address(R(3)),Address(R(29)));R(30)=R(3)+4;Compare(R(30));if(s.cr6.eq)return false;
  Snapshot();Scatter();Allocator(0x82bd1a78u);R(11)=Word(R(3));R(4)=R(30)-4;R(11)=Word(R(11)+12u);Call(0x82bd1a8cu);return true;
 }
};
}
bool Apply(GuestAddress entry,GuestMemory& m,GuestServices& guest,Registers& s){if(entry!=0x82bd1900u)return false;Gather g{m,guest,s};g.Enter();const bool success=g.Run();g.Leave(success);return true;}
}
