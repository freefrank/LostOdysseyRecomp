#include "lo_semantics/grid_transform_buffer61.h"
#include "lo_semantics/recovery_abi.h"

namespace lo::semantic::gpu::grid_transform_buffer61 {
namespace {
using recovery_abi::Address;
void Compare(Registers& s, std::uint64_t a, std::uint64_t b)
{
    const auto left = Address(a), right = Address(b);
    s.cr6 = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), s.xer_so};
}
void HasStorage(GuestMemory& memory, Registers& s)
{
    auto& r = s.r;
    for (unsigned offset = 8u; offset <= 16u; offset += 4u) {
        r[11] = memory.ReadU32(Address(r[3] + offset));
        Compare(s, r[11], 0u);
        if (s.cr6.eq) { r[3] = 0u; return; }
    }
    r[11] = memory.ReadU32(Address(r[3] + 20u));
    r[3] = 1u;
    Compare(s, r[11], 0u);
    if (s.cr6.eq) r[3] = 0u;
}
void CountRepeatedAddresses(GuestMemory& memory, Registers& s)
{
    auto& r = s.r;
    r[11] = r[3]; r[3] = 0u;
    r[10] = memory.ReadU32(Address(r[11] + 8u));
    Compare(s, r[10], 0u);
    if (s.cr6.eq) return;
    r[7] = memory.ReadU32(Address(r[11] + 20u));
    r[4] = r[10];
    r[11] = memory.ReadU32(Address(r[11] + 16u));
    do {
        r[10] = memory.ReadU32(Address(r[11]));
        r[9] = memory.ReadU32(Address(r[11] + 4u));
        r[5] = (r[10] << 1u) & 0xfffffffeu;
        r[8] = memory.ReadU32(Address(r[11] + 8u));
        r[6] = (r[9] << 1u) & 0xfffffffeu;
        r[10] += r[5]; r[6] = r[9] + r[6];
        r[9] = (r[10] << 2u) & 0xfffffffcu;
        r[10] = (r[6] << 2u) & 0xfffffffcu;
        r[6] = (r[8] << 1u) & 0xfffffffeu;
        r[9] += r[7]; r[8] += r[6]; r[10] += r[7];
        r[8] = (r[8] << 2u) & 0xfffffffcu;
        Compare(s, r[9], r[10]);
        r[8] += r[7];
        bool repeated = s.cr6.eq;
        if (!repeated) { Compare(s, r[10], r[8]); repeated = s.cr6.eq; }
        if (!repeated) { Compare(s, r[8], r[9]); repeated = s.cr6.eq; }
        if (repeated) ++r[3];
        --r[4]; r[11] += 12u;
        Compare(s, r[4], 0u);
    } while (!s.cr6.eq);
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state)
{
    switch (entry) {
    case 0x82bd17f0u: HasStorage(memory, state); return true;
    case 0x82bd1830u: CountRepeatedAddresses(memory, state); return true;
    case 0x82bdac60u:
        state.r[11] = 0u;
        for (unsigned offset = 0u; offset <= 24u; offset += 4u)
            memory.WriteU32(Address(state.r[3] + offset), Address(state.r[11]));
        return true;
    default: return false;
    }
}
}
