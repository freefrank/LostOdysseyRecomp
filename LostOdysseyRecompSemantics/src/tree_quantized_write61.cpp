#include "lo_semantics/tree_quantized_write61.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
namespace lo::semantic::gpu::tree_quantized_write61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Write {
    GuestMemory &m;
    Dependencies deps;
    Registers &s;
    bool compact;
    unsigned Stride() const { return compact ? 20u : 24u; }
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void StageNode() {
        auto &r = s.r;
        r[11] = Word(r[31] + 8u);
        r[10] = r[1] + 80u;
        r[9] = Stride() / 4u;
        r[11] += r[30];
        s.ctr = r[9];
        do {
            r[9] = Word(r[11]);
            r[11] += 4u;
            Store(r[10], r[9]);
            r[10] += 4u;
            --s.ctr;
        } while (Address(s.ctr));
        Compare(r[26]);
        if (s.cr6.eq)
            return;
        // Guest stack scratch is ordinary RAM. Capture the local record before
        // swapping disjoint fields, then retain the PPC-visible final byte loads.
        std::array<std::uint8_t, 24> bytes{};
        for (unsigned i = 0; i < Stride(); ++i)
            bytes[i] = m.ReadU8(Address(r[1] + 80u + i));
        for (unsigned i = 0; i < Stride(); ++i) {
            const auto width = i < 12 ? 2u : 4u;
            m.WriteU8(Address(r[1] + 80u + i), bytes[(i / width) * width + width - 1u - i % width]);
        }
        r[11] = bytes[13];
        r[10] = bytes[17];
        if (compact) {
            r[9] = bytes[0];
            r[8] = bytes[2];
            r[7] = bytes[4];
            r[6] = bytes[6];
            r[5] = bytes[8];
            r[4] = bytes[10];
            r[3] = bytes[18];
        } else {
            r[8] = bytes[0];
            r[9] = bytes[21];
            r[25] = bytes[22];
            r[7] = bytes[2];
            r[6] = bytes[4];
            r[5] = bytes[6];
            r[4] = bytes[8];
            r[3] = bytes[10];
        }
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = (compact ? 0x82bdb668u : 0x82bdc840u);
        for (unsigned i = compact ? 26u : 25u; i < 32; ++i)
            WriteU64(m, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        Store(r[1] - 8u, r[12]);
        const auto stack = r[1];
        r[1] -= compact ? 160u : 176u;
        Store(r[1], stack);
        r[31] = r[3];
        r[27] = r[4];
        r[29] = r[5];
        r[3] = Word(r[31] + 4u);
        s.lr = (compact ? 0x82bdb680u : 0x82bdc858u);
        (void)tree_scalar_write61::Apply(0x82bd7d58u, m, deps, s);
        r[11] = Word(r[31] + 4u);
        r[28] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[26] = Address(r[27]) & 255u;
            r[30] = 0;
            do {
                StageNode();
                r[5] = Stride();
                r[4] = r[1] + 80u;
                r[3] = r[29];
                s.lr = (compact ? 0x82bdb778u : 0x82bdc970u);
                (void)crt_reader_units61::Apply(0x82bd1160u, m, deps.guest, s);
                r[11] = Word(r[31] + 4u);
                ++r[28];
                r[30] += Stride();
                Compare(r[28], r[11]);
            } while (s.cr6.lt);
        }
        constexpr GuestAddress returns[]{0x82bdc994u, 0x82bdc9a4u, 0x82bdc9b4u,
                                         0x82bdc9c4u, 0x82bdc9d4u, 0x82bdc9e4u};
        constexpr GuestAddress compactReturns[]{0x82bdb79cu, 0x82bdb7acu, 0x82bdb7bcu,
                                                0x82bdb7ccu, 0x82bdb7dcu, 0x82bdb7ecu};
        for (unsigned axis = 0; axis < 6; ++axis) {
            r[5] = r[29];
            if (axis)
                r[4] = r[27];
            if (s.cached_fp_control & 0x8040u) {
                s.cached_fp_control &= ~0x8040u;
                deps.fp.SetHostFpControl(s.cached_fp_control);
            }
            s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
                double(std::bit_cast<float>(Word(r[31] + 12u + 4u * axis))));
            if (!axis)
                r[4] = r[27];
            s.lr = compact ? compactReturns[axis] : returns[axis];
            (void)tree_scalar_write61::Apply(0x82bd7e18u, m, deps, s);
        }
        r[3] = 1;
        r[1] += compact ? 160u : 176u;
        for (unsigned i = compact ? 26u : 25u; i < 32; ++i)
            r[i] = ReadU64(m, Address(r[1] - 16u - 8u * (31u - i)));
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &m, Dependencies deps, Registers &s) {
    if (entry != 0x82bdc838u && entry != 0x82bdb660u)
        return false;
    Write{m, deps, s, entry == 0x82bdb660u}.Run();
    return true;
}
} // namespace lo::semantic::gpu::tree_quantized_write61
