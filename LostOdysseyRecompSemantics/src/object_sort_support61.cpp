#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::object_sort_support61 {
namespace {
using recovery_abi::Address;
constexpr GuestAddress InitialLevel = 0x821baa74u;

void InitializeFloatState(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[10] = 0xffffffff821c0000ull;
    r[11] = 0u;
    if (state.cached_fp_control & 0x8040u) {
        state.cached_fp_control &= ~0x8040u;
        deps.fp.SetHostFpControl(state.cached_fp_control);
    }
    const double level = std::bit_cast<float>(memory.ReadU32(InitialLevel));
    state.fpr_bits[0] = std::bit_cast<std::uint64_t>(level);
    memory.WriteU32(Address(r[3] + 12u),
        std::bit_cast<std::uint32_t>(static_cast<float>(level)));
    memory.WriteU32(Address(r[3]), Address(r[11]));
    memory.WriteU32(Address(r[3] + 4u), Address(r[11]));
    memory.WriteU32(Address(r[3] + 8u), Address(r[11]));
}

void InitializeSentinelState(GuestMemory& memory, Registers& state)
{
    auto& r = state.r;
    r[11] = 0u;
    r[10] = 1u;
    r[9] = 0xffffffff80000000ull;
    memory.WriteU32(Address(r[3] + 4u), Address(r[11]));
    memory.WriteU32(Address(r[3] + 8u), Address(r[11]));
    memory.WriteU32(Address(r[3] + 12u), Address(r[11]));
    memory.WriteU32(Address(r[3] + 16u), Address(r[11]));
    memory.WriteU8(Address(r[3] + 20u), static_cast<std::uint8_t>(r[10]));
    memory.WriteU32(Address(r[3]), Address(r[9]));
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state)
{
    switch (entry) {
    case 0x82bd2a08u: InitializeFloatState(memory, deps, state); return true;
    case 0x82bd2c08u:
        // A true tail call: no new link address or caller frame is introduced.
        return crt_reader_sort_float61::Apply(0x82bd2a28u, memory, deps, state);
    case 0x82bd2c50u: InitializeSentinelState(memory, state); return true;
    default: return false;
    }
}
}
