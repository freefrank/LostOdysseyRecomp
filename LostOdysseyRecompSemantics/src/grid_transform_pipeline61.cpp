#include "lo_semantics/grid_transform_pipeline61.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/grid_storage_initialize61.h"
#include "lo_semantics/grid_neighbor_update61.h"
#include "lo_semantics/grid_transform_routes61.h"
#include "lo_semantics/grid_transform_support61.h"
#include "lo_semantics/object_grid_transform61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::grid_transform_pipeline61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
constexpr std::uint64_t TextPage=0xffffffff820d0000ull,GlobalPage=0xffffffff832e0000ull;
std::int32_t Signed(std::uint64_t v){return std::bit_cast<std::int32_t>(Address(v));}
std::uint64_t Product(std::uint64_t a,std::uint64_t b){return std::uint64_t(std::int64_t(Signed(a))*std::int64_t(Signed(b)));}
double Single(double v){return static_cast<float>(v);}
struct Pipeline {
 GuestMemory& m;Dependencies d;Registers& s;VectorState& vectors;
 std::uint64_t& R(unsigned i){return s.r[i];}
 double F(unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);}
 void F(unsigned i,double v){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(v);}
 double Load(std::uint64_t a){return std::bit_cast<float>(m.ReadU32(Address(a)));}
 void Store(unsigned off,unsigned i){m.WriteU32(Address(R(1)+off),std::bit_cast<std::uint32_t>(static_cast<float>(F(i))));}
 void FloatMode(){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;d.read.fp.SetHostFpControl(s.cached_fp_control);}}
 void Compare(std::uint64_t a,std::uint64_t b,bool sign=false){
  if(sign){const auto x=Signed(a),y=Signed(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};}
  else {const auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};}}
 void Indirect(GuestAddress lr){s.ctr=R(11);s.lr=lr;d.guest.CallIndirect(Address(s.ctr)&~3u,m,s,vectors,d.spatial.diagnostics.machine);}
 void Lower(GuestAddress entry,GuestAddress lr){
  s.lr=lr;
  switch(entry){
  case 0x82bd09c0u:case 0x82bd09f8u:(void)crt_reader_units61::Apply(entry,m,d.write.engine.sort.guest,s);break;
  case 0x82b9c298u:case 0x82bd18c0u:(void)diagnostic_format_routes61::Apply(entry,m,d.spatial.diagnostics,s);break;
  case 0x82bb1e58u:(void)grid_storage_initialize61::Apply(entry,m,{d.write.engine.sort.guest,d.read.fp},s);break;
  case 0x82bb2098u:(void)object_grid_read61::Apply(entry,m,d.read,s);break;
  case 0x82bb23c0u:(void)grid_neighbor_update61::Apply(entry,m,s);break;
  case 0x82bd7950u:(void)grid_transform_routes61::Apply(entry,m,s);break;
  case 0x82bd7a60u:(void)transform_owner_build61::Apply(entry,m,d.spatial,s);break;
  case 0x82bd7a20u:(void)transform_owner_routes61::Apply(entry,m,d.spatial.owner,s);break;
  case 0x822d3068u:break; // Already mapped empty leaf, no additional credit.
  case 0x82b7bc40u:crt_reader_chain61::ApplySupport_B7BC40(m,d.write.engine.sort.accepted,s);break;
  case 0x82bb03b0u:(void)object_grid_probe61::Apply(entry,m,d.probe,s,vectors);break;
  case 0x82bb06d8u:(void)object_grid_transform61::Apply(entry,m,d.read.fp,s);break;
  case 0x82bafec0u:(void)object_sort_dispatch61::Apply(entry,m,d.write,s);break;
  default:(void)grid_transform_support61::Apply(entry,m,d.read.fp,s);break;
  }
 }
 void Enter(){
  R(12)=s.lr;s.lr=0x82bb2640u;
  for(unsigned i=14;i<32;++i)WriteU64(m,Address(R(1)-16u-8u*(31u-i)),R(i));
  m.WriteU32(Address(R(1)-8u),Address(R(12)));
  R(12)=R(1)-152u;s.lr=0x82bb2648u;
  for(unsigned i=26;i<32;++i)WriteU64(m,Address(R(12)-8u*(32u-i)),s.fpr_bits[i]);
  const auto old=R(1);R(1)-=640u;m.WriteU32(Address(R(1)),Address(old));
  R(31)=R(3);R(27)=R(4);R(17)=R(5);R(26)=R(6);R(16)=R(7);R(29)=R(8);R(30)=R(9);
  m.WriteU32(Address(R(1)+668u),Address(R(4)));m.WriteU32(Address(R(1)+684u),Address(R(6)));m.WriteU32(Address(R(1)+692u),Address(R(7)));
 }
 void Leave(std::uint64_t result,GuestAddress continuation){
  R(3)=result;R(1)+=640u;R(12)=R(1)-152u;s.lr=continuation;
  for(unsigned i=26;i<32;++i)s.fpr_bits[i]=ReadU64(m,Address(R(12)-8u*(32u-i)));
  for(unsigned i=14;i<32;++i)R(i)=ReadU64(m,Address(R(1)-16u-8u*(31u-i)));
  R(12)=m.ReadU32(Address(R(1)-8u));s.lr=R(12);
 }
 void Report(unsigned severity,unsigned text,unsigned line,GuestAddress continuation,bool existing_file=false){
  R(10)=m.ReadU32(Address(R(30)));R(11)=TextPage;R(7)=line;R(5)=R(11)+text;
  if(existing_file)R(6)=R(28);else {R(11)=TextPage;R(6)=R(11)+25108u;}
  R(4)=severity;R(3)=R(30);R(11)=m.ReadU32(Address(R(10)));Indirect(continuation);
 }
 // Returns true when the load/argument branch has already finished the call.
 bool InitializeOrRead(){
  Compare(R(27),0u);if(s.cr6.eq){Compare(R(30),0u);if(!s.cr6.eq)Report(1,25064,997,0x82bb26b0u);Leave(0,0x82bb26c0u);return true;}
  Compare(R(16),0u);R(11)=TextPage;R(28)=R(11)+25108u;
  if(!s.cr6.eq){R(11)=Address(R(29))&255u;Compare(R(11),0u);
   if(!s.cr6.eq){
    constexpr std::array<unsigned,4> magic{80,77,65,80};
    constexpr std::array<GuestAddress,4> return_to{0x82bb26e8u,0x82bb26fcu,0x82bb2710u,0x82bb2724u};
    for(unsigned i=0;i<4;++i){R(3)=R(16);Lower(0x82bd09c0u,return_to[i]);R(11)=Address(R(3))&255u;Compare(R(11),magic[i]);
     if(!s.cr6.eq){Compare(R(30),0u);if(!s.cr6.eq)Report(4,24864,1008,0x82bb283cu);Leave(0,0x82bb284cu);return true;}}
    R(3)=R(16);Lower(0x82bd09f8u,0x82bb2738u);Compare(R(3),4u);
    if(!s.cr6.eq){Compare(R(30),0u);if(!s.cr6.eq)Report(4,24992,1015,0x82bb2774u,true);Leave(0,0x82bb2784u);return true;}
    R(11)=TextPage;R(6)=0;R(7)=R(11)+24928u;R(5)=1019;R(4)=R(28);R(3)=206;Lower(0x82b9c298u,0x82bb27a4u);
    R(3)=R(16);Lower(0x82bd09f8u,0x82bb27acu);R(17)=R(3);
   }
  }
  R(5)=R(27)+128u;R(4)=R(17);R(3)=R(31);Lower(0x82bb1e58u,0x82bb27c0u);
  m.WriteU32(Address(R(31)+112u),Address(R(27)));R(11)=Address(R(29))&255u;Compare(R(11),0u);
  if(!s.cr6.eq){R(5)=R(16);R(4)=R(26);R(3)=R(31);Lower(0x82bb2098u,0x82bb27e0u);R(11)=Address(R(3))&255u;Compare(R(11),0u);
   if(!s.cr6.eq){R(3)=R(31);Lower(0x82bb23c0u,0x82bb27f4u);Leave(1,0x82bb2804u);return true;}}
  return false;
 }
 bool BuildSpatialIndex(){
  R(11)=~std::uint64_t{0};R(3)=R(1)+192u;m.WriteU32(Address(R(1)+80u),Address(R(11)));Lower(0x82f2b308u,0x82bb2864u);
  R(11)=m.ReadU32(Address(R(27)+4u));R(3)=R(1)+192u;m.WriteU32(Address(R(1)+204u),Address(R(11)));
  R(11)=m.ReadU32(Address(R(27)+8u));m.WriteU32(Address(R(1)+200u),Address(R(11)));
  R(5)=m.ReadU32(Address(R(27)+12u));R(4)=m.ReadU32(Address(R(27)+16u));Lower(0x82bd18c0u,0x82bb2884u);
  R(3)=R(1)+128u;Lower(0x82bd1278u,0x82bb288cu);R(11)=R(1)+192u;R(22)=0;
  m.WriteU8(Address(R(1)+153u),0);m.WriteU8(Address(R(1)+154u),0);m.WriteU8(Address(R(1)+155u),0);m.WriteU32(Address(R(1)+128u),Address(R(11)));
  R(11)=1;m.WriteU8(Address(R(1)+152u),1);m.WriteU32(Address(R(1)+132u),1);R(11)=34;m.WriteU32(Address(R(1)+136u),34);
  R(3)=R(1)+160u;Lower(0x82bd7950u,0x82bb28bcu);R(4)=R(1)+128u;R(3)=R(1)+160u;Lower(0x82bd7a60u,0x82bb28c8u);
  R(11)=Address(R(3))&255u;Compare(R(11),0u);
  if(!s.cr6.eq)return true;
  Compare(R(30),0u);if(!s.cr6.eq)Report(4,24812,1093,0x82bb2904u,true);
  R(3)=R(1)+160u;Lower(0x82bd7a20u,0x82bb290cu);R(3)=R(1)+192u;Lower(0x822d3068u,0x82bb2914u);Leave(0,0x82bb2924u);return false;
 }
 void Coordinate(unsigned reg,unsigned spill,unsigned fpr){
  R(11)=Address(R(reg));WriteU64(m,Address(R(1)+spill),R(11));FloatMode();
  F(0,double(std::bit_cast<std::int64_t>(ReadU64(m,Address(R(1)+spill)))));F(fpr,Single(F(0)));
 }
 void ProbeCell(){
  Coordinate(23,296,12);
  // Physical coordinates retain the original single rounding at each stage.
  F(0,Load(R(31)+76));F(13,Load(R(31)+40));F(11,Load(R(31)+80));
  F(10,Load(R(31)+44));F(9,Load(R(31)+48));F(8,Load(R(31)+28));
  F(7,Load(R(31)+32));F(11,Single(F(11)*F(27)-F(10)));
  F(0,Single(F(12)*F(0)-F(13)));F(10,Load(R(31)+84));
  F(31,Single(F(11)+F(7)));F(11,Load(R(31)+36));
  F(10,Single(F(26)*F(10)-F(9)));F(29,Single(F(0)+F(8)));F(30,Single(F(10)+F(11)));
  R(11)=m.ReadU8(Address(R(28)+R(22)));Compare(R(11),0);
  if(s.cr6.eq){R(5)=3;R(4)=R(1)+112;R(3)=R(27);Store(112,29);Store(116,31);Store(120,30);
   Lower(0x82bb03b0u,0x82bb2a24u);R(30)=R(3);R(11)=R(3)+1u;m.WriteU8(Address(R(28)+R(22)),std::uint8_t(R(11)));
  }else {R(11)-=1;R(11)=std::countl_zero(Address(R(11)));R(11)=(Address(R(11))>>5u)&1u;R(30)=R(11)^1u;}
  R(7)=R(1)+80u;R(6)=0;R(5)=R(1)+160u;R(4)=R(1)+96u;R(3)=R(1)+368u;
  Store(96,29);Store(100,31);Store(104,30);Lower(0x82bd78e8u,0x82bb2a68u);
  R(29)=Address(R(30))&255u;Compare(R(29),0);
  if(!s.cr6.eq){R(11)=m.ReadU32(Address(R(31)+108u));R(10)=m.ReadU32(Address(R(1)+424u));m.WriteU32(Address(R(11)+R(19)),Address(R(10)));}
 }
 void RadiusBounds(){
  F(28,Load(R(1)+412u));
  constexpr std::array<unsigned,3> scales{64,68,72},slots{88,92,84};
  constexpr std::array<GuestAddress,3> lr{0x82bb2a90u,0x82bb2ab0u,0x82bb2ad0u};
  for(unsigned axis=0;axis<3;++axis){F(0,Load(R(31)+scales[axis]));if(axis==0)F(1,Single(F(0)*F(28)));else {F(0,Single(F(0)*F(28)));F(1,F(0));}
   Lower(0x822c5128u,lr[axis]);F(13,Single(F(1)));
   const double v=F(13);const auto converted=std::isfinite(v)&&v>=-2147483648.0&&v<2147483648.0?static_cast<std::int32_t>(v):std::numeric_limits<std::int32_t>::min();
   s.fpr_bits[13]=std::uint64_t(std::int64_t(converted));R(11)=R(1)+slots[axis];m.WriteU32(Address(R(11)),Address(s.fpr_bits[13]));}
  F(0,Single(F(28)*F(28)));
  R(10)=m.ReadU32(Address(R(1)+88u));R(4)=R(23)-R(10);Compare(R(4),0,true);if(s.cr6.lt)R(4)=0;
  R(9)=m.ReadU32(Address(R(1)+92u));R(24)=R(20)-R(9);Compare(R(24),0,true);if(s.cr6.lt)R(24)=0;
  R(8)=m.ReadU32(Address(R(1)+84u));R(11)=R(18)-R(8);Compare(R(11),0,true);if(s.cr6.lt)R(11)=0;
  R(30)=R(10)+R(23);Compare(R(30),R(21),true);if(s.cr6.gt)R(30)=R(21);
  R(26)=R(9)+R(20);Compare(R(26),R(21),true);if(s.cr6.gt)R(26)=R(21);
  R(25)=R(8)+R(18);Compare(R(25),R(21),true);if(s.cr6.gt)R(25)=R(21);
 }
 // Four original scalar unroll lanes plus a remainder share the same sphere
 // predicate. Layout records retain their distinct guest spill/FPR scratch.
 void PropagateCandidate(unsigned lane){
  constexpr std::array<std::array<unsigned,3>,5> spills{{{312,328,344},{240,352,304},{216,336,272},{288,320,224},{232,248,264}}};
  const bool tail=lane==4,swapped=lane==1||lane==3;
  const auto at=Address(R(11)+(lane==0?~std::uint64_t{0}:lane<4?lane-1u:0u));
  const unsigned scratch=tail?10:16;R(scratch)=m.ReadU8(at);Compare(R(scratch),0);if(!s.cr6.eq)return;
  const auto x=Address(lane==1?R(10)-1u:lane==2?R(10):lane==3?R(10)+1u:R(9));
  R(scratch)=x;R(tail?6:15)=Address(R(8));R(tail?5:14)=Address(R(7));
  const auto layout=spills[lane];
  WriteU64(m,Address(R(1)+layout[0]),x);WriteU64(m,Address(R(1)+layout[1]),R(tail?6:15));WriteU64(m,Address(R(1)+layout[2]),R(tail?5:14));
  FloatMode();const double xc=Single(double(std::bit_cast<std::int64_t>(ReadU64(m,Address(R(1)+layout[0])))));
  const double yc=Single(double(std::bit_cast<std::int64_t>(ReadU64(m,Address(R(1)+layout[1])))));
  const double zc=Single(double(std::bit_cast<std::int64_t>(ReadU64(m,Address(R(1)+layout[2])))));
  F(4,swapped?yc:xc);F(3,swapped?xc:yc);F(2,zc);
  F(13,Load(R(31)+76));F(12,Load(R(31)+40));F(11,Load(R(31)+80));F(10,Load(R(31)+44));
  F(9,Load(R(31)+28));F(8,Load(R(31)+32));F(7,Load(R(31)+84));F(6,Load(R(31)+48));F(5,Load(R(31)+36));
  const double dx=Single(Single(Single(xc*F(13)-F(12))+F(9))-F(29));
  const double dy=Single(Single(Single(yc*F(11)-F(10))+F(8))-F(31));
  const double zoffset=Single(zc*F(7)-F(6));const double dz=Single(Single(zoffset+F(5))-F(30));
  const double squarex=Single(dx*dx),squarexy=Single(dy*dy+squarex);
  F(11,dz);F(12,swapped?squarex:dy);if(swapped)F(10,zoffset);
  F(13,Single(dz*dz+squarexy));const auto a=F(13),b=F(0);
  s.cr6={std::uint8_t(a<b),std::uint8_t(a>b),std::uint8_t(a==b),std::uint8_t(std::isnan(a)||std::isnan(b))};
  if(!s.cr6.lt)return;
  R(scratch)=R(29)+1u;m.WriteU8(at,std::uint8_t(R(scratch)));
 }
 void PropagateSphere(){
  Compare(R(11),R(25),true);R(7)=R(11);if(s.cr6.gt)return;
  do{
   Compare(R(24),R(26),true);R(11)=m.ReadU32(Address(R(31)+92u));R(3)=Product(R(11),R(7));
   R(8)=R(24);
   if(!s.cr6.gt){R(11)=R(30)-R(4);R(27)=R(11)+1u;do{
    R(11)=m.ReadU32(Address(R(31)+88u));R(5)=Product(R(11),R(8));R(9)=R(4);Compare(R(27),4,true);
    if(!s.cr6.lt){R(6)=R(30)-3u;R(10)=R(4)+2u;R(11)=R(5)+R(3);R(11)+=R(4);R(11)+=R(28);R(11)+=1;
     do{for(unsigned lane=0;lane<4;++lane)PropagateCandidate(lane);R(9)+=4u;R(10)+=4u;R(11)+=4u;Compare(R(9),R(6),true);}while(!s.cr6.gt);}
    Compare(R(9),R(30),true);if(!s.cr6.gt){R(11)=R(9)+R(5);R(11)+=R(3);R(11)+=R(28);
     do{PropagateCandidate(4);R(9)+=1;R(11)+=1;Compare(R(9),R(30),true);}while(!s.cr6.gt);}
    R(8)+=1;Compare(R(8),R(26),true);
   }while(!s.cr6.gt);}
   R(7)+=1;Compare(R(7),R(25),true);
  }while(!s.cr6.gt);
  R(16)=m.ReadU32(Address(R(1)+692u));R(27)=m.ReadU32(Address(R(1)+668u));
 }
 void ClassifyGrid(){
  R(3)=R(1)+368u;Lower(0x82bd78c0u,0x82bb2930u);
  R(10)=Product(R(17),R(17));R(11)=GlobalPage;R(30)=Product(R(10),R(17));R(3)=m.ReadU32(Address(R(11)-2744u));
  R(5)=1;R(4)=R(30);R(11)=m.ReadU32(Address(R(3)));R(11)=m.ReadU32(Address(R(11)+8u));Indirect(0x82bb2958u);
  R(28)=R(3);R(5)=R(30);R(4)=0;Lower(0x82b7bc40u,0x82bb2968u);
  R(18)=R(22);R(21)=R(17)-1u;Compare(R(17),0,true);if(!s.cr6.gt)return;
  do{Coordinate(18,256,26);R(20)=0;
   do{Coordinate(20,280,27);R(19)=Address(R(22))<<2u;R(23)=0;
    do{ProbeCell();RadiusBounds();PropagateSphere();R(23)+=1;R(19)+=4;R(22)+=1;Compare(R(23),R(17),true);}while(s.cr6.lt);
    R(20)+=1;Compare(R(20),R(17),true);
   }while(s.cr6.lt);
   R(18)+=1;Compare(R(18),R(17),true);
  }while(s.cr6.lt);
  R(26)=m.ReadU32(Address(R(1)+684u));
 }
 void EncodeAndRelease(){
  Compare(R(28),0);if(!s.cr6.eq){R(11)=GlobalPage;R(4)=R(28);R(3)=m.ReadU32(Address(R(11)-2744u));
   R(11)=m.ReadU32(Address(R(3)));R(11)=m.ReadU32(Address(R(11)+20u));Indirect(0x82bb2fb4u);}
  R(4)=R(1)+160u;R(3)=R(31);Lower(0x82bb06d8u,0x82bb2fc0u);
  R(5)=R(16);R(4)=R(26);R(3)=R(31);Lower(0x82bafec0u,0x82bb2fd0u);
  R(3)=R(31);Lower(0x82bb23c0u,0x82bb2fd8u);
  R(3)=R(1)+368u;Lower(0x82bd78d8u,0x82bb2fe0u);
  R(3)=R(1)+160u;Lower(0x82bd7a20u,0x82bb2fe8u);
  R(3)=R(1)+192u;Lower(0x822d3068u,0x82bb2ff0u);Leave(1,0x82bb3000u);
 }
};
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& scalar,VectorState& vectors){
 if(entry!=0x82bb2638u)return false;
 Pipeline p{memory,dependencies,scalar,vectors};p.Enter();if(p.InitializeOrRead())return true;
 if(!p.BuildSpatialIndex())return true;p.ClassifyGrid();p.EncodeAndRelease();return true;
}
}
