#include "lo_semantics/tree_flat_write61.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
namespace lo::semantic::gpu::tree_flat_write61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Write {
    GuestMemory &m;
    Dependencies deps;
    Registers &s;
    bool compact;
    unsigned Stride() const { return compact ? 32u : 36u; }
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Stage() {
        auto &r = s.r;
        r[11] = Word(r[30] + 8u);
        Compare(r[28]);
        r[11] += r[31];
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            deps.fp.SetHostFpControl(s.cached_fp_control);
        }
        for (unsigned offset = 0; offset < 24u; offset += 4u) {
            s.fpr_bits[0] =
                std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(r[11] + offset))));
            Store(r[1] + 80u + offset, std::bit_cast<std::uint32_t>(static_cast<float>(
                                           std::bit_cast<double>(s.fpr_bits[0]))));
        }
        r[10] = Word(r[11] + 24u);
        Store(r[1] + 104u, r[10]);
        if (compact) {
            r[11] = Word(r[11] + 28u);
            Store(r[1] + 108u, r[11]);
        } else {
            r[10] = Word(r[11] + 28u);
            Store(r[1] + 108u, r[10]);
            r[11] = Word(r[11] + 32u);
            Store(r[1] + 112u, r[11]);
        }
        if (s.cr6.eq)
            return;
        std::array<std::uint8_t, 36> bytes{};
        for (unsigned i = 0; i < Stride(); ++i)
            bytes[i] = m.ReadU8(Address(r[1] + 80u + i));
        for (unsigned i = 0; i < Stride(); ++i)
            m.WriteU8(Address(r[1] + 80u + i), bytes[(i & ~3u) + 3u - i % 4u]);
        for (unsigned i = 0; i < 8; ++i)
            r[11u - i] = bytes[4u * i + 1u];
        if (compact)
            r[3] = bytes[30];
        else {
            r[3] = bytes[33];
            r[26] = bytes[34];
        }
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = compact ? 0x82bddb28u : 0x82bdd870u;
        for (unsigned i = compact ? 27u : 26u; i < 32; ++i)
            WriteU64(m, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        Store(r[1] - 8u, r[12]);
        const auto stack = r[1];
        r[1] -= compact ? 160u : 176u;
        Store(r[1], stack);
        r[30] = r[3];
        r[31] = r[4];
        r[27] = r[5];
        r[3] = Word(r[30] + 4u);
        s.lr = compact ? 0x82bddb40u : 0x82bdd888u;
        (void)tree_scalar_write61::Apply(0x82bd7d58u, m, deps, s);
        r[11] = Word(r[30] + 4u);
        r[29] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[28] = Address(r[31]) & 255u;
            r[31] = 0;
            do {
                Stage();
                r[5] = Stride();
                r[4] = r[1] + 80u;
                r[3] = r[27];
                s.lr = compact ? 0x82bddcb8u : 0x82bdda28u;
                (void)crt_reader_units61::Apply(0x82bd1160u, m, deps.guest, s);
                r[11] = Word(r[30] + 4u);
                ++r[29];
                r[31] += Stride();
                Compare(r[29], r[11]);
            } while (s.cr6.lt);
        }
        r[3] = 1;
        r[1] += compact ? 160u : 176u;
        for (unsigned i = compact ? 27u : 26u; i < 32; ++i)
            r[i] = ReadU64(m, Address(r[1] - 16u - 8u * (31u - i)));
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &m, Dependencies deps, Registers &s) {
    if (entry != 0x82bdd868u && entry != 0x82bddb20u)
        return false;
    Write{m, deps, s, entry == 0x82bddb20u}.Run();
    return true;
}
} // namespace lo::semantic::gpu::tree_flat_write61
