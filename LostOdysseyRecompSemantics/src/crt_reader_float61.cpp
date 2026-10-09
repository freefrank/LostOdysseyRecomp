#include "lo_semantics/crt_reader_float61.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::crt_reader_float61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

constexpr unsigned CurrentNode = 0u;
constexpr unsigned LastWrite = 16u;
constexpr unsigned NodeData = 0u;
constexpr unsigned NodeUsed = 4u;
constexpr unsigned NodeCapacity = 8u;
constexpr unsigned FloatBytes = 4u;

void DisableFlush(Dependencies deps, Registers& state)
{
    if (state.cached_fp_control & 0x8040u) {
        state.cached_fp_control &= ~0x8040u;
        deps.fp.SetHostFpControl(state.cached_fp_control);
    }
}

void AppendFloat(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[12] = state.lr;
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    WriteU64(memory, Address(r[1] - 16u), r[31]);
    DisableFlush(deps, state);
    WriteU64(memory, Address(r[1] - 24u), state.fpr_bits[31]);
    const auto caller_sp = r[1];
    r[1] -= 112u;
    memory.WriteU32(Address(r[1]), Address(caller_sp));
    r[31] = r[3];
    state.fpr_bits[31] = state.fpr_bits[1];

    // Flush the reader's pending bit accumulator before reserving float space.
    // This accepted lower and its allocator share the entire mutable state.
    state.lr = 0x82bd10f8u;
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0c18u, memory, deps.guest, state);
    r[4] = memory.ReadU32(Address(r[3] + CurrentNode));
    r[11] = memory.ReadU32(Address(r[4] + NodeUsed));
    r[10] = memory.ReadU32(Address(r[4] + NodeCapacity));
    r[11] += FloatBytes;
    const auto required = Address(r[11]);
    const auto capacity = Address(r[10]);
    state.cr6 = {std::uint8_t(required < capacity), std::uint8_t(required > capacity),
        std::uint8_t(required == capacity), state.xer_so};
    if (state.cr6.gt) {
        r[5] = 0u;
        state.lr = 0x82bd1118u;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd07d8u, memory, deps.guest, state);
    }

    // Allocation may replace the current node and mutate FPR31. Reload through
    // live r31 and retain the original store order (including guest aliasing).
    r[11] = memory.ReadU32(Address(r[31] + CurrentNode));
    r[3] = r[31];
    r[9] = memory.ReadU32(Address(r[11] + NodeUsed));
    r[10] = memory.ReadU32(Address(r[11] + NodeData));
    r[10] += r[9];
    memory.WriteU32(Address(r[31] + LastWrite), Address(r[10]));
    r[10] = memory.ReadU32(Address(r[11] + NodeUsed));
    r[10] += FloatBytes;
    memory.WriteU32(Address(r[11] + NodeUsed), Address(r[10]));
    r[11] = memory.ReadU32(Address(r[31] + LastWrite));
    DisableFlush(deps, state);
    const float value = static_cast<float>(std::bit_cast<double>(state.fpr_bits[31]));
    memory.WriteU32(Address(r[11]), std::bit_cast<std::uint32_t>(value));

    r[1] += 112u;
    r[12] = memory.ReadU32(Address(r[1] - 8u));
    state.lr = r[12];
    state.fpr_bits[31] = ReadU64(memory, Address(r[1] - 24u));
    r[31] = ReadU64(memory, Address(r[1] - 16u));
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state)
{
    if (entry != 0x82bd10d8u) return false;
    AppendFloat(memory, deps, state);
    return true;
}
}
