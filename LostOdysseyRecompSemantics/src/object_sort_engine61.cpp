#include "lo_semantics/object_sort_engine61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/crt_close_next61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/recovery_abi.h"

#include <array>
#include <bit>

namespace lo::semantic::gpu::object_sort_engine61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

void Compare(Registers& state, std::uint64_t lhs, std::uint64_t rhs)
{
    const auto left = Address(lhs), right = Address(rhs);
    state.cr6 = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), state.xer_so};
}
std::uint64_t Product(std::uint64_t lhs, std::uint64_t rhs)
{
    return std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(lhs))) *
        std::int64_t(std::bit_cast<std::int32_t>(Address(rhs))));
}
std::uint64_t ShiftLeft(std::uint64_t value, std::uint64_t count)
{
    const auto shift = Address(count) & 63u;
    return shift >= 32u ? 0u : Address(value) << shift;
}
std::uint64_t ShiftRight(std::uint64_t value, std::uint64_t count)
{
    const auto shift = Address(count) & 63u;
    return shift >= 32u ? 0u : Address(value) >> shift;
}
void DivideLowWord(std::uint64_t& target, std::uint64_t value, std::uint64_t divisor)
{
    // The generated original writes only the low word of its destination.
    target = (target & 0xffffffff00000000ull) | (Address(value) / Address(divisor));
}

void Support(GuestAddress entry, GuestAddress continuation, GuestMemory& memory,
    Dependencies deps, Registers& state)
{
    state.lr = continuation;
    (void)object_sort_support61::Apply(entry, memory, {deps.sort.guest, deps.fp}, state);
}
void Grow(GuestAddress continuation, unsigned offset, GuestMemory& memory,
    Dependencies deps, Registers& state)
{
    state.r[3] = state.r[1] + offset;
    state.r[4] = 1u;
    state.lr = continuation;
    (void)reader_buffer_growth61::Apply(0x82bd2870u, memory,
        {deps.sort.guest, deps.fp}, state);
}
void FlushByte(GuestAddress continuation, GuestMemory& memory,
    Dependencies deps, Registers& state)
{
    if (!state.cr6.eq) return;
    state.r[3] = state.r[31];
    memory.WriteU8(Address(state.r[31] + 24u), std::uint8_t(state.r[22]));
    state.lr = continuation;
    (void)crt_close_next61::Apply(0x82bd0938u, memory, deps.sort.guest, state);
}

// Variable fields (the count and absolute coordinates) share one write path.
// r30 is the live high-to-low bit mask; the value register is re-read after
// every callback. Scratch registers and continuation identify the guest call.
void WriteVariableBits(unsigned value_register, GuestAddress continuation,
    GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    do {
        r[9] = std::uint64_t((r[30] & r[value_register] & 0xffffffffu) != 0u);
        r[10] = (memory.ReadU8(Address(r[31] + 25u)) << 1u) & 0xffu;
        r[11] = (memory.ReadU8(Address(r[31] + 24u)) + 1u) & 0xffu;
        r[4] = r[9] | r[10];
        Compare(state, r[11], 8u);
        memory.WriteU8(Address(r[31] + 24u), std::uint8_t(r[11]));
        memory.WriteU8(Address(r[31] + 25u), std::uint8_t(r[4]));
        FlushByte(continuation, memory, deps, state);
        r[30] = Address(r[30]) >> 1u;
        Compare(state, r[30], 0u);
    } while (!state.cr6.eq);
}

// The neighbor code is semantic data, not an instruction stream. The original
// specializes a few powers of two and changes CR0 only for immediate masks.
constexpr std::array<GuestAddress, 32> CodeReturns{{
    0x82bae524u, 0x82bae588u, 0x82bae5f0u, 0x82bae65cu,
    0x82bae6bcu, 0x82bae720u, 0x82bae78cu, 0x82bae7f8u,
    0x82bae858u, 0x82bae8c4u, 0x82bae928u, 0x82bae98cu,
    0x82bae9f0u, 0x82baea54u, 0x82baeac0u, 0x82baeb2cu,
    0x82baeb84u, 0x82baebe8u, 0x82baec54u, 0x82baecc0u,
    0x82baed24u, 0x82baed88u, 0x82baedf4u, 0x82baee60u,
    0x82baeec4u, 0x82baef28u, 0x82baef94u, 0x82baf06cu,
    0x82baf144u, 0x82baf214u, 0x82baf350u, 0x82baf474u}};
constexpr bool ChangesCr0(unsigned code)
{
    return code == 5u || code == 9u || code == 10u || code == 11u ||
        code == 13u || (code >= 17u && code <= 23u) ||
        (code >= 25u && code <= 27u) || code == 29u;
}

void WriteCode(unsigned code, GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    if (code == 2u) r[11] = 16u;
    else r[30] = 16u;
    do {
        const auto old_byte = memory.ReadU8(Address(r[31] + 25u));
        if (code == 2u) r[30] = Address(r[11]) >> 1u;
        const auto pending = memory.ReadU8(Address(r[31] + 24u));
        r[11] = (pending + 1u) & 0xffu;
        if (code == 0u) {
            r[10] = old_byte << 1u;
            r[4] = r[10] & 0xffu;
        } else if (code == 1u || code == 2u) {
            r[10] = old_byte;
            r[9] = (r[30] & 0xffffffff00000001ull) | (r[10] << 1u);
            r[4] = r[9] & 0xffu;
        } else if (code == 4u || code == 8u || code == 16u) {
            const unsigned shift = code == 4u ? 2u : (code == 8u ? 3u : 4u);
            r[9] = (Address(r[30]) >> shift) & 1u;
            r[10] = (old_byte << 1u) | r[9];
            r[4] = r[10] & 0xffu;
        } else {
            const auto masked = Address(r[30]) & code;
            if (ChangesCr0(code)) {
                state.cr0 = {0u, std::uint8_t(masked != 0u),
                    std::uint8_t(masked == 0u), state.xer_so};
            }
            r[9] = (old_byte << 1u) & 0xffu;
            r[10] = std::uint64_t(masked != 0u);
            r[4] = r[9] | r[10];
        }
        Compare(state, r[11], 8u);
        const bool count_first = code == 2u || code == 4u || code == 8u || code == 16u;
        if (count_first) memory.WriteU8(Address(r[31] + 24u), std::uint8_t(r[11]));
        memory.WriteU8(Address(r[31] + 25u), std::uint8_t(r[4]));
        if (!count_first) memory.WriteU8(Address(r[31] + 24u), std::uint8_t(r[11]));
        FlushByte(CodeReturns[code], memory, deps, state);
        if (code == 2u) r[11] = r[30];
        else r[30] = Address(r[30]) >> 1u;
        Compare(state, r[30], 0u);
    } while (!state.cr6.eq);
}

struct Delta { std::int32_t x, y, z; };
constexpr std::array<Delta, 26> Neighbors{{
    {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1},
    {-1,-1,0}, {1,1,0}, {-1,1,0}, {1,-1,0}, {0,-1,-1}, {0,1,1},
    {0,-1,1}, {0,1,-1}, {-1,0,-1}, {1,0,1}, {-1,0,1}, {1,0,-1},
    {-1,-1,-1}, {1,1,1}, {-1,-1,1}, {1,1,-1}, {1,-1,-1}, {-1,1,1},
    {1,-1,1}, {-1,1,-1}}};
unsigned SelectCode(const Registers& state)
{
    const auto dx = std::bit_cast<std::int32_t>(Address(state.r[7]));
    const auto dy = std::bit_cast<std::int32_t>(Address(state.r[10]));
    const auto dz = std::bit_cast<std::int32_t>(Address(state.r[11]));
    for (unsigned code = 0u; code < Neighbors.size(); ++code) {
        const auto delta = Neighbors[code];
        if (dx == delta.x && dy == delta.y && dz == delta.z) return code;
    }
    if (dy == 0 && dz == 0) return 26u;
    if (dx == 0 && dy != 0 && dz == 0) return 27u;
    if (dx == 0 && dy == 0 && dz != 0) return 28u;
    if (dx != 0 && dy != 0 && dz == 0) return 29u;
    if (dx != 0 && dy == 0 && dz != 0) return 30u;
    return 31u;
}

void WriteAbsoluteCoordinates(unsigned code, GuestMemory& memory,
    Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[11] = r[21] - 1u;
    if (code < 29u) {
        r[30] = ShiftLeft(r[20], r[11]);
        Compare(state, r[30], 0u);
        if (!state.cr6.eq) {
            constexpr std::array<GuestAddress, 3> returns{
                0x82baeff8u, 0x82baf0d0u, 0x82baf1a8u};
            WriteVariableBits(54u - code, returns[code - 26u], memory, deps, state);
        }
        return;
    }
    r[29] = ShiftLeft(r[20], r[11]);
    r[30] = r[29];
    Compare(state, r[29], 0u);
    if (!state.cr6.eq) {
        const auto continuation = code == 29u ? 0x82baf27cu :
            (code == 30u ? 0x82baf3b8u : 0x82baf4dcu);
        WriteVariableBits(28u, continuation, memory, deps, state);
    }
    r[30] = r[29];
    Compare(state, r[29], 0u);
    if (!state.cr6.eq) {
        const auto continuation = code == 29u ? 0x82baf2dcu :
            (code == 30u ? 0x82baf418u : 0x82baf53cu);
        WriteVariableBits(code == 30u ? 26u : 27u, continuation, memory, deps, state);
    }
    if (code == 31u) {
        r[30] = r[29];
        Compare(state, r[29], 0u);
        if (!state.cr6.eq)
            WriteVariableBits(26u, 0x82baf59cu, memory, deps, state);
    }
}

// Append one coordinate/key to a stack-local descriptor. Allocation belongs
// to Grow; the append must reload capacity data and payload after that call.
void AppendWord(unsigned offset, unsigned value_register, GuestMemory& memory,
    Registers& state, bool z_coordinate = false)
{
    auto& r = state.r;
    if (z_coordinate) {
        r[11] = memory.ReadU32(Address(r[1] + offset + 4u));
        r[30] = r[22];
        r[9] = memory.ReadU32(Address(r[1] + offset + 8u));
        r[10] = Address(r[11]) << 2u;
        r[11] = r[22];
        memory.WriteU32(Address(r[9] + r[10]), Address(r[value_register]));
        r[10] = memory.ReadU32(Address(r[1] + offset + 4u));
        r[10] += 1u;
        memory.WriteU32(Address(r[1] + offset + 4u), Address(r[10]));
    } else {
        r[11] = memory.ReadU32(Address(r[1] + offset + 4u));
        r[10] = memory.ReadU32(Address(r[1] + offset + 8u));
        r[11] = Address(r[11]) << 2u;
        const auto destination = Address(r[10] + r[11]);
        memory.WriteU32(destination, Address(r[value_register]));
        r[11] = memory.ReadU32(Address(r[1] + offset + 4u));
        r[11] += 1u;
        memory.WriteU32(Address(r[1] + offset + 4u), Address(r[11]));
    }
}

void SplitAndBuildKeys(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    do {
        r[11] = memory.ReadU32(Address(r[26]));
        DivideLowWord(r[29], r[11], r[25]);
        const auto capacity = memory.ReadU32(Address(r[1] + 112u));
        r[9] = memory.ReadU32(Address(r[1] + 116u));
        r[10] = Product(r[25], r[29]);
        r[11] -= r[10];
        DivideLowWord(r[28], r[11], r[24]);
        r[10] = Product(r[28], r[24]);
        r[27] = r[11] - r[10];
        Compare(state, r[9], capacity);
        if (state.cr6.eq) Grow(0x82bae330u, 112u, memory, deps, state);
        AppendWord(112u, 27u, memory, state);
        r[11] = memory.ReadU32(Address(r[1] + 96u));
        r[10] = memory.ReadU32(Address(r[1] + 100u));
        Compare(state, r[10], r[11]);
        if (state.cr6.eq) Grow(0x82bae368u, 96u, memory, deps, state);
        AppendWord(96u, 28u, memory, state);
        r[11] = memory.ReadU32(Address(r[1] + 80u));
        r[10] = memory.ReadU32(Address(r[1] + 84u));
        Compare(state, r[10], r[11]);
        if (state.cr6.eq) Grow(0x82bae3a0u, 80u, memory, deps, state);
        AppendWord(80u, 29u, memory, state, true);
        r[30] = r[22];
        r[11] = r[22];
        Compare(state, r[21], 0u);
        if (!state.cr6.eq) {
            r[10] = r[20];
            do {
                const auto coordinate_bit = r[11], key_bit = r[10];
                const auto mask = ShiftLeft(r[20], coordinate_bit);
                r[6] = ShiftRight(mask & r[27], coordinate_bit);
                r[5] = ShiftRight(mask & r[29], coordinate_bit);
                r[7] = ShiftLeft(r[5], key_bit - 1u);
                r[8] = ShiftLeft(r[6], key_bit + 1u) | r[7];
                r[9] = r[8] | ShiftLeft(ShiftRight(mask & r[28], coordinate_bit), key_bit);
                r[30] = r[9] | r[30];
                r[11] += 1u;
                r[10] += 3u;
                Compare(state, r[11], r[21]);
            } while (state.cr6.lt);
        }
        r[11] = memory.ReadU32(Address(r[1] + 128u));
        r[10] = memory.ReadU32(Address(r[1] + 132u));
        Compare(state, r[10], r[11]);
        if (state.cr6.eq) Grow(0x82bae438u, 128u, memory, deps, state);
        AppendWord(128u, 30u, memory, state);
        r[23] -= 1u;
        r[26] += 4u;
        Compare(state, r[23], 0u);
    } while (!state.cr6.eq);
}

void Encode(GuestMemory& memory, Dependencies deps, Registers& state)
{
    auto& r = state.r;
    r[12] = state.lr;
    state.lr = 0x82bae208u;
    for (unsigned index = 19u; index < 32u; ++index)
        WriteU64(memory, Address(r[1] - 16u - (31u - index) * 8u), r[index]);
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    const auto caller_sp = r[1];
    r[1] -= 288u;
    memory.WriteU32(Address(r[1]), Address(caller_sp));
    r[24] = r[5];
    r[31] = r[4];
    r[22] = 0u;
    r[21] = r[22];
    Compare(state, r[24], 32u);
    if (!state.cr6.gt) r[21] = 5u;
    else {
        Compare(state, r[24], 64u);
        if (!state.cr6.gt) r[21] = 6u;
        else {
            Compare(state, r[24], 128u);
            if (!state.cr6.gt) r[21] = 7u;
            else {
                Compare(state, r[24], 256u);
                if (!state.cr6.gt) r[21] = 8u;
            }
        }
    }
    r[19] = memory.ReadU32(Address(r[3] + 4u));
    r[29] = memory.ReadU32(Address(r[3] + 8u));
    r[30] = 0xffffffff80000000ull;
    WriteVariableBits(19u, 0x82bae2acu, memory, deps, state);
    for (unsigned i = 0u; i < 4u; ++i) {
        r[3] = r[1] + 128u - i * 16u;
        Support(0x82bd2a08u, 0x82bae2c0u + i * 8u, memory, deps, state);
    }
    r[20] = 1u;
    Compare(state, r[19], 0u);
    if (!state.cr6.eq) {
        r[25] = Product(r[24], r[24]);
        r[26] = r[29];
        r[23] = r[19];
        SplitAndBuildKeys(memory, deps, state);
    }

    r[3] = r[1] + 144u;
    Support(0x82bd2c50u, 0x82bae46cu, memory, deps, state);
    r[4] = memory.ReadU32(Address(r[1] + 136u));
    r[5] = r[19];
    r[6] = 0u;
    r[3] = r[1] + 144u;
    state.lr = 0x82bae480u;
    (void)crt_reader_bucket_sort61::Apply(0x82bd2df0u, memory, deps.sort, state);
    r[23] = memory.ReadU32(Address(r[3] + 4u));
    r[25] = r[22];
    Compare(state, r[19], 0u);
    if (!state.cr6.eq) {
        r[11] = 0xffffffff83210000ull;
        r[24] = r[11] + 24972u;
        r[10] = memory.ReadU32(Address(r[24] - 8u));
        r[9] = memory.ReadU32(Address(r[24] - 4u));
        r[8] = memory.ReadU32(Address(r[24]));
        do {
            r[11] = Address(r[25]) << 2u;
            r[11] = memory.ReadU32(Address(r[23] + r[11]));
            r[11] = Address(r[11]) << 2u;
            r[28] = memory.ReadU32(Address(r[1] + 120u));
            r[27] = memory.ReadU32(Address(r[1] + 104u));
            r[26] = memory.ReadU32(Address(r[1] + 88u));
            r[28] = memory.ReadU32(Address(r[28] + r[11]));
            r[27] = memory.ReadU32(Address(r[27] + r[11]));
            r[26] = memory.ReadU32(Address(r[26] + r[11]));
            r[7] = r[28] - r[10];
            r[10] = r[27] - r[9];
            r[11] = r[26] - r[8];
            const auto code = SelectCode(state);
            WriteCode(code, memory, deps, state);
            if (code >= 26u) WriteAbsoluteCoordinates(code, memory, deps, state);
            memory.WriteU32(Address(r[24] - 8u), Address(r[28]));
            memory.WriteU32(Address(r[24] - 4u), Address(r[27]));
            memory.WriteU32(Address(r[24]), Address(r[26]));
            r[10] = r[28];
            r[9] = r[27];
            r[8] = r[26];
            r[25] += 1u;
            Compare(state, r[25], r[19]);
        } while (state.cr6.lt);
    }
    r[3] = r[1] + 144u;
    state.lr = 0x82baf5d4u;
    (void)crt_reader_follow61::Apply(0x82bd2c78u, memory, deps.sort.guest, state);
    for (unsigned i = 0u; i < 4u; ++i) {
        r[3] = r[1] + 80u + i * 16u;
        Support(0x82bd2c08u, 0x82baf5dcu + i * 8u, memory, deps, state);
    }
    r[1] += 288u;
    for (unsigned index = 19u; index < 32u; ++index)
        r[index] = ReadU64(memory, Address(r[1] - 16u - (31u - index) * 8u));
    r[12] = memory.ReadU32(Address(r[1] - 8u));
    state.lr = r[12];
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state)
{
    if (entry != 0x82bae200u) return false;
    Encode(memory, deps, state);
    return true;
}
}
