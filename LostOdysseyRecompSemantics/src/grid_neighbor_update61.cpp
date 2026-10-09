#include "lo_semantics/grid_neighbor_update61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace lo::semantic::gpu::grid_neighbor_update61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr std::uint64_t Missing = ~std::uint64_t{0};

void CompareUnsigned(Registers& s, std::uint64_t left, std::uint64_t right)
{
    const auto a = Address(left), b = Address(right);
    s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b), s.xer_so};
}
void CompareSigned(Registers& s, std::uint64_t left, std::int32_t right)
{
    const auto a = std::bit_cast<std::int32_t>(Address(left));
    s.cr6 = {std::uint8_t(a < right), std::uint8_t(a > right), std::uint8_t(a == right), s.xer_so};
}
std::uint64_t WordProduct(std::uint64_t a, std::uint64_t b)
{
    // Match the generated full signed 32x32 product, including its high word.
    return static_cast<std::uint64_t>(
        std::int64_t(std::bit_cast<std::int32_t>(Address(a))) *
        std::bit_cast<std::int32_t>(Address(b)));
}

// Each corner has distinct observable scratch registers in the guest ABI.
// A missing boundary corner succeeds without reading the buffer. Otherwise
// retain the exact word-offset and bit-test results for the caller's state.
bool MarkedCorner(GuestMemory& memory, Registers& s,
    unsigned index, unsigned offset, unsigned pointer, unsigned value)
{
    auto& r = s.r;
    CompareSigned(s, r[index], -1);
    if (s.cr6.eq) return true;
    r[pointer] = memory.ReadU32(Address(r[3] + 108u));
    r[offset] = (std::uint64_t(Address(r[index])) << 2u) & 0xfffffffcu;
    r[value] = memory.ReadU32(Address(r[offset] + r[pointer]));
    r[value] = ~r[value];
    r[value] = (Address(r[value]) >> 31u) & 1u;
    CompareSigned(s, r[value], 0);
    return s.cr6.eq;
}
void Update(GuestMemory& memory, Registers& s)
{
    auto& r = s.r;
    r[12] = s.lr;
    s.lr = 0x82bb23c8u;
    for (unsigned i = 25u; i <= 31u; ++i)
        WriteU64(memory, Address(r[1] - 8u * (33u - i)), r[i]);
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));

    r[26] = memory.ReadU32(Address(r[3] + 88u));
    r[27] = 0u;
    CompareUnsigned(s, r[26], 0u);
    if (!s.cr6.eq) {
        r[31] = Missing;
        do {
            r[28] = 0u;
            do {
                r[29] = 0u;
                do {
                    // Flatten (x,y,z), then form the eight positive corners.
                    r[11] = memory.ReadU32(Address(r[3] + 88u));
                    r[10] = memory.ReadU32(Address(r[3] + 92u));
                    r[8] = WordProduct(r[11], r[28]);
                    r[9] = WordProduct(r[10], r[27]);
                    r[9] += r[8];
                    r[8] = r[11] - 1u;
                    r[9] += r[29];
                    CompareUnsigned(s, r[29], r[8]);
                    r[10] += r[9];
                    r[8] = r[11] + r[9];
                    r[11] += r[10];
                    r[4] = r[9] + 1u;
                    r[30] = r[8] + 1u;
                    r[5] = r[10] + 1u;
                    r[7] = r[11] + 1u;
                    if (s.cr6.eq) r[7] = r[5] = r[30] = r[4] = r[31];
                    r[6] = memory.ReadU32(Address(r[3] + 88u));
                    r[6] -= 1u;
                    CompareUnsigned(s, r[28], r[6]);
                    if (s.cr6.eq) r[7] = r[11] = r[30] = r[8] = r[31];
                    CompareUnsigned(s, r[27], r[6]);
                    if (s.cr6.eq) r[7] = r[11] = r[5] = r[10] = r[31];

                    const bool marked = MarkedCorner(memory, s, 9u, 25u, 6u, 6u) &&
                        MarkedCorner(memory, s, 4u, 4u, 6u, 6u) &&
                        MarkedCorner(memory, s, 8u, 8u, 6u, 8u) &&
                        MarkedCorner(memory, s, 10u, 10u, 8u, 10u) &&
                        MarkedCorner(memory, s, 5u, 8u, 10u, 10u) &&
                        MarkedCorner(memory, s, 11u, 11u, 10u, 11u) &&
                        MarkedCorner(memory, s, 30u, 10u, 11u, 11u) &&
                        MarkedCorner(memory, s, 7u, 10u, 11u, 11u);
                    if (marked) {
                        r[10] = memory.ReadU32(Address(r[3] + 108u));
                        r[11] = (std::uint64_t(Address(r[9])) << 2u) & 0xfffffffcu;
                        r[9] = memory.ReadU32(Address(r[11] + r[10]));
                        r[9] |= 0x40000000u;
                        memory.WriteU32(Address(r[11] + r[10]), Address(r[9]));
                    }
                    ++r[29]; CompareUnsigned(s, r[29], r[26]);
                } while (s.cr6.lt);
                ++r[28]; CompareUnsigned(s, r[28], r[26]);
            } while (s.cr6.lt);
            ++r[27]; CompareUnsigned(s, r[27], r[26]);
        } while (s.cr6.lt);
    }
    r[3] = 1u;
    for (unsigned i = 25u; i <= 31u; ++i)
        r[i] = ReadU64(memory, Address(r[1] - 8u * (33u - i)));
    r[12] = memory.ReadU32(Address(r[1] - 8u));
    s.lr = r[12];
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state)
{
    if (entry != 0x82bb23c0u) return false;
    Update(memory, state);
    return true;
}
}
