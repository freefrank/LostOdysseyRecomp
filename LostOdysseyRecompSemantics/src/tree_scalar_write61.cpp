#include "lo_semantics/tree_scalar_write61.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::tree_scalar_write61 {
bool Apply(GuestAddress entry, GuestMemory &m, Dependencies deps, Registers &s) {
    using recovery_abi::Address;
    if (entry != 0x82bd7d58u && entry != 0x82bd7e18u)
        return false;
    const bool floating = entry == 0x82bd7e18u;
    auto &r = s.r;
    r[12] = s.lr;
    m.WriteU32(Address(r[1] - 8u), Address(r[12]));
    const auto stack = r[1];
    r[1] -= 96u;
    m.WriteU32(Address(r[1]), Address(stack));
    r[11] = Address(r[4]) & 255u;
    const auto gradual = [&]() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            deps.fp.SetHostFpControl(s.cached_fp_control);
        }
    };
    if (floating) {
        gradual();
        m.WriteU32(Address(r[1] + 116u), std::bit_cast<std::uint32_t>(static_cast<float>(
                                             std::bit_cast<double>(s.fpr_bits[1]))));
    } else
        m.WriteU32(Address(r[1] + 116u), Address(r[3]));
    s.cr6 = {0u, std::uint8_t(r[11] != 0u), std::uint8_t(r[11] == 0u), s.xer_so};
    if (!s.cr6.eq) {
        for (unsigned i = 0; i < 2; ++i) {
            r[11] = m.ReadU8(Address(r[1] + 116u + i));
            r[10] = m.ReadU8(Address(r[1] + 119u - i));
            m.WriteU8(Address(r[1] + 119u - i), std::uint8_t(r[11]));
            m.WriteU8(Address(r[1] + 116u + i), std::uint8_t(r[10]));
        }
    }
    r[3] = r[5];
    if (floating) {
        gradual();
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
            double(std::bit_cast<float>(m.ReadU32(Address(r[1] + 116u)))));
        s.lr = 0x82bd7e60u;
        (void)crt_reader_float61::Apply(0x82bd10d8u, m, deps, s);
    } else {
        r[4] = m.ReadU32(Address(r[1] + 116u));
        s.lr = 0x82bd7da0u;
        (void)crt_reader_units61::Apply(0x82bd1050u, m, deps.guest, s);
    }
    r[1] += 96u;
    r[12] = m.ReadU32(Address(r[1] - 8u));
    s.lr = r[12];
    return true;
}
} // namespace lo::semantic::gpu::tree_scalar_write61
