#include "lo_semantics/curve_tangent_update61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::curve_tangent_update61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
double F(const Registers& s,unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);}
void F(Registers& s,unsigned i,double value){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(value);}
double Single(double value){return static_cast<float>(value);}
void Load(GuestMemory& m,Registers& s,unsigned i,std::uint64_t address){F(s,i,std::bit_cast<float>(m.ReadU32(Address(address))));}
void Store(GuestMemory& m,Registers& s,unsigned i,std::uint64_t address){m.WriteU32(Address(address),std::bit_cast<std::uint32_t>(static_cast<float>(F(s,i))));}
void Flush(float_triplet_transfer::NativeServices& fp,Registers& s){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;fp.SetHostFpControl(s.cached_fp_control);}}
void SignedCompare(Registers& s,std::uint64_t a,std::uint64_t b=0u){
    const auto x=std::bit_cast<std::int32_t>(Address(a)),y=std::bit_cast<std::int32_t>(Address(b));
    s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
void ModeCompare(Registers& s,unsigned value){const auto x=Address(s.r[10]);s.cr6={std::uint8_t(x<value),std::uint8_t(x>value),std::uint8_t(x==value),s.xer_so};}
bool SmoothMode(Registers& s){ModeCompare(s,1u);if(s.cr6.eq)return true;ModeCompare(s,3u);if(s.cr6.eq)return true;ModeCompare(s,4u);return s.cr6.eq;}
void Blend(GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s){
    auto& r=s.r;Flush(fp,s);Load(m,s,13,r[11]+4u);F(s,0,Single(F(s,9)-F(s,1)));Load(m,s,11,r[11]+36u);Load(m,s,12,r[11]+8u);
    F(s,11,Single(F(s,11)-F(s,13)));Load(m,s,10,r[11]+40u);Load(m,s,7,r[11]-28u);F(s,10,Single(F(s,10)-F(s,12)));
    Load(m,s,6,r[11]-24u);F(s,13,Single(F(s,13)-F(s,7)));F(s,12,Single(F(s,12)-F(s,6)));F(s,0,Single(F(s,0)*F(s,8)));
    F(s,13,Single(F(s,13)+F(s,11)));F(s,12,Single(F(s,12)+F(s,10)));F(s,13,Single(F(s,13)*F(s,0)));Store(m,s,13,r[1]-16u);
    F(s,0,Single(F(s,12)*F(s,0)));Store(m,s,0,r[1]-12u);r[10]=ReadU64(m,Address(r[1]-16u));
    WriteU64(m,Address(r[1]-24u),r[10]);WriteU64(m,Address(r[1]-32u),r[10]);
}
void Interior(GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s){
    auto& r=s.r;ModeCompare(s,1u);if(!s.cr6.eq)return;
    r[10]=m.ReadU8(Address(r[11]-4u));
    if(SmoothMode(s)){
        r[10]=m.ReadU8(Address(r[11]+28u));if(SmoothMode(s)){Blend(m,fp,s);return;}
    }
    r[10]=m.ReadU8(Address(r[11]-4u));ModeCompare(s,2u);
    if(s.cr6.eq){r[10]=r[1]-24u;r[9]=r[1]-32u;WriteU64(m,Address(r[10]),r[6]);WriteU64(m,Address(r[9]),r[6]);}
}
}
bool Apply(GuestAddress entry,GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s){
    if(entry!=0x8262b498u)return false;
    auto& r=s.r;r[10]=m.ReadU32(Address(r[3]+4u));r[6]=0u;r[8]=r[6];SignedCompare(s,r[10]);if(!s.cr6.gt)return true;
    r[9]=r[11]=0xffffffff82190000ull;Flush(fp,s);Load(m,s,9,r[9]-27252u);Load(m,s,8,r[11]-26728u);
    do {
        r[9]=m.ReadU32(Address(r[3]));r[7]=(r[8]<<5u)&0xffffffe0u;SignedCompare(s,r[8]);r[11]=r[9]+r[7];
        r[5]=ReadU64(m,Address(r[11]+12u));WriteU64(m,Address(r[1]-24u),r[5]);r[5]=ReadU64(m,Address(r[11]+20u));WriteU64(m,Address(r[1]-32u),r[5]);
        if(s.cr6.eq){
            s.xer_ca=std::uint8_t(Address(r[10])>0u);--r[10];const auto count=std::bit_cast<std::int32_t>(Address(r[10]));
            s.cr0={std::uint8_t(count<0),std::uint8_t(count>0),std::uint8_t(count==0),s.xer_so};
            bool clear=!s.cr0.gt;
            if(s.cr0.gt){r[10]=m.ReadU8(Address(r[9]+28u));ModeCompare(s,1u);clear=s.cr6.eq;}
            if(clear){r[10]=r[1]-32u;WriteU64(m,Address(r[10]),r[6]);}
        }else{
            --r[10];SignedCompare(s,r[8],r[10]);r[10]=m.ReadU8(Address(r[11]+28u));
            if(s.cr6.lt)Interior(m,fp,s);
            else{ModeCompare(s,1u);if(s.cr6.eq){r[10]=r[1]-24u;WriteU64(m,Address(r[10]),r[6]);}}
        }
        r[10]=ReadU64(m,Address(r[1]-24u));++r[8];WriteU64(m,Address(r[11]+12u),r[10]);
        r[11]=m.ReadU32(Address(r[3]));r[10]=ReadU64(m,Address(r[1]-32u));r[11]+=r[7];WriteU64(m,Address(r[11]+20u),r[10]);
        r[10]=m.ReadU32(Address(r[3]+4u));SignedCompare(s,r[8],r[10]);
    }while(s.cr6.lt);
    return true;
}
}
