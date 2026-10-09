#include "lo_semantics/geometry_box61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>

namespace lo::semantic::gpu::geometry_box61 {
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
bool Overlap(unsigned query,GuestMemory& memory,Dependencies d,Registers& s) {
    auto& r=s.r;Math f{memory,s};
    r[11]=memory.ReadU32(Address(r[query]+96u));
    if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;d.fp.SetHostFpControl(s.cached_fp_control);}
    f.Load(0,r[query]+64u);f.Load(5,r[query]+40u);
    if(query==28u)r[30]=0u;
    ++r[11];memory.WriteU32(Address(r[query]+96u),Address(r[11]));
    f.Load(13,r[31]);f.Put(4,f.Get(0)-f.Get(13));f.Load(3,r[31]+12u);f.Put(13,f.Get(3)+f.Get(5));f.Abs(0,4);
    if(f.Separated(0,13))return false;
    f.Load(0,r[query]+68u);f.Load(13,r[31]+4u);f.Put(8,f.Get(0)-f.Get(13));f.Load(7,r[31]+16u);f.Load(6,r[query]+44u);
    f.Put(0,f.Get(6)+f.Get(7));f.Abs(13,8);if(f.Separated(13,0))return false;
    f.Load(0,r[query]+72u);f.Load(13,r[31]+8u);f.Put(12,f.Get(0)-f.Get(13));f.Load(11,r[31]+20u);f.Load(10,r[query]+48u);
    f.Put(0,f.Get(10)+f.Get(11));f.Abs(13,12);if(f.Separated(13,0))return false;
    // Cross-axis tests retain each multiply/add rounding and its scratch FPR.
    f.Load(13,r[query]+60u);f.Put(0,f.Get(10)*f.Get(7));f.Put(2,f.Get(13)*f.Get(8));f.Load(9,r[query]+56u);
    f.Put(0,f.Get(11)*f.Get(6)+f.Get(0));f.Put(2,f.Get(9)*f.Get(12)-f.Get(2));f.Abs(2,2);if(f.Separated(2,0))return false;
    f.Load(0,r[query]+52u);f.Put(11,f.Get(11)*f.Get(5));f.Put(12,f.Get(0)*f.Get(12));f.Put(11,f.Get(10)*f.Get(3)+f.Get(11));
    f.Put(13,f.Get(13)*f.Get(4)-f.Get(12));f.Abs(13,13);if(f.Separated(13,11))return false;
    f.Put(13,f.Get(9)*f.Get(4));f.Put(12,f.Get(7)*f.Get(5));f.Put(0,f.Get(0)*f.Get(8)-f.Get(13));f.Put(13,f.Get(6)*f.Get(3)+f.Get(12));
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
    auto& r=s.r;const unsigned first=paired?29u:27u,frame=paired?112u:128u,query=paired?30u:28u,end=paired?29u:27u;
    r[12]=s.lr;s.lr=paired?0x82bd5558u:0x82bd5b48u;
    for(unsigned i=first;i<=31u;++i)WriteU64(memory,Address(r[1]-8u*(33u-i)),r[i]);
    memory.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller=r[1];r[1]-=frame;memory.WriteU32(Address(r[1]),Address(caller));
    r[31]=r[4];r[end]=r[5];r[query]=r[3];Compare(s,r[31],r[end]);
    if(s.cr6.lt)do {
        const bool overlaps=Overlap(query,memory,d,s);
        if(paired) {
            if(overlaps) {
                r[11]=memory.ReadU32(Address(r[31]+24u));r[10]=r[11]&0x80000000u;Compare(s,r[10],0u);
                if(!s.cr6.eq) {
                    r[4]=Address(r[11])&0x3fffffffu;Leaf(query,0x82bd5664u,memory,d,s);if(Stop(query,memory,s))break;
                    r[11]=memory.ReadU32(Address(r[31]+24u));r[10]=r[11]&0x40000000u;Compare(s,r[10],0u);
                    if(!s.cr6.eq) {
                        r[11]=Address(r[11])&0x3fffffffu;r[4]=r[11]+1u;Leaf(query,0x82bd5694u,memory,d,s);if(Stop(query,memory,s))break;
                    }
                }
            } else {
                r[11]=memory.ReadU32(Address(r[31]+24u));r[11]&=0x40000000u;Compare(s,r[11],0u);
                if(s.cr6.eq){r[11]=memory.ReadU32(Address(r[31]+28u));r[11]=(std::uint64_t(Address(r[11]))<<5u)&0xffffffe0u;r[31]+=r[11];}
            }
            r[31]+=32u;
        } else {
            if(overlaps)r[30]=1u;
            r[11]=memory.ReadU32(Address(r[31]+24u));r[29]=r[11]&0x80000000u;Compare(s,r[29],0u);
            if(!s.cr6.eq) {
                Compare(s,r[30],0u);
                if(!s.cr6.eq){r[4]=Address(r[11])&0x3fffffffu;Leaf(query,0x82bd5c64u,memory,d,s);if(Stop(query,memory,s))break;}
            }
            Compare(s,r[30],0u);
            if(s.cr6.eq){Compare(s,r[29],0u);if(s.cr6.eq){
                r[11]=memory.ReadU32(Address(r[31]+32u));r[10]=(std::uint64_t(Address(r[11]))<<3u)&0xfffffff8u;
                r[11]+=r[10];r[11]=(std::uint64_t(Address(r[11]))<<2u)&0xfffffffcu;r[31]+=r[11];
            }}
            r[31]+=36u;
        }
        Compare(s,r[31],r[end]);
    }while(s.cr6.lt);
    r[1]+=frame;
    for(unsigned i=first;i<=31u;++i)r[i]=ReadU64(memory,Address(r[1]-8u*(33u-i)));
    r[12]=memory.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies d,Registers& s) {
    if(entry!=0x82bd5550u&&entry!=0x82bd5b40u)return false;
    Walk(entry==0x82bd5550u,memory,d,s);return true;
}
}
