#include "lo_semantics/mesh_convex_check61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_convex_check61 {
namespace {
using recovery_abi::Address;
struct Check {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &fp;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(v)));
    }
    void Load(unsigned i, std::uint64_t p) {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Integer(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Compare(double a, double b) {
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
    }
    // Near-zero-area faces collapse their shortest edge, rewrite every index,
    // and remove repeated-index faces by swapping from the array tail.
    bool CollapseBody() {
        auto &r = s.r;
        r[27] = r[3];
        r[28] = r[4];
        r[31] = r[5];
        r[26] = r[6];
        r[4] = Word(r[27]);
        Integer(r[4]);
        if (s.cr6.eq)
            return false;
        Integer(r[28]);
        if (s.cr6.eq)
            return false;
        Integer(r[31]);
        if (s.cr6.eq)
            return false;
        r[11] = 0xffffffff82210000ull;
        Load(31, r[11] + 22344);
        for (;;) {
            r[3] = 0;
            r[5] = 0;
            Integer(r[4]);
            if (s.cr6.eq) {
                r[11] = Address(r[3]) & 255u;
                Integer(r[11]);
                return true;
            }
            r[30] = r[28] + 8;
            r[8] = r[30];
            bool small = false;
            do {
                r[11] = Word(r[8] - 4);
                r[10] = Word(r[8] - 8);
                r[7] = Address(r[11]) << 1;
                r[9] = Word(r[8]);
                r[6] = Address(r[10]) << 1;
                r[11] += r[7];
                r[7] = Address(r[9]) << 1;
                r[10] += r[6];
                r[9] += r[7];
                r[11] = Address(r[11]) << 2;
                r[10] = Address(r[10]) << 2;
                r[9] = Address(r[9]) << 2;
                r[11] += r[31];
                r[10] += r[31];
                r[9] += r[31];
                Load(0, r[11]);
                Load(11, r[10]);
                Load(12, r[11] + 8);
                Single(11, F(11) - F(0));
                Load(8, r[9]);
                Load(9, r[10] + 8);
                Single(0, F(8) - F(0));
                Single(9, F(9) - F(12));
                Load(13, r[11] + 4);
                Load(6, r[9] + 8);
                Load(10, r[10] + 4);
                Single(12, F(6) - F(12));
                Load(7, r[9] + 4);
                Single(10, F(10) - F(13));
                Single(13, F(7) - F(13));
                Single(8, F(9) * F(0));
                Single(6, F(12) * F(10));
                Single(7, F(13) * F(11));
                Single(12, F(12) * F(11) - F(8));
                Single(13, F(13) * F(9) - F(6));
                Single(0, F(0) * F(10) - F(7));
                Single(12, F(12) * F(12));
                Single(0, F(0) * F(0) + F(12));
                Single(0, F(13) * F(13) + F(0));
                Single(0, std::sqrt(F(0)));
                Compare(F(0), F(31));
                if (s.cr6.lt) {
                    small = true;
                    break;
                }
                r[11] = Word(r[27]);
                ++r[5];
                r[8] += 12;
                Integer(r[5], r[11]);
            } while (s.cr6.lt);
            if (!small) {
                r[11] = Address(r[3]) & 255u;
                Integer(r[11]);
                return true;
            }
            r[11] = Address(r[26]) & 255u;
            Integer(r[11]);
            if (s.cr6.eq)
                return false;
            r[11] = Address(r[5]) << 1;
            r[4] = 0;
            r[11] += r[5];
            r[11] = Address(r[11]) << 2;
            r[11] += r[28];
            r[10] = Word(r[11]);
            r[9] = Word(r[11] + 4);
            r[7] = Address(r[10]) << 1;
            r[8] = Word(r[11] + 8);
            r[6] = Address(r[9]) << 1;
            r[10] += r[7];
            r[7] = Address(r[8]) << 1;
            r[9] += r[6];
            r[8] += r[7];
            r[10] = Address(r[10]) << 2;
            r[9] = Address(r[9]) << 2;
            r[8] = Address(r[8]) << 2;
            r[10] += r[31];
            r[9] += r[31];
            r[8] += r[31];
            Load(0, r[10] + 4);
            Load(11, r[9] + 4);
            Load(9, r[8] + 4);
            Single(5, F(0) - F(11));
            Single(0, F(0) - F(9));
            Load(13, r[10] + 8);
            Load(10, r[9] + 8);
            Single(11, F(11) - F(9));
            Load(8, r[8] + 8);
            Single(4, F(13) - F(10));
            Single(13, F(13) - F(8));
            Load(12, r[10]);
            Single(10, F(10) - F(8));
            Load(7, r[9]);
            Load(6, r[8]);
            Single(3, F(12) - F(7));
            Single(12, F(12) - F(6));
            Single(9, F(7) - F(6));
            Single(8, F(5) * F(5));
            Single(0, F(0) * F(0));
            Single(11, F(11) * F(11));
            Single(0, F(13) * F(13) + F(0));
            Single(13, F(4) * F(4) + F(8));
            Single(11, F(10) * F(10) + F(11));
            Single(0, F(12) * F(12) + F(0));
            Single(12, F(3) * F(3) + F(13));
            Single(11, F(9) * F(9) + F(11));
            Single(13, std::sqrt(F(0)));
            Single(0, std::sqrt(F(12)));
            Single(12, std::sqrt(F(11)));
            Compare(F(13), F(0));
            if (s.cr6.lt) {
                s.fpr_bits[0] = s.fpr_bits[13];
                r[4] = 1;
            }
            Compare(F(12), F(0));
            if (s.cr6.lt) {
                r[9] = Word(r[11] + 4);
                r[10] = Word(r[11] + 8);
            } else {
                Integer(r[4]);
                if (s.cr6.eq) {
                    r[10] = Word(r[11] + 4);
                    r[9] = Word(r[11]);
                } else {
                    Integer(r[4], 1);
                    r[9] = Word(r[11] + (s.cr6.eq ? 0 : 4));
                    r[10] = Word(r[11] + 8);
                }
            }
            r[8] = 0;
            r[11] = r[30];
            do {
                for (int offset : {-8, -4, 0}) {
                    r[7] = Word(r[11] + offset);
                    Integer(r[7], r[10]);
                    if (s.cr6.eq)
                        Word(r[11] + offset, r[9]);
                }
                r[7] = Word(r[27]);
                ++r[8];
                r[11] += 12;
                Integer(r[8], r[7]);
            } while (s.cr6.lt);
            r[11] = Address(r[7]);
            Integer(r[11]);
            if (!s.cr6.eq) {
                r[30] = r[28];
                r[29] = r[11];
                do {
                    r[3] = r[30];
                    s.lr = 0x82bb8b44u;
                    Repeated();
                    r[11] = Address(r[3]) & 255u;
                    Integer(r[11]);
                    if (!s.cr6.eq) {
                        r[11] = Word(r[27]);
                        --r[11];
                        r[10] = Address(r[11]) << 1;
                        r[10] += r[11];
                        Word(r[27], r[11]);
                        r[10] = Address(r[10]) << 2;
                        r[10] += r[28];
                        for (unsigned offset : {0u, 4u, 8u}) {
                            r[11] = Word(r[10] + offset);
                            Word(r[30] + offset, r[11]);
                        }
                    } else
                        r[30] += 12;
                    --r[29];
                    Integer(r[29]);
                } while (!s.cr6.eq);
            }
            r[4] = Word(r[27]);
            Integer(r[4], 4);
            if (!s.cr6.gt)
                return false;
            r[3] = 1;
            r[11] = Address(r[3]) & 255u;
            Integer(r[11]);
        }
    }
    void Collapse() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bb88b0u;
        for (unsigned i = 26; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            fp.SetHostFpControl(s.cached_fp_control);
        }
        recovery_abi::WriteU64(m, Address(r[1] - 64), s.fpr_bits[31]);
        auto old = r[1];
        r[1] -= 144;
        Word(r[1], old);
        r[3] = CollapseBody() ? 1 : 0;
        r[1] += 144;
        s.fpr_bits[31] = recovery_abi::ReadU64(m, Address(r[1] - 64));
        for (unsigned i = 26; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Reverse() {
        auto &r = s.r;
        r[10] = Word(r[3] + 4);
        r[11] = Word(r[3] + 8);
        r[11] ^= r[10];
        Word(r[3] + 4, r[11]);
        r[10] = Word(r[3] + 8);
        r[11] ^= r[10];
        Word(r[3] + 8, r[11]);
        r[11] = Word(r[3] + 4);
        r[10] = Word(r[3] + 8);
        r[11] ^= r[10];
        Word(r[3] + 4, r[11]);
    }
    void Repeated() {
        auto &r = s.r;
        r[9] = Word(r[3]);
        r[11] = Word(r[3] + 4);
        Integer(r[9], r[11]);
        if (s.cr6.eq) {
            r[3] = 1;
            return;
        }
        r[10] = Word(r[3] + 8);
        Integer(r[11], r[10]);
        if (s.cr6.eq) {
            r[3] = 1;
            return;
        }
        r[11] = r[9] - r[10];
        r[11] = std::countl_zero(Address(r[11]));
        r[3] = (r[11] >> 5) & 1u;
    }
    void Side() {
        auto &r = s.r;
        Integer(r[4]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[10] = Word(r[3] + 4);
        Load(8, r[5]);
        r[11] = Word(r[3]);
        Load(4, r[5] + 8);
        r[8] = Address(r[10]) << 1;
        r[9] = Word(r[3] + 8);
        r[7] = Address(r[11]) << 1;
        Load(5, r[5] + 4);
        r[10] += r[8];
        r[8] = Address(r[9]) << 1;
        r[11] += r[7];
        r[9] += r[8];
        r[11] = Address(r[11]) << 2;
        r[10] = Address(r[10]) << 2;
        r[9] = Address(r[9]) << 2;
        r[11] += r[4];
        r[10] += r[4];
        r[9] += r[4];
        Load(0, r[11]);
        Load(13, r[10]);
        Single(6, F(8) - F(0));
        Load(7, r[9]);
        Single(0, F(0) - F(13));
        Load(10, r[11] + 8);
        Single(13, F(7) - F(13));
        Load(9, r[10] + 8);
        Single(7, F(10) - F(9));
        Load(12, r[11] + 4);
        Load(11, r[10] + 4);
        Single(10, F(4) - F(10));
        Load(3, r[9] + 4);
        Single(8, F(12) - F(11));
        Load(2, r[9] + 8);
        Single(11, F(3) - F(11));
        Single(9, F(2) - F(9));
        r[11] = 0xffffffff82000000ull;
        Single(12, F(5) - F(12));
        Single(4, F(13) * F(7));
        Single(5, F(0) * F(11));
        Single(3, F(9) * F(8));
        Single(0, F(0) * F(9) - F(4));
        Single(13, F(13) * F(8) - F(5));
        Single(11, F(11) * F(7) - F(3));
        Single(0, F(12) * F(0));
        Single(0, F(10) * F(13) + F(0));
        Single(13, F(6) * F(11) + F(0));
        Load(0, r[11] + 3664);
        r[11] = 1;
        Compare(F(13), F(0));
        if (s.cr6.lt)
            r[11] = 0;
        r[3] = r[11] & 255;
    }
    void Store(unsigned i, std::uint64_t p) { Word(p, std::bit_cast<std::uint32_t>(float(F(i)))); }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void OrientBody() {
        auto &r = s.r;
        r[28] = r[4];
        r[27] = r[7];
        for (unsigned reg : {3u, 28u, 5u, 6u}) {
            Integer(r[reg]);
            if (s.cr6.eq) {
                r[3] = 0;
                return;
            }
        }
        r[9] = Address(r[3]);
        r[11] = 0xffffffff82000000ull;
        r[10] = 0;
        auto n = std::bit_cast<std::int32_t>(Address(r[3]));
        s.cr6 = {std::uint8_t(n < 4), std::uint8_t(n > 4), std::uint8_t(n == 4), s.xer_so};
        recovery_abi::WriteU64(m, Address(r[1] + 80), r[9]);
        Load(13, r[11] + 3664);
        r[11] = 0xffffffff82000000ull;
        s.fpr_bits[12] = s.fpr_bits[13];
        Store(13, r[1] + 88);
        s.fpr_bits[11] = s.fpr_bits[13];
        Store(12, r[1] + 92);
        Store(11, r[1] + 96);
        s.fpr_bits[0] = recovery_abi::ReadU64(m, Address(r[1] + 80));
        s.fpr_bits[0] =
            std::bit_cast<std::uint64_t>(double(std::bit_cast<std::int64_t>(s.fpr_bits[0])));
        Single(10, F(0));
        Load(0, r[11] + 30596);
        Single(0, F(0) / F(10));
        // Average packed vertices, retaining the original four-wide accumulation.
        if (!s.cr6.lt) {
            r[10] = r[3] - 4;
            r[11] = r[28] + 8;
            r[10] = Address(r[10]) >> 2;
            r[9] = r[10] + 1;
            r[10] = Address(r[9]) << 2;
            do {
                Load(10, r[11] - 8);
                --r[9];
                Load(9, r[11] - 4);
                Single(13, F(10) * F(0) + F(13));
                Load(8, r[11]);
                Single(12, F(9) * F(0) + F(12));
                Single(11, F(8) * F(0) + F(11));
                Load(7, r[11] + 4);
                Load(6, r[11] + 8);
                Integer(r[9]);
                Load(5, r[11] + 12);
                Load(4, r[11] + 16);
                Load(3, r[11] + 20);
                Load(2, r[11] + 24);
                Load(1, r[11] + 28);
                Load(31, r[11] + 32);
                Single(13, F(7) * F(0) + F(13));
                Load(10, r[11] + 36);
                Single(12, F(6) * F(0) + F(12));
                r[11] += 48;
                Single(11, F(5) * F(0) + F(11));
                Single(13, F(4) * F(0) + F(13));
                Single(12, F(3) * F(0) + F(12));
                Single(11, F(2) * F(0) + F(11));
                Single(13, F(1) * F(0) + F(13));
                Single(12, F(31) * F(0) + F(12));
                Single(11, F(10) * F(0) + F(11));
            } while (!s.cr6.eq);
            Store(11, r[1] + 96);
            Store(12, r[1] + 92);
            Store(13, r[1] + 88);
        }
        Integer(r[10], r[3]);
        if (s.cr6.lt) {
            r[11] = Address(r[10]) << 1;
            r[9] = r[3] - r[10];
            r[11] += r[10];
            r[11] = Address(r[11]) << 2;
            r[11] += r[28] + 8;
            do {
                --r[9];
                Load(10, r[11] - 8);
                Load(9, r[11] - 4);
                Single(13, F(10) * F(0) + F(13));
                Load(8, r[11]);
                Single(12, F(9) * F(0) + F(12));
                r[11] += 12;
                Single(11, F(0) * F(8) + F(11));
                Integer(r[9]);
            } while (!s.cr6.eq);
            Store(11, r[1] + 96);
            Store(12, r[1] + 92);
            Store(13, r[1] + 88);
        }
        r[29] = 1;
        Integer(r[5]);
        if (!s.cr6.eq) {
            r[31] = r[6];
            r[30] = r[5];
            do {
                r[5] = r[1] + 88;
                r[4] = r[28];
                r[3] = r[31];
                s.lr = 0x82bb8850u;
                Side();
                r[11] = r[3] & 255;
                Integer(r[11]);
                if (!s.cr6.eq) {
                    r[11] = r[27] & 255;
                    Integer(r[11]);
                    if (!s.cr6.eq) {
                        r[3] = r[31];
                        s.lr = 0x82bb8870u;
                        Reverse();
                    }
                    r[29] = 0;
                }
                --r[30];
                r[31] += 12;
                Integer(r[30]);
            } while (!s.cr6.eq);
        }
        r[3] = r[29];
    }
    void Orient() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bb86d0u;
        for (unsigned i = 27; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        Gradual();
        recovery_abi::WriteU64(m, Address(r[1] - 56), s.fpr_bits[31]);
        auto old = r[1];
        r[1] -= 160;
        Word(r[1], old);
        OrientBody();
        r[1] += 160;
        Gradual();
        s.fpr_bits[31] = recovery_abi::ReadU64(m, Address(r[1] - 56));
        for (unsigned i = 27; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    bool ConvexBody() {
        auto &r = s.r;
        r[11] = Word(r[3] + 4);
        r[28] = Word(r[11] + 16);
        Integer(r[28]);
        if (s.cr6.eq)
            return false;
        r[10] = Word(r[11] + 8);
        Integer(r[10]);
        if (s.cr6.eq)
            return false;
        r[10] = Word(r[11] + 40);
        Integer(r[10]);
        if (s.cr6.eq)
            return false;
        r[29] = Word(r[11] + 36);
        r[30] = 0;
        Integer(r[29]);
        if (s.cr6.eq)
            return true;
        r[31] = Word(r[11] + 12);
        r[11] = 0xffffffff82000000ull;
        r[4] = r[10];
        r[3] = r[10] + 12;
        Load(0, r[11] + 2904);
        do {
            r[8] = 0;
            Integer(r[31]);
            if (!s.cr6.eq) {
                r[5] = m.ReadU16(Address(r[4]));
                r[7] = r[28];
                do {
                    r[6] = 0;
                    r[11] = 0;
                    Integer(r[5]);
                    if (!s.cr6.eq) {
                        r[10] = Word(r[4] + 4);
                        r[9] = r[8] & 255;
                        do {
                            r[27] = m.ReadU8(Address(r[10] + r[11]));
                            Integer(r[27], r[9]);
                            if (s.cr6.eq) {
                                r[6] = 1;
                                break;
                            }
                            ++r[11];
                            Integer(r[11], r[5]);
                        } while (s.cr6.lt);
                    }
                    r[11] = r[6] & 255;
                    Integer(r[11]);
                    if (s.cr6.eq) {
                        Load(12, r[7] + 4);
                        Load(13, r[3] + 4);
                        Single(13, F(13) * F(12));
                        Load(11, r[7] + 8);
                        Load(12, r[3] + 8);
                        Load(10, r[7]);
                        Load(9, r[3]);
                        Load(8, r[3] + 12);
                        Single(13, F(12) * F(11) + F(13));
                        Single(13, F(10) * F(9) + F(13));
                        Single(13, F(13) + F(8));
                        Compare(F(13), F(0));
                        if (s.cr6.gt)
                            return false;
                    }
                    ++r[8];
                    r[7] += 12;
                    Integer(r[8], r[31]);
                } while (s.cr6.lt);
            }
            ++r[30];
            r[4] += 36;
            r[3] += 36;
            Integer(r[30], r[29]);
        } while (s.cr6.lt);
        return true;
    }
    void Convex() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bb8e90u;
        for (unsigned i = 27; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        r[3] = ConvexBody() ? 1 : 0;
        for (unsigned i = 27; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, float_triplet_transfer::NativeServices &fp,
           Registers &s) {
    Check x{m, fp, s};
    switch (e) {
    case 0x82bb88a8u:
        x.Collapse();
        break;
    case 0x82bb86c8u:
        x.Orient();
        break;
    case 0x82bd8fa8u:
        x.Reverse();
        break;
    case 0x82bd91e0u:
        x.Repeated();
        break;
    case 0x82bd90a0u:
        x.Side();
        break;
    case 0x82bb8e88u:
        x.Convex();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_convex_check61
