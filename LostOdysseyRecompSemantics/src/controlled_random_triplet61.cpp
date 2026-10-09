#include "lo_semantics/controlled_random_triplet61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::controlled_random_triplet61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
double F(const Registers& s,unsigned i){return std::bit_cast<double>(s.fpr_bits[i]);}
void F(Registers& s,unsigned i,double value){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(value);}
double Single(double value){return static_cast<float>(value);}
void Load(GuestMemory& m,Registers& s,unsigned i,std::uint64_t p){F(s,i,std::bit_cast<float>(m.ReadU32(Address(p))));}
void Store(GuestMemory& m,Registers& s,unsigned i,std::uint64_t p){m.WriteU32(Address(p),std::bit_cast<std::uint32_t>(static_cast<float>(F(s,i))));}
void Flush(float_triplet_transfer::NativeServices& fp,Registers& s){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;fp.SetHostFpControl(s.cached_fp_control);}}
void Compare(Registers& s,std::uint64_t v){const auto x=Address(v);s.cr6={0u,std::uint8_t(x!=0u),std::uint8_t(x==0u),s.xer_so};}
std::uint64_t Product(std::uint64_t a,std::uint64_t b){return std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(a)))*std::int64_t(std::bit_cast<std::int32_t>(Address(b))));}
std::uint64_t SignWord(std::uint64_t v){return std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(v))));}
void Truncate(Registers& s,unsigned to,unsigned from){
    const double value=F(s,from);std::int32_t converted;
    // Match the generated saturate-high / x64 cvttsd2si result, including its
    // indefinite signed minimum for unordered or below-range values.
    if(value>double(std::numeric_limits<std::int32_t>::max()))converted=std::numeric_limits<std::int32_t>::max();
    else if(std::isnan(value)||value<double(std::numeric_limits<std::int32_t>::min()))converted=std::numeric_limits<std::int32_t>::min();
    else converted=static_cast<std::int32_t>(value);
    s.fpr_bits[to]=std::uint64_t(std::int64_t(converted));
}
void IntegerToDouble(Registers& s,unsigned i){F(s,i,double(std::bit_cast<std::int64_t>(s.fpr_bits[i])));}
void GenerateFractions(GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s,unsigned flags_offset,unsigned enable_shift){
    auto& r=s.r;r[8]=0xffffffff83310000ull;r[11]=196280320u;r[6]=r[1]-16u;r[10]=r[11]|33845u;
    r[11]=907608064u;r[9]=m.ReadU32(Address(r[8]+13948u));r[11]|=25451u;r[9]=Product(r[9],r[10]);r[9]+=r[11];
    r[7]=Address(r[9])&0x7fffffu;r[9]=Product(r[9],r[10]);r[7]|=0x3f800000u;m.WriteU32(Address(r[1]-16u),Address(r[7]));
    r[7]=r[11];r[11]+=r[9];r[9]=Address(r[11])&0x7fffffu;r[11]=Product(r[11],r[10]);r[10]=r[9]|0x3f800000u;
    r[11]+=r[7];r[7]=0xffffffff82190000ull;m.WriteU32(Address(r[1]-12u),Address(r[10]));r[10]=Address(r[11])&0x7fffffu;
    m.WriteU32(Address(r[8]+13948u),Address(r[11]));r[10]|=0x3f800000u;r[11]=m.ReadU32(Address(r[3]+flags_offset));
    m.WriteU32(Address(r[1]-8u),Address(r[10]));r[10]=(Address(r[11])>>(32u-enable_shift))&1u;Compare(s,r[10]);
    Flush(fp,s);Load(m,s,0,r[1]-16u);Truncate(s,13,0);m.WriteU32(Address(r[6]),Address(s.fpr_bits[13]));
    Load(m,s,13,r[1]-12u);Truncate(s,11,13);Load(m,s,12,r[1]-8u);Truncate(s,10,12);
    r[9]=m.ReadU32(Address(r[1]-16u));r[8]=r[1]-16u;r[9]=SignWord(r[9]);m.WriteU32(Address(r[8]),Address(s.fpr_bits[11]));
    WriteU64(m,Address(r[1]-8u),r[9]);r[9]=m.ReadU32(Address(r[1]-16u));r[8]=r[1]-16u;r[9]=SignWord(r[9]);
    s.fpr_bits[11]=ReadU64(m,Address(r[1]-8u));IntegerToDouble(s,11);m.WriteU32(Address(r[8]),Address(s.fpr_bits[10]));
    r[8]=0xffffffff82000000ull;WriteU64(m,Address(r[1]-8u),r[9]);Load(m,s,9,r[8]+3664u);F(s,11,Single(F(s,11)));F(s,0,Single(F(s,0)-F(s,11)));
    r[9]=m.ReadU32(Address(r[1]-16u));r[9]=SignWord(r[9]);s.fpr_bits[11]=ReadU64(m,Address(r[1]-8u));IntegerToDouble(s,11);
    WriteU64(m,Address(r[1]-8u),r[9]);r[9]=0xffffffff821c0000ull;F(s,11,Single(F(s,11)));F(s,11,Single(F(s,13)-F(s,11)));
    s.fpr_bits[13]=ReadU64(m,Address(r[1]-8u));IntegerToDouble(s,13);F(s,13,Single(F(s,13)));F(s,10,Single(F(s,12)-F(s,13)));
    Load(m,s,13,r[9]-21900u);Load(m,s,12,r[7]-27252u);
}
void WriteAxis(GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s,unsigned axis,unsigned source,std::uint32_t negative_mask){
    auto& r=s.r;
    if(!s.cr6.eq){
        r[9]=r[11]&negative_mask;Compare(s,r[9]);
        if(!s.cr6.eq){if(axis!=0u)Flush(fp,s);F(s,0,Single(F(s,source)*F(s,13)-F(s,12)));Store(m,s,0,r[5]+4u*axis);return;}
        Compare(s,r[10]);if(!s.cr6.eq){Flush(fp,s);Store(m,s,source,r[5]+4u*axis);return;}
    }
    r[11]&=negative_mask;Compare(s,r[11]);Flush(fp,s);
    if(!s.cr6.eq){s.fpr_bits[0]=s.fpr_bits[source]^0x8000000000000000ull;Store(m,s,0,r[5]+4u*axis);}
    else Store(m,s,9,r[5]+4u*axis);
}
}
bool Apply(GuestAddress entry,GuestMemory& m,float_triplet_transfer::NativeServices& fp,Registers& s){
    unsigned offset,shift;if(entry==0x8261e470u){offset=160u;shift=2u;}else if(entry==0x82620f40u){offset=164u;shift=6u;}else return false;
    GenerateFractions(m,fp,s,offset,shift);WriteAxis(m,fp,s,0u,0u,0x08000000u>>(shift-2u));
    for(unsigned axis=1u;axis<3u;++axis){s.r[11]=m.ReadU32(Address(s.r[3]+offset));s.r[10]=(Address(s.r[11])>>(32u-shift-axis))&1u;Compare(s,s.r[10]);
        WriteAxis(m,fp,s,axis,axis==1u?11u:10u,0x08000000u>>(shift-2u+axis));}
    return true;
}
}
