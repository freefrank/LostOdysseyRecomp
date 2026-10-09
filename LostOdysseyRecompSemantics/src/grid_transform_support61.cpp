#include "lo_semantics/grid_transform_support61.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <limits>

namespace lo::semantic::gpu::grid_transform_support61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
constexpr std::uint64_t FloatPage = 0xffffffff82000000ull;
constexpr std::uint64_t VisitVtable = 0xffffffff820d6db8ull;

void DisableFlush(NativeServices& native, Registers& state)
{
    if (state.cached_fp_control & 0x8040u) {
        state.cached_fp_control &= ~0x8040u;
        native.SetHostFpControl(state.cached_fp_control);
    }
}
double Float(const Registers& state, unsigned index)
{
    return std::bit_cast<double>(state.fpr_bits[index]);
}
void SetFloat(Registers& state, unsigned index, double value)
{
    state.fpr_bits[index] = std::bit_cast<std::uint64_t>(value);
}

void RoundUp(GuestMemory& memory, NativeServices& native, Registers& state)
{
    DisableFlush(native, state);
    const auto input = Float(state, 1u);
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    // Keep the original integer conversion before the large-value selection.
    // Nonfinite/out-of-range host conversion is outside this finite contract.
    const auto integer = input > static_cast<double>(maximum) ? maximum :
        static_cast<std::int64_t>(input);
    state.fpr_bits[0] = std::bit_cast<std::uint64_t>(integer);
    state.r[11] = FloatPage;
    SetFloat(state, 13u, std::fabs(input));
    SetFloat(state, 12u, std::bit_cast<double>(ReadU64(memory, 0x820029c0u)));
    state.r[11] = FloatPage;
    SetFloat(state, 0u, static_cast<double>(integer));
    SetFloat(state, 12u, Float(state, 12u) - Float(state, 13u));
    SetFloat(state, 11u, -Float(state, 13u));
    SetFloat(state, 13u, std::bit_cast<double>(ReadU64(memory, 0x82000f28u)));
    SetFloat(state, 10u, Float(state, 0u) - input);
    SetFloat(state, 13u, Float(state, 0u) + Float(state, 13u));
    SetFloat(state, 0u, Float(state, 10u) >= 0.0 ? Float(state, 0u) : Float(state, 13u));
    SetFloat(state, 0u, Float(state, 12u) >= 0.0 ? Float(state, 0u) : input);
    SetFloat(state, 1u, Float(state, 11u) >= 0.0 ? input : Float(state, 0u));
}

void InitializeFormat(GuestMemory& memory, NativeServices& native, Registers& state)
{
    auto& r = state.r;
    r[11] = FloatPage;
    r[10] = 0x7fff0000u;
    r[8] = ~std::uint64_t{0};
    r[9] = r[10] | 0xffffu;
    DisableFlush(native, state);
    SetFloat(state, 0u, std::bit_cast<float>(memory.ReadU32(0x82000e50u)));
    r[10] = 0u;
    r[11] = 1u;
    r[7] = 34u;
    const auto value = std::bit_cast<std::uint32_t>(static_cast<float>(Float(state, 0u)));
    memory.WriteU32(Address(r[3] + 12u), value);
    memory.WriteU32(Address(r[3] + 20u), value);
    memory.WriteU32(Address(r[3] + 16u), Address(r[8]));
    memory.WriteU32(Address(r[3] + 8u), Address(r[9]));
    memory.WriteU32(Address(r[3] + 4u), Address(r[11]));
    memory.WriteU32(Address(r[3]), Address(r[10]));
    memory.WriteU32(Address(r[3] + 8u), Address(r[7]));
    memory.WriteU32(Address(r[3] + 4u), Address(r[11]));
    memory.WriteU8(Address(r[3] + 24u), std::uint8_t(r[11]));
    memory.WriteU8(Address(r[3] + 25u), std::uint8_t(r[11]));
    memory.WriteU8(Address(r[3] + 26u), std::uint8_t(r[10]));
    memory.WriteU8(Address(r[3] + 27u), std::uint8_t(r[10]));
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, NativeServices& native, Registers& state)
{
    auto& r = state.r;
    switch (entry) {
    case 0x822c5128u: RoundUp(memory, native, state); return true;
    case 0x82f2b308u:
        r[11] = 0u;
        for (unsigned offset = 0u; offset < 24u; offset += 4u)
            memory.WriteU32(Address(r[3] + offset), Address(r[11]));
        return true;
    case 0x82bd1278u: InitializeFormat(memory, native, state); return true;
    case 0x82bd78c0u:
        r[11] = VisitVtable;
        r[10] = ~std::uint64_t{0};
        memory.WriteU32(Address(r[3] + 56u), Address(r[10]));
        memory.WriteU32(Address(r[3]), Address(r[11]));
        return true;
    case 0x82bd78d8u:
        r[11] = VisitVtable;
        memory.WriteU32(Address(r[3]), Address(r[11]));
        return true;
    case 0x82bd78e8u:
        r[10] = r[3];
        state.cr6 = {0u, std::uint8_t(Address(r[5]) != 0u),
            std::uint8_t(Address(r[5]) == 0u), state.xer_so};
        r[3] = 0u;
        if (!state.cr6.eq) {
            r[11] = memory.ReadU32(Address(r[5] + 4u));
            memory.WriteU32(Address(r[10] + 4u), Address(r[11]));
        }
        return true;
    default: return false;
    }
}
}
