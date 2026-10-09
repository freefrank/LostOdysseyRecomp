#include "lo_semantics/indexed_record_accumulate61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::indexed_record_accumulate61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
double F(const Registers& s,unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);}
void Single(Registers& s,unsigned i,double v){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(static_cast<float>(v)));}
void Load(GuestMemory& m,Registers& s,unsigned i,std::uint64_t p){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(m.ReadU32(Address(p)))));}
void Store(GuestMemory& m,const Registers& s,std::uint64_t p){m.WriteU32(Address(p),std::bit_cast<std::uint32_t>(static_cast<float>(F(s,0))));}
void SignedCompare(Registers& s,std::uint64_t a,std::int32_t b=0){const auto x=std::bit_cast<std::int32_t>(Address(a));s.cr6={std::uint8_t(x<b),std::uint8_t(x>b),std::uint8_t(x==b),s.xer_so};}
void ScaleSource(GuestMemory& m,Registers& s,bool tail){
    Load(m,s,0,s.r[9]);
    if(tail)Load(m,s,13,s.r[9]+4u);
    Single(s,0,F(s,0)*F(s,1));
    if(!tail)Load(m,s,13,s.r[9]+4u);
    if(tail)Load(m,s,12,s.r[9]+8u);
    Single(s,13,F(s,13)*F(s,1));
    if(!tail)Load(m,s,12,s.r[9]+8u);
    Single(s,12,F(s,12)*F(s,1));
}
void AddTriplet(GuestMemory& m,Registers& s,std::uint64_t target,bool tail){
    Single(s,0,F(s,11)+F(s,0));Store(m,s,target);
    if(!tail)Load(m,s,0,target+4u);
    Single(s,0,F(s,tail?10u:0u)+F(s,13));Store(m,s,target+4u);
    if(!tail)Load(m,s,0,target+8u);
    Single(s,0,F(s,tail?9u:0u)+F(s,12));Store(m,s,target+8u);
}
void Accumulate(GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s,std::uint64_t index,bool tail){
    auto& r=s.r;r[11]=m.ReadU16(Address(index));
    r[11]=std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[11])))*std::int64_t(std::bit_cast<std::int32_t>(Address(r[6]))));
    r[11]+=r[7];r[10]=m.ReadU32(Address(r[11]+92u));r[10]&=1u;SignedCompare(s,r[10]);if(!s.cr6.eq)return;
    r[9]=r[11]+r[5];
    if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;fp.SetHostFpControl(s.cached_fp_control);}
    Load(m,s,11,r[11]+48u);if(tail)Load(m,s,10,r[11]+52u);r[10]=r[11]+32u;if(tail)Load(m,s,9,r[11]+56u);
    ScaleSource(m,s,tail);AddTriplet(m,s,r[11]+48u,tail);
    // This second load is required even when r5 points at the first destination.
    ScaleSource(m,s,tail);Load(m,s,11,r[10]);if(tail){Load(m,s,10,r[10]+4u);Load(m,s,9,r[10]+8u);}
    AddTriplet(m,s,r[10],tail);
}
}
bool Apply(GuestAddress entry,GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s){
    if(entry!=0x822cd290u)return false;
    auto& r=s.r;WriteU64(m,Address(r[1]-8u),r[31]);r[11]=m.ReadU32(Address(r[4]+124u));r[7]=m.ReadU32(Address(r[4]+56u));--r[11];
    r[6]=m.ReadU32(Address(r[4]+120u));r[31]=m.ReadU32(Address(r[4]+60u));r[10]=r[11]+1u;r[3]=r[11];SignedCompare(s,r[10],4);
    if(!s.cr6.lt){
        r[11]=r[3]-2u;r[4]=Address(r[10])>>2u;r[11]=(r[11]<<1u)&0xfffffffeu;r[8]=r[11]+r[31];r[11]=(r[4]<<2u)&0xfffffffcu;r[3]-=r[11];
        do{
            Accumulate(m,fp,s,r[8]+4u,false);Accumulate(m,fp,s,r[8]+2u,false);Accumulate(m,fp,s,r[8],false);Accumulate(m,fp,s,r[8]-2u,false);
            --r[4];r[8]-=8u;const auto n=Address(r[4]);s.cr6={0u,std::uint8_t(n!=0u),std::uint8_t(n==0u),s.xer_so};
        }while(!s.cr6.eq);
    }
    SignedCompare(s,r[3]);if(!s.cr6.lt){
        r[11]=(r[3]<<1u)&0xfffffffeu;r[8]=r[11]+r[31];
        do{Accumulate(m,fp,s,r[8],true);--r[3];r[8]-=2u;SignedCompare(s,r[3]);}while(!s.cr6.lt);
    }
    r[31]=ReadU64(m,Address(r[1]-8u));return true;
}
}
