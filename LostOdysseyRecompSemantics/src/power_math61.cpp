#include "lo_semantics/power_math61.h"
#include "lo_semantics/legacy_fp_flagged_routes.h"
#include "lo_semantics/power_log61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::power_math61 {
namespace {
using recovery_abi::Address;
struct Power {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &fp;
    Registers &s;
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void V(unsigned i, double v) { s.fpr_bits[i] = std::bit_cast<std::uint64_t>(v); }
    void Move(unsigned i, unsigned j) {
        Mode();
        s.fpr_bits[i] = s.fpr_bits[j];
    }
    void Mode() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Load(unsigned i, std::uint64_t p) {
        Mode();
        s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(p));
    }
    void Save(unsigned i, std::uint64_t p) {
        Mode();
        recovery_abi::WriteU64(m, Address(p), s.fpr_bits[i]);
    }
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(double a, double b) {
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
    }
    void Unsigned(std::uint64_t a, std::uint32_t b) {
        auto x = Address(a);
        s.cr6 = {std::uint8_t(x < b), std::uint8_t(x > b), std::uint8_t(x == b), s.xer_so};
    }
    void Signed(auto &cr, std::uint64_t a, std::int32_t b) {
        auto x = std::bit_cast<std::int32_t>(Address(a));
        cr = {std::uint8_t(x < b), std::uint8_t(x > b), std::uint8_t(x == b), s.xer_so};
    }
    void Round(unsigned i) {
        auto v = F(i);
        auto x = v > double(std::numeric_limits<std::int64_t>::max())
                     ? std::numeric_limits<std::int64_t>::max()
                     : std::llrint(v);
        V(i, double(x));
    }
    void Truncate(unsigned i) {
        auto v = F(i);
        auto x = v > double(std::numeric_limits<std::int32_t>::max())
                     ? std::numeric_limits<std::int32_t>::max()
                     : std::int32_t(v);
        s.fpr_bits[i] = std::uint64_t(std::int64_t(x));
    }
    void ArithmeticShift(unsigned i, unsigned bits) {
        auto x = std::bit_cast<std::int32_t>(Address(s.r[i]));
        s.xer_ca = x < 0 && (Address(s.r[i]) & ((1u << bits) - 1));
        s.r[i] = std::uint64_t(std::int64_t(x >> bits));
    }
    void AddCarry(unsigned i) {
        auto old = s.r[i];
        s.r[i] += s.xer_ca;
        s.xer_ca = Address(s.r[i]) < Address(old);
    }
    void Lower(GuestAddress e, GuestAddress continuation) {
        s.lr = continuation;
        if (e == 0x82301a68u)
            (void)power_log61::Apply(e, m, fp, s);
        else
            (void)power_fp_support61::Apply(e, m, fp, s);
    }
    void CopySign() {
        struct Native final : legacy_fp_classification::HostFpServices {
            float_triplet_transfer::NativeServices &fp;
            explicit Native(float_triplet_transfer::NativeServices &v) : fp(v) {}
            void SetHostFpControl(std::uint32_t v) override { fp.SetHostFpControl(v); }
        } native(fp);
        legacy_fp_flagged_routes::Registers c;
        auto &v = c.numeric.classifier;
        v.integer.r = s.r;
        v.integer.sp = s.r[1];
        v.f0_bits = s.fpr_bits[0];
        v.f1_bits = s.fpr_bits[1];
        c.f2_bits = s.fpr_bits[2];
        v.cached_fp_control = s.cached_fp_control;
        (void)legacy_fp_flagged_routes::Apply(0x82b7def0u, m, native, c);
        for (unsigned i : {9u, 10u, 11u})
            s.r[i] = v.integer.r[i];
        s.fpr_bits[0] = v.f0_bits;
        s.fpr_bits[1] = v.f1_bits;
        s.cached_fp_control = v.cached_fp_control;
    }
    void Overflow() {
        s.r[11] = 0xffffffff83210000ull;
        Load(0, s.r[11] + 21760);
        V(1, F(0) * F(25));
    }
    void Underflow() {
        Mode();
        V(1, F(25) * F(27));
    }
    void Nonfinite() {
        auto &r = s.r;
        r[10] = r[11] & 0x7ff8;
        Unsigned(r[10], 32752);
        if (s.cr6.eq) {
            r[11] = Word(r[1] + 192) & 0x7ffffu;
            Signed(s.cr0, r[11], 0);
            if (!s.cr0.eq) {
                V(1, F(28) + F(30));
                return;
            }
            r[11] = Word(r[1] + 196);
            Unsigned(r[11], 0);
            if (!s.cr6.eq) {
                V(1, F(28) + F(30));
                return;
            }
        }
        r[11] = r[9] & 0x7ff8;
        Unsigned(r[11], 32752);
        if (s.cr6.eq) {
            r[9] = Word(r[1] + 200) & 0x7ffffu;
            Signed(s.cr0, r[9], 0);
            if (!s.cr0.eq) {
                V(1, F(28) + F(30));
                return;
            }
            r[9] = Word(r[1] + 204);
            Unsigned(r[9], 0);
            if (!s.cr6.eq) {
                V(1, F(28) + F(30));
                return;
            }
        }
        Unsigned(r[10], 32760);
        if (s.cr6.eq) {
            V(1, F(28) + F(30));
            return;
        }
        Unsigned(r[11], 32760);
        if (s.cr6.eq) {
            V(1, F(28) + F(30));
            return;
        }
        r[5] = r[1] + 88;
        Move(2, 30);
        Move(1, 28);
        Lower(0x82b7e6d8u, 0x82b7ed30u);
        Load(1, r[1] + 88);
    }
    bool IntegerPower() {
        auto &r = s.r;
        Compare(F(30), F(0));
        if (s.cr6.gt)
            return false;
        Move(1, 28);
        Lower(0x82b7e668u, 0x82b7e9d8u);
        Signed(s.cr0, r[3], 0);
        if (s.cr0.eq)
            return false;
        Move(1, 30);
        Lower(0x82b7e668u, 0x82b7e9e8u);
        Signed(s.cr0, r[3], 0);
        if (s.cr0.eq)
            return false;
        Mode();
        Compare(F(30), F(27));
        if (!s.cr6.gt)
            return false;
        r[11] = r[1] + 88;
        Move(0, 30);
        Truncate(0);
        r[10] = Word(r[1] + 80);
        Move(31, 26);
        Word(r[11], s.fpr_bits[0]);
        r[11] = Word(r[1] + 88);
        r[31] = std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[11]))) *
                              std::int64_t(std::bit_cast<std::int32_t>(Address(r[10]))));
        Signed(s.cr6, r[11], 0);
        if (!s.cr6.eq) {
            do {
                r[10] = r[11] & 1;
                Signed(s.cr0, r[10], 0);
                if (!s.cr0.eq)
                    V(31, F(31) * F(29));
                V(29, F(29) * F(29));
                ArithmeticShift(11, 1);
                Signed(s.cr0, r[11], 0);
            } while (!s.cr0.eq);
        }
        Move(1, 31);
        Lower(0x82b822f0u, 0x82b7ea3cu);
        r[4] = r[3] + r[31];
        Signed(s.cr6, r[4], 2560);
        if (s.cr6.gt) {
            r[11] = 0xffffffff83210000ull;
            Load(0, r[11] + 21760);
            V(0, F(0) * F(31));
            V(1, F(0) * F(25));
            return true;
        }
        Signed(s.cr6, r[4], -2557);
        if (s.cr6.lt) {
            V(0, F(31) * F(25));
            V(1, F(0) * F(27));
            return true;
        }
        Scale();
        return true;
    }
    void Scale() {
        Signed(s.cr6, s.r[4], 1024);
        if (s.cr6.gt) {
            Overflow();
            return;
        }
        Signed(s.cr6, s.r[4], -1021);
        if (s.cr6.lt) {
            Underflow();
            return;
        }
        Move(1, 31);
        Lower(0x82b822c8u, 0x82b7ecc0u);
        Mode();
        V(1, F(1) * F(25));
    }
    void LogReduction() {
        // Locate the nearby 1/16 exponent-table interval, then use either the
        // existing log kernel or a small rational correction around its node.
        auto &r = s.r;
        r[10] = 0xffffffff820d0000ull;
        r[11] = 1;
        r[31] = r[10] + 12240;
        Load(0, r[31] + 72);
        Compare(F(29), F(0));
        if (!s.cr6.gt)
            r[11] = 9;
        r[10] = Address(r[11]) << 3;
        r[9] = r[31] + 32;
        Load(0, r[10] + r[9]);
        Compare(F(29), F(0));
        if (!s.cr6.gt)
            r[11] += 4;
        r[10] = Address(r[11]) << 3;
        r[9] = r[31] + 16;
        Load(0, r[10] + r[9]);
        Compare(F(29), F(0));
        if (!s.cr6.gt)
            r[11] += 2;
        r[10] = Word(r[1] + 80);
        r[10] = Address(r[10]) << 4;
        r[9] = r[10] - r[11];
        Signed(s.cr0, r[9], 0);
        r[10] = r[11] - r[10];
        if (!s.cr0.lt)
            r[10] = r[9];
        Signed(s.cr6, r[10], 1);
        r[10] = 0xffffffff82000000ull;
        Load(31, r[10] + 4232);
        if (s.cr6.eq) {
            Move(1, 28);
            Lower(0x82301a68u, 0x82b7eae8u);
            Load(0, r[31] + 216);
            Move(13, 27);
            V(12, F(1) * F(0));
            return;
        }
        r[10] = Address(r[11]) << 3;
        Load(11, r[31] + 256);
        r[8] = r[31] + 8;
        Load(10, r[31] + 216);
        ++r[11];
        r[7] = r[31] + 144;
        ArithmeticShift(11, 1);
        Load(0, r[10] + r[8]);
        AddCarry(11);
        V(12, F(29) - F(0));
        r[10] = std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[9]))));
        r[11] = Address(r[11]) << 3;
        V(0, F(0) + F(29));
        recovery_abi::WriteU64(m, Address(r[1] + 88), r[10]);
        Load(9, r[1] + 88);
        Load(13, r[11] + r[7]);
        r[11] = 0xffffffff82000000ull;
        V(9, double(std::bit_cast<std::int64_t>(s.fpr_bits[9])));
        V(13, F(12) - F(13));
        V(12, F(9) * F(31));
        Load(9, r[31] + 224);
        V(13, F(13) / F(0));
        Load(0, r[11] + 4112);
        V(0, F(13) * F(0));
        V(13, F(0) * F(0));
        V(8, F(0) * F(9));
        Load(9, r[31] + 248);
        V(9, F(13) * F(11) + F(9));
        Load(11, r[31] + 240);
        V(9, F(9) * F(13) + F(11));
        Load(11, r[31] + 232);
        V(11, F(9) * F(13) + F(11));
        V(13, F(11) * F(13));
        V(13, F(13) * F(0));
        V(13, F(13) * F(10) + F(8));
        V(13, F(13) + F(0));
    }
    void GeneralPower() {
        auto &r = s.r;
        LogReduction();
        // Split exponent products to retain the guest's residual before exp.
        r[11] = 0xffffffff82220000ull;
        Mode();
        V(10, F(13) * F(30));
        Load(0, r[11] - 32352);
        V(11, F(30) * F(0));
        Move(13, 11);
        Round(13);
        V(13, F(13) * F(31));
        V(11, F(30) - F(13));
        V(11, F(11) * F(12) + F(10));
        V(10, F(11) * F(0));
        Round(10);
        V(10, F(10) * F(31));
        V(13, F(13) * F(12) + F(10));
        V(11, F(11) - F(10));
        V(12, F(13) * F(0));
        Round(12);
        V(12, F(12) * F(31));
        V(13, F(13) - F(12));
        V(13, F(13) + F(11));
        V(11, F(13) * F(0));
        Round(11);
        V(11, F(11) * F(31));
        V(12, F(12) + F(11));
        V(13, F(13) - F(11));
        V(0, F(12) * F(0));
        Load(12, r[31] + 320);
        Compare(F(0), F(12));
        if (s.cr6.gt) {
            Overflow();
            return;
        }
        Load(12, r[31] + 328);
        Compare(F(0), F(12));
        if (s.cr6.lt) {
            Underflow();
            return;
        }
        r[11] = r[1] + 88;
        Truncate(0);
        Compare(F(13), F(27));
        Word(r[11], s.fpr_bits[0]);
        r[11] = Word(r[1] + 88);
        if (s.cr6.gt) {
            ++r[11];
            V(13, F(13) - F(31));
        }
        Load(0, r[31] + 312);
        r[10] = std::countl_zero(Address(r[11]));
        Load(12, r[31] + 304);
        r[9] = r[11];
        ArithmeticShift(9, 4);
        V(12, F(13) * F(0) + F(12));
        Load(0, r[31] + 296);
        r[10] = std::countl_zero(Address(r[10]));
        AddCarry(9);
        r[10] = ((r[10] >> 5) & 1u) ^ 1u;
        r[30] = r[10] + r[9];
        r[10] = r[31] + 8;
        r[9] = Address(r[30]) << 4;
        V(12, F(12) * F(13) + F(0));
        Load(0, r[31] + 288);
        r[11] = r[9] - r[11];
        r[11] = Address(r[11]) << 3;
        for (unsigned offset : {280u, 272u, 264u}) {
            V(12, F(12) * F(13) + F(0));
            Load(0, r[31] + offset);
        }
        V(0, F(12) * F(13) + F(0));
        Load(12, r[11] + r[10]);
        V(0, F(0) * F(13) + F(26));
        V(31, F(0) * F(12));
        Move(1, 31);
        Lower(0x82b822f0u, 0x82b7eca4u);
        r[4] = r[3] + r[30];
        Scale();
    }
    void Body() {
        auto &r = s.r;
        Move(30, 2);
        r[11] = 0xffffffff82000000ull;
        Move(28, 1);
        Save(30, r[1] + 200);
        Save(28, r[1] + 192);
        Load(27, r[11] + 4072);
        Compare(F(30), F(27));
        if (s.cr6.eq) {
            r[11] = 0xffffffff82000000ull;
            Load(1, r[11] + 3880);
            return;
        }
        Compare(F(28), F(27));
        if (s.cr6.eq) {
            Move(1, 30);
            Lower(0x82b7e668u, 0x82b7e8b8u);
            Compare(F(30), F(27));
            if (s.cr6.lt) {
                r[11] = 0xffffffff83210000ull;
                Signed(s.cr6, r[3], 1);
                Load(1, r[11] + 21760);
                if (s.cr6.eq) {
                    Move(2, 28);
                    s.lr = 0x82b7e8d8u;
                    CopySign();
                }
                return;
            }
            Compare(F(30), F(27));
            if (s.cr6.gt) {
                Signed(s.cr6, r[3], 1);
                Move(1, s.cr6.eq ? 28 : 27);
                return;
            }
        }
        r[11] = m.ReadU16(Address(r[1] + 192));
        r[9] = m.ReadU16(Address(r[1] + 200));
        r[10] = r[11] & 0x7ff0;
        Unsigned(r[10], 32752);
        if (s.cr6.eq) {
            Nonfinite();
            return;
        }
        r[10] = r[9] & 0x7ff0;
        Unsigned(r[10], 32752);
        if (s.cr6.eq) {
            Nonfinite();
            return;
        }
        r[11] = 0xffffffff82000000ull;
        Compare(F(28), F(27));
        Load(26, r[11] + 3880);
        Move(25, 26);
        if (s.cr6.lt) {
            Move(1, 30);
            Lower(0x82b7e668u, 0x82b7e938u);
            Signed(s.cr6, r[3], 1);
            if (s.cr6.eq) {
                r[11] = 0xffffffff82220000ull;
                Load(25, r[11] - 32248);
            } else {
                Signed(s.cr6, r[3], 2);
                if (!s.cr6.eq) {
                    r[11] = 0xffffffff83210000ull;
                    Load(1, r[11] + 21768);
                    return;
                }
            }
            s.fpr_bits[28] ^= 0x8000000000000000ull;
        }
        Mode();
        s.fpr_bits[13] = s.fpr_bits[30] & 0x7fffffffffffffffull;
        r[11] = 0xffffffff83210000ull;
        Load(0, r[11] + 20416);
        Compare(F(13), F(0));
        if (s.cr6.gt) {
            Compare(F(30), F(27));
            if (s.cr6.lt)
                V(28, F(26) / F(28));
            Compare(F(28), F(26));
            if (s.cr6.gt) {
                Overflow();
                return;
            }
            Compare(F(28), F(26));
            if (s.cr6.lt) {
                Underflow();
                return;
            }
            Move(1, 25);
            return;
        }
        r[4] = r[1] + 80;
        Move(1, 28);
        Lower(0x82b823c8u, 0x82b7e9bcu);
        r[11] = 0xffffffff820d0000ull;
        Move(29, 1);
        Load(0, r[11] + 12576);
        if (!IntegerPower())
            GeneralPower();
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        Word(r[1] - 8, r[12]);
        recovery_abi::WriteU64(m, Address(r[1] - 24), r[30]);
        recovery_abi::WriteU64(m, Address(r[1] - 16), r[31]);
        r[12] = r[1] - 24;
        s.lr = 0x82b7e878u;
        for (unsigned i = 25; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[12] - 8 * (32 - i)), s.fpr_bits[i]);
        auto old = r[1];
        r[1] -= 176;
        Word(r[1], old);
        Body();
        r[1] += 176;
        r[12] = r[1] - 24;
        s.lr = 0x82b7ed48u;
        for (unsigned i = 25; i < 32; ++i)
            s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(r[12] - 8 * (32 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
        r[30] = recovery_abi::ReadU64(m, Address(r[1] - 24));
        r[31] = recovery_abi::ReadU64(m, Address(r[1] - 16));
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, float_triplet_transfer::NativeServices &fp,
           Registers &s) {
    if (e != 0x82b7e860u)
        return false;
    Power{m, fp, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::power_math61
