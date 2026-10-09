#include "lo_semantics/memory_input61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::memory_input61 {
bool Apply(GuestAddress entry, GuestMemory &m, float_triplet_transfer::NativeServices &native,
           Registers &s) {
    using recovery_abi::Address;
    using recovery_abi::ReadU64;
    using recovery_abi::WriteU64;
    auto &r = s.r;
    if (entry == 0x82bde550u || entry == 0x82bde568u || entry == 0x82bde580u) {
        const auto width = entry == 0x82bde550u ? 1u : entry == 0x82bde568u ? 2u : 4u;
        r[11] = r[3];
        r[10] = m.ReadU32(Address(r[11] + 4u));
        r[9] = r[10] + width;
        r[3] = width == 1   ? m.ReadU8(Address(r[10]))
               : width == 2 ? m.ReadU16(Address(r[10]))
                            : m.ReadU32(Address(r[10]));
        m.WriteU32(Address(r[11] + 4u), Address(r[9]));
        return true;
    }
    if (entry == 0x82bde598u || entry == 0x82bde5b8u) {
        const bool wide = entry == 0x82bde5b8u;
        r[11] = m.ReadU32(Address(r[3] + 4u));
        r[10] = r[11] + (wide ? 8u : 4u);
        r[11] = wide ? ReadU64(m, Address(r[11])) : m.ReadU32(Address(r[11]));
        m.WriteU32(Address(r[3] + 4u), Address(r[10]));
        if (wide)
            WriteU64(m, Address(r[1] - 16u), r[11]);
        else
            m.WriteU32(Address(r[1] - 16u), Address(r[11]));
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            native.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[1] = wide ? ReadU64(m, Address(r[1] - 16u))
                             : std::bit_cast<std::uint64_t>(
                                   double(std::bit_cast<float>(m.ReadU32(Address(r[1] - 16u)))));
        return true;
    }
    if (entry != 0x82bde5d8u)
        return false;
    r[12] = s.lr;
    m.WriteU32(Address(r[1] - 8u), Address(r[12]));
    WriteU64(m, Address(r[1] - 24u), r[30]);
    WriteU64(m, Address(r[1] - 16u), r[31]);
    const auto stack = r[1];
    r[1] -= 112u;
    m.WriteU32(Address(r[1]), Address(stack));
    r[31] = r[3];
    r[3] = r[4];
    r[30] = r[5];
    r[4] = m.ReadU32(Address(r[31] + 4u));
    s.lr = 0x82bde600u;
    (void)crt_copy_full_context::Apply(0x82b7a0b0u, m, s);
    r[11] = m.ReadU32(Address(r[31] + 4u));
    r[11] += r[30];
    m.WriteU32(Address(r[31] + 4u), Address(r[11]));
    r[1] += 112u;
    r[12] = m.ReadU32(Address(r[1] - 8u));
    s.lr = r[12];
    r[30] = ReadU64(m, Address(r[1] - 24u));
    r[31] = ReadU64(m, Address(r[1] - 16u));
    return true;
}
} // namespace lo::semantic::gpu::memory_input61
