#include "lo_semantics/indexed_record_retire61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::indexed_record_retire61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
double F(const Registers& s,unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);}
void Load(GuestMemory& m,Registers& s,unsigned i,std::uint64_t p){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(m.ReadU32(Address(p)))));}
void Flush(float_triplet_transfer::NativeServices& fp,Registers& s){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;fp.SetHostFpControl(s.cached_fp_control);}}
void SignedCompare(Registers& s,std::uint64_t a,std::int32_t b=0){const auto x=std::bit_cast<std::int32_t>(Address(a));s.cr6={std::uint8_t(x<b),std::uint8_t(x>b),std::uint8_t(x==b),s.xer_so};}
std::uint64_t Product(std::uint64_t a,std::uint64_t b){return std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(a)))*std::int64_t(std::bit_cast<std::int32_t>(Address(b))));}
std::uint64_t IndexBytes(std::uint64_t count){return(count<<1u)&0xfffffffeu;}
bool Expired(GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s,unsigned record){
    Flush(fp,s);Load(m,s,12,s.r[record]+12u);const auto a=F(s,12),b=F(s,13);const bool unordered=std::isnan(a)||std::isnan(b);
    s.cr6={std::uint8_t(!unordered&&a<b),std::uint8_t(!unordered&&a>b),std::uint8_t(!unordered&&a==b),std::uint8_t(unordered)};return s.cr6.gt;
}
// The five retirement sites use the same clear/swap/count algorithm, with
// distinct live scratch registers. Parameterizing those roles retains each
// original register value without caching guest pointers across stores.
void Retire(GuestMemory& m,Registers& s,unsigned record,unsigned clear_offset,unsigned saved_index,unsigned tail,unsigned position,bool preceding=false){
    auto& r=s.r;r[clear_offset]=m.ReadU32(Address(r[3]+112u));r[saved_index]=r[11];r[11]=r[clear_offset]+r[record];
    for(unsigned offset=0u;offset<20u;offset+=4u)m.WriteU32(Address(r[11]+offset),std::bit_cast<std::uint32_t>(static_cast<float>(F(s,0))));
    r[tail]=m.ReadU32(Address(r[3]+124u));r[11]=m.ReadU32(Address(r[3]+60u));r[tail]=IndexBytes(r[tail]);r[tail]+=r[11];
    if(preceding)r[11]+=r[position];
    r[tail]=m.ReadU16(Address(r[tail]-2u));
    m.WriteU16(Address(preceding?r[11]-2u:r[position]+r[11]),std::uint16_t(r[tail]));
    r[11]=m.ReadU32(Address(r[3]+124u));r[tail]=m.ReadU32(Address(r[3]+60u));r[11]=IndexBytes(r[11]);r[11]+=r[tail];
    m.WriteU16(Address(r[11]-2u),std::uint16_t(r[saved_index]));r[11]=m.ReadU32(Address(r[3]+124u));--r[11];m.WriteU32(Address(r[3]+124u),Address(r[11]));
}
void FourRecords(GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s){
    auto& r=s.r;
    r[11]=m.ReadU32(Address(r[3]+60u));r[10]=m.ReadU32(Address(r[3]+120u));r[9]=m.ReadU32(Address(r[3]+56u));
    r[11]=m.ReadU16(Address(r[6]+r[11]));r[10]=Product(r[10],r[11]);r[10]+=r[9];
    if(Expired(m,fp,s,10u))Retire(m,s,10u,9u,8u,10u,6u);
    r[11]=m.ReadU32(Address(r[3]+60u));r[10]=m.ReadU32(Address(r[3]+120u));r[11]+=r[6];r[9]=m.ReadU32(Address(r[3]+56u));
    r[11]=m.ReadU16(Address(r[11]-2u));r[10]=Product(r[10],r[11]);r[10]+=r[9];
    if(Expired(m,fp,s,10u))Retire(m,s,10u,9u,8u,10u,6u,true);
    r[8]=r[6]-6u;r[11]=m.ReadU32(Address(r[3]+60u));r[9]=m.ReadU32(Address(r[3]+120u));r[10]=r[8]+2u;r[7]=m.ReadU32(Address(r[3]+56u));
    r[11]=m.ReadU16(Address(r[10]+r[11]));r[9]=Product(r[9],r[11]);r[9]+=r[7];
    if(Expired(m,fp,s,9u))Retire(m,s,9u,7u,31u,9u,10u);
    r[11]=m.ReadU32(Address(r[3]+60u));r[10]=m.ReadU32(Address(r[3]+120u));r[9]=m.ReadU32(Address(r[3]+56u));
    r[11]=m.ReadU16(Address(r[8]+r[11]));r[10]=Product(r[10],r[11]);r[10]+=r[9];
    if(Expired(m,fp,s,10u))Retire(m,s,10u,9u,7u,10u,8u);
}
}
bool Apply(GuestAddress entry,GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s){
    if(entry!=0x822cba60u)return false;
    auto& r=s.r;WriteU64(m,Address(r[1]-8u),r[31]);r[11]=m.ReadU32(Address(r[3]+124u));r[9]=0xffffffff82190000ull;
    --r[11];r[10]=r[11]+1u;r[4]=r[11];Flush(fp,s);Load(m,s,13,r[9]-27252u);r[11]=0xffffffff82000000ull;SignedCompare(s,r[10],4);Load(m,s,0,r[11]+3664u);
    if(!s.cr6.lt){
        r[5]=Address(r[10])>>2u;r[6]=IndexBytes(r[4]);r[11]=(r[5]<<2u)&0xfffffffcu;r[4]-=r[11];
        do{FourRecords(m,fp,s);--r[5];r[6]-=8u;const auto n=Address(r[5]);s.cr6={0u,std::uint8_t(n!=0u),std::uint8_t(n==0u),s.xer_so};}while(!s.cr6.eq);
    }
    SignedCompare(s,r[4]);
    if(!s.cr6.lt){
        r[9]=IndexBytes(r[4]);
        do{
            r[11]=m.ReadU32(Address(r[3]+60u));r[10]=m.ReadU32(Address(r[3]+120u));r[8]=m.ReadU32(Address(r[3]+56u));
            r[11]=m.ReadU16(Address(r[9]+r[11]));r[10]=Product(r[10],r[11]);r[10]+=r[8];
            if(Expired(m,fp,s,10u))Retire(m,s,10u,8u,7u,10u,9u);
            --r[4];r[9]-=2u;SignedCompare(s,r[4]);
        }while(!s.cr6.lt);
    }
    r[31]=ReadU64(m,Address(r[1]-8u));return true;
}
}
