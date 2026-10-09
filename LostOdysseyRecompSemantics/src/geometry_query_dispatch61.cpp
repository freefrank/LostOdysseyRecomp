#include "lo_semantics/geometry_query_dispatch61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/geometry_query_prepare61.h"
#include "lo_semantics/geometry_box61.h"
#include "lo_semantics/geometry_quantized_box61.h"
#include "lo_semantics/geometry_paired_range61.h"
#include "lo_semantics/geometry_tree_range61.h"
#include "lo_semantics/geometry_quantized_unbounded61.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::geometry_query_dispatch61 {
namespace {
using ScalarDependencies=reader_buffer_growth61::Dependencies;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
double Float(const Registers& s, unsigned n) { return std::bit_cast<double>(s.fpr_bits[n]); }
void Float(Registers& s, unsigned n, double value) { s.fpr_bits[n] = std::bit_cast<std::uint64_t>(value); }
double Single(double value) { return static_cast<float>(value); }
double Load(GuestMemory& m, std::uint64_t address) { return std::bit_cast<float>(m.ReadU32(Address(address))); }
void Store(GuestMemory& m, std::uint64_t address, double value) { m.WriteU32(Address(address), std::bit_cast<std::uint32_t>(static_cast<float>(value))); }
void Compare(Registers& s, std::uint64_t a, std::uint64_t b) {
    s.cr6 = {std::uint8_t(Address(a)<Address(b)), std::uint8_t(Address(a)>Address(b)), std::uint8_t(Address(a)==Address(b)), s.xer_so};
}
std::int32_t Signed(std::uint64_t value) { return std::bit_cast<std::int32_t>(Address(value)); }
void CompareSigned(Registers& s, std::int32_t a, std::int32_t b) {
    s.cr6={std::uint8_t(a<b),std::uint8_t(a>b),std::uint8_t(a==b),s.xer_so};
}
void CompareFloat(Registers& s, double a, double b) {
    s.cr6 = {std::uint8_t(a<b), std::uint8_t(a>b), std::uint8_t(a==b), std::uint8_t(std::isnan(a)||std::isnan(b))};
}
void DisableFlush(ScalarDependencies deps, Registers& s) {
    if (s.cached_fp_control & 0x8040u) { s.cached_fp_control &= ~0x8040u; deps.fp.SetHostFpControl(s.cached_fp_control); }
}
void EnterFrame(GuestMemory& memory, Registers& s) {
    auto& r=s.r; r[12]=s.lr;
    for (unsigned n=25;n<32;++n) WriteU64(memory,Address(r[1]-16u-8u*(31u-n)),r[n]);
    memory.WriteU32(Address(r[1]-8u),Address(r[12]));
    s.lr=0x82bd7260u; r[12]=r[1]-64u; s.lr=0x82bd7268u;
    for (unsigned n=27;n<32;++n) WriteU64(memory,Address(r[12]-8u*(32u-n)),s.fpr_bits[n]);
    const auto caller=r[1]; r[1]-=192u; memory.WriteU32(Address(r[1]),Address(caller));
}
void LeaveFrame(GuestMemory& memory, Registers& s) {
    auto& r=s.r; r[1]+=192u; r[12]=r[1]-64u;
    for (unsigned n=27;n<32;++n) s.fpr_bits[n]=ReadU64(memory,Address(r[12]-8u*(32u-n)));
    for (unsigned n=25;n<32;++n) r[n]=ReadU64(memory,Address(r[1]-16u-8u*(31u-n)));
    r[12]=memory.ReadU32(Address(r[1]-8u)); s.lr=r[12];
}
// Edge vectors and determinant for a consecutive triangle record.
void TriangleEdges(GuestMemory& memory,ScalarDependencies deps,Registers& s){
    auto& r=s.r;
    r[9] = memory.ReadU32(Address(Address(r[11]) + 0));
    r[8] = memory.ReadU32(Address(Address(r[11]) + 8));
    r[7] = memory.ReadU32(Address(Address(r[11]) + 4));
    r[11] = recovery_abi::WordRotateMask(r[9], 1, 0xFFFFFFFEull);
    memory.WriteU32(Address(Address(r[31]) + 100), Address(r[6]));
    DisableFlush(deps, s);
    Float(s, 13, Load(memory, Address(r[31]) + 32));
    r[11] = r[9] + r[11];
    Float(s, 11, Load(memory, Address(r[31]) + 28));
    r[9] = recovery_abi::WordRotateMask(r[8], 1, 0xFFFFFFFEull);
    Float(s, 12, Load(memory, Address(r[31]) + 36));
    r[11] = recovery_abi::WordRotateMask(r[11], 2, 0xFFFFFFFCull);
    r[9] = r[8] + r[9];
    r[11] = r[11] + r[10];
    r[9] = recovery_abi::WordRotateMask(r[9], 2, 0xFFFFFFFCull);
    r[8] = recovery_abi::WordRotateMask(r[7], 1, 0xFFFFFFFEull);
    r[9] = r[9] + r[10];
    r[8] = r[7] + r[8];
    Float(s, 0, Load(memory, Address(r[11]) + 0));
    Float(s, 10, Load(memory, Address(r[11]) + 4));
    r[8] = recovery_abi::WordRotateMask(r[8], 2, 0xFFFFFFFCull);
    Float(s, 9, Load(memory, Address(r[9]) + 0));
    r[10] = r[8] + r[10];
    Float(s, 1, Single(Float(s, 9) - Float(s, 0)));
    Float(s, 7, Load(memory, Address(r[9]) + 4));
    Float(s, 6, Load(memory, Address(r[9]) + 8));
    Float(s, 31, Single(Float(s, 7) - Float(s, 10)));
    Float(s, 9, Load(memory, Address(r[11]) + 8));
    Float(s, 30, Single(Float(s, 6) - Float(s, 9)));
    Float(s, 5, Load(memory, Address(r[10]) + 8));
    Float(s, 9, Single(Float(s, 5) - Float(s, 9)));
    Float(s, 8, Load(memory, Address(r[10]) + 0));
    Float(s, 8, Single(Float(s, 8) - Float(s, 0)));
    Float(s, 4, Load(memory, Address(r[10]) + 4));
    Float(s, 10, Single(Float(s, 4) - Float(s, 10)));
    Float(s, 7, Single(Float(s, 13) * Float(s, 1)));
    Float(s, 6, Single(Float(s, 12) * Float(s, 31)));
    Float(s, 5, Single(Float(s, 11) * Float(s, 30)));
    Float(s, 11, Single(Float(s, 11) * Float(s, 31) - Float(s, 7)));
    Float(s, 13, Single(Float(s, 13) * Float(s, 30) - Float(s, 6)));
    Float(s, 12, Single(Float(s, 12) * Float(s, 1) - Float(s, 5)));
    Float(s, 7, Single(Float(s, 11) * Float(s, 9)));
    Float(s, 7, Single(Float(s, 13) * Float(s, 8) + Float(s, 7)));
    Float(s, 2, Single(Float(s, 12) * Float(s, 10) + Float(s, 7)));
}
// Backface-culling test retains unnormalized barycentrics until acceptance.
bool Front(GuestMemory& memory,Registers& s){
    auto& r=s.r;
    CompareFloat(s, Float(s, 2), Float(s, 28));
    if (s.cr6.lt) return false;
    Float(s, 3, Load(memory, Address(r[31]) + 24));
    Float(s, 7, Load(memory, Address(r[11]) + 8));
    Float(s, 7, Single(Float(s, 3) - Float(s, 7)));
    Float(s, 5, Load(memory, Address(r[31]) + 16));
    Float(s, 0, Single(Float(s, 5) - Float(s, 0)));
    Float(s, 5, Load(memory, Address(r[31]) + 20));
    Float(s, 6, Load(memory, Address(r[11]) + 4));
    Float(s, 6, Single(Float(s, 5) - Float(s, 6)));
    Float(s, 4, Load(memory, Address(r[31]) + 136));
    s.fpr_bits[5] = s.fpr_bits[4] ^ 0x8000000000000000;
    Float(s, 11, Single(Float(s, 7) * Float(s, 11)));
    Float(s, 13, Single(Float(s, 0) * Float(s, 13) + Float(s, 11)));
    Float(s, 3, Single(Float(s, 6) * Float(s, 12) + Float(s, 13)));
    Store(memory, Address(r[31]) + 84, Float(s, 3));
    CompareFloat(s, Float(s, 3), Float(s, 5));
    if (s.cr6.lt) return false;
    Float(s, 4, Single(Float(s, 4) + Float(s, 2)));
    CompareFloat(s, Float(s, 3), Float(s, 4));
    if (s.cr6.gt) return false;
    Float(s, 13, Single(Float(s, 9) * Float(s, 0)));
    Float(s, 12, Single(Float(s, 6) * Float(s, 8)));
    Float(s, 11, Single(Float(s, 7) * Float(s, 10)));
    Float(s, 13, Single(Float(s, 7) * Float(s, 8) - Float(s, 13)));
    Float(s, 12, Single(Float(s, 10) * Float(s, 0) - Float(s, 12)));
    Float(s, 0, Load(memory, Address(r[31]) + 32));
    Float(s, 10, Load(memory, Address(r[31]) + 36));
    Float(s, 11, Single(Float(s, 6) * Float(s, 9) - Float(s, 11)));
    Float(s, 9, Load(memory, Address(r[31]) + 28));
    Float(s, 0, Single(Float(s, 0) * Float(s, 13)));
    Float(s, 0, Single(Float(s, 10) * Float(s, 12) + Float(s, 0)));
    Float(s, 0, Single(Float(s, 9) * Float(s, 11) + Float(s, 0)));
    Store(memory, Address(r[31]) + 88, Float(s, 0));
    CompareFloat(s, Float(s, 0), Float(s, 5));
    if (s.cr6.lt) return false;
    Float(s, 0, Single(Float(s, 0) + Float(s, 3)));
    CompareFloat(s, Float(s, 0), Float(s, 4));
    if (s.cr6.gt) return false;
    Float(s, 0, Single(Float(s, 12) * Float(s, 30)));
    Float(s, 0, Single(Float(s, 11) * Float(s, 1) + Float(s, 0)));
    Float(s, 13, Single(Float(s, 13) * Float(s, 31) + Float(s, 0)));
    Store(memory, Address(r[31]) + 80, Float(s, 13));
    r[11] = memory.ReadU32(Address(Address(r[31]) + 80));
    r[11] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000ull);
    Compare(s, Address(r[11]), 0);
    if (!s.cr6.eq) return false;
    Float(s, 0, Single(Float(s, 29) / Float(s, 2)));
    Float(s, 12, Load(memory, Address(r[31]) + 88));
    Float(s, 13, Single(Float(s, 13) * Float(s, 0)));
    Store(memory, Address(r[31]) + 80, Float(s, 13));
    Float(s, 13, Single(Float(s, 3) * Float(s, 0)));
    Store(memory, Address(r[31]) + 84, Float(s, 13));
    Float(s, 0, Single(Float(s, 12) * Float(s, 0)));
    Store(memory, Address(r[31]) + 88, Float(s, 0));
    return true;
}
// Two-sided test normalizes before checking barycentrics.
bool BothSides(GuestMemory& memory,ScalarDependencies deps,Registers& s){
    auto& r=s.r;
    DisableFlush(deps, s);
    CompareFloat(s, Float(s, 2), Float(s, 27));
    if (s.cr6.gt) {
    CompareFloat(s, Float(s, 2), Float(s, 28));
    if (s.cr6.lt) return false;
    }
    DisableFlush(deps, s);
    Float(s, 7, Load(memory, Address(r[11]) + 8));
    Float(s, 5, Single(Float(s, 29) / Float(s, 2)));
    Float(s, 3, Load(memory, Address(r[31]) + 24));
    Float(s, 7, Single(Float(s, 3) - Float(s, 7)));
    Float(s, 4, Load(memory, Address(r[31]) + 16));
    Float(s, 0, Single(Float(s, 4) - Float(s, 0)));
    Float(s, 6, Load(memory, Address(r[11]) + 4));
    Float(s, 4, Load(memory, Address(r[31]) + 20));
    Float(s, 6, Single(Float(s, 4) - Float(s, 6)));
    Float(s, 11, Single(Float(s, 7) * Float(s, 11)));
    Float(s, 13, Single(Float(s, 0) * Float(s, 13) + Float(s, 11)));
    Float(s, 13, Single(Float(s, 6) * Float(s, 12) + Float(s, 13)));
    Float(s, 4, Single(Float(s, 13) * Float(s, 5)));
    Store(memory, Address(r[31]) + 84, Float(s, 4));
    r[11] = memory.ReadU32(Address(Address(r[31]) + 84));
    r[10] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000ull);
    Compare(s, Address(r[10]), 0);
    if (!s.cr6.eq) return false;
    Compare(s, Address(r[11]), Address(r[25]));
    if (s.cr6.gt) return false;
    Float(s, 13, Single(Float(s, 9) * Float(s, 0)));
    Float(s, 12, Single(Float(s, 6) * Float(s, 8)));
    Float(s, 11, Single(Float(s, 7) * Float(s, 10)));
    Float(s, 13, Single(Float(s, 7) * Float(s, 8) - Float(s, 13)));
    Float(s, 0, Single(Float(s, 10) * Float(s, 0) - Float(s, 12)));
    Float(s, 10, Load(memory, Address(r[31]) + 36));
    Float(s, 12, Single(Float(s, 6) * Float(s, 9) - Float(s, 11)));
    Float(s, 11, Load(memory, Address(r[31]) + 32));
    Float(s, 9, Load(memory, Address(r[31]) + 28));
    Float(s, 11, Single(Float(s, 11) * Float(s, 13)));
    Float(s, 11, Single(Float(s, 10) * Float(s, 0) + Float(s, 11)));
    Float(s, 11, Single(Float(s, 9) * Float(s, 12) + Float(s, 11)));
    Float(s, 11, Single(Float(s, 11) * Float(s, 5)));
    Store(memory, Address(r[31]) + 88, Float(s, 11));
    r[11] = memory.ReadU32(Address(Address(r[31]) + 88));
    r[11] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000ull);
    Compare(s, Address(r[11]), 0);
    if (!s.cr6.eq) return false;
    Float(s, 11, Single(Float(s, 4) + Float(s, 11)));
    CompareFloat(s, Float(s, 11), Float(s, 29));
    if (s.cr6.gt) return false;
    Float(s, 0, Single(Float(s, 0) * Float(s, 30)));
    Float(s, 0, Single(Float(s, 12) * Float(s, 1) + Float(s, 0)));
    Float(s, 0, Single(Float(s, 13) * Float(s, 31) + Float(s, 0)));
    Float(s, 0, Single(Float(s, 0) * Float(s, 5)));
    Store(memory, Address(r[31]) + 80, Float(s, 0));
    r[11] = memory.ReadU32(Address(Address(r[31]) + 80));
    r[11] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000ull);
    Compare(s, Address(r[11]), 0);
    if (!s.cr6.eq) return false;
    return true;
}
// Scalar lowers retain their existing API; this adapter keeps guest callbacks
// connected to the entire borrowed vector file throughout the dispatch tree.
struct ScalarServices final:crt_close_recursive_buffer_context::GuestServices{
    geometry_unbounded_range61::GuestServices& guest;VectorState& vectors;
    ScalarServices(geometry_unbounded_range61::GuestServices& g,VectorState& v):guest(g),vectors(v){}
    void CallIndirect(GuestAddress target,GuestMemory& memory,Registers& state)override{guest.CallIndirect(target,memory,state,vectors);}
};
// The brute-force path writes each accepted hit; unlike the closest-range helper
// it does not tighten the query's distance bound after every accepted triangle.
void Record(GuestMemory& memory,ScalarDependencies deps,Registers& s){
    auto& r=s.r;r[11]=memory.ReadU32(Address(r[31]+4u));r[29]=r[31]+76u;
    r[30]=memory.ReadU32(Address(r[31]+92u));r[11]|=4u;Compare(s,r[30],0u);
    memory.WriteU32(Address(r[29]),Address(r[27]));memory.WriteU32(Address(r[31]+4u),Address(r[11]));
    r[11]=memory.ReadU32(Address(r[31]+104u));++r[11];memory.WriteU32(Address(r[31]+104u),Address(r[11]));
    if(s.cr6.eq)return;
    r[11]=memory.ReadU8(Address(r[31]+140u));Compare(s,r[11],0u);
    if(!s.cr6.eq){r[11]=memory.ReadU32(Address(r[30]+4u));r[11]&=0xfffffffcu;Compare(s,r[11],0u);
        if(!s.cr6.eq){r[11]=memory.ReadU32(Address(r[30]+8u));Compare(s,r[11],0u);if(s.cr6.eq)return;
            DisableFlush(deps,s);Float(s,0,Load(memory,r[31]+80u));Float(s,13,Load(memory,r[11]+4u));
            CompareFloat(s,Float(s,0),Float(s,13));if(!s.cr6.lt)return;
            r[10]=Address(r[27]);memory.WriteU32(Address(r[11]),Address(r[10]));
            for(unsigned offset=4;offset<=12;offset+=4){r[10]=memory.ReadU32(Address(r[29]+offset));memory.WriteU32(Address(r[11]+offset),Address(r[10]));}return;
        }
    }
    Compare(s,r[29],0u);if(s.cr6.eq)return;
    r[11]=memory.ReadU32(Address(r[30]+4u));r[10]=memory.ReadU32(Address(r[30]));r[11]+=4u;Compare(s,r[11],r[10]);
    if(s.cr6.gt){r[4]=4u;r[3]=r[30];s.lr=0x82bd765cu;(void)reader_buffer_growth61::Apply(0x82bd2870u,memory,deps,s);}
    r[10]=memory.ReadU32(Address(r[30]+4u));r[5]=16u;r[11]=memory.ReadU32(Address(r[30]+8u));r[4]=r[29];r[10]=Address(r[10])<<2u;r[3]=r[11]+r[10];
    s.lr=0x82bd7678u;(void)crt_copy_full_context::Apply(0x82b7a0b0u,memory,s);
    r[11]=memory.ReadU32(Address(r[30]+4u));r[11]+=4u;memory.WriteU32(Address(r[30]+4u),Address(r[11]));
}
void BruteForce(GuestMemory& memory,ScalarDependencies deps,Registers& s){
    auto& r=s.r;r[11]=memory.ReadU32(Address(r[31]+12u));r[27]=0u;r[26]=memory.ReadU32(Address(r[11]+8u));Compare(s,r[26],0u);if(s.cr6.eq)return;
    r[9]=0xffffffff82030000ull;r[10]=0xffffffff82000000ull;r[11]=0xffffffff820a0000ull;r[28]=0u;r[25]=0x3f800000u;
    DisableFlush(deps,s);Float(s,27,Load(memory,r[9]+23736u));Float(s,29,Load(memory,r[10]+30596u));Float(s,28,Load(memory,r[11]+27532u));
    do{
        r[11]=memory.ReadU32(Address(r[31]+12u));r[9]=memory.ReadU8(Address(r[31]+141u));r[10]=memory.ReadU32(Address(r[31]+100u));Compare(s,r[9],0u);
        r[6]=r[10]+1u;r[9]=memory.ReadU32(Address(r[11]+16u));r[10]=memory.ReadU32(Address(r[11]+20u));r[11]=r[9]+r[28];
        TriangleEdges(memory,deps,s);const bool hit=s.cr6.eq?BothSides(memory,deps,s):Front(memory,s);if(hit)Record(memory,deps,s);
        r[11]=memory.ReadU32(Address(r[31]+4u));r[11]&=5u;s.cr0={0u,std::uint8_t(r[11]>0u),std::uint8_t(r[11]==0u),s.xer_so};
        Compare(s,r[11],5u);if(s.cr6.eq)return;
        ++r[27];r[28]+=12u;Compare(s,r[27],r[26]);
    }while(s.cr6.lt);
}
// Copy the quantization coefficients before loading the live node-array fields.
void Scales(GuestMemory& memory,ScalarDependencies deps,Registers& s){
    DisableFlush(deps,s);
    for(unsigned i=0;i<6u;++i){Float(s,0,Load(memory,s.r[11]+12u+4u*i));Store(memory,s.r[31]+108u+4u*i,Float(s,0));}
}
void TreeRoute(GuestMemory& memory,Dependencies deps,ScalarDependencies scalar,Registers& s,VectorState& vectors){
    auto& r=s.r;r[11]=memory.ReadU32(Address(r[30]+8u));r[3]=r[31];r[10]=~r[11];r[11]=Address(r[11])&1u;
    r[10]=recovery_abi::WordRotateMask(r[10],31,1u);CompareSigned(s,Signed(r[10]),0);
    const bool paired=s.cr6.eq;
    CompareSigned(s,Signed(r[11]),0);const bool quantized=!s.cr6.eq;
    if(quantized){
        r[11]=memory.ReadU32(Address(r[30]+16u));r[10]=0x7f7f0000u;r[10]|=65535u;Scales(memory,scalar,s);
        r[9]=memory.ReadU32(Address(r[31]+132u));r[4]=memory.ReadU32(Address(r[11]+8u));Compare(s,r[9],r[10]);r[10]=memory.ReadU32(Address(r[11]+4u));
        r[11]=Address(r[10])<<(paired?2u:1u);r[11]+=r[10];r[11]=Address(r[11])<<(paired?2u:3u);r[5]=r[11]+r[4];
        if(paired){if(s.cr6.eq){s.lr=0x82bd7758u;(void)geometry_paired_range61::Apply(0x82bd5910u,memory,deps,s,vectors);}
            else{s.lr=0x82bd7740u;(void)geometry_quantized_box61::Apply(0x82bd56d8u,memory,scalar,s);}}
        else{if(s.cr6.eq){s.lr=0x82bd7848u;(void)geometry_quantized_unbounded61::Apply(0x82bd6fd0u,memory,deps,s,vectors);}
            else{s.lr=0x82bd7830u;(void)geometry_quantized_box61::Apply(0x82bd5cb0u,memory,scalar,s);}}
    }else{
        r[11]=0x7f7f0000u;r[10]=memory.ReadU32(Address(r[31]+132u));r[9]=r[11]|65535u;r[11]=memory.ReadU32(Address(r[30]+16u));Compare(s,r[10],r[9]);
        if(paired){r[4]=memory.ReadU32(Address(r[11]+8u));r[11]=memory.ReadU32(Address(r[11]+4u));r[11]=Address(r[11])<<5u;r[5]=r[11]+r[4];
            if(s.cr6.eq){s.lr=0x82bd77b0u;(void)geometry_tree_range61::Apply(0x82bd6c58u,memory,deps,s,vectors);}
            else{s.lr=0x82bd7798u;(void)geometry_box61::Apply(0x82bd5550u,memory,scalar,s);}}
        else{r[10]=memory.ReadU32(Address(r[11]+4u));r[4]=memory.ReadU32(Address(r[11]+8u));r[11]=Address(r[10])<<3u;r[11]+=r[10];r[11]=Address(r[11])<<2u;r[5]=r[11]+r[4];
            if(s.cr6.eq){s.lr=0x82bd78a8u;(void)geometry_unbounded_range61::Apply(0x82bd6e28u,memory,deps,s,vectors);}
            else{s.lr=0x82bd7890u;(void)geometry_box61::Apply(0x82bd5b40u,memory,scalar,s);}}
    }
}
void Dispatch(GuestMemory& memory,Dependencies deps,Registers& s,VectorState& vectors){
    EnterFrame(memory,s);auto& r=s.r;r[31]=r[3];r[30]=r[5];r[5]=r[6];Compare(s,r[30],0u);
    r[11]=memory.ReadU32(Address(r[31]+4u));memory.WriteU32(Address(r[31]+8u),Address(r[30]));r[11]|=16u;memory.WriteU32(Address(r[31]+4u),Address(r[11]));
    if(!s.cr6.eq){r[11]=memory.ReadU32(Address(r[30]+4u));r[10]=std::countl_zero(Address(r[11]));r[10]=recovery_abi::WordRotateMask(r[10],27,1u);
        memory.WriteU32(Address(r[31]+12u),Address(r[11]));r[10]^=1u;CompareSigned(s,Signed(r[10]),0);}
    if(s.cr6.eq){r[3]=0u;LeaveFrame(memory,s);return;}
    ScalarServices services(deps.guest,vectors);ScalarDependencies scalar{services,deps.fp};
    r[6]=r[7];r[3]=r[31];s.lr=0x82bd72ccu;(void)geometry_query_prepare61::Apply(0x82bd5f28u,memory,scalar,s);
    CompareSigned(s,Signed(r[3]),0);
    if(s.cr6.eq){r[11]=memory.ReadU32(Address(r[31]+8u));Compare(s,r[11],0u);bool brute=false;
        if(!s.cr6.eq){r[11]=memory.ReadU32(Address(r[11]+8u));r[11]&=4u;CompareSigned(s,Signed(r[11]),0);brute=!s.cr6.eq;}
        if(brute)BruteForce(memory,scalar,s);else TreeRoute(memory,deps,scalar,s,vectors);
    }
    r[3]=1u;LeaveFrame(memory,s);
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& s,VectorState& vectors){
    if(entry!=0x82bd7258u)return false;
    Dispatch(memory,deps,s,vectors);return true;
}
}
