#include "lo_semantics/owned_tree_expand61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::owned_tree_expand61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
double F(const Registers& s,unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);}
void F(Registers& s,unsigned i,double value){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(value);}
double Single(double value){return static_cast<float>(value);}
void Load(GuestMemory& m,Registers& s,unsigned i,std::uint64_t a){F(s,i,std::bit_cast<float>(m.ReadU32(Address(a))));}
void Store(GuestMemory& m,Registers& s,unsigned i,std::uint64_t a){m.WriteU32(Address(a),std::bit_cast<std::uint32_t>(static_cast<float>(F(s,i))));}
void Flush(Dependencies d,Registers& s){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;d.fp.SetHostFpControl(s.cached_fp_control);}}
void Compare(Registers& s,std::uint64_t a,std::uint64_t b){const auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};}
void CompareFloat(Registers& s,double a,double b){const bool u=std::isnan(a)||std::isnan(b);s.cr6={std::uint8_t(!u&&a<b),std::uint8_t(!u&&a>b),std::uint8_t(!u&&a==b),std::uint8_t(u)};}
std::uint64_t Scale4(std::uint64_t value){return(value<<2u)&0xfffffffcu;}
void ExtendPlane(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;r[11]=m.ReadU8(Address(r[30]+56u));Compare(s,r[11],0u);
    if(!s.cr6.eq){
        r[11]=r[30]+32u;Flush(d,s);
        for(unsigned offset=0u;offset<24u;offset+=4u){Load(m,s,0,r[31]+offset);Store(m,s,0,r[11]+offset);}
        m.WriteU8(Address(r[30]+56u),std::uint8_t(r[29]));
    }
    r[11]=m.ReadU32(Address(r[30]+16u));Flush(d,s);Load(m,s,13,r[31]);Store(m,s,13,r[1]+80u);
    r[10]=r[29];r[9]=r[11]+8u;Load(m,s,13,r[31]+4u);Store(m,s,13,r[1]+84u);r[9]=Scale4(r[9]);
    Load(m,s,13,r[31]+8u);Store(m,s,13,r[1]+88u);Load(m,s,0,r[30]+12u);Load(m,s,13,r[9]+r[30]);CompareFloat(s,F(s,0),F(s,13));
    for(unsigned axis=0u;axis<3u;++axis){Load(m,s,13,r[31]+12u+axis*4u);Store(m,s,13,r[1]+96u+axis*4u);}
    bool extend=s.cr6.lt;
    if(extend)r[9]=r[1]+80u;
    else{
        r[9]=Scale4(r[11]+11u);Flush(d,s);Load(m,s,13,r[9]+r[30]);CompareFloat(s,F(s,0),F(s,13));
        extend=s.cr6.gt;if(extend)r[9]=r[1]+96u;
    }
    if(extend){r[11]=Scale4(r[11]);r[10]=1u;Flush(d,s);Store(m,s,0,r[11]+r[9]);}
    r[11]=r[10]&255u;Compare(s,r[11],0u);
    if(!s.cr6.eq){
        Flush(d,s);
        for(unsigned axis=0u;axis<3u;++axis){Load(m,s,0,r[1]+80u+axis*4u);Store(m,s,0,r[31]+axis*4u);}
        for(unsigned axis=0u;axis<3u;++axis){Load(m,s,0,r[1]+96u+axis*4u);Store(m,s,0,r[31]+12u+axis*4u);}
    }
}
void Inflate(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;Flush(d,s);Load(m,s,0,r[30]+20u);Load(m,s,13,r[31]);Load(m,s,10,r[31]+12u);
    F(s,13,Single(F(s,13)-F(s,0)));Load(m,s,9,r[31]+16u);Load(m,s,8,r[31]+20u);Load(m,s,12,r[31]+4u);Load(m,s,11,r[31]+8u);
    F(s,12,Single(F(s,12)-F(s,0)));Store(m,s,13,r[31]);F(s,13,Single(F(s,10)+F(s,0)));F(s,11,Single(F(s,11)-F(s,0)));
    Store(m,s,12,r[31]+4u);F(s,10,Single(F(s,9)+F(s,0)));Store(m,s,11,r[31]+8u);F(s,0,Single(F(s,8)+F(s,0)));
    Store(m,s,13,r[31]+12u);Store(m,s,10,r[31]+16u);Store(m,s,0,r[31]+20u);
}
void Expand(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bdaa50u;
    for(unsigned i=29u;i<=31u;++i)WriteU64(m,Address(r[1]-16u-8u*(31u-i)),r[i]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller_sp=r[1];r[1]-=144u;m.WriteU32(Address(r[1]),Address(caller_sp));
    r[11]=0xffffffff832e0000ull;r[30]=r[4];r[31]=r[3];r[3]=r[30];r[6]=r[31];
    r[10]=m.ReadU32(Address(r[11]-2728u));r[9]=m.ReadU32(Address(r[30]));++r[10];
    r[4]=m.ReadU32(Address(r[31]+32u));r[5]=m.ReadU32(Address(r[31]+36u));m.WriteU32(Address(r[11]-2728u),Address(r[10]));
    r[11]=m.ReadU32(Address(r[9]+4u));s.ctr=r[11];s.lr=0x82bdaa8cu;d.guest.CallIndirect(Address(s.ctr)&~3u,m,s);
    r[11]=m.ReadU32(Address(r[30]+16u));r[29]=0u;
    const auto axis=std::bit_cast<std::int32_t>(Address(r[11]));s.cr6={std::uint8_t(axis<-1),std::uint8_t(axis>-1),std::uint8_t(axis==-1),s.xer_so};
    if(!s.cr6.eq)ExtendPlane(m,d,s);
    r[11]=m.ReadU32(Address(r[30]+20u));Compare(s,r[11],0u);if(!s.cr6.eq)Inflate(m,d,s);
    r[4]=r[30];r[3]=r[31];s.lr=0x82bdabfcu;(void)owned_tree_build61::Apply(0x82bd9928u,m,d,s);
    r[11]=m.ReadU32(Address(r[31]+24u));r[3]=r[11]&0xfffffffeu;Compare(s,r[3],0u);
    if(!s.cr6.eq){r[29]=r[3]+40u;m.WriteU32(Address(r[3]+28u),Address(r[31]));}
    Compare(s,r[29],0u);if(!s.cr6.eq)m.WriteU32(Address(r[29]+28u),Address(r[31]));
    Compare(s,r[3],0u);if(!s.cr6.eq){r[4]=r[30];s.lr=0x82bdac30u;Expand(m,d,s);}
    Compare(s,r[29],0u);if(!s.cr6.eq){r[4]=r[30];r[3]=r[29];s.lr=0x82bdac44u;Expand(m,d,s);}
    r[10]=m.ReadU32(Address(r[31]+36u));r[11]=m.ReadU32(Address(r[30]+60u));r[11]+=r[10];m.WriteU32(Address(r[30]+60u),Address(r[11]));
    r[1]+=144u;for(unsigned i=29u;i<=31u;++i)r[i]=ReadU64(m,Address(r[1]-16u-8u*(31u-i)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state){if(entry!=0x82bdaa48u)return false;Expand(memory,deps,state);return true;}
}
