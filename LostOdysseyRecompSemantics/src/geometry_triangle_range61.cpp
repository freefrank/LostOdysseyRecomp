#include "lo_semantics/geometry_triangle_range61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::geometry_triangle_range61 {
namespace {
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
void CompareFloat(Registers& s, double a, double b) {
    s.cr6 = {std::uint8_t(a<b), std::uint8_t(a>b), std::uint8_t(a==b), std::uint8_t(std::isnan(a)||std::isnan(b))};
}
void DisableFlush(Dependencies deps, Registers& s) {
    if (s.cached_fp_control & 0x8040u) { s.cached_fp_control &= ~0x8040u; deps.fp.SetHostFpControl(s.cached_fp_control); }
}
void EnterFrame(GuestMemory& memory, Registers& s) {
    auto& r=s.r; r[12]=s.lr;
    for (unsigned n=25;n<32;++n) WriteU64(memory,Address(r[1]-16u-8u*(31u-n)),r[n]);
    memory.WriteU32(Address(r[1]-8u),Address(r[12]));
    s.lr=0x82bd4c48u; r[12]=r[1]-64u; s.lr=0x82bd4c50u;
    for (unsigned n=25;n<32;++n) WriteU64(memory,Address(r[12]-8u*(32u-n)),s.fpr_bits[n]);
    const auto caller=r[1]; r[1]-=208u; memory.WriteU32(Address(r[1]),Address(caller));
}
void LeaveFrame(GuestMemory& memory, Registers& s) {
    auto& r=s.r; r[1]+=208u; r[12]=r[1]-64u;
    for (unsigned n=25;n<32;++n) s.fpr_bits[n]=ReadU64(memory,Address(r[12]-8u*(32u-n)));
    for (unsigned n=25;n<32;++n) r[n]=ReadU64(memory,Address(r[1]-16u-8u*(31u-n)));
    r[12]=memory.ReadU32(Address(r[1]-8u)); s.lr=r[12];
}
// Decode the three vertices in original load order. Triangle records are 12
// bytes; r9 is the first vertex, r7 second, r8 third. The determinant and cross
// product stages deliberately retain their scratch FPRs across rejection paths.
void EdgesAndDeterminant(bool indexed, GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r=s.r;
    r[9] = memory.ReadU32(Address(r[11]) + 0);
    r[8] = memory.ReadU32(Address(r[11]) + 8);
    r[7] = memory.ReadU32(Address(r[11]) + 4);
    r[11] = recovery_abi::WordRotateMask(r[9], 1, 0xFFFFFFFEu);
    memory.WriteU32(Address(r[31] + 100u), Address(r[5]));
    DisableFlush(deps, s);
    Float(s, 11, Load(memory, Address(r[31]) + 28));
    r[11] = r[9] + r[11];
    Float(s, 13, Load(memory, Address(r[31]) + 32));
    r[9] = recovery_abi::WordRotateMask(r[8], 1, 0xFFFFFFFEu);
    Float(s, 12, Load(memory, Address(r[31]) + 36));
    r[11] = recovery_abi::WordRotateMask(r[11], 2, 0xFFFFFFFCu);
    r[9] = r[8] + r[9];
    r[11] = r[11] + r[10];
    r[9] = recovery_abi::WordRotateMask(r[9], 2, 0xFFFFFFFCu);
    r[8] = recovery_abi::WordRotateMask(r[7], 1, 0xFFFFFFFEu);
    r[9] = r[9] + r[10];
    r[8] = r[7] + r[8];
    Float(s, 0, Load(memory, Address(r[11]) + 0));
    Float(s, 10, Load(memory, Address(r[11]) + 4));
    r[8] = recovery_abi::WordRotateMask(r[8], 2, 0xFFFFFFFCu);
    Float(s, 9, Load(memory, Address(r[9]) + 0));
    r[10] = r[8] + r[10];
    Float(s, 1, Single(Float(s, 9) - Float(s, 0)));
    Float(s, 6, Load(memory, Address(r[9]) + 8));
    Float(s, 9, Load(memory, Address(r[11]) + 8));
    Float(s, 30, Single(Float(s, 6) - Float(s, 9)));
    Float(s, 7, Load(memory, Address(r[9]) + 4));
    Float(s, 31, Single(Float(s, 7) - Float(s, 10)));
    Float(s, 5, Load(memory, Address(r[10]) + 4));
    Float(s, 10, Single(Float(s, 5) - Float(s, 10)));
    Float(s, 4, Load(memory, Address(r[10]) + 8));
    Float(s, 9, Single(Float(s, 4) - Float(s, 9)));
    Float(s, 8, Load(memory, Address(r[10]) + 0));
    Float(s, 8, Single(Float(s, 8) - Float(s, 0)));
    // Direction crossed with the third-minus-first edge, then dotted with
    // the second-minus-first edge. The culling CR6 remains live until return.
    Float(s, 7, Single(Float(s, 13) * Float(s, 1)));
    Float(s, 6, Single((indexed ? Float(s, 30) * Float(s, 11) : Float(s, 11) * Float(s, 30))));
    Float(s, 5, Single((indexed ? Float(s, 31) * Float(s, 12) : Float(s, 12) * Float(s, 31))));
    Float(s, 11, Single((indexed ? Float(s, 31) * Float(s, 11) : Float(s, 11) * Float(s, 31)) - Float(s, 7)));
    Float(s, 12, Single(Float(s, 12) * Float(s, 1) - Float(s, 6)));
    Float(s, 13, Single((indexed ? Float(s, 30) * Float(s, 13) : Float(s, 13) * Float(s, 30)) - Float(s, 5)));
    Float(s, 7, Single(Float(s, 12) * Float(s, 10)));
    Float(s, 7, Single(Float(s, 11) * Float(s, 9) + Float(s, 7)));
    Float(s, 2, Single(Float(s, 13) * Float(s, 8) + Float(s, 7)));
}
// Cull backfaces and test unnormalized barycentric coordinates with tolerance.
bool OneSided(bool indexed, GuestMemory& memory, Registers& s) {
    auto& r=s.r;
    CompareFloat(s, Float(s, 2), Float(s, 27));
    if (s.cr6.lt) return false;
    // Origin minus first vertex; test barycentric u before forming v and t.
    Float(s, 3, Load(memory, Address(r[31]) + 20));
    Float(s, 7, Load(memory, Address(r[11]) + 4));
    Float(s, 7, Single(Float(s, 3) - Float(s, 7)));
    Float(s, 6, Load(memory, Address(r[11]) + 8));
    Float(s, 25, Load(memory, Address(r[31]) + 24));
    Float(s, 6, Single(Float(s, 25) - Float(s, 6)));
    Float(s, 3, Load(memory, Address(r[31]) + 16));
    Float(s, 0, Single(Float(s, 3) - Float(s, 0)));
    Float(s, 4, Load(memory, Address(r[31]) + 136));
    s.fpr_bits[5] = s.fpr_bits[4] ^ 0x8000000000000000;
    Float(s, 12, Single(Float(s, 7) * Float(s, 12)));
    Float(s, 12, Single(Float(s, 6) * Float(s, 11) + Float(s, 12)));
    Float(s, 3, Single(Float(s, 0) * Float(s, 13) + Float(s, 12)));
    Store(memory, Address(r[31]) + 84, Float(s, 3));
    CompareFloat(s, Float(s, 3), Float(s, 5));
    if (s.cr6.lt) return false;
    Float(s, 4, Single(Float(s, 4) + Float(s, 2)));
    CompareFloat(s, Float(s, 3), Float(s, 4));
    if (s.cr6.gt) return false;
    Float(s, 13, Single(Float(s, 9) * Float(s, 0)));
    Float(s, 12, Single(Float(s, 7) * Float(s, 8)));
    Float(s, 11, Single(Float(s, 6) * Float(s, 10)));
    Float(s, 13, Single(Float(s, 6) * Float(s, 8) - Float(s, 13)));
    Float(s, 12, Single(Float(s, 10) * Float(s, 0) - Float(s, 12)));
    Float(s, 0, Load(memory, Address(r[31]) + 32));
    Float(s, 10, Load(memory, Address(r[31]) + 36));
    Float(s, 11, Single(Float(s, 7) * Float(s, 9) - Float(s, 11)));
    Float(s, 9, Load(memory, Address(r[31]) + 28));
    Float(s, 0, Single(Float(s, 0) * Float(s, 13)));
    Float(s, 0, Single(Float(s, 10) * Float(s, 12) + Float(s, 0)));
    Float(s, 0, Single((indexed ? Float(s, 11) * Float(s, 9) : Float(s, 9) * Float(s, 11)) + Float(s, 0)));
    Store(memory, Address(r[31]) + 88, Float(s, 0));
    CompareFloat(s, Float(s, 0), Float(s, 5));
    if (s.cr6.lt) return false;
    Float(s, 0, Single(Float(s, 0) + Float(s, 3)));
    CompareFloat(s, Float(s, 0), Float(s, 4));
    if (s.cr6.gt) return false;
    Float(s, 0, Single(Float(s, 13) * Float(s, 31)));
    Float(s, 0, Single(Float(s, 12) * Float(s, 30) + Float(s, 0)));
    Float(s, 13, Single(Float(s, 11) * Float(s, 1) + Float(s, 0)));
    Store(memory, Address(r[31]) + 80, Float(s, 13));
    r[11] = memory.ReadU32(Address(r[31]) + 80);
    r[11] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000u);
    Compare(s, Address(r[11]), 0);
    if (!s.cr6.eq) return false;
    // Normalize the accepted distance and barycentrics only after all tests.
    Float(s, 0, Single(Float(s, 28) / Float(s, 2)));
    Float(s, 12, Load(memory, Address(r[31]) + 88));
    Float(s, 13, Single(Float(s, 13) * Float(s, 0)));
    Store(memory, Address(r[31]) + 80, Float(s, 13));
    Float(s, 13, Single(Float(s, 3) * Float(s, 0)));
    Store(memory, Address(r[31]) + 84, Float(s, 13));
    Float(s, 0, Single((indexed ? Float(s, 12) * Float(s, 0) : Float(s, 0) * Float(s, 12))));
    Store(memory, Address(r[31]) + 88, Float(s, 0));
    return true;
}
// Two-sided mode normalizes before testing barycentric coordinates.
bool TwoSided(GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r=s.r;
    DisableFlush(deps, s);
    CompareFloat(s, Float(s, 2), Float(s, 26));
    if (s.cr6.gt) {
        CompareFloat(s, Float(s, 2), Float(s, 27));
        if (s.cr6.lt) return false;
    }
    DisableFlush(deps, s);
    Float(s, 7, Load(memory, Address(r[11]) + 4));
    Float(s, 5, Single(Float(s, 28) / Float(s, 2)));
    Float(s, 4, Load(memory, Address(r[31]) + 20));
    Float(s, 7, Single(Float(s, 4) - Float(s, 7)));
    Float(s, 6, Load(memory, Address(r[11]) + 8));
    Float(s, 3, Load(memory, Address(r[31]) + 24));
    Float(s, 6, Single(Float(s, 3) - Float(s, 6)));
    Float(s, 4, Load(memory, Address(r[31]) + 16));
    Float(s, 0, Single(Float(s, 4) - Float(s, 0)));
    Float(s, 12, Single(Float(s, 7) * Float(s, 12)));
    Float(s, 12, Single(Float(s, 6) * Float(s, 11) + Float(s, 12)));
    Float(s, 13, Single(Float(s, 0) * Float(s, 13) + Float(s, 12)));
    Float(s, 4, Single(Float(s, 13) * Float(s, 5)));
    Store(memory, Address(r[31]) + 84, Float(s, 4));
    r[11] = memory.ReadU32(Address(r[31]) + 84);
    r[10] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000u);
    Compare(s, Address(r[10]), 0);
    if (!s.cr6.eq) return false;
    Compare(s, Address(r[11]), Address(r[25]));
    if (s.cr6.gt) return false;
    Float(s, 13, Single(Float(s, 9) * Float(s, 0)));
    Float(s, 12, Single(Float(s, 7) * Float(s, 8)));
    Float(s, 11, Single(Float(s, 6) * Float(s, 10)));
    Float(s, 13, Single(Float(s, 6) * Float(s, 8) - Float(s, 13)));
    Float(s, 0, Single(Float(s, 10) * Float(s, 0) - Float(s, 12)));
    Float(s, 10, Load(memory, Address(r[31]) + 36));
    Float(s, 12, Single(Float(s, 7) * Float(s, 9) - Float(s, 11)));
    Float(s, 11, Load(memory, Address(r[31]) + 32));
    Float(s, 9, Load(memory, Address(r[31]) + 28));
    Float(s, 11, Single(Float(s, 11) * Float(s, 13)));
    Float(s, 11, Single(Float(s, 10) * Float(s, 0) + Float(s, 11)));
    Float(s, 11, Single(Float(s, 12) * Float(s, 9) + Float(s, 11)));
    Float(s, 11, Single(Float(s, 11) * Float(s, 5)));
    Store(memory, Address(r[31]) + 88, Float(s, 11));
    r[11] = memory.ReadU32(Address(r[31]) + 88);
    r[11] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000u);
    Compare(s, Address(r[11]), 0);
    if (!s.cr6.eq) return false;
    Float(s, 11, Single(Float(s, 11) + Float(s, 4)));
    CompareFloat(s, Float(s, 11), Float(s, 28));
    if (s.cr6.gt) return false;
    Float(s, 13, Single(Float(s, 13) * Float(s, 31)));
    Float(s, 0, Single(Float(s, 0) * Float(s, 30) + Float(s, 13)));
    Float(s, 0, Single(Float(s, 12) * Float(s, 1) + Float(s, 0)));
    Float(s, 0, Single(Float(s, 0) * Float(s, 5)));
    Store(memory, Address(r[31]) + 80, Float(s, 0));
    r[11] = memory.ReadU32(Address(r[31]) + 80);
    r[11] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000u);
    Compare(s, Address(r[11]), 0);
    if (!s.cr6.eq) return false;
    return true;
}
// All hit records are four words. Capacity/count use words, not bytes. Every
// callback is followed by fresh descriptor reads; mutable service state is live.
void RecordHit(bool indexed, GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r=s.r; const unsigned hit=indexed?28u:26u;
    r[11]=memory.ReadU32(Address(r[31]+104u)); r[hit]=r[31]+76u;
    r[10]=memory.ReadU32(Address(r[31]+4u)); ++r[11];
    r[30]=memory.ReadU32(Address(r[31]+92u)); r[10]|=4u; Compare(s,r[30],0u);
    memory.WriteU32(Address(r[hit]),Address(r[6]));
    memory.WriteU32(Address(r[31]+104u),Address(r[11]));
    memory.WriteU32(Address(r[31]+4u),Address(r[10]));
    if (s.cr6.eq) return;
    r[11]=memory.ReadU8(Address(r[31]+140u)); Compare(s,r[11],0u);
    if (!s.cr6.eq) {
        r[11]=memory.ReadU32(Address(r[30]+4u)); r[11]&=0xfffffffcu; Compare(s,r[11],0u);
        if (!s.cr6.eq) {
            r[11]=memory.ReadU32(Address(r[30]+8u)); Compare(s,r[11],0u);
            if (s.cr6.eq) return;
            DisableFlush(deps,s); Float(s,0,Load(memory,r[31]+80u)); Float(s,13,Load(memory,r[11]+4u));
            CompareFloat(s,Float(s,0),Float(s,13)); if (!s.cr6.lt) return;
            r[10]=Address(r[6]); memory.WriteU32(Address(r[11]),Address(r[10]));
            for (unsigned offset=4;offset<=12;offset+=4) {
                r[10]=memory.ReadU32(Address(r[hit]+offset)); memory.WriteU32(Address(r[11]+offset),Address(r[10]));
            }
            return;
        }
    }
    Compare(s,r[hit],0u); if (s.cr6.eq) return;
    r[11]=memory.ReadU32(Address(r[30]+4u)); r[10]=memory.ReadU32(Address(r[30])); r[11]+=4u;
    Compare(s,r[11],r[10]);
    if (s.cr6.gt) {
        r[4]=4u; r[3]=r[30]; s.lr=indexed?0x82bd5010u:0x82bd547cu;
        (void)reader_buffer_growth61::Apply(0x82bd2870u,memory,deps,s);
    }
    r[10]=memory.ReadU32(Address(r[30]+4u)); r[5]=16u;
    r[11]=memory.ReadU32(Address(r[30]+8u)); r[4]=r[hit];
    r[10]=Address(r[10])<<2u; r[3]=r[11]+r[10]; s.lr=indexed?0x82bd502cu:0x82bd5498u;
    (void)crt_copy_full_context::Apply(0x82b7a0b0u,memory,s);
    r[11]=memory.ReadU32(Address(r[30]+4u)); r[11]+=4u;
    memory.WriteU32(Address(r[30]+4u),Address(r[11]));
}
// Tighten the distance limit and ray box after acceptance. Keep the two single
// multiplications separate and retain store/reload ordering for guest aliasing.
void NarrowRay(GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r=s.r;
    DisableFlush(deps, s);
    Float(s, 0, Load(memory, Address(r[31]) + 80));
    Store(memory, Address(r[31]) + 132, Float(s, 0));
    Float(s, 13, Load(memory, Address(r[31]) + 28));
    Float(s, 13, Single(Float(s, 13) * Float(s, 29)));
    Float(s, 12, Load(memory, Address(r[31]) + 32));
    Float(s, 12, Single(Float(s, 12) * Float(s, 29)));
    Float(s, 11, Load(memory, Address(r[31]) + 36));
    Float(s, 11, Single(Float(s, 11) * Float(s, 29)));
    Float(s, 13, Single(Float(s, 13) * Float(s, 0)));
    Store(memory, Address(r[31]) + 52, Float(s, 13));
    Float(s, 12, Single(Float(s, 12) * Float(s, 0)));
    Store(memory, Address(r[31]) + 56, Float(s, 12));
    Float(s, 0, Single(Float(s, 11) * Float(s, 0)));
    Store(memory, Address(r[31]) + 60, Float(s, 0));
    Float(s, 0, Load(memory, Address(r[31]) + 16));
    Float(s, 11, Load(memory, Address(r[31]) + 60));
    Float(s, 0, Single(Float(s, 0) + Float(s, 13)));
    Float(s, 13, Load(memory, Address(r[31]) + 20));
    Float(s, 13, Single(Float(s, 12) + Float(s, 13)));
    Float(s, 12, Load(memory, Address(r[31]) + 24));
    Float(s, 12, Single(Float(s, 11) + Float(s, 12)));
    Store(memory, Address(r[31]) + 64, Float(s, 0));
    Store(memory, Address(r[31]) + 68, Float(s, 13));
    Store(memory, Address(r[31]) + 72, Float(s, 12));
    Float(s, 0, Load(memory, Address(r[31]) + 52));
    Float(s, 13, Load(memory, Address(r[31]) + 56));
    s.fpr_bits[0] = s.fpr_bits[0] & ~0x8000000000000000;
    Float(s, 12, Load(memory, Address(r[31]) + 60));
    s.fpr_bits[13] = s.fpr_bits[13] & ~0x8000000000000000;
    s.fpr_bits[12] = s.fpr_bits[12] & ~0x8000000000000000;
    Store(memory, Address(r[31]) + 40, Float(s, 0));
    Store(memory, Address(r[31]) + 44, Float(s, 13));
    Store(memory, Address(r[31]) + 48, Float(s, 12));
}
void Query(GuestMemory& memory, Dependencies deps, Registers& s) {
    EnterFrame(memory,s); auto& r=s.r; r[31]=r[3]; r[8]=Address(r[4])<<2u;
    r[11]=memory.ReadU32(Address(r[31]+8u)); r[9]=memory.ReadU32(Address(r[11]+24u));
    r[10]=memory.ReadU32(Address(r[11]+32u)); Compare(s,r[10],0u);
    r[11]=memory.ReadU32(Address(r[9]+r[8])); r[9]=Address(r[11])&15u; r[27]=r[9]+1u;
    // Both layouts visit exactly low-nibble+1 triangles. Only their cursor
    // registers differ; saved registers are never cached across service calls.
    const bool indexed=!s.cr6.eq;
    if (indexed) { r[11]=(Address(r[11])>>2u)&0x3ffffffcu; Compare(s,r[27],0u); r[29]=r[11]+r[10]; }
    else { r[29]=Address(r[11])>>4u; Compare(s,r[27],0u); }
    if (s.cr6.eq) { LeaveFrame(memory,s); return; }
    if (!indexed) { r[11]=Address(r[29])<<1u; r[8]=0xffffffff82020000ull; r[11]=r[29]+r[11]; r[9]=0xffffffff82030000ull; r[28]=Address(r[11])<<2u; }
    else { r[8]=0xffffffff82020000ull; r[9]=0xffffffff82030000ull; }
    r[10]=0xffffffff82000000ull; r[11]=0xffffffff820a0000ull;
    if (indexed) r[25]=0x3f800000u;
    DisableFlush(deps,s); Float(s,29,Load(memory,r[8]-1552u));
    if (!indexed) r[25]=0x3f800000u;
    Float(s,26,Load(memory,r[9]+23736u)); Float(s,28,Load(memory,r[10]+30596u)); Float(s,27,Load(memory,r[11]+27532u));
    do {
        r[11]=memory.ReadU32(Address(r[31]+12u));
        if (indexed) {
            --r[27]; r[10]=memory.ReadU32(Address(r[31]+100u)); r[9]=memory.ReadU8(Address(r[31]+141u));
            r[6]=memory.ReadU32(Address(r[29])); r[5]=r[10]+1u; Compare(s,r[9],0u);
            r[10]=memory.ReadU32(Address(r[11]+20u)); r[29]+=4u; r[9]=memory.ReadU32(Address(r[11]+16u));
            r[11]=Address(r[6])<<1u; r[11]=r[6]+r[11]; r[11]=Address(r[11])<<2u; r[11]=r[9]+r[11];
        } else {
            r[9]=r[28]; r[8]=memory.ReadU8(Address(r[31]+141u)); r[6]=r[29];
            r[10]=memory.ReadU32(Address(r[31]+100u)); --r[27]; Compare(s,r[8],0u); r[5]=r[10]+1u;
            r[8]=memory.ReadU32(Address(r[11]+16u)); ++r[29]; r[10]=memory.ReadU32(Address(r[11]+20u));
            r[28]+=12u; r[11]=r[8]+r[9];
        }
        EdgesAndDeterminant(indexed,memory,deps,s);
        const bool intersects=s.cr6.eq?TwoSided(memory,deps,s):OneSided(indexed,memory,s);
        if (intersects) {
            r[11]=memory.ReadU32(Address(r[31]+80u)); r[10]=memory.ReadU32(Address(r[31]+132u)); Compare(s,r[11],r[10]);
            if (s.cr6.lt) { RecordHit(indexed,memory,deps,s); NarrowRay(memory,deps,s); }
        }
        r[11]=memory.ReadU32(Address(r[31]+4u)); r[11]&=5u;
        s.cr0={0u,std::uint8_t(r[11]>0u),std::uint8_t(r[11]==0u),s.xer_so};
        Compare(s,r[11],5u); if (s.cr6.eq) break;
        Compare(s,r[27],0u);
    } while (!s.cr6.eq);
    LeaveFrame(memory,s);
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& s) {
    if (entry!=0x82bd4c40u) return false;
    Query(memory,deps,s); return true;
}
}
