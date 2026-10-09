#include "lo_semantics/geometry_math61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>

namespace lo::semantic::gpu::geometry_math61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s, std::uint64_t a, std::uint64_t b) {
    const auto x=Address(a),y=Address(b);
    s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
void DisableFlush(Dependencies d, Registers& s) {
    if(s.cached_fp_control&0x8040u) {s.cached_fp_control&=~0x8040u;d.fp.SetHostFpControl(s.cached_fp_control);}
}
// All arithmetic stages round once to binary32, matching the selected scalar
// body. In particular multiply-add expressions do not add an intermediate
// single-precision rounding; this is not a host SIMD/vector implementation.
struct Floats {
    GuestMemory& memory; Dependencies deps; Registers& s;
    double Get(unsigned i) const {return std::bit_cast<double>(s.fpr_bits[i]);}
    void Put(unsigned i,double value) {DisableFlush(deps,s);s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(float(value)));}
    void Load(unsigned i,std::uint64_t a) {DisableFlush(deps,s);s.fpr_bits[i]=std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(memory.ReadU32(Address(a)))));}
    void Store(unsigned i,std::uint64_t a) {DisableFlush(deps,s);memory.WriteU32(Address(a),std::bit_cast<std::uint32_t>(float(Get(i))));}
    void Compare(unsigned a,unsigned b) {
        const auto x=Get(a),y=Get(b);const bool nan=std::isnan(x)||std::isnan(y);
        s.cr6={std::uint8_t(!nan&&x<y),std::uint8_t(!nan&&x>y),std::uint8_t(!nan&&x==y),std::uint8_t(nan)};
    }
};
std::uint64_t TwiceWord(std::uint64_t x) {return (std::uint64_t(Address(x))<<1u)&0xfffffffeu;}
std::uint64_t WordOffset(std::uint64_t x) {return (std::uint64_t(Address(x))<<2u)&0xfffffffcu;}

// Assemble triangle edges and the direction cross-product. The indexed and
// consecutive layouts have distinct evaluation orders in their generated
// scalar body; retain those orders, including observable scratch FPRs.
void Edges(bool indexed,GuestMemory& memory,Registers& s,Floats& f) {
    auto& r=s.r;
    r[9]=memory.ReadU32(Address(r[11]));r[8]=memory.ReadU32(Address(r[11]+8u));r[7]=memory.ReadU32(Address(r[11]+4u));
    r[11]=TwiceWord(r[9]);memory.WriteU32(Address(r[31]+100u),Address(r[5]));
    if(indexed) {f.Load(11,r[31]+28u);r[11]+=r[9];f.Load(13,r[31]+32u);}
    else {f.Load(13,r[31]+32u);r[11]+=r[9];f.Load(11,r[31]+28u);}
    r[9]=TwiceWord(r[8]);f.Load(12,r[31]+36u);r[11]=WordOffset(r[11]);r[9]+=r[8];
    r[11]+=r[10];r[9]=WordOffset(r[9]);r[8]=TwiceWord(r[7]);r[9]+=r[10];r[8]+=r[7];
    f.Load(0,r[11]);
    if(indexed) r[8]=WordOffset(r[8]);
    f.Load(10,r[11]+4u);
    if(!indexed) r[8]=WordOffset(r[8]);
    f.Load(9,r[9]);r[10]+=r[8];f.Put(1,f.Get(9)-f.Get(0));
    if(indexed) {
        f.Load(6,r[9]+8u);f.Load(9,r[11]+8u);f.Put(30,f.Get(6)-f.Get(9));
        f.Load(7,r[9]+4u);f.Put(31,f.Get(7)-f.Get(10));
        f.Load(5,r[10]+4u);f.Put(10,f.Get(5)-f.Get(10));f.Load(8,r[10]);f.Put(8,f.Get(8)-f.Get(0));
        f.Load(4,r[10]+8u);f.Put(9,f.Get(4)-f.Get(9));
        f.Put(7,f.Get(1)*f.Get(13));f.Put(5,f.Get(30)*f.Get(11));f.Put(6,f.Get(31)*f.Get(12));
        f.Put(11,f.Get(31)*f.Get(11)-f.Get(7));f.Put(12,f.Get(1)*f.Get(12)-f.Get(5));f.Put(13,f.Get(30)*f.Get(13)-f.Get(6));
        f.Put(7,f.Get(12)*f.Get(10));f.Put(7,f.Get(13)*f.Get(8)+f.Get(7));f.Put(2,f.Get(11)*f.Get(9)+f.Get(7));
    } else {
        f.Load(7,r[9]+4u);f.Load(6,r[9]+8u);f.Put(31,f.Get(7)-f.Get(10));
        f.Load(9,r[11]+8u);f.Put(30,f.Get(6)-f.Get(9));f.Load(5,r[10]+8u);f.Put(9,f.Get(5)-f.Get(9));
        f.Load(8,r[10]);f.Put(8,f.Get(8)-f.Get(0));f.Load(4,r[10]+4u);f.Put(10,f.Get(4)-f.Get(10));
        f.Put(7,f.Get(13)*f.Get(1));f.Put(6,f.Get(31)*f.Get(12));f.Put(5,f.Get(30)*f.Get(11));
        f.Put(11,f.Get(31)*f.Get(11)-f.Get(7));f.Put(13,f.Get(30)*f.Get(13)-f.Get(6));f.Put(12,f.Get(12)*f.Get(1)-f.Get(5));
        f.Put(7,f.Get(11)*f.Get(9));f.Put(7,f.Get(13)*f.Get(8)+f.Get(7));f.Put(2,f.Get(12)*f.Get(10)+f.Get(7));
    }
}
bool Nonnegative(unsigned offset,GuestMemory& memory,Registers& s) {
    s.r[11]=memory.ReadU32(Address(s.r[31]+offset));s.r[11]&=0x80000000u;
    Compare(s,s.r[11],0u);return s.cr6.eq;
}
// Front-facing triangles use the object's tolerance before normalization.
// Two-sided triangles instead test normalized barycentric coordinates.
bool Intersect(bool indexed,bool cull,GuestMemory& memory,Registers& s,Floats& f) {
    auto& r=s.r;
    if(cull) {
        f.Compare(2,28);if(s.cr6.lt)return false;
        if(indexed) {
            f.Load(3,r[31]+24u);f.Load(7,r[11]+8u);f.Put(7,f.Get(3)-f.Get(7));
            f.Load(5,r[31]+16u);f.Put(0,f.Get(5)-f.Get(0));f.Load(5,r[31]+20u);f.Load(6,r[11]+4u);f.Put(6,f.Get(5)-f.Get(6));
        } else {
            f.Load(5,r[31]+20u);f.Load(7,r[11]+4u);f.Put(7,f.Get(5)-f.Get(7));
            f.Load(3,r[31]+24u);f.Load(6,r[11]+8u);f.Put(6,f.Get(3)-f.Get(6));f.Load(5,r[31]+16u);f.Put(0,f.Get(5)-f.Get(0));
        }
        f.Load(4,r[31]+136u);s.fpr_bits[5]=s.fpr_bits[4]^0x8000000000000000ull;
        if(indexed) {f.Put(11,f.Get(7)*f.Get(11));f.Put(13,f.Get(0)*f.Get(13)+f.Get(11));f.Put(3,f.Get(6)*f.Get(12)+f.Get(13));}
        else {f.Put(12,f.Get(7)*f.Get(12));f.Put(12,f.Get(6)*f.Get(11)+f.Get(12));f.Put(3,f.Get(0)*f.Get(13)+f.Get(12));}
        f.Store(3,r[31]+84u);f.Compare(3,5);if(s.cr6.lt)return false;
        f.Put(4,indexed?f.Get(4)+f.Get(2):f.Get(2)+f.Get(4));f.Compare(3,4);if(s.cr6.gt)return false;
        f.Put(13,f.Get(9)*f.Get(0));f.Put(12,f.Get(indexed?6u:7u)*f.Get(8));f.Put(11,f.Get(indexed?7u:6u)*f.Get(10));
        f.Put(13,f.Get(indexed?7u:6u)*f.Get(8)-f.Get(13));f.Put(12,f.Get(10)*f.Get(0)-f.Get(12));
        f.Load(0,r[31]+32u);f.Load(10,r[31]+36u);f.Put(11,f.Get(indexed?6u:7u)*f.Get(9)-f.Get(11));f.Load(9,r[31]+28u);
        f.Put(0,f.Get(0)*f.Get(13));f.Put(0,f.Get(10)*f.Get(12)+f.Get(0));f.Put(0,indexed?f.Get(11)*f.Get(9)+f.Get(0):f.Get(9)*f.Get(11)+f.Get(0));
        f.Store(0,r[31]+88u);f.Compare(0,5);if(s.cr6.lt)return false;
        f.Put(0,f.Get(0)+f.Get(3));f.Compare(0,4);if(s.cr6.gt)return false;
        if(indexed) {f.Put(0,f.Get(12)*f.Get(30));f.Put(0,f.Get(11)*f.Get(1)+f.Get(0));f.Put(13,f.Get(13)*f.Get(31)+f.Get(0));}
        else {f.Put(0,f.Get(13)*f.Get(31));f.Put(0,f.Get(12)*f.Get(30)+f.Get(0));f.Put(13,f.Get(11)*f.Get(1)+f.Get(0));}
        f.Store(13,r[31]+80u);if(!Nonnegative(80u,memory,s))return false;
        f.Put(0,f.Get(29)/f.Get(2));f.Load(12,r[31]+88u);f.Put(13,f.Get(13)*f.Get(0));f.Store(13,r[31]+80u);
        f.Put(13,f.Get(3)*f.Get(0));f.Store(13,r[31]+84u);f.Put(0,indexed?f.Get(12)*f.Get(0):f.Get(0)*f.Get(12));f.Store(0,r[31]+88u);
    } else {
        f.Compare(2,27);
        if(s.cr6.gt) {f.Compare(2,28);if(s.cr6.lt)return false;}
        f.Load(7,r[11]+(indexed?8u:4u));f.Put(5,f.Get(29)/f.Get(2));
        if(indexed) {
            f.Load(3,r[31]+24u);f.Put(7,f.Get(3)-f.Get(7));f.Load(4,r[31]+16u);f.Put(0,f.Get(4)-f.Get(0));
            f.Load(6,r[11]+4u);f.Load(4,r[31]+20u);f.Put(6,f.Get(4)-f.Get(6));
            f.Put(11,f.Get(7)*f.Get(11));f.Put(13,f.Get(0)*f.Get(13)+f.Get(11));f.Put(13,f.Get(6)*f.Get(12)+f.Get(13));
        } else {
            f.Load(4,r[31]+20u);f.Put(7,f.Get(4)-f.Get(7));f.Load(6,r[11]+8u);f.Load(3,r[31]+24u);f.Put(6,f.Get(3)-f.Get(6));
            f.Load(4,r[31]+16u);f.Put(0,f.Get(4)-f.Get(0));f.Put(12,f.Get(7)*f.Get(12));f.Put(12,f.Get(6)*f.Get(11)+f.Get(12));f.Put(13,f.Get(0)*f.Get(13)+f.Get(12));
        }
        f.Put(4,f.Get(13)*f.Get(5));f.Store(4,r[31]+84u);r[11]=memory.ReadU32(Address(r[31]+84u));r[10]=r[11]&0x80000000u;
        Compare(s,r[10],0u);if(!s.cr6.eq)return false;Compare(s,r[11],r[25]);if(s.cr6.gt)return false;
        f.Put(13,f.Get(9)*f.Get(0));f.Put(12,f.Get(indexed?6u:7u)*f.Get(8));f.Put(11,f.Get(indexed?7u:6u)*f.Get(10));
        f.Put(13,f.Get(indexed?7u:6u)*f.Get(8)-f.Get(13));f.Put(0,f.Get(10)*f.Get(0)-f.Get(12));
        f.Load(10,r[31]+36u);f.Put(12,f.Get(indexed?6u:7u)*f.Get(9)-f.Get(11));f.Load(11,r[31]+32u);f.Load(9,r[31]+28u);
        f.Put(11,f.Get(11)*f.Get(13));f.Put(11,f.Get(10)*f.Get(0)+f.Get(11));f.Put(11,f.Get(12)*f.Get(9)+f.Get(11));f.Put(11,f.Get(11)*f.Get(5));
        f.Store(11,r[31]+88u);if(!Nonnegative(88u,memory,s))return false;
        f.Put(11,f.Get(11)+f.Get(4));f.Compare(11,29);if(s.cr6.gt)return false;
        if(indexed) {f.Put(0,f.Get(0)*f.Get(30));f.Put(0,f.Get(12)*f.Get(1)+f.Get(0));f.Put(0,f.Get(13)*f.Get(31)+f.Get(0));}
        else {f.Put(13,f.Get(13)*f.Get(31));f.Put(0,f.Get(0)*f.Get(30)+f.Get(13));f.Put(0,f.Get(12)*f.Get(1)+f.Get(0));}
        f.Put(0,f.Get(0)*f.Get(5));f.Store(0,r[31]+80u);if(!Nonnegative(80u,memory,s))return false;
    }
    return true;
}
// Hit records are four words: triangle id, distance, and two barycentric
// coordinates. A nearest-only destination replaces record zero; otherwise
// append four words, allowing the existing allocator to move its storage.
void Record(bool indexed,GuestMemory& memory,Dependencies d,Registers& s,Floats& f) {
    auto& r=s.r;const unsigned record=indexed?28u:26u;
    r[11]=memory.ReadU32(Address(r[31]+80u));r[10]=memory.ReadU32(Address(r[31]+132u));
    Compare(s,r[11],r[10]);if(!s.cr6.lt)return;
    r[11]=memory.ReadU32(Address(r[31]+104u));r[record]=r[31]+76u;r[10]=memory.ReadU32(Address(r[31]+4u));
    ++r[11];r[30]=memory.ReadU32(Address(r[31]+92u));r[10]|=4u;Compare(s,r[30],0u);
    memory.WriteU32(Address(r[record]),Address(r[6]));memory.WriteU32(Address(r[31]+104u),Address(r[11]));memory.WriteU32(Address(r[31]+4u),Address(r[10]));
    if(s.cr6.eq)return;
    r[11]=memory.ReadU8(Address(r[31]+140u));Compare(s,r[11],0u);
    if(!s.cr6.eq) {
        r[11]=memory.ReadU32(Address(r[30]+4u));r[11]&=0xfffffffcu;Compare(s,r[11],0u);
        if(!s.cr6.eq) {
            r[11]=memory.ReadU32(Address(r[30]+8u));Compare(s,r[11],0u);if(s.cr6.eq)return;
            f.Load(0,r[31]+80u);f.Load(13,r[11]+4u);f.Compare(0,13);if(!s.cr6.lt)return;
            r[10]=Address(r[6]);memory.WriteU32(Address(r[11]),Address(r[10]));
            for(unsigned offset=4u;offset<=12u;offset+=4u) {
                r[10]=memory.ReadU32(Address(r[record]+offset));memory.WriteU32(Address(r[11]+offset),Address(r[10]));
            }
            return;
        }
    }
    Compare(s,r[record],0u);if(s.cr6.eq)return;
    r[11]=memory.ReadU32(Address(r[30]+4u));r[10]=memory.ReadU32(Address(r[30]));r[11]+=4u;Compare(s,r[11],r[10]);
    if(s.cr6.gt) {
        r[4]=4u;r[3]=r[30];s.lr=indexed?0x82bd4810u:0x82bd4bf0u;
        (void)reader_buffer_growth61::Apply(0x82bd2870u,memory,d,s);
    }
    if(indexed) {
        r[10]=memory.ReadU32(Address(r[30]+4u));r[5]=16u;r[11]=memory.ReadU32(Address(r[30]+8u));
        r[4]=r[record];r[10]=WordOffset(r[10]);r[3]=r[11]+r[10];
    } else {
        r[11]=memory.ReadU32(Address(r[30]+4u));r[5]=16u;r[10]=memory.ReadU32(Address(r[30]+8u));
        r[4]=r[record];r[11]=WordOffset(r[11]);r[3]=r[11]+r[10];
    }
    s.lr=indexed?0x82bd482cu:0x82bd4c0cu;(void)crt_copy_full_context::Apply(0x82b7a0b0u,memory,s);
    r[11]=memory.ReadU32(Address(r[30]+4u));r[11]+=4u;memory.WriteU32(Address(r[30]+4u),Address(r[11]));
}
void Visit(GuestMemory& memory,Dependencies d,Registers& s) {
    auto& r=s.r;Floats f{memory,d,s};
    r[12]=s.lr;s.lr=0x82bd4450u;
    for(unsigned i=25u;i<=31u;++i)WriteU64(memory,Address(r[1]-8u*(33u-i)),r[i]);
    memory.WriteU32(Address(r[1]-8u),Address(r[12]));r[12]=r[1]-64u;s.lr=0x82bd4458u;
    DisableFlush(d,s);
    for(unsigned i=27u;i<=31u;++i)WriteU64(memory,Address(r[12]-8u*(32u-i)),s.fpr_bits[i]);
    const auto caller_sp=r[1];r[1]-=192u;memory.WriteU32(Address(r[1]),Address(caller_sp));
    r[31]=r[3];r[8]=WordOffset(r[4]);r[11]=memory.ReadU32(Address(r[31]+8u));
    r[9]=memory.ReadU32(Address(r[11]+24u));r[10]=memory.ReadU32(Address(r[11]+32u));Compare(s,r[10],0u);
    r[11]=memory.ReadU32(Address(r[9]+r[8]));r[9]=Address(r[11])&15u;r[27]=r[9]+1u;
    const bool indexed=!s.cr6.eq;
    if(indexed) {
        r[11]=(Address(r[11])>>2u)&0x3ffffffcu;Compare(s,r[27],0u);r[29]=r[11]+r[10];
    } else {
        r[29]=Address(r[11])>>4u;Compare(s,r[27],0u);
        if(!s.cr6.eq) {r[11]=TwiceWord(r[29]);r[9]=0xffffffff82030000ull;r[11]+=r[29];r[10]=0xffffffff82000000ull;r[28]=WordOffset(r[11]);}
    }
    bool ordinary_indexed_exit=false;
    if(!s.cr6.eq) {
        if(indexed) {r[9]=0xffffffff82030000ull;r[10]=0xffffffff82000000ull;}
        r[11]=0xffffffff820a0000ull;
        if(indexed) r[25]=0x3f800000u;
        f.Load(27,r[9]+23736u);
        if(!indexed)r[25]=0x3f800000u;
        f.Load(29,r[10]+30596u);f.Load(28,r[11]+27532u);
        do {
            r[11]=memory.ReadU32(Address(r[31]+12u));
            if(indexed) {
                --r[27];r[10]=memory.ReadU32(Address(r[31]+100u));r[9]=memory.ReadU8(Address(r[31]+141u));
                r[6]=memory.ReadU32(Address(r[29]));r[5]=r[10]+1u;Compare(s,r[9],0u);
                r[10]=memory.ReadU32(Address(r[11]+20u));r[29]+=4u;r[9]=memory.ReadU32(Address(r[11]+16u));
                r[11]=TwiceWord(r[6]);r[11]+=r[6];r[11]=WordOffset(r[11]);r[11]+=r[9];
            } else {
                r[9]=r[28];r[8]=memory.ReadU8(Address(r[31]+141u));r[6]=r[29];r[10]=memory.ReadU32(Address(r[31]+100u));--r[27];
                Compare(s,r[8],0u);r[5]=r[10]+1u;r[8]=memory.ReadU32(Address(r[11]+16u));++r[29];
                r[10]=memory.ReadU32(Address(r[11]+20u));r[28]+=12u;r[11]=r[8]+r[9];
            }
            const bool cull=!s.cr6.eq;
            Edges(indexed,memory,s,f);
            if(Intersect(indexed,cull,memory,s,f))Record(indexed,memory,d,s,f);
            r[11]=memory.ReadU32(Address(r[31]+4u));r[11]&=5u;
            s.cr0={0u,std::uint8_t(r[11]!=0u),std::uint8_t(r[11]==0u),s.xer_so};
            Compare(s,r[11],5u);if(s.cr6.eq)break;
            Compare(s,r[27],0u);
            if(s.cr6.eq) {ordinary_indexed_exit=indexed;break;}
        } while(true);
    }
    r[1]+=192u;r[12]=r[1]-64u;s.lr=ordinary_indexed_exit?0x82bd485cu:0x82bd4c3cu;
    DisableFlush(d,s);
    for(unsigned i=27u;i<=31u;++i)s.fpr_bits[i]=ReadU64(memory,Address(r[12]-8u*(32u-i)));
    for(unsigned i=25u;i<=31u;++i)r[i]=ReadU64(memory,Address(r[1]-8u*(33u-i)));
    r[12]=memory.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies d,Registers& s) {
    if(entry!=0x82bd4448u) return false;
    Visit(memory,d,s);
    return true;
}
}
