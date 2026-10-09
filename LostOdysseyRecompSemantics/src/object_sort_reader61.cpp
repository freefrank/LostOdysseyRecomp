#include "lo_semantics/object_sort_reader61.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/recovery_abi.h"

#include <array>
#include <bit>

namespace lo::semantic::gpu::object_sort_reader61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr unsigned Capacity = 0u, Used = 4u, Payload = 8u;
constexpr unsigned BitMask = 24u, CurrentByte = 25u;

void CompareWord(Registers& s, std::uint64_t a, std::uint64_t b)
{
    const auto left = Address(a), right = Address(b);
    s.cr6 = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), s.xer_so};
}

// A single MSB-first bit reader serves the header, opcode and axis fields.
// Register roles vary for the header; preserve those roles and refill return
// addresses so callbacks and the final scratch state retain the original ABI.
void ReadBits(GuestMemory& memory, Dependencies deps, Registers& s,
    unsigned remaining, unsigned shifted, unsigned accumulator,
    unsigned refill_mask, GuestAddress refill_return)
{
    auto& r = s.r;
    do {
        r[refill_mask] = memory.ReadU8(Address(r[31] + BitMask));
        --r[remaining];
        r[shifted] = (r[accumulator] << 1u) & 0xfffffffeu;
        CompareWord(s, r[refill_mask], 0u);
        if (s.cr6.eq) {
            r[3] = r[31]; s.lr = refill_return;
            (void)crt_reader_units61::Apply(0x82bd09c0u, memory, deps.guest, s);
            memory.WriteU8(Address(r[31] + CurrentByte), static_cast<std::uint8_t>(r[3]));
            memory.WriteU8(Address(r[31] + BitMask), static_cast<std::uint8_t>(r[26]));
        }
        r[11] = memory.ReadU8(Address(r[31] + BitMask));
        CompareWord(s, r[remaining], 0u);
        r[10] = memory.ReadU8(Address(r[31] + CurrentByte));
        r[9] = r[11];
        r[11] >>= 1u;
        r[10] &= r[9];
        r[10] = r[10] == 0u ? 1u : 0u;
        memory.WriteU8(Address(r[31] + BitMask), static_cast<std::uint8_t>(r[11]));
        r[11] = r[10] ^ 1u;
        r[accumulator] = r[11] | r[shifted];
    } while (!s.cr6.eq);
}

// Each row is the spatial meaning of one compact opcode, in x/y/z order.
constexpr std::array<std::array<int, 3>, 26> NeighborDelta{{
    {{-1,0,0}}, {{1,0,0}}, {{0,-1,0}}, {{0,1,0}}, {{0,0,-1}}, {{0,0,1}},
    {{-1,-1,0}}, {{1,1,0}}, {{-1,1,0}}, {{1,-1,0}},
    {{0,-1,-1}}, {{0,1,1}}, {{0,-1,1}}, {{0,1,-1}},
    {{-1,0,-1}}, {{1,0,1}}, {{-1,0,1}}, {{1,0,-1}},
    {{-1,-1,-1}}, {{1,1,1}}, {{-1,-1,1}}, {{1,1,-1}},
    {{1,-1,-1}}, {{-1,1,1}}, {{1,-1,1}}, {{-1,1,-1}}
}};

void ReadCoordinate(GuestMemory& memory, Dependencies deps, Registers& s,
    unsigned axis, GuestAddress refill_return)
{
    auto& r = s.r;
    r[29] = r[27]; r[11] = 0u;
    CompareWord(s, r[27], 0u);
    if (!s.cr6.eq) ReadBits(memory, deps, s, 29u, 28u, 11u, 10u, refill_return);
    memory.WriteU32(Address(r[30] - 8u + axis * 4u), Address(r[11]));
}

void Decode(GuestMemory& memory, Dependencies deps, Registers& s)
{
    auto& r = s.r;
    r[12] = s.lr; s.lr = 0x82baf608u;
    for (unsigned i = 21u; i <= 31u; ++i)
        WriteU64(memory, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    const auto caller_sp = r[1]; r[1] -= 176u;
    memory.WriteU32(Address(r[1]), Address(caller_sp));
    r[21] = r[5]; r[25] = r[3]; r[31] = r[4]; r[27] = 0u;
    for (unsigned width = 5u; width <= 8u; ++width) {
        CompareWord(s, r[21], 1u << width);
        if (!s.cr6.gt) { r[27] = width; break; }
    }
    r[30] = 32u; r[24] = 0u; r[26] = 128u;
    ReadBits(memory, deps, s, 30u, 29u, 24u, 11u, 0x82baf680u);
    r[11] = memory.ReadU32(Address(r[25] + Used));
    CompareWord(s, r[11], 0u);
    if (!s.cr6.eq) { r[11] = 0u; memory.WriteU32(Address(r[25] + Used), 0u); }
    r[23] = 0u;
    CompareWord(s, r[24], 0u);
    if (!s.cr6.eq) {
        r[11] = 0xffffffff83210000ull; r[30] = r[11] + 24972u;
        r[11] = 0xffffffff832e0000ull; r[22] = r[11] - 15288u;
        do {
            r[29] = 5u; r[11] = 0u;
            ReadBits(memory, deps, s, 29u, 28u, 11u, 10u, 0x82baf710u);
            r[10] = (r[11] << 2u) & 0xfffffffcu;
            CompareWord(s, r[11], 31u);
            r[9] = memory.ReadU32(Address(r[10] + r[22])); ++r[9];
            memory.WriteU32(Address(r[10] + r[22]), Address(r[9]));
            if (!s.cr6.gt) {
                const auto opcode = Address(r[11]);
                r[12] = 0xffffffff82bb0000ull - 2180u;
                r[0] = (r[11] << 2u) & 0xfffffffcu;
                r[0] = memory.ReadU32(Address(r[12] + r[0])); s.ctr = r[0];
                if (opcode < NeighborDelta.size()) {
                    for (unsigned axis = 0u; axis < 3u; ++axis) {
                        const auto delta = NeighborDelta[opcode][axis];
                        if (delta == 0) continue;
                        r[11] = memory.ReadU32(Address(r[30] - 8u + axis * 4u));
                        r[11] += static_cast<std::uint64_t>(delta);
                        memory.WriteU32(Address(r[30] - 8u + axis * 4u), Address(r[11]));
                    }
                } else {
                    switch (opcode) {
                    case 26u: ReadCoordinate(memory, deps, s, 0u, 0x82bafa38u); break;
                    case 27u: ReadCoordinate(memory, deps, s, 1u, 0x82bafaa8u); break;
                    case 28u: ReadCoordinate(memory, deps, s, 2u, 0x82bafb18u); break;
                    case 29u:
                        ReadCoordinate(memory, deps, s, 0u, 0x82bafb84u);
                        ReadCoordinate(memory, deps, s, 1u, 0x82bafbf0u); break;
                    case 30u:
                        ReadCoordinate(memory, deps, s, 0u, 0x82bafc60u);
                        ReadCoordinate(memory, deps, s, 2u, 0x82bafcccu); break;
                    case 31u:
                        ReadCoordinate(memory, deps, s, 0u, 0x82bafd38u);
                        ReadCoordinate(memory, deps, s, 1u, 0x82bafda4u);
                        ReadCoordinate(memory, deps, s, 2u, 0x82bafe10u); break;
                    }
                }
            }
            // The generated mullw path retains the signed 32x32 product in its 64-bit register;
            // subsequent coordinate additions are full-width before truncation.
            r[11] = memory.ReadU32(Address(r[30]));
            r[10] = memory.ReadU32(Address(r[30] - 4u));
            r[11] = static_cast<std::uint64_t>(std::int64_t(std::bit_cast<std::int32_t>(Address(r[11]))) *
                std::bit_cast<std::int32_t>(Address(r[21])));
            r[9] = memory.ReadU32(Address(r[25] + Used)); r[11] += r[10];
            r[10] = memory.ReadU32(Address(r[25] + Capacity)); CompareWord(s, r[9], r[10]);
            r[10] = memory.ReadU32(Address(r[30] - 8u));
            r[11] = static_cast<std::uint64_t>(std::int64_t(std::bit_cast<std::int32_t>(Address(r[11]))) *
                std::bit_cast<std::int32_t>(Address(r[21])));
            r[29] = r[11] + r[10];
            if (s.cr6.eq) {
                r[4] = 1u; r[3] = r[25]; s.lr = 0x82bafe88u;
                (void)reader_buffer_growth61::Apply(0x82bd2870u, memory, deps, s);
            }
            r[11] = memory.ReadU32(Address(r[25] + Used)); ++r[23];
            r[10] = memory.ReadU32(Address(r[25] + Payload));
            r[11] = (r[11] << 2u) & 0xfffffffcu; CompareWord(s, r[23], r[24]);
            memory.WriteU32(Address(r[11] + r[10]), Address(r[29]));
            r[11] = memory.ReadU32(Address(r[25] + Used)); ++r[11];
            memory.WriteU32(Address(r[25] + Used), Address(r[11]));
        } while (s.cr6.lt);
    }
    r[3] = r[24]; r[1] += 176u;
    for (unsigned i = 21u; i <= 31u; ++i)
        r[i] = ReadU64(memory, Address(r[1] - 16u - 8u * (31u - i)));
    r[12] = memory.ReadU32(Address(r[1] - 8u)); s.lr = r[12];
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state)
{
    if (entry != 0x82baf600u) return false;
    Decode(memory, deps, state); return true;
}
}
