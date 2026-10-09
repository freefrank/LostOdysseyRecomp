#include "lo_semantics/owned_tree_build61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::owned_tree_build61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
double F(const Registers& s,unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);
}
void F(Registers& s,unsigned i,double x){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(x);
}
double Single(double x){return static_cast<float>(x);
}
void Load(GuestMemory& m,Registers& s,unsigned i,std::uint64_t a){F(s,i,std::bit_cast<float>(m.ReadU32(Address(a))));
}
void Store(GuestMemory& m,Registers& s,unsigned i,std::uint64_t a){m.WriteU32(Address(a),std::bit_cast<std::uint32_t>(static_cast<float>(F(s,i))));
}
void Flush(Dependencies d,Registers& s){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;
d.fp.SetHostFpControl(s.cached_fp_control);
}}
void Compare(Registers& s,std::uint64_t a,std::uint64_t b){const auto x=Address(a),y=Address(b);
s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
void CompareFloat(Registers& s,double a,double b){const bool u=std::isnan(a)||std::isnan(b);
s.cr6={std::uint8_t(!u&&a<b),std::uint8_t(!u&&a>b),std::uint8_t(!u&&a==b),std::uint8_t(u)};
}
std::uint64_t Scale4(std::uint64_t x){return (x<<2u)&0xfffffffcu;
}
void Convert(GuestMemory& m,Registers& s,unsigned f,unsigned offset){F(s,f,static_cast<double>(std::bit_cast<std::int64_t>(ReadU64(m,Address(s.r[1]+offset)))));
}
void Partition(GuestAddress next,GuestMemory& m,Dependencies d,Registers& s){s.lr=next;
(void)owned_tree_storage61::Apply(0x82bd9858u,m,d,s);
}
void SelectAxis(unsigned offset,bool minimum,GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;
    if(minimum?s.cr6.lt:s.cr6.gt)r[4]=1u;
    r[11]=Scale4(r[4]);
    r[10]=r[1]+offset;
    Flush(d,s);
    Load(m,s,13u,r[11]+r[10]);
    CompareFloat(s,F(s,0),F(s,13));
    if(minimum?s.cr6.lt:s.cr6.gt)r[4]=2u;
}
void SplitResult(GuestMemory& m,Registers& s){
    auto& r=s.r;
    r[30]=r[3];
    Compare(s,r[30],0u);
    if(!s.cr6.eq){r[11]=m.ReadU32(Address(r[31]+36u));
    Compare(s,r[30],r[11]);
    if(!s.cr6.eq)return;
    }
    r[26]=r[25];
}
void Coordinate(GuestAddress next,unsigned offset,GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;
    r[11]=m.ReadU32(Address(r[28]));
    r[5]=r[1]+offset;
    r[4]=m.ReadU32(Address(r[30]));
    r[3]=r[28];
    r[30]+=4u;
    r[11]=m.ReadU32(Address(r[11]+16u));
    s.ctr=r[11];
    s.lr=next;
    d.guest.CallIndirect(Address(s.ctr)&~3u,m,s);
}
void Extent(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;
    Flush(d,s);
    Load(m,s,0u,r[31]);
    r[11]=0xffffffff82020000ull;
    Load(m,s,12u,r[31]+4u);
    r[4]=r[25];
    Load(m,s,13u,r[31]+12u);
    Load(m,s,10u,r[31]+16u);
    F(s,13,Single(F(s,13)-F(s,0)));
    F(s,12,Single(F(s,10)-F(s,12)));
    Load(m,s,0u,r[31]+20u);
    Load(m,s,11u,r[31]+8u);
    F(s,11,Single(F(s,0)-F(s,11)));
    Load(m,s,0u,r[11]-1552u);
    F(s,13,Single(F(s,13)*F(s,0)));
    Store(m,s,13,r[1]+96u);
    F(s,12,Single(F(s,12)*F(s,0)));
    Store(m,s,12,r[1]+100u);
    F(s,0,Single(F(s,11)*F(s,0)));
    Store(m,s,0,r[1]+104u);
    CompareFloat(s,F(s,12),F(s,13));
    SelectAxis(96u,false,m,d,s);
    r[5]=r[28];
    r[3]=r[31];
    Partition(0x82bd9a20u,m,d,s);
    SplitResult(m,s);
}
// Gather per-word coordinates twice: first the mean, then squared deviations.
// Every arithmetic stage rounds through float, matching the selected PPC flow.
void Variance(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;
    r[11]=0xffffffff82000000ull;
    r[10]=m.ReadU32(Address(r[31]+36u));
    r[30]=m.ReadU32(Address(r[31]+32u));
    Flush(d,s);
    Load(m,s,31u,r[11]+3664u);
    r[11]=Scale4(r[10]);
    s.fpr_bits[28]=s.fpr_bits[31];
    r[29]=r[11]+r[30];
    s.fpr_bits[27]=s.fpr_bits[31];
    s.fpr_bits[26]=s.fpr_bits[31];
    Compare(s,r[30],r[29]);
    while(!s.cr6.eq){
        Coordinate(0x82bd9a98u,112u,m,d,s);
        Flush(d,s);
        Load(m,s,0,r[1]+112u);
        F(s,28,Single(F(s,0)+F(s,28)));
        Load(m,s,0,r[1]+116u);
        F(s,27,Single(F(s,0)+F(s,27)));
        Load(m,s,0,r[1]+120u);
        Compare(s,r[30],r[29]);
        F(s,26,Single(F(s,0)+F(s,26)));
    }
    r[11]=m.ReadU32(Address(r[31]+36u));
    Flush(d,s);
    s.fpr_bits[30]=s.fpr_bits[31];
    r[30]=m.ReadU32(Address(r[31]+32u));
    s.fpr_bits[29]=s.fpr_bits[31];
    Compare(s,r[30],r[29]);
    WriteU64(m,Address(r[1]+80u),r[11]);
    Convert(m,s,0,80u);
    r[11]=0xffffffff82000000ull;
    Load(m,s,25,r[11]+30596u);
    F(s,0,Single(F(s,0)));
    F(s,0,Single(F(s,25)/F(s,0)));
    F(s,28,Single(F(s,0)*F(s,28)));
    F(s,27,Single(F(s,0)*F(s,27)));
    F(s,26,Single(F(s,0)*F(s,26)));
    while(!s.cr6.eq){
        Coordinate(0x82bd9b18u,96u,m,d,s);
        Flush(d,s);
        Load(m,s,0,r[1]+96u);
        Load(m,s,13,r[1]+100u);
        F(s,0,Single(F(s,0)-F(s,28)));
        Load(m,s,12,r[1]+104u);
        F(s,13,Single(F(s,13)-F(s,27)));
        F(s,12,Single(F(s,12)-F(s,26)));
        Compare(s,r[30],r[29]);
        F(s,31,Single(F(s,0)*F(s,0)+F(s,31)));
        F(s,30,Single(F(s,13)*F(s,13)+F(s,30)));
        F(s,29,Single(F(s,12)*F(s,12)+F(s,29)));
    }
    r[11]=m.ReadU32(Address(r[31]+36u));
    r[4]=r[25];
    r[11]=Address(r[11]-1u);
    WriteU64(m,Address(r[1]+80u),r[11]);
    Flush(d,s);
    Convert(m,s,0,80u);
    F(s,0,Single(F(s,0)));
    F(s,0,Single(F(s,25)/F(s,0)));
    F(s,13,Single(F(s,0)*F(s,31)));
    Store(m,s,13,r[1]+96u);
    F(s,12,Single(F(s,0)*F(s,30)));
    Store(m,s,12,r[1]+100u);
    F(s,0,Single(F(s,0)*F(s,29)));
    Store(m,s,0,r[1]+104u);
    CompareFloat(s,F(s,12),F(s,13));
    SelectAxis(96u,false,m,d,s);
    r[5]=r[28];
    r[3]=r[31];
    Partition(0x82bd9bb0u,m,d,s);
    SplitResult(m,s);
}
// Compare the squared distance of each partition ratio from one half, then
// rerun the best axis so the array ordering matches the selected split.
void Balanced(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;
    r[5]=r[28];
    r[4]=0u;
    r[3]=r[31];
    Partition(0x82bd9becu,m,d,s);
    r[11]=r[3];
    r[10]=m.ReadU32(Address(r[31]+36u));
    r[5]=r[28];
    r[11]=Address(r[11]);
    r[4]=1u;
    r[3]=r[31];
    WriteU64(m,Address(r[1]+80u),r[10]);
    Flush(d,s);
    s.fpr_bits[0]=ReadU64(m,Address(r[1]+80u));
    WriteU64(m,Address(r[1]+96u),r[11]);
    s.fpr_bits[13]=ReadU64(m,Address(r[1]+96u));
    F(s,0,static_cast<double>(std::bit_cast<std::int64_t>(s.fpr_bits[0])));
    F(s,13,static_cast<double>(std::bit_cast<std::int64_t>(s.fpr_bits[13])));
    F(s,0,Single(F(s,0)));
    F(s,13,Single(F(s,13)));
    F(s,31,Single(F(s,13)/F(s,0)));
    Partition(0x82bd9c2cu,m,d,s);
    r[11]=r[3];
    r[10]=m.ReadU32(Address(r[31]+36u));
    r[5]=r[28];
    r[11]=Address(r[11]);
    r[4]=2u;
    r[3]=r[31];
    WriteU64(m,Address(r[1]+96u),r[10]);
    Flush(d,s);
    Convert(m,s,0,96u);
    WriteU64(m,Address(r[1]+80u),r[11]);
    Convert(m,s,13,80u);
    F(s,0,Single(F(s,0)));
    F(s,13,Single(F(s,13)));
    F(s,30,Single(F(s,13)/F(s,0)));
    Partition(0x82bd9c6cu,m,d,s);
    r[11]=0xffffffff82020000ull;
    r[10]=m.ReadU32(Address(r[31]+36u));
    r[4]=r[25];
    Flush(d,s);
    Load(m,s,0,r[11]-1552u);
    r[11]=r[10];
    F(s,13,Single(F(s,31)-F(s,0)));
    F(s,12,Single(F(s,30)-F(s,0)));
    WriteU64(m,Address(r[1]+96u),r[11]);
    r[11]=Address(r[3]);
    WriteU64(m,Address(r[1]+80u),r[11]);
    F(s,13,Single(F(s,13)*F(s,13)));
    Store(m,s,13,r[1]+128u);
    F(s,12,Single(F(s,12)*F(s,12)));
    Store(m,s,12,r[1]+132u);
    CompareFloat(s,F(s,12),F(s,13));
    s.fpr_bits[13]=ReadU64(m,Address(r[1]+96u));
    s.fpr_bits[12]=ReadU64(m,Address(r[1]+80u));
    F(s,13,static_cast<double>(std::bit_cast<std::int64_t>(s.fpr_bits[13])));
    F(s,12,static_cast<double>(std::bit_cast<std::int64_t>(s.fpr_bits[12])));
    F(s,13,Single(F(s,13)));
    F(s,12,Single(F(s,12)));
    F(s,13,Single(F(s,12)/F(s,13)));
    F(s,0,Single(F(s,13)-F(s,0)));
    F(s,0,Single(F(s,0)*F(s,0)));
    Store(m,s,0,r[1]+136u);
    SelectAxis(128u,true,m,d,s);
    r[5]=r[28];
    r[3]=r[31];
    Partition(0x82bd9cfcu,m,d,s);
    SplitResult(m,s);
}
// Sort axis IDs by descending half extent, retrying failed partitions in
// that order. The count limit and forced-half fallback live in Build.
void RetryAxes(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;
    Flush(d,s);
    Load(m,s,0,r[31]);
    r[10]=0xffffffff82020000ull;
    Load(m,s,13,r[31]+12u);
    r[8]=r[25];
    Load(m,s,12,r[31]+4u);
    F(s,13,Single(F(s,13)-F(s,0)));
    Load(m,s,10,r[31]+16u);
    r[11]=1u;
    Load(m,s,0,r[31]+20u);
    F(s,12,Single(F(s,10)-F(s,12)));
    Load(m,s,11,r[31]+8u);
    r[9]=2u;
    F(s,11,Single(F(s,0)-F(s,11)));
    Load(m,s,0,r[10]-1552u);
    m.WriteU32(Address(r[1]+80u),Address(r[8]));
    r[7]=3u;
    m.WriteU32(Address(r[1]+84u),Address(r[11]));
    m.WriteU32(Address(r[1]+88u),Address(r[9]));
    F(s,13,Single(F(s,13)*F(s,0)));
    Store(m,s,13,r[1]+96u);
    F(s,12,Single(F(s,12)*F(s,0)));
    Store(m,s,12,r[1]+100u);
    F(s,0,Single(F(s,11)*F(s,0)));
    Store(m,s,0,r[1]+104u);
    do {
        r[10]=Scale4(r[8]);
        r[6]=r[1]+96u;
        r[5]=Scale4(r[11]);
        r[4]=r[1]+96u;
        Flush(d,s);
        Load(m,s,0,r[10]+r[6]);
        Load(m,s,13,r[5]+r[4]);
        CompareFloat(s,F(s,0),F(s,13));
        if(s.cr6.lt){r[10]=r[8];
        r[8]=r[11];
        r[11]=r[10];
        }
        r[10]=Scale4(r[11]);
        r[6]=r[1]+96u;
        r[5]=Scale4(r[9]);
        r[4]=r[1]+96u;
        Flush(d,s);
        Load(m,s,0,r[10]+r[6]);
        Load(m,s,13,r[5]+r[4]);
        CompareFloat(s,F(s,0),F(s,13));
        if(s.cr6.lt){r[10]=r[11];
        r[11]=r[9];
        r[9]=r[10];
        }--r[7];
        Compare(s,r[7],0u);
    }while(!s.cr6.eq);
    m.WriteU32(Address(r[1]+80u),Address(r[8]));
    r[27]=r[25];
    r[30]=m.ReadU32(Address(r[1]+80u));
    r[26]=r[25];
    m.WriteU32(Address(r[1]+88u),Address(r[9]));
    r[29]=r[1]+80u;
    m.WriteU32(Address(r[1]+84u),Address(r[11]));
    for(;;){
        Compare(s,r[27],3u);
        if(s.cr6.eq)break;
        r[5]=r[28];
        r[4]=m.ReadU32(Address(r[29]));
        r[3]=r[31];
        Partition(0x82bd9e20u,m,d,s);
        r[30]=r[3];
        Compare(s,r[30],0u);
        bool failed=s.cr6.eq;
        if(!failed){r[11]=m.ReadU32(Address(r[31]+36u));
        Compare(s,r[30],r[11]);
        failed=s.cr6.eq;
        }
        if(failed){++r[27];
        r[29]+=4u;
        }else r[26]=1u;
        r[11]=r[26]&255u;
        Compare(s,r[11],0u);
        if(!s.cr6.eq)break;
    }
}
void Leave(unsigned result,GuestAddress fp_return,GuestMemory& m,Registers& s){
    auto& r=s.r;
    r[3]=result;
    r[1]+=272u;
    r[12]=r[1]-64u;
    s.lr=fp_return;
    for(unsigned i=25u;i<=31u;++i)s.fpr_bits[i]=ReadU64(m,Address(r[12]-56u+8u*(i-25u)));
    for(unsigned i=25u;i<=31u;++i)r[i]=ReadU64(m,Address(r[1]-16u-8u*(31u-i)));
    r[12]=m.ReadU32(Address(r[1]-8u));
    s.lr=r[12];
}
// Split policy and child allocation are separate from axis scoring. Arena
// children carry the borrowed tag; heap children have a two-element header.
void Build(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;
    r[12]=s.lr;
    s.lr=0x82bd9930u;
    for(unsigned i=25u;i<=31u;++i)WriteU64(m,Address(r[1]-16u-8u*(31u-i)),r[i]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));
    r[12]=r[1]-64u;
    s.lr=0x82bd9938u;
    for(unsigned i=25u;i<=31u;++i)WriteU64(m,Address(r[12]-56u+8u*(i-25u)),s.fpr_bits[i]);
    const auto caller_sp=r[1];
    r[1]-=272u;
    m.WriteU32(Address(r[1]),Address(caller_sp));
    r[28]=r[4];
    r[31]=r[3];
    Compare(s,r[28],0u);
    if(s.cr6.eq){Leave(0u,0x82bd9f90u,m,s);
    return;
    }r[5]=m.ReadU32(Address(r[31]+36u));
    Compare(s,r[5],1u);
    if(s.cr6.eq){Leave(1u,0x82bd9968u,m,s);
    return;
    }
    r[11]=m.ReadU32(Address(r[28]));
    r[6]=r[31];
    r[4]=m.ReadU32(Address(r[31]+32u));
    r[3]=r[28];
    r[11]=m.ReadU32(Address(r[11]+20u));
    s.ctr=r[11];
    s.lr=0x82bd9988u;
    d.guest.CallIndirect(Address(s.ctr)&~3u,m,s);
    const auto eligible=std::bit_cast<std::int32_t>(Address(r[3]));
    s.cr6={std::uint8_t(eligible<0),std::uint8_t(eligible>0),std::uint8_t(eligible==0),s.xer_so};
    if(s.cr6.eq){Leave(1u,0x82bd9968u,m,s);
    return;
    }
    r[11]=m.ReadU32(Address(r[28]+8u));
    r[26]=1u;
    r[25]=0u;
    r[10]=r[11]&1u;
    Compare(s,r[10],0u);
    if(!s.cr6.eq)Extent(m,d,s);
    else {r[10]=r[11]&2u;
    Compare(s,r[10],0u);
    if(!s.cr6.eq)Variance(m,d,s);
        else {r[10]=r[11]&8u;
        Compare(s,r[10],0u);
        if(!s.cr6.eq)Balanced(m,d,s);
            else {r[10]=r[11]&4u;
            Compare(s,r[10],0u);
            if(!s.cr6.eq)RetryAxes(m,d,s);
                else {r[11]&=16u;
                Compare(s,r[11],0u);
                if(s.cr6.eq){Leave(0u,0x82bd9f90u,m,s);
                return;
                }
                    r[11]=m.ReadU32(Address(r[31]+36u));
                    r[30]=r[11]>>1u;
                    }}}}
    r[11]=r[26]&255u;
    Compare(s,r[11],0u);
    if(s.cr6.eq){r[11]=m.ReadU32(Address(r[31]+36u));
    r[10]=m.ReadU32(Address(r[28]+4u));
    Compare(s,r[11],r[10]);
        if(!s.cr6.gt){Leave(1u,0x82bd9968u,m,s);
        return;
        }r[11]=m.ReadU32(Address(r[28]+68u));
        ++r[11];
        m.WriteU32(Address(r[28]+68u),Address(r[11]));
        r[11]=m.ReadU32(Address(r[31]+36u));
        r[30]=r[11]>>1u;
        }
    r[9]=m.ReadU32(Address(r[28]+28u));
    Compare(s,r[9],0u);
    if(!s.cr6.eq){r[11]=m.ReadU32(Address(r[28]+64u));
    r[10]=Scale4(r[11]);
    r[11]+=r[10];
    r[11]=(r[11]<<3u)&0xfffffff8u;
    r[11]+=r[9];
    r[11]|=1u;
    }
    else {s.lr=0x82bd9ec8u;
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,m,d.guest,s);
        r[11]=m.ReadU32(Address(r[3]));
        r[5]=26u;
        r[4]=84u;
        r[11]=m.ReadU32(Address(r[11]));
        s.ctr=r[11];
        s.lr=0x82bd9ee0u;
        d.guest.CallIndirect(Address(s.ctr)&~3u,m,s);
        Compare(s,r[3],0u);
        if(s.cr6.eq){Leave(0u,0x82bd9f90u,m,s);
        return;
        }
        r[10]=2u;
        r[11]=r[3]+4u;
        Compare(s,r[11],0u);
        m.WriteU32(Address(r[3]),Address(r[10]));
        for(unsigned offset:{24u,28u,32u,36u,64u,68u,72u,76u})m.WriteU32(Address(r[11]+offset),Address(r[25]));
        if(s.cr6.eq){Leave(0u,0x82bd9f90u,m,s);
        return;
        }}
    m.WriteU32(Address(r[31]+24u),Address(r[11]));
    r[11]=m.ReadU32(Address(r[28]+64u));
    r[11]+=2u;
    m.WriteU32(Address(r[28]+64u),Address(r[11]));
    r[11]=m.ReadU32(Address(r[31]+24u));
    r[11]&=0xfffffffeu;
    Compare(s,r[11],0u);
    r[10]=r[11]+40u;
    if(s.cr6.eq)r[10]=r[25];
    r[8]=m.ReadU32(Address(r[31]+32u));
    r[9]=Scale4(r[30]);
    m.WriteU32(Address(r[11]+36u),Address(r[30]));
    r[3]=1u;
    m.WriteU32(Address(r[11]+32u),Address(r[8]));
    r[11]=m.ReadU32(Address(r[31]+32u));
    r[11]+=r[9];
    m.WriteU32(Address(r[10]+32u),Address(r[11]));
    r[11]=m.ReadU32(Address(r[31]+36u));
    r[11]-=r[30];
    m.WriteU32(Address(r[10]+36u),Address(r[11]));
    Leave(1u,0x82bd9f7cu,m,s);
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state){if(entry!=0x82bd9928u)return false;
Build(memory,deps,state);
return true;
}
}
