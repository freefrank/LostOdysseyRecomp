#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <limits>

namespace lo::semantic::gpu::reader_buffer_growth61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr unsigned Capacity = 0u, Used = 4u, Payload = 8u, Growth = 12u;
constexpr GuestAddress GrowthThreshold = 0x82000e50u;

void CompareWord(Registers& state, std::uint64_t left, std::uint64_t right)
{
    const auto a = Address(left), b = Address(right);
    state.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), state.xer_so};
}

// Match the generated signed truncation, including its saturation boundary.
std::int64_t Truncate(double value)
{
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if (value > static_cast<double>(maximum)) return maximum;
    if (std::isnan(value) || value >= 0x1p63 || value < -0x1p63) return minimum;
    return static_cast<std::int64_t>(value);
}

void Grow(GuestMemory& memory, Dependencies deps, Registers& state)
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
    if (state.cached_fp_control & 0x8040u) {
        state.cached_fp_control &= ~0x8040u;
        deps.fp.SetHostFpControl(state.cached_fp_control);
    }
    const double growth = std::bit_cast<float>(memory.ReadU32(Address(r[31] + Growth)));
    const double threshold = std::bit_cast<float>(memory.ReadU32(GrowthThreshold));
    state.fpr_bits[0] = std::bit_cast<std::uint64_t>(growth);
    state.fpr_bits[13] = std::bit_cast<std::uint64_t>(threshold);
    const bool unordered = std::isnan(growth) || std::isnan(threshold);
    state.cr6 = {std::uint8_t(!unordered && growth < threshold),
        std::uint8_t(!unordered && growth > threshold),
        std::uint8_t(!unordered && growth == threshold), std::uint8_t(unordered)};

    if (!state.cr6.gt) {
        r[3] = 0u;
    } else {
        r[11] = memory.ReadU32(Address(r[31] + Capacity));
        CompareWord(state, r[11], 0u);
        if (state.cr6.eq) {
            r[10] = 2u;
        } else {
            // The original spills the integer conversion in the guest frame;
            // both its bytes and the double/single rounding stages are visible.
            r[10] = r[1] + 80u;
            WriteU64(memory, Address(r[1] + 80u), r[11]);
            const double capacity = static_cast<float>(static_cast<double>(r[11]));
            state.fpr_bits[13] = std::bit_cast<std::uint64_t>(capacity);
            const double grown = static_cast<float>(capacity * growth);
            state.fpr_bits[0] = std::bit_cast<std::uint64_t>(Truncate(grown));
            memory.WriteU32(Address(r[10]), Address(state.fpr_bits[0]));
            r[10] = memory.ReadU32(Address(r[1] + 80u));
        }
        r[11] = memory.ReadU32(Address(r[31] + Used));
        memory.WriteU32(Address(r[31] + Capacity), Address(r[10]));
        r[11] += r[4];
        CompareWord(state, r[10], r[11]);
        if (state.cr6.lt)
            memory.WriteU32(Address(r[31] + Capacity), Address(r[11]));

        state.lr = 0x82bd28fcu;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, memory, deps.guest, state);
        r[11] = memory.ReadU32(Address(r[31] + Capacity));
        r[5] = 64u;
        r[4] = (r[11] << 2u) & 0xfffffffcu;
        r[11] = memory.ReadU32(Address(r[3]));
        r[11] = memory.ReadU32(Address(r[11]));
        state.ctr = r[11];
        state.lr = 0x82bd2918u;
        deps.guest.CallIndirect(Address(state.ctr) & ~3u, memory, state);
        r[30] = r[3];
        CompareWord(state, r[30], 0u);
        if (state.cr6.eq) {
            r[3] = 0u;
        } else {
            r[11] = memory.ReadU32(Address(r[31] + Used));
            CompareWord(state, r[11], 0u);
            if (!state.cr6.eq) {
                r[5] = (r[11] << 2u) & 0xfffffffcu;
                r[4] = memory.ReadU32(Address(r[31] + Payload));
                state.lr = 0x82bd293cu;
                (void)crt_copy_full_context::Apply(0x82b7a0b0u, memory, state);
            }
            r[11] = memory.ReadU32(Address(r[31] + Payload));
            CompareWord(state, r[11], 0u);
            if (!state.cr6.eq) {
                state.lr = 0x82bd294cu;
                (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, memory, deps.guest, state);
                r[11] = memory.ReadU32(Address(r[3]));
                r[4] = memory.ReadU32(Address(r[31] + Payload));
                r[11] = memory.ReadU32(Address(r[11] + 12u));
                state.ctr = r[11];
                state.lr = 0x82bd2960u;
                deps.guest.CallIndirect(Address(state.ctr) & ~3u, memory, state);
                r[11] = 0u;
                memory.WriteU32(Address(r[31] + Payload), Address(r[11]));
            }
            r[3] = 1u;
            memory.WriteU32(Address(r[31] + Payload), Address(r[30]));
        }
    }
    r[1] += 112u;
    r[12] = memory.ReadU32(Address(r[1] - 8u));
    state.lr = r[12];
    r[30] = ReadU64(memory, Address(r[1] - 24u));
    r[31] = ReadU64(memory, Address(r[1] - 16u));
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state)
{
    if (entry != 0x82bd2870u) return false;
    Grow(memory, deps, state);
    return true;
}
}
