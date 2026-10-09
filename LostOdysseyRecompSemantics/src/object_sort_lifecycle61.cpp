#include "lo_semantics/object_sort_lifecycle61.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::object_sort_lifecycle61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr std::uint64_t FloatPage = 0xffffffff82000000ull;
constexpr std::uint64_t VtablePage = 0xffffffff820d0000ull;
constexpr unsigned Resource = 108u;

void DisableFlush(Dependencies deps, Registers& state)
{
    if (state.cached_fp_control & 0x8040u) {
        state.cached_fp_control &= ~0x8040u;
        deps.fp.SetHostFpControl(state.cached_fp_control);
    }
}
void LoadFloat(GuestMemory& memory, Dependencies deps, Registers& state,
    unsigned index, GuestAddress address)
{
    DisableFlush(deps, state);
    state.fpr_bits[index] = std::bit_cast<std::uint64_t>(
        double(std::bit_cast<float>(memory.ReadU32(address))));
}
void StoreFloat(GuestMemory& memory, Dependencies deps, Registers& state,
    unsigned index, GuestAddress address)
{
    DisableFlush(deps, state);
    memory.WriteU32(address, std::bit_cast<std::uint32_t>(
        float(std::bit_cast<double>(state.fpr_bits[index]))));
}
void Initialize(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[10] = FloatPage;
    r[8] = FloatPage;
    r[11] = r[3] + 4u;
    LoadFloat(memory, deps, state, 0u, Address(r[10] + 3596u));
    r[10] = VtablePage;
    LoadFloat(memory, deps, state, 13u, Address(r[8] + 3428u));
    r[8] = FloatPage;
    r[9] = r[10] + 24804u;
    r[10] = 0u;
    LoadFloat(memory, deps, state, 12u, Address(r[8] + 3664u));
    memory.WriteU32(Address(r[3]), Address(r[9]));
    for (unsigned offset = 0u; offset < 12u; offset += 4u)
        StoreFloat(memory, deps, state, 0u, Address(r[11] + offset));
    for (unsigned offset = 12u; offset < 24u; offset += 4u)
        StoreFloat(memory, deps, state, 13u, Address(r[11] + offset));
    memory.WriteU32(Address(r[3] + 88u), Address(r[10]));
    StoreFloat(memory, deps, state, 12u, Address(r[3] + 96u));
    memory.WriteU32(Address(r[3] + 92u), Address(r[10]));
    StoreFloat(memory, deps, state, 12u, Address(r[3] + 100u));
    memory.WriteU32(Address(r[3] + 104u), Address(r[10]));
    memory.WriteU32(Address(r[3] + Resource), Address(r[10]));
    memory.WriteU32(Address(r[3] + 112u), Address(r[10]));
}
void Cleanup(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[12] = state.lr;
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    WriteU64(memory, Address(r[1] - 16u), r[31]);
    const auto caller_sp = r[1];
    r[1] -= 96u;
    memory.WriteU32(Address(r[1]), Address(caller_sp));
    r[31] = r[3];
    r[11] = VtablePage;
    r[11] += 24804u;
    r[4] = memory.ReadU32(Address(r[31] + Resource));
    memory.WriteU32(Address(r[31]), Address(r[11]));
    state.cr6 = {0u, std::uint8_t(r[4] != 0u), std::uint8_t(r[4] == 0u), state.xer_so};
    if (!state.cr6.eq) {
        r[11] = 0xffffffff832e0000ull;
        r[3] = memory.ReadU32(Address(r[11] - 2744u));
        r[11] = memory.ReadU32(Address(r[3]));
        r[11] = memory.ReadU32(Address(r[11] + 20u));
        state.ctr = r[11];
        state.lr = 0x82bae1e4u;
        deps.guest.CallIndirect(Address(state.ctr) & ~3u, memory, state);
        r[11] = 0u;
        memory.WriteU32(Address(r[31] + Resource), Address(r[11]));
    }
    r[1] += 96u;
    r[12] = memory.ReadU32(Address(r[1] - 8u));
    state.lr = r[12];
    r[31] = ReadU64(memory, Address(r[1] - 16u));
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state)
{
    switch (entry) {
    case 0x82bb25d0u: Initialize(memory, deps, state); return true;
    case 0x82bae1a0u: Cleanup(memory, deps, state); return true;
    default: return false;
    }
}
}
