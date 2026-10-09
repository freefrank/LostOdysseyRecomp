#include "lo_semantics/geometry_query_prepare61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::geometry_query_prepare61 {
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
std::int32_t Signed(std::uint64_t value) { return std::bit_cast<std::int32_t>(Address(value)); }
void CompareSigned(Registers& s, std::int32_t a, std::int32_t b) {
    s.cr6={std::uint8_t(a<b),std::uint8_t(a>b),std::uint8_t(a==b),s.xer_so};
}
void CompareFloat(Registers& s, double a, double b) {
    s.cr6 = {std::uint8_t(a<b), std::uint8_t(a>b), std::uint8_t(a==b), std::uint8_t(std::isnan(a)||std::isnan(b))};
}
void DisableFlush(Dependencies deps, Registers& s) {
    if (s.cached_fp_control & 0x8040u) { s.cached_fp_control &= ~0x8040u; deps.fp.SetHostFpControl(s.cached_fp_control); }
}
void EnterFrame(GuestMemory& memory, Registers& s) {
    auto& r=s.r; r[12]=s.lr;
    for (unsigned n=29;n<32;++n) WriteU64(memory,Address(r[1]-16u-8u*(31u-n)),r[n]);
    memory.WriteU32(Address(r[1]-8u),Address(r[12]));
    s.lr=0x82bd5f30u; r[12]=r[1]-32u; s.lr=0x82bd5f38u;
    for (unsigned n=28;n<32;++n) WriteU64(memory,Address(r[12]-8u*(32u-n)),s.fpr_bits[n]);
    const auto caller=r[1]; r[1]-=144u; memory.WriteU32(Address(r[1]),Address(caller));
}
void LeaveFrame(GuestMemory& memory, Registers& s) {
    auto& r=s.r; r[1]+=144u; r[12]=r[1]-32u;
    for (unsigned n=28;n<32;++n) s.fpr_bits[n]=ReadU64(memory,Address(r[12]-8u*(32u-n)));
    for (unsigned n=29;n<32;++n) r[n]=ReadU64(memory,Address(r[1]-16u-8u*(31u-n)));
    r[12]=memory.ReadU32(Address(r[1]-8u)); s.lr=r[12];
}
// Transform origin and direction independently; translation applies only to origin.
void TransformRay(GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r=s.r;
    DisableFlush(deps, s);
    Float(s, 0, Load(memory, Address(r[4]) + 16));
    Float(s, 10, Load(memory, Address(r[5]) + 4));
    Float(s, 8, Load(memory, Address(r[5]) + 20));
    Float(s, 10, Single(Float(s, 10) * Float(s, 0)));
    Float(s, 6, Load(memory, Address(r[5]) + 36));
    Float(s, 8, Single(Float(s, 8) * Float(s, 0)));
    Float(s, 0, Single(Float(s, 6) * Float(s, 0)));
    Float(s, 13, Load(memory, Address(r[4]) + 12));
    Float(s, 11, Load(memory, Address(r[5]) + 0));
    Float(s, 9, Load(memory, Address(r[5]) + 16));
    Float(s, 7, Load(memory, Address(r[5]) + 32));
    Float(s, 12, Load(memory, Address(r[4]) + 20));
    Float(s, 5, Load(memory, Address(r[5]) + 8));
    Float(s, 4, Load(memory, Address(r[5]) + 24));
    Float(s, 3, Load(memory, Address(r[5]) + 40));
    Float(s, 11, Single(Float(s, 13) * Float(s, 11) + Float(s, 10)));
    Float(s, 10, Single(Float(s, 9) * Float(s, 13) + Float(s, 8)));
    Float(s, 9, Single(Float(s, 7) * Float(s, 13) + Float(s, 0)));
    Float(s, 0, Single(Float(s, 5) * Float(s, 12) + Float(s, 11)));
    Store(memory, Address(r[31]) + 28, Float(s, 0));
    Float(s, 13, Single(Float(s, 4) * Float(s, 12) + Float(s, 10)));
    Store(memory, Address(r[31]) + 32, Float(s, 13));
    Float(s, 12, Single(Float(s, 3) * Float(s, 12) + Float(s, 9)));
    Store(memory, Address(r[31]) + 36, Float(s, 12));
    Float(s, 13, Load(memory, Address(r[5]) + 52));
    Float(s, 8, Load(memory, Address(r[5]) + 4));
    Float(s, 6, Load(memory, Address(r[5]) + 20));
    Float(s, 11, Single(Float(s, 13) * Float(s, 8)));
    Float(s, 10, Single(Float(s, 13) * Float(s, 6)));
    Float(s, 0, Load(memory, Address(r[5]) + 56));
    Float(s, 7, Load(memory, Address(r[5]) + 8));
    Float(s, 5, Load(memory, Address(r[5]) + 24));
    Float(s, 12, Load(memory, Address(r[5]) + 48));
    Float(s, 4, Load(memory, Address(r[5]) + 0));
    Float(s, 3, Load(memory, Address(r[5]) + 16));
    Float(s, 1, Load(memory, Address(r[5]) + 36));
    Float(s, 13, Single(Float(s, 13) * Float(s, 1)));
    Float(s, 9, Load(memory, Address(r[4]) + 0));
    Float(s, 2, Load(memory, Address(r[5]) + 32));
    Float(s, 11, Single(Float(s, 0) * Float(s, 7) + Float(s, 11)));
    Float(s, 31, Load(memory, Address(r[5]) + 40));
    Float(s, 10, Single(Float(s, 0) * Float(s, 5) + Float(s, 10)));
    Float(s, 0, Single(Float(s, 0) * Float(s, 31) + Float(s, 13)));
    Float(s, 30, Single(-(Float(s, 12) * Float(s, 4) + Float(s, 11))));
    Float(s, 11, Load(memory, Address(r[4]) + 8));
    Float(s, 29, Single(-(Float(s, 12) * Float(s, 3) + Float(s, 10))));
    Float(s, 10, Load(memory, Address(r[4]) + 4));
    Float(s, 7, Single(Float(s, 7) * Float(s, 11)));
    Float(s, 6, Single(Float(s, 6) * Float(s, 10)));
    Float(s, 28, Single(Float(s, 1) * Float(s, 10)));
    Float(s, 0, Single(-(Float(s, 12) * Float(s, 2) + Float(s, 0))));
    Float(s, 7, Single(Float(s, 9) * Float(s, 4) + Float(s, 7)));
    Float(s, 6, Single(Float(s, 3) * Float(s, 9) + Float(s, 6)));
    Float(s, 9, Single(Float(s, 2) * Float(s, 9) + Float(s, 28)));
    Float(s, 13, Single(Float(s, 8) * Float(s, 10) + Float(s, 7)));
    Float(s, 10, Single(Float(s, 5) * Float(s, 11) + Float(s, 6)));
    Float(s, 11, Single(Float(s, 31) * Float(s, 11) + Float(s, 9)));
    Float(s, 13, Single(Float(s, 13) + Float(s, 30)));
    Store(memory, Address(r[31]) + 16, Float(s, 13));
    Float(s, 12, Single(Float(s, 10) + Float(s, 29)));
    Store(memory, Address(r[31]) + 20, Float(s, 12));
    Float(s, 0, Single(Float(s, 11) + Float(s, 0)));
    Store(memory,r[31]+24u,Float(s,0));
}
// Test-count storage precedes vertex loads. The two fast paths use different
// volatile registers and floating operation orders; preserve those differences.
void TriangleEdges(bool cached, GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r=s.r;
    r[9] = memory.ReadU32(Address(Address(r[11]) + 0));
    r[8] = memory.ReadU32(Address(Address(r[11]) + 8));
    r[7] = memory.ReadU32(Address(Address(r[11]) + 4));
    r[11] = recovery_abi::WordRotateMask(r[9], 1, 0xFFFFFFFEull);
    memory.WriteU32(Address(Address(r[31]) + 100), Address(r[cached?4u:6u]));
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
    Float(s, 7, Single((cached ? Float(s, 1) * Float(s, 13) : Float(s, 13) * Float(s, 1))));
    Float(s, 6, Single((cached ? Float(s, 31) * Float(s, 12) : Float(s, 12) * Float(s, 31))));
    Float(s, 5, Single((cached ? Float(s, 30) * Float(s, 11) : Float(s, 11) * Float(s, 30))));
    Float(s, 11, Single((cached ? Float(s, 31) * Float(s, 11) : Float(s, 11) * Float(s, 31)) - Float(s, 7)));
    Float(s, 13, Single((cached ? Float(s, 30) * Float(s, 13) : Float(s, 13) * Float(s, 30)) - Float(s, 6)));
    Float(s, 12, Single((cached ? Float(s, 1) * Float(s, 12) : Float(s, 12) * Float(s, 1)) - Float(s, 5)));
    Float(s, 7, Single(Float(s, 11) * Float(s, 9)));
    Float(s, 7, Single(Float(s, 13) * Float(s, 8) + Float(s, 7)));
    Float(s, 2, Single(Float(s, 12) * Float(s, 10) + Float(s, 7)));
}
// Single triangle: backface rejection then tolerance-space barycentrics.
bool SingleFront(GuestMemory& memory, Registers& s) {
    auto& r=s.r;
    r[10] = -2113273856;
    Float(s, 7, Load(memory, Address(r[10]) + 27532));
    CompareFloat(s, Float(s, 2), Float(s, 7));
    if (s.cr6.lt) return false;
    Float(s, 5, Load(memory, Address(r[31]) + 20));
    Float(s, 7, Load(memory, Address(r[11]) + 4));
    Float(s, 7, Single(Float(s, 5) - Float(s, 7)));
    Float(s, 3, Load(memory, Address(r[31]) + 24));
    Float(s, 6, Load(memory, Address(r[11]) + 8));
    Float(s, 6, Single(Float(s, 3) - Float(s, 6)));
    Float(s, 5, Load(memory, Address(r[31]) + 16));
    Float(s, 0, Single(Float(s, 5) - Float(s, 0)));
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
    Float(s, 0, Single(Float(s, 9) * Float(s, 11) + Float(s, 0)));
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
    r[11] = memory.ReadU32(Address(Address(r[31]) + 80));
    r[11] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000ull);
    Compare(s, Address(r[11]), 0);
    if (!s.cr6.eq) return false;
    r[11] = -2113929216;
    Float(s, 12, Load(memory, Address(r[31]) + 88));
    Float(s, 0, Load(memory, Address(r[11]) + 30596));
    Float(s, 0, Single(Float(s, 0) / Float(s, 2)));
    Float(s, 13, Single(Float(s, 13) * Float(s, 0)));
    Store(memory, Address(r[31]) + 80, Float(s, 13));
    Float(s, 13, Single(Float(s, 3) * Float(s, 0)));
    Store(memory, Address(r[31]) + 84, Float(s, 13));
    Float(s, 0, Single(Float(s, 0) * Float(s, 12)));
    Store(memory, Address(r[31]) + 88, Float(s, 0));
    return true;
}
// Single triangle: two-sided normalized barycentrics and nonnegative distance.
bool SingleBoth(GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r=s.r;
    r[10] = -2113732608;
    DisableFlush(deps, s);
    Float(s, 7, Load(memory, Address(r[10]) + 23736));
    CompareFloat(s, Float(s, 2), Float(s, 7));
    if (s.cr6.gt) {
    r[10] = -2113273856;
    Float(s, 7, Load(memory, Address(r[10]) + 27532));
    CompareFloat(s, Float(s, 2), Float(s, 7));
    if (s.cr6.lt) return false;
    }
    
    DisableFlush(deps, s);
    Float(s, 7, Load(memory, Address(r[11]) + 4));
    Float(s, 3, Load(memory, Address(r[31]) + 20));
    Float(s, 6, Load(memory, Address(r[11]) + 8));
    Float(s, 7, Single(Float(s, 3) - Float(s, 7)));
    r[11] = -2113929216;
    Float(s, 3, Load(memory, Address(r[31]) + 16));
    Float(s, 0, Single(Float(s, 3) - Float(s, 0)));
    Float(s, 4, Load(memory, Address(r[11]) + 30596));
    Float(s, 5, Single(Float(s, 4) / Float(s, 2)));
    Float(s, 2, Load(memory, Address(r[31]) + 24));
    Float(s, 6, Single(Float(s, 2) - Float(s, 6)));
    Float(s, 12, Single(Float(s, 7) * Float(s, 12)));
    Float(s, 12, Single(Float(s, 6) * Float(s, 11) + Float(s, 12)));
    Float(s, 13, Single(Float(s, 0) * Float(s, 13) + Float(s, 12)));
    Float(s, 3, Single(Float(s, 13) * Float(s, 5)));
    Store(memory, Address(r[31]) + 84, Float(s, 3));
    r[11] = memory.ReadU32(Address(Address(r[31]) + 84));
    r[10] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000ull);
    Compare(s, Address(r[10]), 0);
    if (!s.cr6.eq) return false;
    r[10] = 1065353216;
    Compare(s, Address(r[11]), Address(r[10]));
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
    Float(s, 11, Single(Float(s, 9) * Float(s, 12) + Float(s, 11)));
    Float(s, 11, Single(Float(s, 11) * Float(s, 5)));
    Store(memory, Address(r[31]) + 88, Float(s, 11));
    r[11] = memory.ReadU32(Address(Address(r[31]) + 88));
    r[11] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000ull);
    Compare(s, Address(r[11]), 0);
    if (!s.cr6.eq) return false;
    Float(s, 11, Single(Float(s, 11) + Float(s, 3)));
    CompareFloat(s, Float(s, 11), Float(s, 4));
    if (s.cr6.gt) return false;
    Float(s, 13, Single(Float(s, 13) * Float(s, 31)));
    Float(s, 0, Single(Float(s, 0) * Float(s, 30) + Float(s, 13)));
    Float(s, 0, Single(Float(s, 12) * Float(s, 1) + Float(s, 0)));
    Float(s, 0, Single(Float(s, 0) * Float(s, 5)));
    Store(memory, Address(r[31]) + 80, Float(s, 0));
    r[11] = memory.ReadU32(Address(Address(r[31]) + 80));
    r[11] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000ull);
    Compare(s, Address(r[11]), 0);
    if (!s.cr6.eq) return false;
    return true;
}
// Cached triangle: backface rejection then tolerance-space barycentrics.
bool CachedFront(GuestMemory& memory, Registers& s) {
    auto& r=s.r;
    r[10] = -2113273856;
    Float(s, 7, Load(memory, Address(r[10]) + 27532));
    CompareFloat(s, Float(s, 2), Float(s, 7));
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
    r[11] = -2113929216;
    Float(s, 12, Load(memory, Address(r[31]) + 88));
    Float(s, 0, Load(memory, Address(r[11]) + 30596));
    Float(s, 0, Single(Float(s, 0) / Float(s, 2)));
    Float(s, 13, Single(Float(s, 0) * Float(s, 13)));
    Store(memory, Address(r[31]) + 80, Float(s, 13));
    Float(s, 13, Single(Float(s, 0) * Float(s, 3)));
    Store(memory, Address(r[31]) + 84, Float(s, 13));
    Float(s, 0, Single(Float(s, 0) * Float(s, 12)));
    Store(memory, Address(r[31]) + 88, Float(s, 0));
    return true;
}
// Cached triangle: two-sided normalized barycentrics and nonnegative distance.
bool CachedBoth(GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r=s.r;
    r[10] = -2113732608;
    DisableFlush(deps, s);
    Float(s, 7, Load(memory, Address(r[10]) + 23736));
    CompareFloat(s, Float(s, 2), Float(s, 7));
    if (s.cr6.gt) {
    r[10] = -2113273856;
    Float(s, 7, Load(memory, Address(r[10]) + 27532));
    CompareFloat(s, Float(s, 2), Float(s, 7));
    if (s.cr6.lt) return false;
    }
    
    DisableFlush(deps, s);
    Float(s, 7, Load(memory, Address(r[11]) + 8));
    Float(s, 6, Load(memory, Address(r[11]) + 4));
    r[11] = -2113929216;
    Float(s, 3, Load(memory, Address(r[31]) + 16));
    Float(s, 0, Single(Float(s, 3) - Float(s, 0)));
    Float(s, 3, Load(memory, Address(r[31]) + 20));
    Float(s, 6, Single(Float(s, 3) - Float(s, 6)));
    Float(s, 4, Load(memory, Address(r[11]) + 30596));
    Float(s, 5, Single(Float(s, 4) / Float(s, 2)));
    Float(s, 2, Load(memory, Address(r[31]) + 24));
    Float(s, 7, Single(Float(s, 2) - Float(s, 7)));
    Float(s, 11, Single(Float(s, 7) * Float(s, 11)));
    Float(s, 13, Single(Float(s, 0) * Float(s, 13) + Float(s, 11)));
    Float(s, 13, Single(Float(s, 6) * Float(s, 12) + Float(s, 13)));
    Float(s, 3, Single(Float(s, 13) * Float(s, 5)));
    Store(memory, Address(r[31]) + 84, Float(s, 3));
    r[11] = memory.ReadU32(Address(Address(r[31]) + 84));
    r[10] = recovery_abi::WordRotateMask(r[11], 0, 0x80000000ull);
    Compare(s, Address(r[10]), 0);
    if (!s.cr6.eq) return false;
    r[10] = 1065353216;
    Compare(s, Address(r[11]), Address(r[10]));
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
    Float(s, 11, Single(Float(s, 11) + Float(s, 3)));
    CompareFloat(s, Float(s, 11), Float(s, 4));
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
// A successful cached candidate marks bits2+3; the single-triangle shortcut
// marks bit2. The count and hit record are written before optional output work.
void Record(bool cached, GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r=s.r;
    r[11]=memory.ReadU32(Address(r[31]+80u)); r[10]=memory.ReadU32(Address(r[31]+132u));
    Compare(s,r[11],r[10]); if(!s.cr6.lt)return;
    r[11]=memory.ReadU32(Address(r[31]+104u));
    if(cached) {r[10]=r[5]|12u;r[29]=r[31]+76u;} else {r[29]=r[31]+76u;r[10]=r[5]|4u;}
    r[30]=memory.ReadU32(Address(r[31]+92u)); ++r[11]; Compare(s,r[30],0u);
    if(!cached)memory.WriteU32(Address(r[29]),Address(r[3]));
    memory.WriteU32(Address(r[31]+4u),Address(r[10]));memory.WriteU32(Address(r[31]+104u),Address(r[11]));
    if(cached){r[11]=memory.ReadU32(Address(r[6]));memory.WriteU32(Address(r[29]),Address(r[11]));}
    if(s.cr6.eq)return;
    r[11]=memory.ReadU8(Address(r[31]+140u));Compare(s,r[11],0u);
    if(!s.cr6.eq){
        r[11]=memory.ReadU32(Address(r[30]+4u));r[11]&=0xfffffffcu;Compare(s,r[11],0u);
        if(!s.cr6.eq){
            r[11]=memory.ReadU32(Address(r[30]+8u));Compare(s,r[11],0u);if(s.cr6.eq)return;
            DisableFlush(deps,s);Float(s,0,Load(memory,r[31]+80u));Float(s,13,Load(memory,r[11]+4u));
            CompareFloat(s,Float(s,0),Float(s,13));if(!s.cr6.lt)return;
            if(cached)r[10]=memory.ReadU32(Address(r[29]));else {r[10]=Address(r[3]);r[3]=1u;}
            memory.WriteU32(Address(r[11]),Address(r[10]));
            for(unsigned offset=4;offset<=12;offset+=4){r[10]=memory.ReadU32(Address(r[29]+offset));memory.WriteU32(Address(r[11]+offset),Address(r[10]));}
            return;
        }
    }
    Compare(s,r[29],0u);if(s.cr6.eq)return;
    r[11]=memory.ReadU32(Address(r[30]+4u));r[10]=memory.ReadU32(Address(r[30]));r[11]+=4u;Compare(s,r[11],r[10]);
    if(s.cr6.gt){r[4]=4u;r[3]=r[30];s.lr=cached?0x82bd6840u:0x82bd6454u;(void)reader_buffer_growth61::Apply(0x82bd2870u,memory,deps,s);}
    if(cached){r[10]=memory.ReadU32(Address(r[30]+4u));r[5]=16u;r[11]=memory.ReadU32(Address(r[30]+8u));r[4]=r[29];r[10]=Address(r[10])<<2u;}
    else {r[11]=memory.ReadU32(Address(r[30]+4u));r[5]=16u;r[10]=memory.ReadU32(Address(r[30]+8u));r[4]=r[29];r[11]=Address(r[11])<<2u;}
    r[3]=r[11]+r[10];s.lr=cached?0x82bd685cu:0x82bd6470u;(void)crt_copy_full_context::Apply(0x82b7a0b0u,memory,s);
    r[11]=memory.ReadU32(Address(r[30]+4u));r[11]+=4u;memory.WriteU32(Address(r[30]+4u),Address(r[11]));
}
// Finite length creates the ray segment's center and half extent. The sentinel
// uses absolute direction instead and leaves the center/half-vector slots alone.
void Bounds(GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r=s.r;r[11]=0x7f7f0000u;r[10]=memory.ReadU32(Address(r[31]+132u));r[11]|=65535u;
    Compare(s,r[10],r[11]);
    if(!s.cr6.eq){
    r[11] = -2113798144;
    DisableFlush(deps, s);
    Float(s, 12, Load(memory, Address(r[31]) + 28));
    Float(s, 11, Load(memory, Address(r[31]) + 32));
    Float(s, 10, Load(memory, Address(r[31]) + 36));
    Float(s, 0, Load(memory, Address(r[31]) + 132));
    Float(s, 13, Load(memory, Address(r[11]) + -1552));
    Float(s, 12, Single(Float(s, 12) * Float(s, 13)));
    Float(s, 11, Single(Float(s, 11) * Float(s, 13)));
    Float(s, 13, Single(Float(s, 10) * Float(s, 13)));
    Float(s, 12, Single(Float(s, 12) * Float(s, 0)));
    Store(memory, Address(r[31]) + 52, Float(s, 12));
    Float(s, 11, Single(Float(s, 11) * Float(s, 0)));
    Store(memory, Address(r[31]) + 56, Float(s, 11));
    Float(s, 0, Single(Float(s, 13) * Float(s, 0)));
    Store(memory, Address(r[31]) + 60, Float(s, 0));
    Float(s, 13, Load(memory, Address(r[31]) + 16));
    Float(s, 0, Float(s, 12));
    Float(s, 12, Float(s, 11));
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
    Float(s, 12, Load(memory, Address(r[31]) + 60));
    }else{
    DisableFlush(deps, s);
    Float(s, 0, Load(memory, Address(r[31]) + 28));
    Float(s, 13, Load(memory, Address(r[31]) + 32));
    Float(s, 12, Load(memory, Address(r[31]) + 36));
    }
    DisableFlush(deps, s);
    s.fpr_bits[12] = s.fpr_bits[12] & ~0x8000000000000000;
    r[3] = 0;
    s.fpr_bits[13] = s.fpr_bits[13] & ~0x8000000000000000;
    Store(memory, Address(r[31]) + 48, Float(s, 12));
    s.fpr_bits[0] = s.fpr_bits[0] & ~0x8000000000000000;
    Store(memory, Address(r[31]) + 44, Float(s, 13));
    Store(memory, Address(r[31]) + 40, Float(s, 0));
}
void Prepare(GuestMemory& memory, Dependencies deps, Registers& s) {
    EnterFrame(memory,s);auto& r=s.r;r[31]=r[3];r[3]=0u;
    r[10]=memory.ReadU32(Address(r[31]+4u));r[11]=memory.ReadU32(Address(r[31]+92u));
    r[10]=recovery_abi::WordRotateMask(r[10],0,0xfffffffffffffff3ull);
    memory.WriteU32(Address(r[31]+96u),Address(r[3]));memory.WriteU32(Address(r[31]+100u),Address(r[3]));
    Compare(s,r[11],0u);memory.WriteU32(Address(r[31]+104u),Address(r[3]));memory.WriteU32(Address(r[31]+4u),Address(r[10]));
    if(!s.cr6.eq){r[10]=memory.ReadU32(Address(r[11]+4u));Compare(s,r[10],0u);if(!s.cr6.eq)memory.WriteU32(Address(r[11]+4u),Address(r[3]));}
    Compare(s,r[5],0u);
    if(!s.cr6.eq)TransformRay(memory,deps,s);
    else{
        DisableFlush(deps,s);
        // Interleave loads/stores; the input may alias the query's ray fields.
        for(unsigned axis=0;axis<3u;++axis){Float(s,0,Load(memory,r[4]+12u+4u*axis));Store(memory,r[31]+28u+4u*axis,Float(s,0));}
        for(unsigned axis=0;axis<3u;++axis){Float(s,0,Load(memory,r[4]+4u*axis));Store(memory,r[31]+16u+4u*axis,Float(s,0));}
    }
    r[11]=memory.ReadU32(Address(r[31]+8u));Compare(s,r[11],0u);
    bool single=false;
    if(!s.cr6.eq){r[11]=memory.ReadU32(Address(r[11]+8u));r[11]&=4u;CompareSigned(s,Signed(r[11]),0);
        if(!s.cr6.eq){r[5]=memory.ReadU32(Address(r[31]+4u));r[11]=r[5]&16u;CompareSigned(s,Signed(r[11]),0);single=s.cr6.eq;}}
    if(single){
        r[10]=memory.ReadU32(Address(r[31]+12u));r[11]=memory.ReadU32(Address(r[31]+100u));
        r[9]=memory.ReadU8(Address(r[31]+141u));r[6]=r[11]+1u;Compare(s,r[9],0u);
        r[11]=memory.ReadU32(Address(r[10]+16u));r[10]=memory.ReadU32(Address(r[10]+20u));
        TriangleEdges(false,memory,deps,s);
        const bool hit=s.cr6.eq?SingleBoth(memory,deps,s):SingleFront(memory,s);
        if(hit)Record(false,memory,deps,s);
        r[3]=1u;LeaveFrame(memory,s);return;
    }
    r[5]=memory.ReadU32(Address(r[31]+4u));r[11]=r[5]&2u;CompareSigned(s,Signed(r[11]),0);
    bool candidate=false;
    if(!s.cr6.eq){r[11]=Address(r[5])&1u;CompareSigned(s,Signed(r[11]),0);
        if(!s.cr6.eq){Compare(s,r[6],0u);if(!s.cr6.eq){r[10]=memory.ReadU32(Address(r[6]));CompareSigned(s,Signed(r[10]),-1);candidate=!s.cr6.eq;}}}
    if(candidate){
        r[11]=memory.ReadU32(Address(r[31]+12u));r[9]=Address(r[10])<<1u;r[8]=memory.ReadU32(Address(r[31]+100u));
        r[10]+=r[9];r[7]=memory.ReadU8(Address(r[31]+141u));r[4]=r[8]+1u;r[9]=Address(r[10])<<2u;
        r[8]=memory.ReadU32(Address(r[11]+16u));Compare(s,r[7],0u);r[10]=memory.ReadU32(Address(r[11]+20u));r[11]=r[9]+r[8];
        TriangleEdges(true,memory,deps,s);
        const bool hit=s.cr6.eq?CachedBoth(memory,deps,s):CachedFront(memory,s);
        if(hit)Record(true,memory,deps,s);
        r[11]=memory.ReadU32(Address(r[31]+4u));r[11]&=4u;CompareSigned(s,Signed(r[11]),0);
        if(!s.cr6.eq){r[3]=1u;LeaveFrame(memory,s);return;}
    }
    Bounds(memory,deps,s);LeaveFrame(memory,s);
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& s){
    if(entry!=0x82bd5f28u)return false;
    Prepare(memory,deps,s);return true;
}
}
