#include "lo_semantics/tree_compact_flatten61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::tree_compact_flatten61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Flatten {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &native;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Load(unsigned i, std::uint64_t p) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(static_cast<float>(v)));
    }
    void Save(unsigned i, std::uint64_t p) {
        Store(p, std::bit_cast<std::uint32_t>(static_cast<float>(F(i))));
    }
    void CompareZero(std::uint64_t v) {
        auto x = Address(v);
        s.cr6 = {0u, std::uint8_t(x != 0), std::uint8_t(x == 0), s.xer_so};
    }
    void Coordinates() {
        auto &r = s.r;
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            native.SetHostFpControl(s.cached_fp_control);
        }
        Load(0, r[11]);
        r[10] = (r[4] << 5u) & 0xffffffe0u;
        Load(13, r[11] + 12u);
        Single(13, F(13) + F(0));
        Load(10, r[11] + 16u);
        Load(0, r[11] + 20u);
        r[31] = r[10] + r[29];
        Load(12, r[11] + 4u);
        r[10] = 0xffffffff82020000ull;
        Load(11, r[11] + 8u);
        Single(12, F(10) + F(12));
        Single(11, F(0) + F(11));
        Load(0, r[10] - 1552u);
        Single(13, F(13) * F(0));
        Save(13, r[31]);
        Single(12, F(12) * F(0));
        Save(12, r[31] + 4u);
        Single(11, F(11) * F(0));
        Save(11, r[31] + 8u);
        Load(12, r[11]);
        Load(13, r[11] + 12u);
        Single(13, F(13) - F(12));
        Load(11, r[11] + 16u);
        Load(12, r[11] + 4u);
        Single(12, F(11) - F(12));
        Load(10, r[11] + 20u);
        Load(11, r[11] + 8u);
        Single(11, F(10) - F(11));
        Single(13, F(13) * F(0));
        Save(13, r[31] + 12u);
        Single(12, F(12) * F(0));
        Save(12, r[31] + 16u);
        Single(0, F(11) * F(0));
        Save(0, r[31] + 20u);
    }
    void Visit() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bdce40u;
        for (unsigned i = 26; i < 32; ++i)
            WriteU64(m, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        Store(r[1] - 8u, r[12]);
        const auto stack = r[1];
        r[1] -= 144u;
        Store(r[1], stack);
        r[11] = r[6];
        r[29] = r[3];
        r[30] = r[5];
        r[10] = Word(r[11] + 24u);
        r[6] = r[10] & 0xfffffffeu;
        CompareZero(r[6]);
        r[27] = r[6] + 40u;
        if (s.cr6.eq)
            r[27] = 0;
        Coordinates();
        r[11] = Word(r[6] + 24u);
        r[11] &= 0xfffffffeu;
        CompareZero(r[11]);
        if (!s.cr6.eq) {
            r[11] = Word(r[27] + 24u);
            r[11] &= 0xfffffffeu;
            CompareZero(r[11]);
            if (s.cr6.eq) {
                r[11] = r[6];
                r[6] = r[27];
                r[27] = r[11];
            }
        }
        r[11] = Word(r[6] + 24u);
        r[28] = 1;
        r[4] = Word(r[30]);
        r[11] &= 0xfffffffeu;
        r[26] = r[4];
        CompareZero(r[11]);
        if (s.cr6.eq) {
            r[11] = Word(r[6] + 32u);
            r[11] = Word(r[11]);
            r[11] |= 0x80000000u;
            Store(r[31] + 24u, r[11]);
        } else {
            r[10] = 0;
            r[11] = r[4] + 1u;
            r[10] |= 57005u;
            r[5] = r[30];
            r[3] = r[29];
            Store(r[30], r[11]);
            Store(r[31] + 24u, r[10]);
            s.lr = 0x82bdcf6cu;
            Visit();
        }
        r[11] = Word(r[27] + 24u);
        r[11] &= 0xfffffffeu;
        CompareZero(r[11]);
        if (s.cr6.eq) {
            r[11] = Word(r[31] + 24u);
            r[28] = 0;
            r[11] |= 0x40000000u;
            Store(r[31] + 24u, r[11]);
        } else {
            r[4] = Word(r[30]);
            r[6] = r[27];
            r[5] = r[30];
            r[11] = r[4] + 1u;
            r[3] = r[29];
            Store(r[30], r[11]);
            s.lr = 0x82bdcfacu;
            Visit();
        }
        r[11] = Address(r[28]) & 255u;
        CompareZero(r[11]);
        if (!s.cr6.eq) {
            r[11] = Word(r[30]);
            r[11] -= r[26];
            Store(r[31] + 28u, r[11]);
        } else {
            r[11] = 0;
            Store(r[31] + 28u, r[11]);
        }
        r[1] += 144u;
        for (unsigned i = 26; i < 32; ++i)
            r[i] = ReadU64(m, Address(r[1] - 16u - 8u * (31u - i)));
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &m, float_triplet_transfer::NativeServices &native,
           Registers &s) {
    if (entry != 0x82bdce38u)
        return false;
    Flatten{m, native, s}.Visit();
    return true;
}
} // namespace lo::semantic::gpu::tree_compact_flatten61
