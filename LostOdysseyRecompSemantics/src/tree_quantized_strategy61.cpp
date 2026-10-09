#include "lo_semantics/tree_quantized_strategy61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/tree_compact_flatten61.h"
#include "lo_semantics/tree_flatten36_61.h"
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::tree_quantized_strategy61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Quantize {
    GuestMemory &m;
    Dependencies deps;
    Registers &s;
    bool compact;
    std::uint32_t Word(std::uint64_t a) { return m.ReadU32(Address(a)); }
    void Store(std::uint64_t a, std::uint64_t v) { m.WriteU32(Address(a), Address(v)); }
    double F(unsigned f) { return std::bit_cast<double>(s.fpr_bits[f]); }
    void F(unsigned f, double v) { s.fpr_bits[f] = std::bit_cast<std::uint64_t>(v); }
    void Single(unsigned f, double v) { F(f, static_cast<float>(v)); }
    void Flush() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            deps.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Load(unsigned f, std::uint64_t a) {
        Flush();
        F(f, std::bit_cast<float>(Word(a)));
    }
    void Save(unsigned f, std::uint64_t a) {
        Store(a, std::bit_cast<std::uint32_t>(static_cast<float>(F(f))));
    }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void CompareFp(double a, double b) {
        const bool unordered = std::isnan(a) || std::isnan(b);
        s.cr6 = {std::uint8_t(!unordered && a < b), std::uint8_t(!unordered && a > b),
                 std::uint8_t(!unordered && a == b), std::uint8_t(unordered)};
    }
    void Allocator(GuestAddress lr) {
        s.lr = lr;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m, deps.guest, s);
    }
    void Indirect(GuestAddress lr) {
        s.ctr = s.r[11];
        s.lr = lr;
        deps.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    template <class T> void Truncate() {
        const auto value = F(0);
        constexpr auto maximum = std::numeric_limits<T>::max();
        constexpr auto minimum = std::numeric_limits<T>::min();
        if (value > double(maximum))
            s.fpr_bits[0] = std::uint64_t(std::int64_t(maximum));
        else if (!std::isfinite(value) || std::trunc(value) < double(minimum) ||
                 value >= std::ldexp(1., sizeof(T) * 8u - 1u)) {
            std::feraiseexcept(FE_INVALID);
            s.fpr_bits[0] = std::uint64_t(std::int64_t(minimum));
        } else
            s.fpr_bits[0] = std::uint64_t(std::int64_t(static_cast<T>(value)));
    }
    bool Storage() {
        auto &r = s.r;
        if (compact) {
            r[11] = Word(r[26] + 4u);
            r[9] = Word(r[26] + 16u);
            r[11] = Word(r[11] + 36u);
            r[10] = (r[11] << 1u) & 0xfffffffeu;
            --r[10];
            Compare(r[9], r[10]);
            if (!s.cr6.eq)
                return false;
            --r[11];
            r[30] = Word(r[31] + 8u);
            Compare(r[30]);
            Store(r[31] + 4u, r[11]);
        } else {
            r[10] = Word(r[26] + 4u);
            r[11] = Word(r[26] + 16u);
            r[10] = Word(r[10] + 36u);
            r[10] = (r[10] << 1u) & 0xfffffffeu;
            --r[10];
            Compare(r[11], r[10]);
            if (!s.cr6.eq)
                return false;
            r[30] = Word(r[31] + 8u);
            Store(r[31] + 4u, r[11]);
            Compare(r[30]);
        }
        if (!s.cr6.eq) {
            Allocator((compact ? 0x82bdd244u : 0x82bdc260u));
            r[11] = Word(r[3]);
            r[4] = r[30] - 4u;
            r[11] = Word(r[11] + 12u);
            Indirect((compact ? 0x82bdd258u : 0x82bdc274u));
            r[11] = 0;
            Store(r[31] + 8u, r[11]);
        }
        if (compact) {
            r[11] = 134152192u;
            r[28] = Word(r[31] + 4u);
            r[29] = ~std::uint64_t(0);
            r[11] |= 65535u;
            r[27] = std::uint64_t(-5);
            Compare(r[28], r[11]);
            if (!s.cr6.gt) {
                r[11] = (r[28] << 5u) & 0xffffffe0u;
                Compare(r[11], r[27]);
                r[30] = r[11] + 4u;
                if (s.cr6.gt)
                    r[30] = r[29];
            } else
                r[30] = r[29];
        } else {
            r[11] = 119275520u;
            r[30] = Word(r[31] + 4u);
            r[29] = ~std::uint64_t(0);
            r[11] |= 29127u;
            r[27] = std::uint64_t(-5);
            Compare(r[30], r[11]);
            if (!s.cr6.gt) {
                r[11] = (r[30] << 3u) & 0xfffffff8u;
                r[11] += r[30];
                r[11] = (r[11] << 2u) & 0xfffffffcu;
                Compare(r[11], r[27]);
                r[28] = r[11] + 4u;
                if (s.cr6.gt)
                    r[28] = r[29];
            } else
                r[28] = r[29];
        }
        Allocator((compact ? 0x82bdd294u : 0x82bdc2b8u));
        r[11] = Word(r[3]);
        r[5] = compact ? 31u : 30u;
        r[4] = compact ? r[30] : r[28];
        r[11] = Word(r[11]);
        Indirect((compact ? 0x82bdd2acu : 0x82bdc2d0u));
        Compare(r[3]);
        if (s.cr6.eq)
            return false;
        r[22] = r[3] + 4u;
        Store(r[3], compact ? r[28] : r[30]);
        Compare(r[22]);
        if (s.cr6.eq)
            return false;
        r[23] = 1;
        r[6] = Word(r[26] + 4u);
        r[5] = r[1] + 80u;
        r[4] = 0;
        r[3] = r[22];
        Store(r[1] + 80u, r[23]);
        s.lr = (compact ? 0x82bdd2e0u : 0x82bdc304u);
        if (compact)
            (void)tree_compact_flatten61::Apply(0x82bdce38u, m, deps.fp, s);
        else
            (void)tree_flatten36_61::Apply(0x82bdbc18u, m, deps.fp, s);
        r[11] = compact ? 214695936u : 178913280u;
        r[30] = Word(r[31] + 4u);
        r[11] |= compact ? 52428u : 43690u;
        Compare(r[30], r[11]);
        if (!s.cr6.gt) {
            r[11] = compact ? ((r[30] << 2u) & 0xfffffffcu) : ((r[30] << 1u) & 0xfffffffeu);
            r[11] += r[30];
            r[11] = compact ? ((r[11] << 2u) & 0xfffffffcu) : ((r[11] << 3u) & 0xfffffff8u);
            Compare(r[11], r[27]);
            if (!s.cr6.gt)
                r[29] = r[11] + 4u;
        }
        Allocator((compact ? 0x82bdd310u : 0x82bdc334u));
        r[11] = Word(r[3]);
        r[5] = compact ? 38u : 32u;
        r[4] = r[29];
        r[11] = Word(r[11]);
        Indirect((compact ? 0x82bdd328u : 0x82bdc34cu));
        Compare(r[3]);
        if (!s.cr6.eq) {
            r[11] = r[3] + 4u;
            Store(r[3], r[30]);
        } else
            r[11] = 0;
        Compare(r[11]);
        Store(r[31] + 8u, r[11]);
        return !s.cr6.eq;
    }
    void Scales() {
        auto &r = s.r;
        r[11] = 0xffffffff82000000ull;
        r[9] = Word(r[31] + 4u);
        Compare(r[9]);
        Load(13, r[11] + 3428u);
        for (unsigned f : {11u, 10u, 9u, 8u, 7u})
            s.fpr_bits[f] = s.fpr_bits[13];
        if (!s.cr6.eq) {
            r[10] = Address(r[9]);
            r[11] = r[22] + 8u;
            do {
                constexpr unsigned maxima[]{13, 11, 10, 9, 8, 7};
                for (unsigned axis = 0; axis < 6; ++axis) {
                    Load(0, r[11] - 8u + 4u * axis);
                    F(12, std::abs(F(0)));
                    CompareFp(F(12), F(maxima[axis]));
                    if (s.cr6.gt)
                        F(maxima[axis], std::abs(F(0)));
                }
                --r[10];
                r[11] += compact ? 32u : 36u;
                Compare(r[10]);
            } while (!s.cr6.eq);
        }
        r[11] = 0xffffffff83210000ull;
        r[24] = m.ReadU8(Address(r[11] + 26224u));
        r[11] = 15;
        Compare(r[24]);
        if (s.cr6.eq)
            r[11] = 16;
        r[10] = 0xffffffff82000000ull;
        Load(0, r[10] + 3664u);
        r[10] = 0xffffffff82220000ull;
        CompareFp(F(13), F(0));
        Load(12, r[10] - 31156u);
        if (!s.cr6.eq)
            Single(2, F(12) / F(13));
        else
            s.fpr_bits[2] = s.fpr_bits[0];
        for (unsigned i = 0; i < 2; ++i) {
            const auto src = 11u - i, dst = 3u + i;
            CompareFp(F(src), F(0));
            if (!s.cr6.eq)
                Single(dst, F(12) / F(src));
            else
                s.fpr_bits[dst] = s.fpr_bits[0];
        }
        constexpr unsigned maxima[]{9, 8, 7}, scale[]{5, 6, 8};
        for (unsigned i = 0; i < 3; ++i) {
            CompareFp(F(maxima[i]), F(0));
            if (s.cr6.eq)
                s.fpr_bits[scale[i]] = s.fpr_bits[0];
            else {
                const auto reg = i == 2 ? 11u : 10u;
                r[reg] = (Address(r[23]) << (r[11] & 31u)) - 1u;
                r[reg] = std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[reg]))));
                WriteU64(m, Address(r[1] + 88u), r[reg]);
                Single(13, double(std::bit_cast<std::int64_t>(ReadU64(m, Address(r[1] + 88u)))));
                Single(scale[i], F(13) / F(maxima[i]));
            }
        }
        r[11] = 0xffffffff82000000ull;
        CompareFp(F(2), F(0));
        Load(12, r[11] + 30596u);
        if (!s.cr6.eq)
            Single(13, F(12) / F(2));
        else
            s.fpr_bits[13] = s.fpr_bits[0];
        Save(13, r[31] + 12u);
        for (unsigned f : {3u, 4u, 5u}) {
            CompareFp(F(f), F(0));
            if (!s.cr6.eq)
                Single(13, F(12) / F(f));
            else
                s.fpr_bits[13] = s.fpr_bits[0];
            if (f != 5u)
                Save(13, r[31] + 12u + 4u * (f - 2u));
        }
        r[25] = r[31] + 24u;
        CompareFp(F(6), F(0));
        Save(13, r[25]);
        if (!s.cr6.eq)
            Single(13, F(12) / F(6));
        else
            s.fpr_bits[13] = s.fpr_bits[0];
        Save(13, r[31] + 28u);
        CompareFp(F(8), F(0));
        if (!s.cr6.eq)
            Single(0, F(12) / F(8));
        Save(0, r[31] + 32u);
    }
    void Enlarge() {
        auto &r = s.r;
        Load(0, r[9] - 8u);
        r[4] = r[5];
        Load(13, r[9] - 20u);
        r[3] = r[1] + 112u;
        Single(7, F(13) + F(0));
        Load(12, r[9] - 4u);
        Load(11, r[9] - 16u);
        Single(0, F(13) - F(0));
        Save(0, r[1] + 128u);
        Single(0, F(11) + F(12));
        Save(0, r[1] + 116u);
        Single(0, F(11) - F(12));
        Load(10, r[9]);
        r[7] = r[25];
        Load(9, r[9] - 12u);
        s.xer_ca = Address(r[31]) <= 0xffffffe8u;
        r[29] = std::uint64_t(-24) - r[31];
        Save(0, r[1] + 132u);
        Single(0, F(9) + F(10));
        Save(0, r[1] + 120u);
        Single(0, F(9) - F(10));
        Save(7, r[1] + 112u);
        r[28] = 3;
        Save(0, r[1] + 136u);
        do {
            r[10] = Word(r[31] + 8u);
            Load(0, r[7] - 12u);
            Load(12, r[3]);
            r[6] = r[23];
            r[30] = r[29] + r[7];
            r[11] = r[4] + 6u;
            r[10] = m.ReadU16(Address(r[10] + r[4]));
            r[10] = std::uint64_t(std::int64_t(std::bit_cast<std::int16_t>(std::uint16_t(r[10]))));
            WriteU64(m, Address(r[1] + 96u), r[10]);
            Single(13, double(std::bit_cast<std::int64_t>(ReadU64(m, Address(r[1] + 96u)))));
            Single(13, F(13) * F(0));
            do {
                r[10] = Word(r[31] + 8u);
                Load(0, r[7]);
                r[8] = m.ReadU16(Address(r[10] + r[11]));
                r[8] = std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[8]))));
                WriteU64(m, Address(r[1] + 104u), r[8]);
                Single(11, double(std::bit_cast<std::int64_t>(ReadU64(m, Address(r[1] + 104u)))));
                Single(0, F(11) * F(0));
                Single(11, F(0) + F(13));
                CompareFp(F(11), F(12));
                bool grow = s.cr6.lt;
                if (!grow) {
                    Single(0, F(13) - F(0));
                    r[8] = r[1] + 128u;
                    Load(11, r[30] + r[8]);
                    CompareFp(F(0), F(11));
                    grow = s.cr6.gt;
                }
                if (grow) {
                    r[8] = m.ReadU16(Address(r[10] + r[11]));
                    ++r[8];
                    m.WriteU16(Address(r[10] + r[11]), std::uint16_t(r[8]));
                } else
                    r[6] = 0;
                r[10] = Word(r[31] + 8u);
                r[8] = m.ReadU16(Address(r[10] + r[11]));
                Compare(r[8]);
                if (s.cr6.eq) {
                    r[6] = 0;
                    m.WriteU16(Address(r[10] + r[11]), std::uint16_t(r[27]));
                }
                r[10] = Address(r[6]) & 255u;
                Compare(r[10]);
            } while (!s.cr6.eq);
            --r[28];
            r[4] += 2u;
            r[7] += 4u;
            r[3] += 4u;
            Compare(r[28]);
        } while (!s.cr6.eq);
    }
    void Pack() {
        auto &r = s.r;
        r[26] = 0;
        Compare(r[9]);
        if (s.cr6.eq)
            return;
        r[11] = 0;
        r[5] = 0;
        r[9] = r[22] + 20u;
        r[27] = r[11] | 65535u;
        do {
            constexpr unsigned factors[]{2, 3, 4, 5, 6, 8};
            for (unsigned i = 0; i < 6; ++i) {
                Load(0, r[9] - 20u + 4u * i);
                if (i == 0)
                    r[11] = Word(r[31] + 8u);
                Single(0, (compact ? (i == 2 || i == 4) : (i == 3 || i == 5))
                              ? F(factors[i]) * F(0)
                              : F(0) * F(factors[i]));
                if (i == 0)
                    Compare(r[24]);
                else {
                    r[11] = Word(r[31] + 8u);
                    r[11] += r[5];
                }
                if (i < 3)
                    Truncate<std::int32_t>();
                else
                    Truncate<std::int64_t>();
                WriteU64(m, Address(r[1] + 88u), s.fpr_bits[0]);
                r[10] = m.ReadU16(Address(r[1] + 94u));
                m.WriteU16(Address(r[11] + (i == 0 ? r[5] : 2u * i)), std::uint16_t(r[10]));
            }
            if (!s.cr6.eq)
                Enlarge();
            r[11] = Word(r[31] + 8u);
            ++r[26];
            r[10] = Word(r[9] + 4u);
            r[11] += r[5];
            Store(r[11] + 12u, r[10]);
            r[11] = Word(r[31] + 8u);
            r[10] = Word(r[9] + 8u);
            if (compact) {
                r[9] += 32u;
                r[11] += r[5];
                r[5] += 20u;
                Store(r[11] + 16u, r[10]);
            } else {
                r[11] += r[5];
                Store(r[11] + 16u, r[10]);
                r[11] = Word(r[31] + 8u);
                r[10] = Word(r[9] + 12u);
                r[9] += 36u;
                r[11] += r[5];
                r[5] += 24u;
                Store(r[11] + 20u, r[10]);
            }
            r[11] = Word(r[31] + 4u);
            Compare(r[26], r[11]);
        } while (s.cr6.lt);
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = (compact ? 0x82bdd1f0u : 0x82bdc210u);
        for (unsigned i = 22; i < 32; ++i)
            WriteU64(m, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        Store(r[1] - 8u, r[12]);
        const auto stack = r[1];
        r[1] -= 240u;
        Store(r[1], stack);
        r[26] = r[4];
        r[31] = r[3];
        Compare(r[26]);
        bool success = false;
        if (!s.cr6.eq && Storage()) {
            Scales();
            Pack();
            Allocator((compact ? 0x82bdd7ccu : 0x82bdc800u));
            r[11] = Word(r[3]);
            r[4] = r[22] - 4u;
            r[11] = Word(r[11] + 12u);
            Indirect((compact ? 0x82bdd7e0u : 0x82bdc814u));
            success = true;
        }
        r[3] = success ? 1u : 0u;
        r[1] += 240u;
        for (unsigned i = 22; i < 32; ++i)
            r[i] = ReadU64(m, Address(r[1] - 16u - 8u * (31u - i)));
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &memory, Dependencies deps, Registers &state) {
    if (entry != 0x82bdc208u && entry != 0x82bdd1e8u)
        return false;
    Quantize{memory, deps, state, entry == 0x82bdd1e8u}.Run();
    return true;
}
} // namespace lo::semantic::gpu::tree_quantized_strategy61
