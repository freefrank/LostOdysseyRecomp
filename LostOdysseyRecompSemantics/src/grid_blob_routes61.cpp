#include "lo_semantics/grid_blob_routes61.h"
#include "lo_semantics/object_sort_lifecycle61.h"
#include "lo_semantics/crt_close_upper61.h"
#include "lo_semantics/crt_reader_cleanup_callers_context.h"
#include "lo_semantics/crt_close_next61.h"
#include "lo_semantics/crt_reader_object_chain61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::grid_blob_routes61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
struct Routes {
 GuestMemory& m;Dependencies d;Registers& s;VectorState& v;
 std::uint64_t& R(unsigned i){return s.r[i];}
 void Compare(std::uint64_t a){s.cr6={0,std::uint8_t(Address(a)!=0u),std::uint8_t(Address(a)==0u),s.xer_so};}
 void Enter(unsigned first,unsigned size,GuestAddress lr){R(12)=s.lr;s.lr=lr;
  for(unsigned i=first;i<32;++i)WriteU64(m,Address(R(1)-16u-8u*(31u-i)),R(i));m.WriteU32(Address(R(1)-8u),Address(R(12)));
  auto old=R(1);R(1)-=size;m.WriteU32(Address(R(1)),Address(old));}
 void Leave(unsigned first,unsigned size,unsigned result){R(3)=result;R(1)+=size;
  for(unsigned i=first;i<32;++i)R(i)=ReadU64(m,Address(R(1)-16u-8u*(31u-i)));R(12)=m.ReadU32(Address(R(1)-8u));s.lr=R(12);}
 void Indirect(GuestAddress lr){s.ctr=R(11);s.lr=lr;d.guest.CallIndirect(Address(s.ctr)&~3u,m,s,v,d.spatial.diagnostics.machine);}
 void Lower(GuestAddress entry,GuestAddress lr){s.lr=lr;switch(entry){
  case 0x82bb25d0u:case 0x82bae1a0u:(void)object_sort_lifecycle61::Apply(entry,m,{d.read.reader.guest,d.read.fp},s);break;
  case 0x82bd0cd0u:(void)crt_close_upper61::Apply(entry,m,d.read.reader,s);break;
  case 0x82bd0a30u:(void)object_grid_read61::Apply(entry,m,d.read,s);break;
  case 0x82bb2638u:(void)grid_transform_pipeline61::Apply(entry,m,d,s,v);break;
  case 0x82b9c298u:(void)diagnostic_format_routes61::Apply(entry,m,d.spatial.diagnostics,s);break;
  case 0x82bd0df0u:(void)crt_reader_cleanup_callers_context::Apply(entry,m,d.read.reader.guest,s);break;
  case 0x82bd0900u:(void)crt_close_next61::Apply(entry,m,d.read.reader.guest,s);break;
  case 0x82bd0ea8u:(void)crt_reader_object_chain61::Apply(entry,m,d.read.reader,s);break;
  case 0x82b7a0b0u:(void)crt_copy_full_context::Apply(entry,m,s);break;
 }}
 void Diagnostic(unsigned text,unsigned line,GuestAddress lr){R(11)=0xffffffff820d0000ull;R(6)=0;R(7)=R(11)+text;
  R(11)=0xffffffff820d0000ull;R(5)=line;R(4)=R(11)+23144u;R(3)=1;Lower(0x82b9c298u,lr);}
 void DeleteAttached(GuestAddress lr){R(11)=m.ReadU32(Address(R(3)));R(4)=1;R(11)=m.ReadU32(Address(R(11)));Indirect(lr);m.WriteU32(Address(R(31)+184u),Address(R(29)));}
 void LoadBlob(){
  Enter(29,144,0x82b9dd98u);R(31)=R(3);R(30)=R(4);R(11)=m.ReadU32(Address(R(4)));Compare(R(11));
  if(!s.cr6.eq){R(11)=m.ReadU32(Address(R(30)+4u));Compare(R(11));}
  if(s.cr6.eq){Diagnostic(23544,388,0x82b9def4u);Leave(29,144,0);return;}
  R(3)=m.ReadU32(Address(R(31)+184u));R(29)=0;Compare(R(3));if(!s.cr6.eq)DeleteAttached(0x82b9dde0u);
  R(11)=0xffffffff832e0000ull;R(5)=32;R(4)=116;R(3)=m.ReadU32(Address(R(11)-2744u));R(11)=m.ReadU32(Address(R(3)));R(11)=m.ReadU32(Address(R(11)+8u));Indirect(0x82b9de04u);
  Compare(R(3));if(!s.cr6.eq){Lower(0x82bb25d0u,0x82b9de10u);R(11)=R(3);}else R(11)=R(29);
  m.WriteU32(Address(R(31)+184u),Address(R(11)));R(5)=m.ReadU32(Address(R(30)+4u));R(4)=m.ReadU32(Address(R(30)));R(3)=R(1)+80u;Lower(0x82bd0cd0u,0x82b9de30u);
  R(4)=0;R(3)=R(1)+80u;Lower(0x82bd0a30u,0x82b9de3cu);
  R(9)=0;R(8)=1;R(7)=R(1)+80u;R(6)=0;R(5)=0;R(4)=R(31);R(3)=m.ReadU32(Address(R(31)+184u));Lower(0x82bb2638u,0x82b9de5cu);
  R(11)=Address(R(3))&255u;Compare(R(11));if(s.cr6.eq){
   R(3)=m.ReadU32(Address(R(31)+184u));Compare(R(3));if(!s.cr6.eq)DeleteAttached(0x82b9de88u);
   Diagnostic(23588,401,0x82b9deacu);R(3)=R(1)+80u;Lower(0x82bd0df0u,0x82b9deb4u);Leave(29,144,0);return;}
  R(3)=R(1)+80u;Lower(0x82bd0df0u,0x82b9dec8u);Leave(29,144,1);
 }
 void SaveBlob(){
  Enter(28,272,0x82ba6100u);R(31)=R(3);R(30)=R(5);R(29)=R(6);R(28)=m.ReadU32(Address(R(4)+8u));
  R(3)=R(1)+112u;Lower(0x82bb25d0u,0x82ba611cu);
  R(5)=0;R(4)=4096;R(3)=R(1)+80u;Lower(0x82bd0cd0u,0x82ba612cu);
  R(9)=R(29);R(8)=0;R(7)=R(1)+80u;R(6)=0;R(5)=R(30);R(4)=R(28);R(3)=R(1)+112u;Lower(0x82bb2638u,0x82ba614cu);
  R(11)=Address(R(3))&255u;Compare(R(11));R(3)=R(1)+80u;
  if(!s.cr6.eq){Lower(0x82bd0900u,0x82ba6160u);m.WriteU32(Address(R(31)),Address(R(3)));
   R(11)=0xffffffff832e0000ull;R(5)=276;R(4)=R(3);R(3)=m.ReadU32(Address(R(11)-2744u));R(11)=m.ReadU32(Address(R(3)));R(11)=m.ReadU32(Address(R(11)+8u));Indirect(0x82ba6184u);
   R(11)=R(3);R(4)=0;R(3)=R(1)+80u;R(30)=m.ReadU32(Address(R(31)));m.WriteU32(Address(R(31)+4u),Address(R(11)));Lower(0x82bd0ea8u,0x82ba619cu);
   R(4)=R(3);R(5)=R(30);R(3)=m.ReadU32(Address(R(31)+4u));Lower(0x82b7a0b0u,0x82ba61acu);
   R(3)=R(1)+80u;Lower(0x82bd0df0u,0x82ba61b4u);R(3)=R(1)+112u;Lower(0x82bae1a0u,0x82ba61bcu);Leave(28,272,1);return;}
  Lower(0x82bd0df0u,0x82ba61ccu);R(3)=R(1)+112u;Lower(0x82bae1a0u,0x82ba61d4u);Leave(28,272,0);
 }
};
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state,VectorState& vectors){
 Routes r{memory,dependencies,state,vectors};if(entry==0x82b9dd90u){r.LoadBlob();return true;}if(entry==0x82ba60f8u){r.SaveBlob();return true;}return false;
}
}
