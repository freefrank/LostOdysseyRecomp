#include "lo_semantics/owned_tree_mesh_build61.h"
#include "lo_semantics/owned_tree_reorder_support61.h"
#include "lo_semantics/grid_transform_buffer61.h"
#include "lo_semantics/owned_tree_construct61.h"
#include "lo_semantics/transform_owner_initialize61.h"
#include "lo_semantics/record_snapshot_gather61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::owned_tree_mesh_build61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
struct Build {
 GuestMemory& m;Dependencies deps;Registers& s;
 std::uint64_t& R(unsigned i){return s.r[i];}
 std::uint32_t Word(std::uint64_t a){return m.ReadU32(Address(a));}
 void Store(std::uint64_t a,std::uint64_t v){m.WriteU32(Address(a),Address(v));}
 void Compare(std::uint64_t a,std::uint64_t b=0){auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};}
 bool Success(){R(11)=Address(R(3))&255u;Compare(R(11));return !s.cr6.eq;}
 void Call(unsigned target,GuestAddress lr){s.ctr=R(target);s.lr=lr;deps.guest.CallIndirect(Address(s.ctr)&~3u,m,s);}
 void Lower(GuestAddress entry,GuestAddress lr){s.lr=lr;switch(entry){
 case 0x82bd0798u:(void)crt_close_recursive_buffer_context::Apply(entry,m,deps.guest,s);break;
 case 0x82bd17f0u:case 0x82bd1830u:case 0x82bdac60u:(void)grid_transform_buffer61::Apply(entry,m,s);break;
 case 0x82bd20f0u:case 0x82bdb1c0u:case 0x82bdb208u:(void)owned_tree_reorder_support61::Apply(entry,m,deps,s);break;
 case 0x82bdad18u:(void)owned_tree_construct61::Apply(entry,m,{deps.guest,deps.fp},s);break;
 case 0x82bd1900u:(void)record_snapshot_gather61::Apply(entry,m,deps.guest,s);break;
 case 0x82b7a0b0u:(void)crt_copy_full_context::Apply(entry,m,s);break;
 case 0x82bd2a08u:case 0x82bd2c08u:(void)object_sort_support61::Apply(entry,m,{deps.guest,deps.fp},s);break;
 case 0x82bd12f8u:(void)transform_owner_initialize61::Apply(entry,m,deps,s);break;
 case 0x82bdb260u:(void)manager_release_context61::Apply(entry,m,deps,s);break;
 case 0x82bd1af8u:(void)crt_reader_follow61::Apply(entry,m,deps.guest,s);break;
 }}
 void Enter(){R(12)=s.lr;s.lr=0x82bd22b0u;for(unsigned i=19;i<32;++i)WriteU64(m,Address(R(1)-16u-8u*(31u-i)),R(i));Store(R(1)-8,R(12));auto old=R(1);R(1)-=384;Store(R(1),old);R(21)=R(4);R(28)=R(3);}
 void Leave(){R(1)+=384;for(unsigned i=19;i<32;++i)R(i)=ReadU64(m,Address(R(1)-16u-8u*(31u-i)));R(12)=Word(R(1)-8);s.lr=R(12);}
 void FreeBounds(GuestAddress lr){R(3)=R(1)+80;Lower(0x82bd1af8u,lr);}
 void Finish(){R(11)=m.ReadU8(Address(R(21)+26));Compare(R(11));if(s.cr6.eq){R(30)=Word(R(28)+12);Compare(R(30));if(!s.cr6.eq){R(3)=R(30);Lower(0x82bdb260u,0x82bd27c4u);Lower(0x82bd0798u,0x82bd27c8u);R(11)=Word(R(3));R(4)=R(30);R(11)=Word(R(11)+12);Call(11,0x82bd27dcu);Store(R(28)+12,R(31));}}
  FreeBounds(0x82bd27e8u);R(3)=R(19);
 }
 void FreeSecond(){R(3)=R(24);Lower(0x82bdb260u,0x82bd278cu);Lower(0x82bd0798u,0x82bd2790u);R(11)=Word(R(3));R(4)=R(24);R(11)=Word(R(11)+12);Call(11,0x82bd27a4u);}
 bool FirstTree(){
  Lower(0x82bd0798u,0x82bd230cu);R(11)=Word(R(3));R(5)=24;R(4)=28;R(11)=Word(R(11));Call(11,0x82bd2324u);Compare(R(3));if(!s.cr6.eq)Lower(0x82bdac60u,0x82bd2330u);else R(3)=R(31);
  Compare(R(3));Store(R(28)+12,R(3));if(s.cr6.eq)return false;
  R(9)=0xffffffff82000000ull;R(11)=Word(R(21));R(10)=0xffffffff820d0000ull;
  Store(R(1)+136,R(31));R(20)=1;Store(R(1)+172,R(31));R(10)+=27732;Store(R(1)+140,R(31));R(30)=~std::uint64_t(0);Store(R(1)+176,R(31));
  if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;deps.fp.SetHostFpControl(s.cached_fp_control);}s.fpr_bits[0]=std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(R(9)+3664))));
  R(9)=0x7fff0000u;Store(R(1)+184,R(11));Store(R(1)+112,R(10));R(9)|=65535;Store(R(1)+116,R(20));Store(R(1)+128,R(30));R(26)=R(21)+4;Store(R(1)+180,R(31));R(10)=5;
  Store(R(1)+124,std::bit_cast<std::uint32_t>(static_cast<float>(std::bit_cast<double>(s.fpr_bits[0]))));Store(R(1)+132,std::bit_cast<std::uint32_t>(static_cast<float>(std::bit_cast<double>(s.fpr_bits[0]))));Store(R(1)+120,R(9));R(9)=R(1)+116;R(11)=Word(R(11)+8);Store(R(1)+136,R(11));R(11)=R(26);s.ctr=R(10);
  do{R(10)=Word(R(11));R(11)+=4;Store(R(9),R(10));R(9)+=4;--s.ctr;}while(Address(s.ctr));
  R(11)=8;R(4)=R(1)+112;Store(R(1)+116,R(11));Lower(0x82bdad18u,0x82bd23d8u);return true;
 }
 void CountLeaves(){R(10)=0xffffffff820d0000ull;R(3)=Word(R(28)+12);R(11)=0xffffffff82bd0000ull;R(23)=R(10)+25268;R(5)=R(1)+80;R(4)=R(11)+6992;Store(R(1)+80,R(31));Store(R(1)+112,R(23));Lower(0x82bdb1c0u,0x82bd2418u);R(11)=Word(R(1)+80);Compare(R(11),1);Store(R(28)+20,R(11));}
 bool ExportLeaves(){
  Lower(0x82bd0798u,0x82bd2440u);R(11)=Word(R(1)+80);R(9)=Word(R(3));R(5)=1;R(10)=Address(R(11))<<1u;R(11)+=R(10);R(10)=Word(R(9));R(4)=Address(R(11))<<3u;Call(10,0x82bd2464u);Store(R(1)+84,R(3));Compare(R(3));if(s.cr6.eq)return false;
  R(11)=0x3fff0000u;R(22)=std::uint64_t(-5);R(25)=R(11)|65535;R(11)=Word(R(1)+80);R(27)=R(11);Compare(R(11),R(25));if(s.cr6.gt)R(29)=R(30);else{R(11)=Address(R(11))<<2u;Compare(R(11),R(22));R(29)=R(11)+4;if(s.cr6.gt)R(29)=R(30);}
  Lower(0x82bd0798u,0x82bd24a4u);R(11)=Word(R(3));R(5)=39;R(4)=R(29);R(11)=Word(R(11));Call(11,0x82bd24bcu);Compare(R(3));if(!s.cr6.eq){Store(R(3),R(27));R(11)=R(3)+4;}else R(11)=R(31);
  Compare(R(11));Store(R(28)+24,R(11));if(s.cr6.eq)return false;
  Store(R(1)+88,R(11));R(3)=Word(R(28)+12);R(10)=0xffffffff82bd0000ull;Store(R(1)+80,R(31));R(4)=R(10)+8552;R(5)=R(1)+80;R(11)=Word(R(3));Store(R(1)+92,R(11));Lower(0x82bdb1c0u,0x82bd2504u);return true;
 }
 void ReorderSource(){R(11)=m.ReadU8(Address(R(21)+27));R(29)=R(20);Compare(R(11));if(!s.cr6.eq){R(11)=Word(R(28)+12);R(3)=Word(R(21));R(10)=Word(R(11)+4);R(5)=Word(R(11));R(4)=Word(R(10)+36);Lower(0x82bd1900u,0x82bd252cu);if(Success())R(29)=R(31);}
  R(11)=Address(R(29))&255u;Compare(R(11));if(s.cr6.eq)return;
  R(11)=Word(R(28)+12);R(11)=Word(R(11)+4);R(11)=Word(R(11)+36);Store(R(28)+28,R(11));Lower(0x82bd0798u,0x82bd255cu);R(11)=Word(R(3));R(5)=62;R(4)=Word(R(28)+28);R(4)=Address(R(4))<<2u;R(11)=Word(R(11));Call(11,0x82bd2578u);Store(R(28)+32,R(3));R(11)=Word(R(28)+12);R(10)=Word(R(28)+28);R(4)=Word(R(11));R(5)=Address(R(10))<<2u;Lower(0x82b7a0b0u,0x82bd2590u);
 }
 bool SecondTree(){Lower(0x82bd0798u,0x82bd2594u);R(11)=Word(R(3));R(5)=24;R(4)=28;R(11)=Word(R(11));Call(11,0x82bd25acu);Compare(R(3));if(s.cr6.eq)return false;
  Lower(0x82bdac60u,0x82bd25b8u);R(24)=R(3);Compare(R(3));if(s.cr6.eq)return false;
  R(11)=0xffffffff820d0000ull;Store(R(1)+252,R(31));R(11)+=25344;Store(R(1)+220,R(31));Store(R(1)+256,R(31));Store(R(1)+260,R(31));Store(R(1)+192,R(11));R(9)=5;R(10)=R(1)+196;R(11)=R(26);s.ctr=R(9);
  do{R(9)=Word(R(11));R(11)+=4;Store(R(10),R(9));R(10)+=4;--s.ctr;}while(Address(s.ctr));
  R(11)=Word(R(1)+80);R(3)=R(24);R(4)=R(1)+192;Store(R(1)+196,R(20));Store(R(1)+216,R(11));R(11)=Word(R(1)+84);Store(R(1)+264,R(11));Lower(0x82bdad18u,0x82bd2624u);Store(R(1)+192,R(23));return true;
 }
 bool CollectLeaves(){R(3)=R(1)+96;Lower(0x82bd2a08u,0x82bd263cu);R(11)=0xffffffff82bd0000ull;R(5)=R(1)+96;R(4)=R(11)+7032;R(3)=R(24);Lower(0x82bdb208u,0x82bd2650u);
  R(11)=Word(R(1)+100);R(29)=R(11);R(27)=R(11);Compare(R(11),R(25));if(!s.cr6.gt){R(11)=Address(R(11))<<2u;Compare(R(11),R(22));if(!s.cr6.gt)R(30)=R(11)+4;}
  Lower(0x82bd0798u,0x82bd2678u);R(11)=Word(R(3));R(5)=39;R(4)=R(30);R(11)=Word(R(11));Call(11,0x82bd2690u);Compare(R(3));if(s.cr6.eq)return false;
  Store(R(3),R(27));R(30)=R(3)+4;Compare(R(30));return !s.cr6.eq;
 }
 void RemapLeaves(){R(10)=R(31);Compare(R(29));if(!s.cr6.eq){R(11)=R(31);do{
   R(9)=Word(R(1)+104);R(7)=R(10);R(10)+=1;R(9)=Word(R(9)+R(11));R(8)=Word(R(28)+24);R(9)=Word(R(9)+32);R(6)=Word(R(9));R(6)=Address(R(6))<<2u;R(8)=Word(R(8)+R(6));Compare(R(10),R(29));Store(R(9),R(7));Store(R(30)+R(11),R(8));R(11)+=4;
  }while(s.cr6.lt);}
  R(29)=Word(R(28)+24);Compare(R(29));if(!s.cr6.eq){Lower(0x82bd0798u,0x82bd271cu);R(11)=Word(R(3));R(4)=R(29)-4;R(11)=Word(R(11)+12);Call(11,0x82bd2730u);Store(R(28)+24,R(31));}Store(R(28)+24,R(30));
  R(3)=R(1)+96;Lower(0x82bd2c08u,0x82bd2740u);R(3)=R(28);R(5)=m.ReadU8(Address(R(21)+25));R(4)=m.ReadU8(Address(R(21)+24));Lower(0x82bd12f8u,0x82bd2750u);
  if(!Success())return;R(3)=Word(R(28)+16);R(4)=R(24);R(11)=Word(R(3));R(11)=Word(R(11)+4);Call(11,0x82bd2774u);if(Success())R(19)=R(20);
 }
 void Run(){R(3)=Word(R(4));Compare(R(3));if(s.cr6.eq){R(3)=0;return;}Lower(0x82bd17f0u,0x82bd22ccu);if(!Success()){R(3)=0;return;}R(3)=Word(R(21));Lower(0x82bd1830u,0x82bd22e0u);R(3)=R(28);Lower(0x82bd20f0u,0x82bd22e8u);R(11)=Word(R(21));R(31)=0;R(19)=R(31);Store(R(28)+4,R(11));for(unsigned off:{80u,84u,88u,92u})Store(R(1)+off,R(31));
  if(!FirstTree()){FreeBounds(0x82bd26b8u);R(3)=0;return;}if(!Success()){R(11)=0xffffffff820d0000ull;R(11)+=25268;Store(R(1)+112,R(11));Finish();return;}
  CountLeaves();if(s.cr6.eq){R(11)=Word(R(28)+8);R(19)=R(20);R(11)|=4;Store(R(28)+8,R(11));Finish();return;}
  if(!ExportLeaves()){FreeBounds(0x82bd26b8u);R(3)=0;return;}ReorderSource();if(!SecondTree()){FreeBounds(0x82bd26b8u);R(3)=0;return;}
  if(Success()){if(!CollectLeaves()){R(3)=R(1)+96;Lower(0x82bd2c08u,0x82bd26b0u);FreeBounds(0x82bd26b8u);R(3)=0;return;}RemapLeaves();}
  FreeSecond();Finish();
 }
};
}
bool Apply(GuestAddress entry,GuestMemory& m,Dependencies deps,Registers& s){if(entry!=0x82bd22a8u)return false;Build b{m,deps,s};b.Enter();b.Run();b.Leave();return true;}
}
