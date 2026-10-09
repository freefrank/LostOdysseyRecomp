#include "lo_semantics/geometry_quantized_box61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>

namespace lo::semantic::gpu::geometry_quantized_box61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s,std::uint64_t a,std::uint64_t b) {
    const auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
struct Math {
    GuestMemory& m;Registers& s;
    double Get(unsigned i)const{return std::bit_cast<double>(s.fpr_bits[i]);}
    void Put(unsigned i,double value){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(float(value)));}
    void Load(unsigned i,std::uint64_t address){s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(m.ReadU32(Address(address)))));}
    void Abs(unsigned to,unsigned from){s.fpr_bits[to]=s.fpr_bits[from]&0x7fffffffffffffffull;}
    bool Separated(unsigned a,unsigned b){const auto x=Get(a),y=Get(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),std::uint8_t(std::isnan(x)||std::isnan(y))};return s.cr6.gt;}
};
std::uint64_t SignedHalf(std::uint32_t value) {
    return std::uint64_t(std::int64_t(std::bit_cast<std::int16_t>(std::uint16_t(value))));
}
bool Overlap(unsigned query,unsigned node,GuestMemory& memory,Dependencies d,Registers& s) {
    auto& r=s.r;Math f{memory,s};
    r[11]=memory.ReadU16(Address(r[node]));
    if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;d.fp.SetHostFpControl(s.cached_fp_control);}
    f.Load(0,r[query]+108u);r[10]=memory.ReadU16(Address(r[node]+6u));f.Load(13,r[query]+120u);
    r[11]=SignedHalf(Address(r[11]));r[8]=memory.ReadU16(Address(r[node]+4u));
    // Centers are signed 16-bit; half extents stay unsigned after lhz/extsw.
    r[9]=memory.ReadU16(Address(r[node]+2u));r[7]=memory.ReadU16(Address(r[node]+8u));f.Load(12,r[query]+112u);
    r[6]=memory.ReadU16(Address(r[node]+10u));r[9]=SignedHalf(Address(r[9]));f.Load(10,r[query]+116u);
    if(query==30u)r[29]=0u;
    WriteU64(memory,Address(r[1]+88u),r[11]);f.Load(5,r[query]+128u);
    WriteU64(memory,Address(r[1]+80u),r[10]);r[10]=SignedHalf(Address(r[8]));r[8]=r[7];f.Load(11,r[query]+64u);r[7]=r[6];
    WriteU64(memory,Address(r[1]+96u),r[9]);f.Load(9,r[query]+124u);r[11]=memory.ReadU32(Address(r[query]+96u));f.Load(8,r[query]+40u);
    WriteU64(memory,Address(r[1]+104u),r[10]);++r[11];WriteU64(memory,Address(r[1]+112u),r[8]);WriteU64(memory,Address(r[1]+120u),r[7]);
    memory.WriteU32(Address(r[query]+96u),Address(r[11]));
    // The guest spills all six widened integers, converts to binary64, then
    // rounds each to binary32 before multiplication by its component scale.
    constexpr unsigned regs[]{6u,7u,4u,3u,2u,1u};
    constexpr unsigned slots[]{88u,80u,96u,104u,112u,120u};
    for(unsigned i=0u;i<6u;++i)s.fpr_bits[regs[i]]=std::bit_cast<std::uint64_t>(double(std::bit_cast<std::int64_t>(ReadU64(memory,Address(r[1]+slots[i])))));
    for(const unsigned reg:regs)f.Put(reg,f.Get(reg));
    f.Put(0,f.Get(6)*f.Get(0));f.Put(7,f.Get(7)*f.Get(13));f.Put(13,f.Get(4)*f.Get(12));
    f.Put(12,f.Get(3)*f.Get(10));f.Put(6,f.Get(2)*f.Get(9));f.Put(10,f.Get(1)*f.Get(5));
    f.Put(5,f.Get(11)-f.Get(0));f.Put(9,f.Get(8)+f.Get(7));f.Abs(0,5);if(f.Separated(0,9))return false;
    f.Load(0,r[query]+68u);f.Put(4,f.Get(0)-f.Get(13));f.Load(3,r[query]+44u);f.Put(0,f.Get(3)+f.Get(6));f.Abs(13,4);if(f.Separated(13,0))return false;
    f.Load(0,r[query]+72u);f.Put(12,f.Get(0)-f.Get(12));f.Load(11,r[query]+48u);f.Put(0,f.Get(11)+f.Get(10));f.Abs(13,12);if(f.Separated(13,0))return false;
    f.Load(13,r[query]+60u);f.Put(0,f.Get(11)*f.Get(6));f.Put(2,f.Get(13)*f.Get(4));f.Load(9,r[query]+56u);
    f.Put(0,f.Get(3)*f.Get(10)+f.Get(0));f.Put(2,f.Get(9)*f.Get(12)-f.Get(2));f.Abs(2,2);if(f.Separated(2,0))return false;
    f.Load(0,r[query]+52u);f.Put(10,f.Get(8)*f.Get(10));f.Put(12,f.Get(0)*f.Get(12));f.Put(11,f.Get(11)*f.Get(7)+f.Get(10));
    f.Put(13,f.Get(13)*f.Get(5)-f.Get(12));f.Abs(13,13);if(f.Separated(13,11))return false;
    f.Put(13,f.Get(9)*f.Get(5));f.Put(12,f.Get(8)*f.Get(6));f.Put(0,f.Get(0)*f.Get(4)-f.Get(13));f.Put(13,f.Get(3)*f.Get(7)+f.Get(12));
    f.Abs(0,0);return !f.Separated(0,13);
}
bool Stop(unsigned query,GuestMemory& memory,Registers& s) {
    s.r[11]=memory.ReadU32(Address(s.r[query]+4u));s.r[11]&=5u;
    s.cr0={0u,std::uint8_t(s.r[11]!=0u),std::uint8_t(s.r[11]==0u),s.xer_so};Compare(s,s.r[11],5u);return s.cr6.eq;
}
void Leaf(unsigned query,GuestAddress next,GuestMemory& memory,Dependencies d,Registers& s) {
    s.r[3]=s.r[query];s.lr=next;(void)geometry_triangle_range61::Apply(0x82bd4c40u,memory,d,s);
}
void Walk(bool paired,GuestMemory& memory,Dependencies d,Registers& s) {
    auto& r=s.r;const unsigned first=paired?29u:27u,frame=paired?160u:176u;
    const unsigned query=paired?31u:30u,node=paired?30u:31u,end=paired?29u:27u;
    r[12]=s.lr;s.lr=paired?0x82bd56e0u:0x82bd5cb8u;
    for(unsigned i=first;i<=31u;++i)WriteU64(memory,Address(r[1]-8u*(33u-i)),r[i]);
    memory.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller=r[1];r[1]-=frame;memory.WriteU32(Address(r[1]),Address(caller));
    r[node]=r[4];r[end]=r[5];r[query]=r[3];Compare(s,r[node],r[end]);
    if(s.cr6.lt)do {
        const bool overlaps=Overlap(query,node,memory,d,s);
        if(paired) {
            if(overlaps) {
                r[11]=memory.ReadU32(Address(r[node]+12u));r[10]=r[11]&0x80000000u;Compare(s,r[10],0u);
                if(!s.cr6.eq) {
                    r[4]=Address(r[11])&0x3fffffffu;Leaf(query,0x82bd5894u,memory,d,s);if(Stop(query,memory,s))break;
                    r[11]=memory.ReadU32(Address(r[node]+12u));r[10]=r[11]&0x40000000u;Compare(s,r[10],0u);
                    if(!s.cr6.eq) {
                        r[11]=Address(r[11])&0x3fffffffu;r[4]=r[11]+1u;Leaf(query,0x82bd58c4u,memory,d,s);if(Stop(query,memory,s))break;
                    }
                }
            } else {
                r[11]=memory.ReadU32(Address(r[node]+12u));r[11]&=0x40000000u;Compare(s,r[11],0u);
                if(s.cr6.eq){
                    r[11]=memory.ReadU32(Address(r[node]+16u));r[10]=(std::uint64_t(Address(r[11]))<<2u)&0xfffffffcu;
                    r[11]+=r[10];r[11]=(std::uint64_t(Address(r[11]))<<2u)&0xfffffffcu;r[node]+=r[11];
                }
            }
            r[node]+=20u;
        } else {
            if(overlaps)r[29]=1u;
            r[11]=memory.ReadU32(Address(r[node]+12u));r[28]=r[11]&0x80000000u;Compare(s,r[28],0u);
            if(!s.cr6.eq) {
                Compare(s,r[29],0u);
                if(!s.cr6.eq){r[4]=Address(r[11])&0x3fffffffu;Leaf(query,0x82bd5e7cu,memory,d,s);if(Stop(query,memory,s))break;}
            }
            Compare(s,r[29],0u);
            if(s.cr6.eq){Compare(s,r[28],0u);if(s.cr6.eq){
                r[11]=memory.ReadU32(Address(r[node]+20u));r[10]=(std::uint64_t(Address(r[11]))<<1u)&0xfffffffeu;
                r[11]+=r[10];r[11]=(std::uint64_t(Address(r[11]))<<3u)&0xfffffff8u;r[node]+=r[11];
            }}
            r[node]+=24u;
        }
        Compare(s,r[node],r[end]);
    }while(s.cr6.lt);
    r[1]+=frame;
    for(unsigned i=first;i<=31u;++i)r[i]=ReadU64(memory,Address(r[1]-8u*(33u-i)));
    r[12]=memory.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies d,Registers& s) {
    if(entry!=0x82bd56d8u&&entry!=0x82bd5cb0u)return false;
    Walk(entry==0x82bd56d8u,memory,d,s);return true;
}
}
