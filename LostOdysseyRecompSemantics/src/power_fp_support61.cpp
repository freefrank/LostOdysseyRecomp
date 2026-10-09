#include "lo_semantics/power_fp_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::power_fp_support61 {
namespace {
using recovery_abi::Address;
struct Support {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &fp;
    Registers &s;
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Value(unsigned i, double v) { s.fpr_bits[i] = std::bit_cast<std::uint64_t>(v); }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Save(unsigned i, std::uint64_t p) {
        Gradual();
        recovery_abi::WriteU64(m, Address(p), s.fpr_bits[i]);
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(p));
    }
    void Signed(auto &cr, std::uint64_t v) {
        auto x = std::bit_cast<std::int32_t>(Address(v));
        cr = {std::uint8_t(x < 0), std::uint8_t(x > 0), std::uint8_t(x == 0), s.xer_so};
    }
    void Zero(std::uint64_t v) {
        auto x = Address(v);
        s.cr6 = {0, std::uint8_t(x != 0), std::uint8_t(x == 0), s.xer_so};
    }
    void Compare(double a, double b) {
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
    }
    void Rounded(unsigned target, unsigned source) {
        const double value = F(source);
        const auto integer = value > double(std::numeric_limits<std::int64_t>::max())
                                 ? std::numeric_limits<std::int64_t>::max()
                                 : std::llrint(value);
        Value(target, double(integer));
    }
    void Parity() {
        auto &r = s.r;
        Save(1, r[1] + 16);
        r[11] = m.ReadU16(Address(r[1] + 16));
        r[11] &= 0x7ff0;
        Signed(s.cr0, r[11]);
        if (s.cr0.eq) {
            r[11] = m.ReadU32(Address(r[1] + 16)) & 0xfffffu;
            Signed(s.cr0, r[11]);
            if (!s.cr0.eq) {
                r[3] = 0;
                return;
            }
            r[11] = m.ReadU32(Address(r[1] + 20));
            Zero(r[11]);
            if (!s.cr6.eq) {
                r[3] = 0;
                return;
            }
        }
        Gradual();
        Rounded(0, 1);
        Compare(F(0), F(1));
        if (!s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = 0xffffffff82000000ull;
        Load(0, r[11] + 3952);
        Value(0, F(1) * F(0));
        Rounded(13, 0);
        Compare(F(13), F(0));
        r[3] = s.cr6.eq ? 2 : 1;
    }
    void Unsigned(std::uint64_t a, std::uint64_t b) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Infinity() {
        s.r[11] = 0xffffffff83210000ull;
        Load(0, s.r[11] + 21760);
    }
    void ZeroValue() {
        s.r[11] = 0xffffffff82000000ull;
        Load(0, s.r[11] + 4072);
    }
    bool SpecialBody() {
        auto &r = s.r;
        s.fpr_bits[31] = s.fpr_bits[2];
        Save(31, r[1] + 136);
        r[9] = 0x7ff00000;
        Save(1, r[1] + 128);
        r[31] = r[5];
        s.fpr_bits[0] = s.fpr_bits[1] & 0x7fffffffffffffffull;
        r[30] = 0;
        r[10] = 0xfffffffffff00000ull;
        r[11] = m.ReadU32(Address(r[1] + 136));
        Unsigned(r[11], r[9]);
        if (s.cr6.eq) {
            r[11] = m.ReadU32(Address(r[1] + 140));
            Zero(r[11]);
            if (s.cr6.eq) {
                r[11] = 0xffffffff82000000ull;
                Load(13, r[11] + 3880);
                Compare(F(0), F(13));
                if (s.cr6.gt) {
                    Infinity();
                    return true;
                }
                Compare(F(0), F(13));
                if (s.cr6.lt) {
                    ZeroValue();
                    return true;
                }
                Save(13, r[31]);
                return false;
            }
        } else {
            Unsigned(r[11], r[10]);
            if (s.cr6.eq) {
                r[11] = m.ReadU32(Address(r[1] + 140));
                Zero(r[11]);
                if (s.cr6.eq) {
                    r[11] = 0xffffffff82000000ull;
                    Load(13, r[11] + 3880);
                    Compare(F(0), F(13));
                    if (s.cr6.gt) {
                        ZeroValue();
                        return true;
                    }
                    Compare(F(0), F(13));
                    if (s.cr6.lt) {
                        Infinity();
                        return true;
                    }
                    r[11] = 0xffffffff83210000ull;
                    r[30] = 1;
                    Load(0, r[11] + 21768);
                    return true;
                }
            }
        }
        r[11] = m.ReadU32(Address(r[1] + 128));
        Unsigned(r[11], r[9]);
        if (s.cr6.eq) {
            r[11] = m.ReadU32(Address(r[1] + 132));
            Zero(r[11]);
            if (!s.cr6.eq)
                return false;
            ZeroValue();
            Compare(F(31), F(0));
            if (s.cr6.gt) {
                Infinity();
                return true;
            }
            r[11] = 0xffffffff82000000ull;
            Load(13, r[11] + 3880);
            Value(0, F(31) >= 0 ? F(13) : F(0));
            return true;
        }
        Unsigned(r[11], r[10]);
        if (!s.cr6.eq)
            return false;
        r[11] = m.ReadU32(Address(r[1] + 132));
        Zero(r[11]);
        if (!s.cr6.eq)
            return false;
        Gradual();
        s.fpr_bits[1] = s.fpr_bits[31];
        s.lr = 0x82b7e7f0u;
        Parity();
        ZeroValue();
        Compare(F(31), F(0));
        if (s.cr6.gt) {
            r[11] = 0xffffffff83210000ull;
            Unsigned(r[3], 1);
            Load(0, r[11] + 21760);
            if (s.cr6.eq)
                s.fpr_bits[0] ^= 0x8000000000000000ull;
            return true;
        }
        Compare(F(31), F(0));
        if (s.cr6.lt) {
            Unsigned(r[3], 1);
            if (s.cr6.eq) {
                r[11] = 0xffffffff83210000ull;
                Load(0, r[11] + 21792);
            }
            return true;
        }
        r[11] = 0xffffffff82000000ull;
        Load(0, r[11] + 3880);
        return true;
    }
    void Special() {
        auto &r = s.r;
        r[12] = s.lr;
        m.WriteU32(Address(r[1] - 8), Address(r[12]));
        recovery_abi::WriteU64(m, Address(r[1] - 24), r[30]);
        recovery_abi::WriteU64(m, Address(r[1] - 16), r[31]);
        Save(31, r[1] - 32);
        auto old = r[1];
        r[1] -= 112;
        m.WriteU32(Address(r[1]), Address(old));
        if (SpecialBody())
            Save(0, r[31]);
        r[3] = r[30];
        r[1] += 112;
        r[12] = m.ReadU32(Address(r[1] - 8));
        s.lr = r[12];
        Load(31, r[1] - 32);
        r[30] = recovery_abi::ReadU64(m, Address(r[1] - 24));
        r[31] = recovery_abi::ReadU64(m, Address(r[1] - 16));
    }
    void Exponent() {
        auto &r = s.r;
        Save(1, r[1] + 16);
        r[11] = m.ReadU16(Address(r[1] + 16));
        r[11] = (r[11] >> 4) & 0x7ff;
        r[11] -= 1022;
        r[3] = std::uint64_t(std::int64_t(std::bit_cast<std::int16_t>(std::uint16_t(r[11]))));
    }
    void ReplaceExponent() {
        auto &r = s.r;
        Save(1, r[1] + 16);
        r[11] = r[4] + 1022;
        Save(1, r[1] - 16);
        r[11] = Address(r[11]) << 4;
        r[10] = m.ReadU16(Address(r[1] + 16));
        r[10] &= 32783;
        Signed(s.cr0, r[10]);
        r[11] |= r[10];
        m.WriteU16(Address(r[1] - 16), std::uint16_t(r[11]));
        Load(1, r[1] - 16);
    }
    void Decompose() {
        auto &r = s.r;
        r[11] = 0xffffffff82000000ull;
        Save(1, r[1] + 16);
        Load(0, r[11] + 4072);
        Compare(F(1), F(0));
        if (s.cr6.eq) {
            r[9] = 0;
            m.WriteU32(Address(r[4]), 0);
            return;
        }
        r[8] = m.ReadU16(Address(r[1] + 16));
        r[7] = r[8];
        r[11] = r[7] & 0x7ff0;
        Signed(s.cr0, r[11]);
        bool subnormal = false;
        if (s.cr0.eq) {
            r[6] = m.ReadU32(Address(r[1] + 16));
            r[10] = Address(r[6]) & 0xfffffu;
            Signed(s.cr0, r[10]);
            r[10] = m.ReadU32(Address(r[1] + 20));
            if (!s.cr0.eq)
                subnormal = true;
            else {
                Zero(r[10]);
                subnormal = !s.cr6.eq;
            }
        }
        if (subnormal) {
            r[9] = std::uint64_t(std::int64_t(-1021));
            Compare(F(1), F(0));
            r[5] = s.cr6.lt ? 1 : 0;
            r[11] = r[7] & 16;
            Signed(s.cr0, r[11]);
            if (s.cr0.eq) {
                do {
                    r[6] = Address(r[6]) << 1;
                    r[11] = r[10] & 0x80000000u;
                    Signed(s.cr0, r[11]);
                    m.WriteU32(Address(r[1] + 16), Address(r[6]));
                    if (!s.cr0.eq) {
                        r[6] |= 1;
                        m.WriteU32(Address(r[1] + 16), Address(r[6]));
                    }
                    r[8] = m.ReadU16(Address(r[1] + 16));
                    r[10] = Address(r[10]) << 1;
                    --r[9];
                    r[11] = r[8] & 16;
                    Signed(s.cr0, r[11]);
                } while (s.cr0.eq);
                m.WriteU32(Address(r[1] + 20), Address(r[10]));
            }
            r[11] = r[8] & 65519;
            Signed(s.cr0, r[11]);
            Signed(s.cr6, r[5]);
            m.WriteU16(Address(r[1] + 16), std::uint16_t(r[11]));
            if (!s.cr6.eq) {
                r[11] |= 32768;
                m.WriteU16(Address(r[1] + 16), std::uint16_t(r[11]));
            }
            Load(0, r[1] + 16);
            Save(0, r[1] - 8);
            Save(0, r[1] - 16);
        } else {
            r[11] = (r[11] >> 4) & 0xfffu;
            Save(1, r[1] - 8);
            Save(1, r[1] - 16);
            r[9] = r[11] - 1022;
        }
        r[11] = m.ReadU16(Address(r[1] - 8));
        r[11] &= 32783;
        Signed(s.cr0, r[11]);
        r[11] |= 16352;
        m.WriteU16(Address(r[1] - 16), std::uint16_t(r[11]));
        Load(1, r[1] - 16);
        m.WriteU32(Address(r[4]), Address(r[9]));
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, float_triplet_transfer::NativeServices &fp,
           Registers &s) {
    Support x{m, fp, s};
    switch (e) {
    case 0x82b7e6d8u:
        x.Special();
        break;
    case 0x82b7e668u:
        x.Parity();
        break;
    case 0x82b822f0u:
        x.Exponent();
        break;
    case 0x82b822c8u:
        x.ReplaceExponent();
        break;
    case 0x82b823c8u:
        x.Decompose();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::power_fp_support61
