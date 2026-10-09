#include "lo_semantics/owned_tree_storage61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>

namespace lo::semantic::gpu::owned_tree_storage61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void DisableFlush(Dependencies deps, Registers& state)
{
    if (state.cached_fp_control & 0x8040u) {
        state.cached_fp_control &= ~0x8040u;
        deps.fp.SetHostFpControl(state.cached_fp_control);
    }
}
void CompareWord(Registers& state, std::uint64_t a, std::uint64_t b)
{
    const auto left = Address(a), right = Address(b);
    state.cr6 = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), state.xer_so};
}
void Partition(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[12] = state.lr; state.lr = 0x82bd9860u;
    for (unsigned i = 25u; i <= 31u; ++i)
        WriteU64(memory, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    DisableFlush(deps, state);
    WriteU64(memory, Address(r[1] - 72u), state.fpr_bits[31]);
    const auto caller_sp = r[1]; r[1] -= 160u;
    memory.WriteU32(Address(r[1]), Address(caller_sp));
    r[28] = r[5]; r[31] = r[3]; r[26] = r[4]; r[6] = r[31]; r[7] = r[26];
    r[11] = memory.ReadU32(Address(r[28])); r[3] = r[28];
    r[5] = memory.ReadU32(Address(r[31] + 36u));
    r[4] = memory.ReadU32(Address(r[31] + 32u));
    r[11] = memory.ReadU32(Address(r[11] + 8u));
    state.ctr = r[11]; state.lr = 0x82bd9898u;
    deps.guest.CallIndirect(Address(state.ctr) & ~3u, memory, state);
    r[11] = memory.ReadU32(Address(r[31] + 36u));
    DisableFlush(deps, state);
    state.fpr_bits[31] = state.fpr_bits[1];
    r[27] = 0u; r[25] = 0u; CompareWord(state, r[11], 0u);
    if (state.cr6.gt) {
        r[29] = 0u; r[30] = 0u;
        do {
            r[11] = memory.ReadU32(Address(r[31] + 32u)); r[5] = r[26];
            r[10] = memory.ReadU32(Address(r[28])); r[3] = r[28];
            r[4] = memory.ReadU32(Address(r[11] + r[30]));
            r[11] = memory.ReadU32(Address(r[10] + 12u));
            state.ctr = r[11]; state.lr = 0x82bd98d8u;
            deps.guest.CallIndirect(Address(state.ctr) & ~3u, memory, state);
            DisableFlush(deps, state);
            const auto score = std::bit_cast<double>(state.fpr_bits[1]);
            const auto threshold = std::bit_cast<double>(state.fpr_bits[31]);
            const bool unordered = std::isnan(score) || std::isnan(threshold);
            state.cr6 = {std::uint8_t(!unordered && score < threshold),
                std::uint8_t(!unordered && score > threshold),
                std::uint8_t(!unordered && score == threshold), std::uint8_t(unordered)};
            if (state.cr6.gt) {
                r[11] = memory.ReadU32(Address(r[31] + 32u)); ++r[27];
                r[9] = memory.ReadU32(Address(r[29] + r[11]));
                r[10] = memory.ReadU32(Address(r[30] + r[11]));
                memory.WriteU32(Address(r[30] + r[11]), Address(r[9]));
                r[11] = memory.ReadU32(Address(r[31] + 32u));
                memory.WriteU32(Address(r[11] + r[29]), Address(r[10]));
                r[29] += 4u;
            }
            r[11] = memory.ReadU32(Address(r[31] + 36u)); ++r[25]; r[30] += 4u;
            CompareWord(state, r[25], r[11]);
        } while (state.cr6.lt);
    }
    r[3] = r[27]; r[1] += 160u;
    DisableFlush(deps, state);
    state.fpr_bits[31] = ReadU64(memory, Address(r[1] - 72u));
    for (unsigned i = 25u; i <= 31u; ++i)
        r[i] = ReadU64(memory, Address(r[1] - 16u - 8u * (31u - i)));
    r[12] = memory.ReadU32(Address(r[1] - 8u)); state.lr = r[12];
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state)
{
    if (entry != 0x82bd9858u) return false;
    Partition(memory, deps, state); return true;
}
}
