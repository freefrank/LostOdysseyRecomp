#include "lo_semantics/mesh_normal_encode61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::mesh_normal_encode61 {
namespace {
using recovery_abi::Address;
struct Normal {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &native;
    Registers &s;
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Set(unsigned i, double v) { s.fpr_bits[i] = std::bit_cast<std::uint64_t>(v); }
    void Single(unsigned i, double v) { Set(i, double(float(v))); }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            native.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        Set(i, double(std::bit_cast<float>(m.ReadU32(Address(p)))));
    }
    void Double(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(p));
    }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(double a, double b) {
        Gradual();
        bool u = std::isnan(a) || std::isnan(b);
        s.cr6 = {std::uint8_t(!u && a < b), std::uint8_t(!u && a > b), std::uint8_t(!u && a == b),
                 std::uint8_t(u)};
    }
    void Abs(unsigned to, unsigned from) {
        Gradual();
        s.fpr_bits[to] = s.fpr_bits[from] & 0x7fffffffffffffffull;
    }
    void Neg(unsigned to, unsigned from) {
        s.fpr_bits[to] = s.fpr_bits[from] ^ 0x8000000000000000ull;
    }
    void Asin() {
        auto &r = s.r;
        Abs(13, 1);
        r[11] = 0xffffffff83210000ull;
        r[11] += 19976;
        Load(0, r[11] + 12);
        Compare(F(13), F(0));
        if (s.cr6.gt) {
            Load(12, r[11] + 4);
            r[10] = 1;
            Set(13, F(12) - F(13));
            Load(11, r[11] + 8);
            Set(0, F(13) * F(0));
            Set(13, std::sqrt(F(0)));
            Set(13, F(13) * F(11));
            Neg(13, 13);
        } else {
            Load(12, r[11]);
            r[10] = 0;
            Set(0, F(13) * F(13));
            Compare(F(1), F(12));
            if (s.cr6.eq)
                return;
        }
        Double(11, r[11] + 120);
        r[10] = Address(r[10]) << 3;
        Set(10, F(11) + F(0));
        Double(12, r[11] + 80);
        Double(11, r[11] + 72);
        r[9] = r[11] + 16;
        Set(11, F(12) * F(0) + F(11));
        Double(12, r[11] + 112);
        Double(9, r[10] + r[9]);
        Set(10, F(10) * F(0) + F(12));
        Double(12, r[11] + 64);
        Set(11, F(11) * F(0) + F(12));
        Double(12, r[11] + 104);
        Set(10, F(10) * F(0) + F(12));
        Double(12, r[11] + 56);
        Set(11, F(11) * F(0) + F(12));
        Double(12, r[11] + 96);
        Set(10, F(10) * F(0) + F(12));
        Double(12, r[11] + 48);
        Set(11, F(11) * F(0) + F(12));
        Double(12, r[11] + 88);
        Set(12, F(10) * F(0) + F(12));
        Set(0, F(11) * F(0));
        Set(0, F(0) * F(13));
        Set(0, F(0) / F(12));
        Set(0, F(0) + F(13));
        Set(0, F(0) + F(9));
        Neg(13, 0);
        Set(1, F(1) >= 0.0 ? F(0) : F(13));
    }
    void Truncate(unsigned to, unsigned from) {
        double v = F(from);
        std::int32_t result;
        if (v > double(std::numeric_limits<std::int32_t>::max()))
            result = std::numeric_limits<std::int32_t>::max();
        else if (!std::isfinite(v) ||
                 std::trunc(v) < double(std::numeric_limits<std::int32_t>::min())) {
            std::feraiseexcept(FE_INVALID);
            result = std::numeric_limits<std::int32_t>::min();
        } else
            result = static_cast<std::int32_t>(v);
        s.fpr_bits[to] = std::uint64_t(std::int64_t(result));
    }
    void Encode() {
        auto &r = s.r;
        r[29] = r[10];
        r[11] = 0xffffffff82000000ull;
        r[10] = 0;
        r[31] = r[8];
        r[30] = r[9];
        r[28] = 1;
        Load(0, r[11] + 3664);
        Store(r[6], r[10]);
        Compare(F(1), F(0));
        Store(r[7], r[10]);
        if (s.cr6.lt)
            Store(r[7], r[28]);
        for (unsigned i = 2; i <= 3; ++i) {
            Compare(F(i), F(0));
            if (s.cr6.lt) {
                r[11] = m.ReadU32(Address(r[7]));
                r[11] |= 1u << (i - 1);
                Store(r[7], r[11]);
            }
        }
        Abs(0, 1);
        Abs(31, 2);
        Abs(29, 3);
        constexpr unsigned left[]{0, 31, 29}, right[]{31, 29, 0};
        for (unsigned i = 0; i < 3; ++i) {
            Compare(F(left[i]), F(right[i]));
            if (s.cr6.gt) {
                r[11] = m.ReadU32(Address(r[6]));
                r[11] |= 1u << i;
                Store(r[6], r[11]);
            }
        }
        Compare(F(0), F(31));
        if (s.cr6.gt) {
            Set(13, F(0));
            Set(0, F(31));
            Set(31, F(13));
        }
        Compare(F(31), F(29));
        if (s.cr6.gt) {
            Set(13, F(31));
            Set(31, F(29));
            Set(29, F(13));
        }
        Compare(F(29), F(0));
        if (s.cr6.gt)
            Set(29, F(0));
        r[11] = 0xffffffff82000000ull;
        r[9] = 0xffffffff820d0000ull;
        r[10] = 0xffffffff820d0000ull;
        Load(30, r[11] + 30596);
        r[11] = 0xffffffff82000000ull;
        Load(27, r[9] + 24744);
        Compare(F(31), F(30));
        Load(25, r[10] + 25684);
        Load(28, r[11] + 3648);
        if (s.cr6.gt)
            Set(26, F(27));
        else {
            Compare(F(31), F(28));
            if (s.cr6.lt)
                Set(26, F(25));
            else {
                Set(1, F(31));
                s.lr = 0x82bb90d0u;
                Asin();
                Single(26, F(1));
            }
        }
        Single(0, -(F(31) * F(31) - F(30)));
        Single(0, std::sqrt(F(0)));
        Single(1, F(29) / F(0));
        Compare(F(1), F(30));
        if (s.cr6.gt)
            Set(13, F(27));
        else {
            Compare(F(1), F(28));
            if (s.cr6.lt)
                Set(13, F(25));
            else {
                s.lr = 0x82bb9104u;
                Asin();
                Single(13, F(1));
            }
        }
        r[11] = Address(r[29]) & 255u;
        auto shift = Address(r[11]) & 63u;
        r[11] = shift >= 32 ? 0u : Address(r[28]) << shift;
        r[11] = Address(r[11]) << 2;
        r[11] -= 4;
        r[11] = std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[11]))));
        recovery_abi::WriteU64(m, Address(r[1] + 80), r[11]);
        r[11] = 0xffffffff82220000ull;
        Double(0, r[1] + 80);
        Set(0, double(std::bit_cast<std::int64_t>(s.fpr_bits[0])));
        Single(12, F(0));
        Load(0, r[11] - 32316);
        Single(0, F(12) * F(0));
        Single(12, F(0) * F(26));
        Single(0, F(0) * F(13));
        Truncate(13, 12);
        Store(r[31], s.fpr_bits[13]);
        Truncate(0, 0);
        Store(r[30], s.fpr_bits[0]);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, float_triplet_transfer::NativeServices &n,
           Registers &s) {
    Normal v{m, n, s};
    if (e == 0x82325048u) {
        v.Asin();
        return true;
    }
    if (e != 0x82bb8fa0u)
        return false;
    auto &r = s.r;
    r[12] = s.lr;
    s.lr = 0x82bb8fa8u;
    for (unsigned i = 28; i < 32; ++i)
        recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
    m.WriteU32(Address(r[1] - 8), Address(r[12]));
    r[12] = r[1] - 40;
    s.lr = 0x82bb8fb0u;
    v.Gradual();
    for (unsigned i = 25; i < 32; ++i)
        recovery_abi::WriteU64(m, Address(r[12] - 8 * (32 - i)), s.fpr_bits[i]);
    auto old = r[1];
    r[1] -= 192;
    m.WriteU32(Address(r[1]), Address(old));
    v.Encode();
    r[1] += 192;
    r[12] = r[1] - 40;
    v.Gradual();
    for (unsigned i = 25; i < 32; ++i)
        s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(r[12] - 8 * (32 - i)));
    for (unsigned i = 28; i < 32; ++i)
        r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
    r[12] = m.ReadU32(Address(r[1] - 8));
    s.lr = r[12];
    return true;
}
} // namespace lo::semantic::gpu::mesh_normal_encode61
