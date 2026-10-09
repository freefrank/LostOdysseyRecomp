#include "lo_semantics/object_grid_probe61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::object_grid_probe61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
double F(const Registers& s,unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);}
void F(Registers& s,unsigned i,double x){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(x);}
double Single(double x){return static_cast<float>(x);}
void Load(GuestMemory& m,Registers& s,unsigned i,std::uint64_t p){F(s,i,std::bit_cast<float>(m.ReadU32(Address(p))));}
void Store(GuestMemory& m,Registers& s,unsigned i,std::uint64_t p){m.WriteU32(Address(p),std::bit_cast<std::uint32_t>(static_cast<float>(F(s,i))));}
void Flush(Dependencies d,Registers& s){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;d.query.fp.SetHostFpControl(s.cached_fp_control);}}
void Compare(Registers& s,std::uint64_t value){const auto x=Address(value);s.cr6={0u,std::uint8_t(x!=0u),std::uint8_t(x==0u),s.xer_so};}
void CompareMode(Registers& s,int mode){const auto x=std::bit_cast<std::int32_t>(Address(s.r[5]));s.cr6={std::uint8_t(x<mode),std::uint8_t(x>mode),std::uint8_t(x==mode),s.xer_so};}
void CompareFloat(Registers& s,double a,double b){const bool u=std::isnan(a)||std::isnan(b);s.cr6={std::uint8_t(!u&&a<b),std::uint8_t(!u&&a>b),std::uint8_t(!u&&a==b),std::uint8_t(u)};}
void Random(GuestMemory& m,Dependencies d,Registers& s,GuestAddress lr){s.lr=lr;(void)crt_random_thread61::Apply(0x82bd2c48u,m,d.random,s);}
void SpillRandom(GuestMemory& m,Registers& s,unsigned offset){s.r[11]=Address(s.r[3]);WriteU64(m,Address(s.r[1]+offset),s.r[11]);}
void ConvertRandom(GuestMemory& m,Registers& s,unsigned f,unsigned offset){s.fpr_bits[f]=ReadU64(m,Address(s.r[1]+offset));F(s,f,double(std::bit_cast<std::int64_t>(s.fpr_bits[f])));F(s,f,Single(F(s,f)));}
void OneRandomDirection(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;Random(m,d,s,0x82bb0454u);SpillRandom(m,s,80u);r[11]=0xffffffff82000000ull;Flush(d,s);Load(m,s,31,r[11]+2880u);
    ConvertRandom(m,s,0,80u);F(s,30,Single(F(s,0)*F(s,31)));
    Random(m,d,s,0x82bb0478u);SpillRandom(m,s,80u);Flush(d,s);ConvertRandom(m,s,0,80u);F(s,29,Single(F(s,0)*F(s,31)));
    Random(m,d,s,0x82bb0494u);r[11]=0xffffffff82020000ull;Flush(d,s);Load(m,s,0,r[11]-1552u);r[11]=Address(r[3]);
    F(s,13,Single(F(s,29)-F(s,0)));Store(m,s,13,r[1]+100u);F(s,12,Single(F(s,30)-F(s,0)));Store(m,s,12,r[1]+96u);
    WriteU64(m,Address(r[1]+80u),r[11]);r[11]=0xffffffff82000000ull;F(s,10,Single(F(s,13)*F(s,13)));
    ConvertRandom(m,s,11,80u);F(s,11,Single(F(s,11)*F(s,31)));F(s,0,Single(F(s,11)-F(s,0)));Store(m,s,0,r[1]+104u);
    F(s,11,Single(F(s,0)*F(s,0)+F(s,10)));Load(m,s,10,r[11]+3664u);F(s,11,Single(F(s,12)*F(s,12)+F(s,11)));
    CompareFloat(s,F(s,11),F(s,10));
    if(!s.cr6.eq){F(s,10,Single(std::sqrt(F(s,11))));r[11]=0xffffffff82000000ull;Load(m,s,11,r[11]+30596u);
        F(s,11,Single(F(s,11)/F(s,10)));F(s,12,Single(F(s,11)*F(s,12)));Store(m,s,12,r[1]+96u);
        F(s,13,Single(F(s,11)*F(s,13)));Store(m,s,13,r[1]+100u);F(s,0,Single(F(s,11)*F(s,0)));Store(m,s,0,r[1]+104u);}
}
void ThreeRandomDirections(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;r[8]=0xffffffff82020000ull;r[9]=r[10]=r[11]=0xffffffff82000000ull;r[31]=r[1]+104u;Flush(d,s);
    Load(m,s,30,r[8]-1552u);r[30]=3u;Load(m,s,31,r[9]+2880u);Load(m,s,29,r[10]+3664u);Load(m,s,28,r[11]+30596u);
    do {
        Random(m,d,s,0x82bb0540u);SpillRandom(m,s,80u);Flush(d,s);ConvertRandom(m,s,0,80u);F(s,0,Single(F(s,0)*F(s,31)));Store(m,s,0,r[31]-8u);
        Random(m,d,s,0x82bb0560u);SpillRandom(m,s,176u);Flush(d,s);ConvertRandom(m,s,0,176u);F(s,0,Single(F(s,0)*F(s,31)));Store(m,s,0,r[31]-4u);
        Random(m,d,s,0x82bb0580u);SpillRandom(m,s,168u);Flush(d,s);ConvertRandom(m,s,0,168u);F(s,0,Single(F(s,0)*F(s,31)));Store(m,s,0,r[31]);
        Load(m,s,13,r[31]-4u);F(s,13,Single(F(s,13)-F(s,30)));Load(m,s,0,r[31]-8u);F(s,0,Single(F(s,0)-F(s,30)));
        Load(m,s,12,r[31]);F(s,12,Single(F(s,12)-F(s,30)));Store(m,s,0,r[31]-8u);Store(m,s,13,r[31]-4u);Store(m,s,12,r[31]);
        F(s,11,Single(F(s,13)*F(s,13)));F(s,11,Single(F(s,0)*F(s,0)+F(s,11)));F(s,11,Single(F(s,12)*F(s,12)+F(s,11)));
        CompareFloat(s,F(s,11),F(s,29));
        if(!s.cr6.eq){F(s,11,Single(std::sqrt(F(s,11))));F(s,11,Single(F(s,28)/F(s,11)));
            F(s,0,Single(F(s,0)*F(s,11)));Store(m,s,0,r[31]-8u);F(s,0,Single(F(s,13)*F(s,11)));Store(m,s,0,r[31]-4u);
            F(s,0,Single(F(s,12)*F(s,11)));Store(m,s,0,r[31]);}
        --r[30];r[31]+=12u;Compare(s,r[30]);
    }while(!s.cr6.eq);
    r[25]=3u;
}
void Directions(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;CompareMode(s,0);
    if(s.cr6.eq){r[11]=0xffffffff82000000ull;Flush(d,s);Load(m,s,0,r[11]+30596u);r[11]=0xffffffff82000000ull;
        Store(m,s,0,r[1]+96u);Load(m,s,0,r[11]+3664u);Store(m,s,0,r[1]+100u);Store(m,s,0,r[1]+104u);return;}
    CompareMode(s,1);
    if(s.cr6.eq){r[11]=0xffffffff82000000ull;Flush(d,s);Load(m,s,0,r[11]+3664u);r[11]=0xffffffff82000000ull;
        Store(m,s,0,r[1]+96u);Store(m,s,0,r[1]+104u);Load(m,s,13,r[11]+30596u);Store(m,s,13,r[1]+100u);return;}
    CompareMode(s,2);
    if(s.cr6.eq){r[11]=0xffffffff82000000ull;Flush(d,s);Load(m,s,0,r[11]+3664u);r[11]=0xffffffff82000000ull;
        Store(m,s,0,r[1]+96u);Store(m,s,0,r[1]+100u);Load(m,s,0,r[11]+30596u);Store(m,s,0,r[1]+104u);return;}
    CompareMode(s,3);if(s.cr6.eq)OneRandomDirection(m,d,s);else ThreeRandomDirections(m,d,s);
}
void Probe(GuestMemory& m,Dependencies d,Registers& s,VectorState& vectors){
    auto& r=s.r;r[24]=r[3];r[27]=r[4];r[25]=1u;Directions(m,d,s);r[29]=0u;Compare(s,r[25]);r[26]=r[29];
    if(!s.cr6.eq){
        r[11]=0xffffffff82000000ull;r[31]=r[1]+104u;r[28]=r[25];Flush(d,s);Load(m,s,31,r[11]+3596u);
        do{
            r[3]=r[1]+192u;s.lr=0x82bb0630u;(void)geometry_support61::Apply(0x82bd43f8u,m,d.query.fp,s);
            Flush(d,s);Load(m,s,0,r[27]);Store(m,s,0,r[1]+144u);r[11]=m.ReadU32(Address(r[1]+196u));
            Load(m,s,0,r[27]+4u);r[7]=0u;Store(m,s,0,r[1]+148u);r[11]&=0xfffffffcu;
            Load(m,s,0,r[27]+8u);r[6]=0u;Store(m,s,0,r[1]+152u);r[4]=r[1]+144u;
            Load(m,s,0,r[31]-8u);r[3]=r[1]+192u;Store(m,s,0,r[1]+156u);r[5]=m.ReadU32(Address(r[24]+48u));
            Load(m,s,0,r[31]-4u);m.WriteU32(Address(r[1]+196u),Address(r[11]));Store(m,s,0,r[1]+160u);
            m.WriteU8(Address(r[1]+333u),std::uint8_t(r[29]));Load(m,s,0,r[31]);Store(m,s,0,r[1]+164u);Store(m,s,31,r[1]+324u);
            s.lr=0x82bb068cu;(void)geometry_query_dispatch61::Apply(0x82bd7258u,m,d.query,s,vectors);
            r[3]=r[1]+192u;r[30]=m.ReadU32(Address(r[1]+296u));s.lr=0x82bb0698u;(void)geometry_support61::Apply(0x82bd4438u,m,d.query.fp,s);
            r[11]=Address(r[30])&1u;Compare(s,r[11]);if(!s.cr6.eq)++r[26];--r[28];r[31]+=12u;Compare(s,r[28]);
        }while(!s.cr6.eq);
    }
    r[11]=(r[26]<<1u)&0xfffffffeu;s.xer_ca=std::uint8_t(Address(r[25])>=Address(r[11]));r[11]=r[25]-r[11];
    const auto carry=s.xer_ca;r[11]=~std::uint64_t(0u)+carry;s.xer_ca=carry;r[3]=Address(r[11])&1u;
}
}
bool Apply(GuestAddress entry,GuestMemory& m,Dependencies d,Registers& s,VectorState& vectors){
    if(entry!=0x82bb03b0u)return false;
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bb03b8u;
    for(unsigned i=24u;i<=31u;++i)WriteU64(m,Address(r[1]-16u-8u*(31u-i)),r[i]);m.WriteU32(Address(r[1]-8u),Address(r[12]));
    r[12]=r[1]-72u;s.lr=0x82bb03c0u;for(unsigned i=28u;i<=31u;++i)WriteU64(m,Address(r[12]-8u*(32u-i)),s.fpr_bits[i]);
    const auto caller_sp=r[1];r[1]-=448u;m.WriteU32(Address(r[1]),Address(caller_sp));Probe(m,d,s,vectors);
    r[1]+=448u;r[12]=r[1]-72u;s.lr=0x82bb06d4u;for(unsigned i=28u;i<=31u;++i)s.fpr_bits[i]=ReadU64(m,Address(r[12]-8u*(32u-i)));
    for(unsigned i=24u;i<=31u;++i)r[i]=ReadU64(m,Address(r[1]-16u-8u*(31u-i)));r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];return true;
}
}
