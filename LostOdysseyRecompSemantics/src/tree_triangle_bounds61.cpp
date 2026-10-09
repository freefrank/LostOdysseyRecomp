#include "lo_semantics/tree_triangle_bounds61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::tree_triangle_bounds61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
struct Geometry {
 GuestMemory& m;float_triplet_transfer::NativeServices& fp;Registers& s;
 std::uint64_t& R(unsigned i){return s.r[i];}
 double F(unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);}
 void Single(unsigned i,double v){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(static_cast<float>(v)));}
 std::uint32_t Word(std::uint64_t p){return m.ReadU32(Address(p));}
 void Load(unsigned i,std::uint64_t p){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));}
 void Store(unsigned i,std::uint64_t p){m.WriteU32(Address(p),std::bit_cast<std::uint32_t>(static_cast<float>(F(i))));}
 void Flush(){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;fp.SetHostFpControl(s.cached_fp_control);}}
 void CompareZero(std::uint64_t v){const auto x=Address(v);s.cr6={0u,std::uint8_t(x!=0),std::uint8_t(x==0),s.xer_so};}
 // Equal/unordered comparisons select the incoming value, as the branch does.
 void Extend(unsigned bound,unsigned value,bool maximum){const auto a=F(bound),b=F(value);const bool u=std::isnan(a)||std::isnan(b);s.cr6={std::uint8_t(!u&&a<b),std::uint8_t(!u&&a>b),std::uint8_t(!u&&a==b),std::uint8_t(u)};
  if(!(maximum?s.cr6.gt:s.cr6.lt))s.fpr_bits[bound]=s.fpr_bits[value];}
 std::uint64_t Twice(std::uint64_t v){return(v<<1u)&0xfffffffeu;}
 std::uint64_t Words(std::uint64_t v){return(v<<2u)&0xfffffffcu;}
 void Bounds(){
  WriteU64(m,Address(R(1)-8u),R(31));Flush();WriteU64(m,Address(R(1)-16u),s.fpr_bits[31]);
  CompareZero(R(4));bool valid=!s.cr6.eq;if(valid){CompareZero(R(5));valid=!s.cr6.eq;}
  if(valid){R(10)=0xffffffff82000000ull;R(11)=Word(R(3)+72u);Load(4,R(10)+3596u);R(10)=0xffffffff82000000ull;R(31)=Word(R(11)+16u);s.fpr_bits[31]=s.fpr_bits[4];R(8)=Word(R(11)+20u);s.fpr_bits[1]=s.fpr_bits[4];Load(5,R(10)+3428u);s.fpr_bits[2]=s.fpr_bits[5];s.fpr_bits[3]=s.fpr_bits[5];
   do{
    R(11)=Word(R(4));--R(5);R(4)+=4u;R(10)=Twice(R(11));R(11)+=R(10);R(11)=Words(R(11));R(11)+=R(31);
    R(10)=Word(R(11));R(9)=Word(R(11)+4u);R(7)=Twice(R(10));R(11)=Word(R(11)+8u);R(3)=Twice(R(9));R(10)+=R(7);R(7)=Twice(R(11));R(10)=Words(R(10));R(9)+=R(3);R(10)+=R(8);R(11)+=R(7);R(9)=Words(R(9));R(11)=Words(R(11));R(9)+=R(8);
    Load(6,R(10));R(11)+=R(8);Extend(4,6,false);Load(7,R(10)+4u);Extend(31,7,false);Load(8,R(10)+8u);Extend(1,8,false);
    Load(9,R(9));Extend(4,9,false);Load(10,R(9)+4u);Extend(31,10,false);Load(11,R(9)+8u);Extend(1,11,false);
    Load(12,R(11));Extend(4,12,false);Load(13,R(11)+4u);Extend(31,13,false);Load(0,R(11)+8u);Extend(1,0,false);
    for(unsigned vertex=0u;vertex<3u;++vertex){Extend(5,6u+3u*vertex,true);Extend(2,7u+3u*vertex,true);Extend(3,vertex==2u?0u:8u+3u*vertex,true);}
    CompareZero(R(5));
   }while(!s.cr6.eq);
   Store(4,R(6));R(3)=1u;Store(31,R(6)+4u);Store(1,R(6)+8u);Store(5,R(6)+12u);Store(2,R(6)+16u);Store(3,R(6)+20u);
  }else R(3)=0u;
  s.fpr_bits[31]=ReadU64(m,Address(R(1)-16u));R(31)=ReadU64(m,Address(R(1)-8u));
 }
 void AxisCentroid(){
  R(10)=Word(R(3)+72u);R(8)=Twice(R(4));R(11)=Words(R(5));R(8)+=R(4);R(8)=Words(R(8));R(9)=Word(R(10)+16u);R(10)=Word(R(10)+20u);R(9)+=R(8);
  R(8)=Word(R(9)+4u);R(7)=Word(R(9)+8u);R(5)=Twice(R(8));R(9)=Word(R(9));R(6)=Twice(R(7));R(8)+=R(5);R(7)+=R(6);R(8)=Words(R(8));R(7)=Words(R(7));R(8)+=R(10);R(7)+=R(10);R(6)=Twice(R(9));R(9)+=R(6);
  Flush();Load(0,R(11)+R(8));Load(13,R(11)+R(7));R(9)=Words(R(9));Single(0,F(13)+F(0));R(10)=R(9)+R(10);Load(13,R(11)+R(10));R(11)=0xffffffff82000000ull;Single(13,F(0)+F(13));Load(0,R(11)+3872u);Single(1,F(13)*F(0));
 }
 void Centroid(){
  R(11)=Word(R(3)+72u);R(9)=Twice(R(4));R(9)+=R(4);R(9)=Words(R(9));R(10)=Word(R(11)+16u);R(11)=Word(R(11)+20u);R(10)+=R(9);
  R(9)=Word(R(10));R(8)=Word(R(10)+4u);R(7)=Word(R(10)+8u);R(6)=Twice(R(9));R(10)=Twice(R(8));R(9)+=R(6);R(8)+=R(10);R(10)=Words(R(9));R(9)=Words(R(8));R(10)+=R(11);R(9)+=R(11);R(8)=Twice(R(7));R(8)+=R(7);
  Flush();Load(0,R(10));Load(13,R(9));R(8)=Words(R(8));Single(0,F(13)+F(0));Load(12,R(9)+4u);Load(13,R(10)+4u);R(11)=R(8)+R(11);Single(13,F(12)+F(13));Load(11,R(9)+8u);Load(12,R(10)+8u);Single(12,F(11)+F(12));
  Load(11,R(11));Load(10,R(11)+4u);Load(9,R(11)+8u);R(11)=0xffffffff82000000ull;Single(11,F(0)+F(11));Single(13,F(10)+F(13));Load(0,R(11)+3872u);Single(12,F(9)+F(12));
  Single(11,F(11)*F(0));Store(11,R(5));Single(13,F(13)*F(0));Store(13,R(5)+4u);Single(0,F(12)*F(0));Store(0,R(5)+8u);
 }
};
}
bool Apply(GuestAddress entry,GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s){Geometry g{m,fp,s};switch(entry){case 0x82bd88e8u:g.Bounds();return true;case 0x82bd8ac0u:g.AxisCentroid();return true;case 0x82bd8b40u:g.Centroid();return true;default:return false;}}
}
