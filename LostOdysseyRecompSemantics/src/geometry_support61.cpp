#include "lo_semantics/geometry_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace lo::semantic::gpu::geometry_support61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr std::uint64_t VtablePage = 0xffffffff820d0000ull;
constexpr std::uint64_t FloatPage = 0xffffffff82000000ull;
constexpr unsigned BaseVtableOffset = 28416u, ExtendedVtableOffset = 27792u;
void BaseInitialize(GuestMemory& memory, Registers& s)
{
    auto& r = s.r;
    r[11] = VtablePage; r[10] = r[11] + BaseVtableOffset; r[11] = 0u;
    memory.WriteU32(Address(r[3]), Address(r[10]));
    memory.WriteU32(Address(r[3] + 4u), Address(r[11]));
    memory.WriteU32(Address(r[3] + 8u), Address(r[11]));
    memory.WriteU32(Address(r[3] + 12u), Address(r[11]));
}
void BaseTeardown(GuestMemory& memory, Registers& s)
{
    s.r[11] = VtablePage; s.r[11] += BaseVtableOffset;
    memory.WriteU32(Address(s.r[3]), Address(s.r[11]));
}
void EnterFrame(GuestMemory& memory, Registers& s)
{
    auto& r = s.r;
    r[12] = s.lr;
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    WriteU64(memory, Address(r[1] - 16u), r[31]);
    const auto caller_sp = r[1]; r[1] -= 96u;
    memory.WriteU32(Address(r[1]), Address(caller_sp)); r[31] = r[3];
}
void LeaveFrame(GuestMemory& memory, Registers& s)
{
    s.r[1] += 96u;
    s.r[12] = memory.ReadU32(Address(s.r[1] - 8u)); s.lr = s.r[12];
    s.r[31] = ReadU64(memory, Address(s.r[1] - 16u));
}
void DisableFlush(float_triplet_transfer::NativeServices& fp, Registers& s)
{
    if (s.cached_fp_control & 0x8040u) {
        s.cached_fp_control &= ~0x8040u;
        fp.SetHostFpControl(s.cached_fp_control);
    }
}
void Initialize(GuestMemory& memory, float_triplet_transfer::NativeServices& fp, Registers& s)
{
    EnterFrame(memory, s);
    s.lr = 0x82bd3d98u; BaseInitialize(memory, s);
    auto& r = s.r;
    r[9] = FloatPage; r[11] = VtablePage; r[3] = r[31];
    r[10] = r[11] + ExtendedVtableOffset; r[11] = 0u;
    DisableFlush(fp, s);
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(
        memory.ReadU32(Address(r[9] + 3596u)))));
    r[9] = FloatPage;
    DisableFlush(fp, s);
    memory.WriteU32(Address(r[31] + 132u), std::bit_cast<std::uint32_t>(
        float(std::bit_cast<double>(s.fpr_bits[0]))));
    memory.WriteU32(Address(r[31]), Address(r[10]));
    memory.WriteU32(Address(r[31] + 92u), Address(r[11]));
    DisableFlush(fp, s);
    s.fpr_bits[13] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(
        memory.ReadU32(Address(r[9] + 3664u)))));
    r[9] = 1u;
    DisableFlush(fp, s);
    memory.WriteU32(Address(r[31] + 136u), std::bit_cast<std::uint32_t>(
        float(std::bit_cast<double>(s.fpr_bits[13]))));
    memory.WriteU32(Address(r[31] + 96u), Address(r[11]));
    memory.WriteU32(Address(r[31] + 100u), Address(r[11]));
    memory.WriteU32(Address(r[31] + 104u), Address(r[11]));
    memory.WriteU8(Address(r[31] + 140u), std::uint8_t(r[11]));
    memory.WriteU8(Address(r[31] + 141u), std::uint8_t(r[9]));
    LeaveFrame(memory, s);
}
}
bool Apply(GuestAddress entry, GuestMemory& memory,
    float_triplet_transfer::NativeServices& fp, Registers& s)
{
    switch (entry) {
    case 0x82bdddf8u: BaseInitialize(memory, s); return true;
    case 0x82bdde18u: BaseTeardown(memory, s); return true;
    case 0x82bd3d80u: Initialize(memory, fp, s); return true;
    case 0x82bd43f8u:
        EnterFrame(memory, s);
        s.lr = 0x82bd4410u; Initialize(memory, fp, s);
        s.r[11] = VtablePage; s.r[3] = s.r[31]; s.r[11] += ExtendedVtableOffset;
        memory.WriteU32(Address(s.r[31]), Address(s.r[11]));
        LeaveFrame(memory, s); return true;
    case 0x82bd4438u:
        s.r[11] = VtablePage; s.r[11] += ExtendedVtableOffset;
        memory.WriteU32(Address(s.r[3]), Address(s.r[11]));
        BaseTeardown(memory, s); return true;
    default: return false;
    }
}
}
