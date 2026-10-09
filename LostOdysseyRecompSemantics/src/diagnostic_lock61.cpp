#include "lo_semantics/diagnostic_lock61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace lo::semantic::gpu::diagnostic_lock61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr std::uint32_t SwapWord(std::uint32_t value)
{
    return (value << 24u) | ((value << 8u) & 0x00ff0000u) |
        ((value >> 8u) & 0x0000ff00u) | (value >> 24u);
}
void MergeMsr(MachineState& machine, std::uint64_t source)
{
    machine.msr = (machine.msr & ~0x8020u) | (Address(source) & 0x8020u);
}
void StoreConditional(unsigned value_register, GuestMemory& memory,
    SynchronizationServices& sync, Registers& state, MachineState& machine)
{
    state.cr0.lt = 0u;
    state.cr0.gt = 0u;
    state.cr0.eq = std::uint8_t(sync.CompareExchangeWord(Address(state.r[11]),
        SwapWord(Address(machine.reserved_bits)), Address(state.r[value_register]), memory));
    state.cr0.so = state.xer_so;
}
void ExchangeFlag(GuestMemory& memory, SynchronizationServices& sync,
    Registers& state, MachineState& machine)
{
    auto& r = state.r;
    r[7] = machine.msr;
    for (;;) {
        MergeMsr(machine, r[13]);
        const auto observed = sync.LoadReservedWord(Address(r[11]), memory);
        machine.reserved_bits = (machine.reserved_bits & 0xffffffff00000000ull) | SwapWord(observed);
        r[8] = observed;
        const auto actual = std::bit_cast<std::int32_t>(Address(r[8]));
        const auto expected = std::bit_cast<std::int32_t>(Address(r[10]));
        state.cr6 = {std::uint8_t(actual < expected), std::uint8_t(actual > expected),
            std::uint8_t(actual == expected), state.xer_so};
        if (!state.cr6.eq) {
            StoreConditional(8u, memory, sync, state, machine);
            MergeMsr(machine, r[7]);
            return;
        }
        StoreConditional(9u, memory, sync, state, machine);
        MergeMsr(machine, r[7]);
        if (state.cr0.eq) return;
    }
}
void Enter(GuestMemory& memory, SynchronizationServices& sync,
    Registers& state, MachineState& machine)
{
    auto& r = state.r;
    r[12] = state.lr;
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    WriteU64(memory, Address(r[1] - 16u), r[31]);
    const auto caller_sp = r[1];
    r[1] -= 96u;
    memory.WriteU32(Address(r[1]), Address(caller_sp));
    r[31] = r[3];
    r[3] = memory.ReadU32(Address(r[31]));
    state.lr = 0x822b29bcu;
    sync.EnterCriticalSection(memory, state, machine);
    r[11] = memory.ReadU32(Address(r[31]));
    r[10] = 0u; r[9] = 1u;
    r[11] += 28u;
    ExchangeFlag(memory, sync, state, machine);
    state.lr = 0x822b29fcu;
    // Complete accepted 82290AA8 leaf: current thread identity from live TLS.
    r[11] = memory.ReadU32(Address(r[13] + 256u));
    r[3] = memory.ReadU32(Address(r[11] + 332u));
    r[11] = memory.ReadU32(Address(r[31]));
    r[10] = r[3];
    r[3] = 1u;
    memory.WriteU32(Address(r[11] + 32u), Address(r[10]));
    r[1] += 96u;
    r[12] = memory.ReadU32(Address(r[1] - 8u)); state.lr = r[12];
    r[31] = ReadU64(memory, Address(r[1] - 16u));
}
void Leave(GuestMemory& memory, SynchronizationServices& sync,
    Registers& state, MachineState& machine)
{
    auto& r = state.r;
    r[12] = state.lr;
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    const auto caller_sp = r[1];
    r[1] -= 96u;
    memory.WriteU32(Address(r[1]), Address(caller_sp));
    r[11] = memory.ReadU32(Address(r[3]));
    r[10] = 1u; r[9] = 0u;
    r[11] += 28u;
    ExchangeFlag(memory, sync, state, machine);
    r[3] = memory.ReadU32(Address(r[3]));
    state.lr = 0x822b3488u;
    sync.LeaveCriticalSection(memory, state, machine);
    r[3] = 1u;
    r[1] += 96u;
    r[12] = memory.ReadU32(Address(r[1] - 8u)); state.lr = r[12];
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, SynchronizationServices& sync,
    Registers& state, MachineState& machine)
{
    switch (entry) {
    case 0x822b29a0u: Enter(memory, sync, state, machine); return true;
    case 0x822b3438u: Leave(memory, sync, state, machine); return true;
    default: return false;
    }
}
}
