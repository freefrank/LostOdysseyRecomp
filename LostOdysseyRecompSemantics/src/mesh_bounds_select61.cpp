#include "lo_semantics/mesh_bounds_select61.h"
#include "lo_semantics/mesh_bounds_math61.h"
#include "lo_semantics/mesh_mass_cache61.h"
#include "lo_semantics/power_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_bounds_select61 {
namespace {
using recovery_abi::Address;
struct Select {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Move(unsigned i, unsigned j) { s.fpr_bits[i] = s.fpr_bits[j]; }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(v)));
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Store(unsigned i, std::uint64_t p) {
        m.WriteU32(Address(p), std::bit_cast<std::uint32_t>(float(F(i))));
    }
    void Integer(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Compare(double a, double b) {
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
    }
    void Call(GuestAddress continuation) {
        s.ctr = s.r[11];
        s.lr = continuation;
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void RecursiveCandidate() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bc9af8u;
        for (unsigned i = 26; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        Gradual();
        recovery_abi::WriteU64(m, Address(r[1] - 64), s.fpr_bits[31]);
        auto old = r[1];
        r[1] -= 160;
        Word(r[1], old);
        r[26] = 0xffffffff832e0000ull;
        r[27] = r[3];
        r[29] = r[5];
        r[31] = r[4];
        r[5] = 256;
        r[3] = Word(r[26] - 2744);
        r[4] = Address(r[29]) << 2;
        r[11] = Word(r[3]);
        r[11] = Word(r[11] + 8);
        Call(0x82bc9b2cu);
        r[28] = r[3];
        Integer(r[29]);
        if (!s.cr6.eq) {
            r[9] = r[28];
            r[10] = r[31];
            r[11] = r[29];
            do {
                r[8] = r[10];
                --r[11];
                r[10] += 12;
                Integer(r[11]);
                Word(r[9], r[8]);
                r[9] += 4;
            } while (!s.cr6.eq);
        }
        r[11] = 0xffffffff82000000ull;
        r[31] = 0;
        Integer(r[29]);
        Load(0, r[11] + 3440);
        r[11] = 0xffffffff82000000ull;
        Load(31, r[11] + 3664);
        Move(10, 31);
        Move(9, 31);
        Move(8, 31);
        if (!s.cr6.eq) {
            r[30] = r[28];
            do {
                r[11] = Word(r[30]);
                Load(13, r[11] + 4);
                Single(13, F(13) - F(9));
                Load(12, r[11] + 8);
                Single(12, F(12) - F(8));
                Load(11, r[11]);
                Single(11, F(11) - F(10));
                Single(13, F(13) * F(13));
                Single(13, F(12) * F(12) + F(13));
                Single(13, F(11) * F(11) + F(13));
                Single(13, -(F(0) * F(0) - F(13)));
                Compare(F(13), F(31));
                if (s.cr6.gt) {
                    r[9] = r[31];
                    Integer(r[31]);
                    if (!s.cr6.eq) {
                        r[11] = r[30];
                        do {
                            r[10] = r[11] - 4;
                            r[8] = Word(r[11]);
                            --r[9];
                            Integer(r[9]);
                            r[7] = Word(r[10]);
                            Word(r[10], r[8]);
                            Word(r[11], r[7]);
                            r[11] -= 4;
                        } while (!s.cr6.eq);
                    }
                    r[6] = 1;
                    r[5] = r[31];
                    r[4] = r[28] + 4;
                    r[3] = r[1] + 80;
                    s.lr = 0x82bc9c08u;
                    (void)mesh_bounds_math61::Apply(0x82bc9928u, m, d.fp, s);
                    Load(0, r[3] + 12);
                    Load(10, r[3]);
                    Load(9, r[3] + 4);
                    Load(8, r[3] + 8);
                }
                ++r[31];
                r[30] += 4;
                Integer(r[31], r[29]);
            } while (s.cr6.lt);
        }
        Gradual();
        Store(0, r[27] + 12);
        Integer(r[28]);
        Store(10, r[27]);
        Store(9, r[27] + 4);
        Store(8, r[27] + 8);
        if (!s.cr6.eq) {
            r[3] = Word(r[26] - 2744);
            r[4] = r[28];
            r[11] = Word(r[3]);
            r[11] = Word(r[11] + 20);
            Call(0x82bc9c58u);
        }
        r[3] = r[27];
        r[1] += 160;
        Gradual();
        s.fpr_bits[31] = recovery_abi::ReadU64(m, Address(r[1] - 64));
        for (unsigned i = 26; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Classify(GuestAddress continuation) {
        s.lr = continuation;
        mesh_mass_cache61::Classify(m, d.fp, s);
    }
    bool Invalid() {
        s.r[11] = s.r[3] & 519u;
        Integer(s.r[11]);
        s.cr0 = s.cr6;
        return !s.cr6.eq;
    }
    void ChooseBody() {
        auto &r = s.r;
        r[31] = r[3];
        r[8] = r[5];
        Integer(r[4]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        Integer(r[8]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[3] = r[1] + 96;
        s.lr = 0x82bc9ca0u;
        (void)mesh_bounds_math61::Apply(0x82bc9040u, m, d.fp, s);
        r[5] = r[4];
        r[4] = r[8];
        r[3] = r[1] + 80;
        s.lr = 0x82bc9cb0u;
        RecursiveCandidate();
        Load(28, r[1] + 80);
        Move(1, 28);
        Classify(0x82bc9cbcu);
        Load(27, r[1] + 108);
        bool fallback = Invalid();
        if (!fallback) {
            Load(29, r[1] + 84);
            Move(1, 29);
            Classify(0x82bc9cd8u);
            fallback = Invalid();
        }
        if (!fallback) {
            Load(30, r[1] + 88);
            Move(1, 30);
            Classify(0x82bc9cf0u);
            fallback = Invalid();
        }
        if (!fallback) {
            Load(31, r[1] + 92);
            Move(1, 31);
            Classify(0x82bc9d08u);
            fallback = Invalid();
        }
        if (!fallback) {
            Gradual();
            Compare(F(31), F(27));
            fallback = s.cr6.gt;
            if (!fallback) {
                r[11] = 0xffffffff82000000ull;
                Load(0, r[11] + 3664);
                Compare(F(31), F(0));
                fallback = s.cr6.lt;
            }
        }
        if (fallback) {
            Load(0, r[1] + 96);
            r[3] = 1;
            Store(0, r[31]);
            Load(13, r[1] + 100);
            Load(0, r[1] + 104);
            Store(13, r[31] + 4);
            Store(0, r[31] + 8);
            Store(27, r[31] + 12);
        } else {
            Store(28, r[31]);
            r[3] = 2;
            Store(29, r[31] + 4);
            Store(30, r[31] + 8);
            Store(31, r[31] + 12);
        }
    }
    void Cook() {
        auto &r = s.r;
        r[12] = s.lr;
        Word(r[1] - 8, r[12]);
        recovery_abi::WriteU64(m, Address(r[1] - 16), r[31]);
        auto old = r[1];
        r[1] -= 128;
        Word(r[1], old);
        r[31] = r[3];
        r[4] = r[1] + 80;
        r[3] = r[1] + 96;
        r[6] = Word(r[31] + 172);
        r[5] = Word(r[31] + 168);
        s.lr = 0x82b9eab8u;
        (void)geometry_primitives61::Apply(0x82bca410u, m, d.fp, s);
        r[11] = 0xffffffff820d0000ull;
        Gradual();
        s.fpr_bits[2] = recovery_abi::ReadU64(m, Address(r[11] + 24112));
        r[11] = 0xffffffff82000000ull;
        s.fpr_bits[1] = recovery_abi::ReadU64(m, Address(r[11] + 4112));
        s.lr = 0x82b9eaccu;
        (void)power_math61::Apply(0x82b7e860u, m, d.fp, s);
        Load(13, r[1] + 80);
        Single(10, F(1));
        Load(11, r[1] + 84);
        Store(13, r[31] + 152);
        Compare(F(13), F(11));
        Move(0, s.cr6.gt ? 13 : 11);
        Load(12, r[1] + 88);
        Store(0, r[31] + 152);
        Compare(F(0), F(12));
        if (!s.cr6.gt)
            Move(0, 12);
        r[11] = r[31] + 112;
        Single(0, F(0) * F(10));
        Store(0, r[31] + 152);
        r[3] = r[31] + 136;
        Load(0, r[1] + 96);
        Load(10, r[1] + 100);
        Load(9, r[1] + 104);
        Store(0, r[11]);
        Store(10, r[11] + 4);
        Store(9, r[11] + 8);
        Store(13, r[11] + 12);
        Store(11, r[11] + 16);
        Store(12, r[11] + 20);
        r[5] = Word(r[31] + 172);
        r[4] = Word(r[31] + 168);
        s.lr = 0x82b9eb44u;
        Choose();
        r[1] += 128;
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
        r[31] = recovery_abi::ReadU64(m, Address(r[1] - 16));
    }
    void Choose() {
        auto &r = s.r;
        r[12] = s.lr;
        Word(r[1] - 8, r[12]);
        recovery_abi::WriteU64(m, Address(r[1] - 16), r[31]);
        r[12] = r[1] - 16;
        s.lr = 0x82bc9c7cu;
        for (unsigned i = 27; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[12] - 8 * (32 - i)), s.fpr_bits[i]);
        auto old = r[1];
        r[1] -= 176;
        Word(r[1], old);
        ChooseBody();
        r[1] += 176;
        r[12] = r[1] - 16;
        s.lr = 0x82bc9d78u;
        for (unsigned i = 27; i < 32; ++i)
            s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(r[12] - 8 * (32 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
        r[31] = recovery_abi::ReadU64(m, Address(r[1] - 16));
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Select x{m, d, s};
    switch (e) {
    case 0x82b9ea90u:
        x.Cook();
        break;
    case 0x82bc9af0u:
        x.RecursiveCandidate();
        break;
    case 0x82bc9c68u:
        x.Choose();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_bounds_select61
