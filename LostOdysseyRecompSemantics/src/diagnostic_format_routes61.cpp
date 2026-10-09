#include "lo_semantics/diagnostic_format_routes61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace lo::semantic::gpu::diagnostic_format_routes61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

void Compare(Registers& state, std::uint64_t left, std::int32_t right)
{
    const auto value = std::bit_cast<std::int32_t>(Address(left));
    state.cr6 = {std::uint8_t(value < right), std::uint8_t(value > right),
        std::uint8_t(value == right), state.xer_so};
}
void CompareZero(Registers& state, std::uint64_t value)
{
    state.cr6 = {0u, std::uint8_t(Address(value) != 0u),
        std::uint8_t(Address(value) == 0u), state.xer_so};
}
void Push(GuestMemory& memory, Registers& state, unsigned size)
{
    const auto caller_sp = state.r[1];
    state.r[1] -= size;
    memory.WriteU32(Address(state.r[1]), Address(caller_sp));
}
void Indirect(GuestAddress continuation, GuestMemory& memory,
    Dependencies deps, Registers& state)
{
    state.ctr = state.r[11]; state.lr = continuation;
    deps.formatter.guest.CallIndirect(Address(state.ctr) & ~3u, memory, state);
}
void Unlock(GuestAddress continuation, GuestMemory& memory, Dependencies deps, Registers& state)
{
    state.r[3] = state.r[21]; state.lr = continuation;
    (void)diagnostic_lock61::Apply(0x822b3438u, memory, deps.synchronization, state, deps.machine);
}
void Format(GuestAddress continuation, GuestMemory& memory, Dependencies deps, Registers& state)
{
    state.lr = continuation;
    (void)crt_narrow_formatter61::Apply(0x82b7d260u, memory, deps.formatter, state);
}
void Dispatch(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[12] = state.lr; state.lr = 0x82bc8b80u;
    for (unsigned i = 21u; i < 32u; ++i)
        WriteU64(memory, Address(r[1] - 16u - (31u - i) * 8u), r[i]);
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    Push(memory, state, 352u);
    r[29] = r[3]; r[25] = r[4]; r[23] = r[5]; r[22] = r[6];
    r[31] = r[7]; r[28] = r[8]; r[27] = r[9];
    r[21] = r[3] + 52u; r[3] = r[21];
    state.lr = 0x82bc8bacu;
    (void)diagnostic_lock61::Apply(0x822b29a0u, memory, deps.synchronization, state, deps.machine);
    r[11] = memory.ReadU32(Address(r[29] + 36u));
    memory.WriteU32(Address(r[29] + 40u), Address(r[25]));
    Compare(state, r[11], 0);
    if (state.cr6.eq) memory.WriteU32(Address(r[29] + 36u), Address(r[25]));
    r[3] = memory.ReadU32(Address(r[29] + 28u));
    CompareZero(state, r[3]);
    if (!state.cr6.eq) {
        Compare(state, r[25], 107);
        if (state.cr6.eq) {
            r[11] = memory.ReadU32(Address(r[3]));
            r[6] = r[22]; r[5] = r[23]; r[4] = r[28];
            r[11] = memory.ReadU32(Address(r[11] + 4u));
            Indirect(0x82bc8bf0u, memory, deps, state);
            Compare(state, r[3], 1);
            if (state.cr6.eq) {
                r[11] = 1u;
                memory.WriteU8(Address(r[31]), std::uint8_t(r[11]));
                Unlock(0x82bc8c24u, memory, deps, state);
                r[3] = 0u;
            } else {
                Compare(state, r[3], 2);
                if (state.cr6.eq) {
                    Unlock(0x82bc8c08u, memory, deps, state);
                    r[3] = 1u;
                } else {
                    Unlock(0x82bc8cfcu, memory, deps, state);
                    r[3] = 0u;
                }
            }
        } else {
            r[6] = r[27]; r[5] = r[28]; r[4] = 160u;
            r[3] = r[1] + 80u; r[30] = r[1] + 80u; r[31] = 160u;
            Format(0x82bc8c4cu, memory, deps, state);
            Compare(state, r[3], -1);
            if (state.cr6.eq) {
                r[11] = 0xf0000u; r[26] = 0xffffffff832e0000ull;
                r[24] = r[11] | 16960u;
                for (;;) {
                    Compare(state, r[31], std::bit_cast<std::int32_t>(Address(r[24])));
                    if (!state.cr6.lt) break;
                    r[3] = memory.ReadU32(Address(r[26] - 2744u));
                    r[31] = Address(r[31]) << 1u;
                    r[4] = r[30]; r[5] = r[31] + 1u;
                    r[11] = memory.ReadU32(Address(r[3]));
                    r[11] = memory.ReadU32(Address(r[11] + 16u));
                    Indirect(0x82bc8c88u, memory, deps, state);
                    r[6] = r[27]; r[5] = r[28]; r[4] = r[31]; r[30] = r[3];
                    Format(0x82bc8c9cu, memory, deps, state);
                    Compare(state, r[3], -1);
                    if (!state.cr6.eq) break;
                }
            }
            r[3] = memory.ReadU32(Address(r[29] + 28u));
            r[11] = memory.ReadU32(Address(r[3]));
            Compare(state, r[25], 208);
            if (state.cr6.eq) {
                r[4] = r[30]; r[11] = memory.ReadU32(Address(r[11] + 8u));
                Indirect(0x82bc8cc4u, memory, deps, state);
                Unlock(0x82bc8cccu, memory, deps, state);
            } else {
                r[7] = r[22]; r[6] = r[23]; r[5] = r[30]; r[4] = r[25];
                r[11] = memory.ReadU32(Address(r[11]));
                Indirect(0x82bc8cf4u, memory, deps, state);
                Unlock(0x82bc8cfcu, memory, deps, state);
            }
            r[3] = 0u;
        }
    } else {
        Unlock(0x82bc8cfcu, memory, deps, state);
        r[3] = 0u;
    }
    r[1] += 352u;
    for (unsigned i = 21u; i < 32u; ++i)
        r[i] = ReadU64(memory, Address(r[1] - 16u - (31u - i) * 8u));
    r[12] = memory.ReadU32(Address(r[1] - 8u)); state.lr = r[12];
}

void Variadic(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[12] = state.lr; memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    WriteU64(memory, Address(r[1] + 56u), r[8]);
    WriteU64(memory, Address(r[1] + 64u), r[9]);
    WriteU64(memory, Address(r[1] + 72u), r[10]);
    Push(memory, state, 96u);
    r[11] = r[1] + 80u; r[10] = r[1] + 152u;
    r[8] = r[7]; r[7] = r[6]; r[6] = r[5]; r[5] = r[4]; r[4] = r[3];
    memory.WriteU32(Address(r[11]), Address(r[10]));
    r[11] = 0xffffffff832e0000ull;
    r[9] = memory.ReadU32(Address(r[1] + 80u));
    r[3] = memory.ReadU32(Address(r[11] - 2740u));
    state.lr = 0x82b9c2e0u; Dispatch(memory, deps, state);
    r[3] = 0u; r[1] += 96u;
    r[12] = memory.ReadU32(Address(r[1] - 8u)); state.lr = r[12];
}
void Diagnostic(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[12] = state.lr; memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    Push(memory, state, 96u);
    r[7] = r[3]; r[6] = 0u; r[3] = 2u;
    state.lr = 0x82b9d344u; Variadic(memory, deps, state);
    r[3] = 0u; r[1] += 96u;
    r[12] = memory.ReadU32(Address(r[1] - 8u)); state.lr = r[12];
}
void SetFormat(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[11] = r[3]; CompareZero(state, r[4]);
    if (!state.cr6.eq) {
        CompareZero(state, r[5]);
        if (!state.cr6.eq) {
            r[3] = 1u;
            memory.WriteU32(Address(r[11] + 16u), Address(r[4]));
            memory.WriteU32(Address(r[11] + 20u), Address(r[5]));
            return;
        }
    }
    r[11] = 0xffffffff820d0000ull;
    r[4] = r[11] + 27656u;
    r[5] = 231u;
    r[11] = 0xffffffff820d0000ull;
    r[3] = r[11] + 27612u;
    // Tail-call the diagnostic prefix without introducing an extra LR/frame.
    Diagnostic(memory, deps, state);
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state)
{
    switch (entry) {
    case 0x82bc8b78u: Dispatch(memory, deps, state); return true;
    case 0x82b9c298u: Variadic(memory, deps, state); return true;
    case 0x82b9d328u: Diagnostic(memory, deps, state); return true;
    case 0x82bd18c0u: SetFormat(memory, deps, state); return true;
    default: return false;
    }
}
}
