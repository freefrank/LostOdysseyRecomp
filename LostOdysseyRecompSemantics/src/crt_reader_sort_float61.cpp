#include "lo_semantics/crt_reader_sort_float61.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>

namespace lo::semantic::gpu::crt_reader_sort_float61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Threshold = 0x82000e50u;
constexpr unsigned PayloadOffset = 8u;
constexpr unsigned LevelOffset = 12u;

// Guest storage stays borrowed. Only the table callback owns payload disposal;
// the reset itself neither allocates nor frees host memory.
void Reset(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[12] = state.lr;
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    WriteU64(memory, Address(r[1] - 24u), r[30]);
    WriteU64(memory, Address(r[1] - 16u), r[31]);
    const auto caller_sp = r[1];
    r[1] -= 112u;
    memory.WriteU32(Address(r[1]), Address(caller_sp));
    r[31] = r[3];
    r[11] = 0xffffffff82000000ull;
    r[30] = 0u;

    if (state.cached_fp_control & 0x8040u) {
        state.cached_fp_control &= ~0x8040u;
        deps.fp.SetHostFpControl(state.cached_fp_control);
    }
    const double level = std::bit_cast<float>(memory.ReadU32(Address(r[31] + LevelOffset)));
    const double threshold = std::bit_cast<float>(memory.ReadU32(Threshold));
    state.fpr_bits[13] = std::bit_cast<std::uint64_t>(level);
    state.fpr_bits[0] = std::bit_cast<std::uint64_t>(threshold);
    const bool unordered = std::isnan(level) || std::isnan(threshold);
    state.cr6 = {std::uint8_t(!unordered && level < threshold),
        std::uint8_t(!unordered && level > threshold),
        std::uint8_t(!unordered && level == threshold), std::uint8_t(unordered)};

    if (!state.cr6.lt) {
        r[11] = memory.ReadU32(Address(r[31] + PayloadOffset));
        state.cr6 = {0u, std::uint8_t(r[11] != 0u),
            std::uint8_t(r[11] == 0u), state.xer_so};
        if (!state.cr6.eq) {
            state.lr = 0x82bd2a68u;
            (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, memory, deps.guest, state);
            r[11] = memory.ReadU32(Address(r[3]));
            r[4] = memory.ReadU32(Address(r[31] + PayloadOffset));
            r[11] = memory.ReadU32(Address(r[11] + 12u));
            state.ctr = r[11];
            state.lr = 0x82bd2a7cu;
            deps.guest.CallIndirect(Address(state.ctr) & ~3u, memory, state);
            // Reload live registers after the mutable guest call, as the PPC
            // does. Do not cache the reader address or assume r30 stays zero.
            memory.WriteU32(Address(r[31] + PayloadOffset), Address(r[30]));
        }
    }
    r[3] = r[31];
    memory.WriteU32(Address(r[31]), Address(r[30]));
    memory.WriteU32(Address(r[31] + 4u), Address(r[30]));
    r[1] += 112u;
    r[12] = memory.ReadU32(Address(r[1] - 8u));
    state.lr = r[12];
    r[30] = ReadU64(memory, Address(r[1] - 24u));
    r[31] = ReadU64(memory, Address(r[1] - 16u));
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state)
{
    if (entry != 0x82bd2a28u) return false;
    Reset(memory, deps, state);
    return true;
}
}
