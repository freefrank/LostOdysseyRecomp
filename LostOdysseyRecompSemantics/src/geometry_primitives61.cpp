#include "lo_semantics/geometry_primitives61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::geometry_primitives61 {
namespace {
using recovery_abi::Address;
struct Geometry {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &native;
    Registers &s;
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(static_cast<float>(v)));
    }
    void Load(unsigned i, std::uint64_t p) {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            native.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[i] =
            std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(m.ReadU32(Address(p)))));
    }
    void Store(unsigned i, std::uint64_t p) {
        m.WriteU32(Address(p), std::bit_cast<std::uint32_t>(static_cast<float>(F(i))));
    }
    void Compare(double a, double b) {
        const bool u = std::isnan(a) || std::isnan(b);
        s.cr6 = {std::uint8_t(!u && a < b), std::uint8_t(!u && a > b), std::uint8_t(!u && a == b),
                 std::uint8_t(u)};
    }
    void Zero(std::uint64_t v) {
        const auto x = Address(v);
        s.cr6 = {0u, std::uint8_t(x != 0), std::uint8_t(x == 0), s.xer_so};
    }
    void Cube() {
        auto &r = s.r;
        Load(0, r[3] + 16u);
        r[11] = 0xffffffff82020000ull;
        Load(12, r[3] + 4u);
        Load(11, r[3] + 8u);
        Single(10, F(0) - F(12));
        Load(9, r[3] + 20u);
        Single(9, F(9) - F(11));
        Load(0, r[3] + 12u);
        Load(13, r[3]);
        Single(8, F(0) - F(13));
        Load(0, r[11] - 1552u);
        Single(10, F(10) * F(0));
        Single(9, F(9) * F(0));
        Single(1, F(8) * F(0));
        Compare(F(10), F(9));
        s.fpr_bits[8] = s.fpr_bits[s.cr6.gt ? 10 : 9];
        Compare(F(1), F(8));
        if (!s.cr6.gt) {
            Compare(F(10), F(9));
            s.fpr_bits[1] = s.fpr_bits[s.cr6.gt ? 10 : 9];
        }
        Load(10, r[3] + 12u);
        Load(9, r[3] + 16u);
        Single(13, F(10) + F(13));
        Load(10, r[3] + 20u);
        Single(12, F(9) + F(12));
        Single(11, F(10) + F(11));
        Single(13, F(13) * F(0));
        Single(12, F(12) * F(0));
        Single(0, F(11) * F(0));
        Single(11, F(13) - F(1));
        Store(11, r[4]);
        Single(10, F(12) - F(1));
        Store(10, r[4] + 4u);
        Single(9, F(0) - F(1));
        Store(9, r[4] + 8u);
        Single(13, F(13) + F(1));
        Store(13, r[4] + 12u);
        Single(12, F(12) + F(1));
        Store(12, r[4] + 16u);
        Single(0, F(0) + F(1));
        Store(0, r[4] + 20u);
    }
    void Contains() {
        auto &r = s.r;
        for (unsigned axis = 0; axis < 5; ++axis) {
            Load(0, r[4] + 4u * axis);
            Load(13, r[3] + 4u * axis);
            Compare(F(0), F(13));
            if (axis < 3 ? s.cr6.gt : s.cr6.lt) {
                r[3] = 0;
                return;
            }
        }
        Load(13, r[3] + 20u);
        r[3] = 0;
        Load(0, r[4] + 20u);
        Compare(F(0), F(13));
        if (!s.cr6.lt)
            r[3] = 1;
    }
    void Corners() {
        auto &r = s.r;
        r[11] = r[3];
        Zero(r[4]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        Load(0, r[11]);
        r[3] = 1;
        Load(13, r[11] + 4u);
        Load(12, r[11] + 8u);
        Load(11, r[11] + 12u);
        Load(10, r[11] + 16u);
        Load(9, r[11] + 20u);
        constexpr unsigned offsets[]{0,  4,  8,  16, 20, 12, 32, 24, 28, 36, 40, 44,
                                     48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92};
        constexpr unsigned values[]{0, 13, 12, 13, 12, 11, 12, 11, 10, 0, 10, 12,
                                    0, 13, 9,  11, 13, 9,  11, 10, 9,  0, 10, 9};
        for (unsigned i = 0; i < 24; ++i)
            Store(values[i], r[4] + offsets[i]);
    }
    void ReverseWinding() {
        auto &r = s.r;
        Load(11, r[3] + 24u);
        Load(0, r[3] + 12u);
        Load(13, r[3] + 16u);
        Load(12, r[3] + 20u);
        Store(11, r[3] + 12u);
        Load(11, r[3] + 28u);
        Store(11, r[3] + 16u);
        Load(11, r[3] + 32u);
        Store(11, r[3] + 20u);
        Store(0, r[3] + 24u);
        Store(13, r[3] + 28u);
        Store(12, r[3] + 32u);
    }
    void Area() {
        auto &r = s.r;
        Load(0, r[3]);
        r[11] = 0xffffffff82020000ull;
        Load(11, r[3] + 12u);
        Load(10, r[3] + 24u);
        Single(11, F(0) - F(11));
        Load(13, r[3] + 4u);
        Single(0, F(0) - F(10));
        Load(9, r[3] + 28u);
        Load(12, r[3] + 8u);
        Single(10, F(13) - F(9));
        Load(8, r[3] + 32u);
        Single(9, F(12) - F(8));
        Load(7, r[3] + 16u);
        Load(8, r[3] + 20u);
        Single(13, F(13) - F(7));
        Single(12, F(12) - F(8));
        Single(8, F(11) * F(9));
        Single(7, F(0) * F(13));
        Single(6, F(12) * F(10));
        Single(0, F(12) * F(0) - F(8));
        Single(11, F(11) * F(10) - F(7));
        Single(13, F(13) * F(9) - F(6));
        Single(0, F(0) * F(0));
        Single(0, F(11) * F(11) + F(0));
        Single(0, F(13) * F(13) + F(0));
        Single(13, std::sqrt(F(0)));
        Load(0, r[11] - 1552u);
        Single(1, F(13) * F(0));
    }
    void Expand() {
        auto &r = s.r;
        r[11] = r[3] + 8u;
        Load(12, r[3] + 12u);
        Load(11, r[3] + 16u);
        r[10] = 0xffffffff82000000ull;
        Load(0, r[3]);
        r[9] = 0xffffffff82000000ull;
        Load(13, r[3] + 4u);
        Single(0, F(12) + F(0));
        Single(13, F(11) + F(13));
        Load(12, r[3] + 20u);
        Load(11, r[11]);
        r[7] = Address(r[5]) & 255u;
        Single(12, F(12) + F(11));
        Load(10, r[3] + 28u);
        Load(9, r[3] + 32u);
        r[8] = 3;
        Load(11, r[3] + 24u);
        Load(7, r[9] + 30596u);
        Single(11, F(11) + F(0));
        Load(0, r[10] + 3872u);
        Single(13, F(10) + F(13));
        r[10] = 0xffffffff82000000ull;
        Single(12, F(9) + F(12));
        Load(6, r[10] + 3664u);
        Single(10, F(11) * F(0));
        Single(9, F(13) * F(0));
        Single(8, F(12) * F(0));
        do {
            r[10] = r[11] - 8u;
            Load(0, r[11]);
            r[9] = r[11] - 4u;
            Single(12, F(0) - F(8));
            Zero(r[7]);
            Load(0, r[10]);
            Load(13, r[9]);
            Single(0, F(0) - F(10));
            Single(13, F(13) - F(9));
            if (!s.cr6.eq) {
                Single(11, F(13) * F(13));
                Single(11, F(12) * F(12) + F(11));
                Single(11, F(0) * F(0) + F(11));
                Compare(F(11), F(6));
                if (!s.cr6.eq) {
                    Single(11, std::sqrt(F(11)));
                    Single(11, F(7) / F(11));
                    Single(0, F(0) * F(11));
                    Single(13, F(13) * F(11));
                    Single(12, F(12) * F(11));
                }
            }
            Single(12, F(12) * F(1));
            Load(11, r[11]);
            Single(0, F(0) * F(1));
            Load(5, r[10]);
            Single(13, F(13) * F(1));
            Load(4, r[9]);
            --r[8];
            Zero(r[8]);
            Single(12, F(11) + F(12));
            Store(12, r[11]);
            Single(0, F(5) + F(0));
            r[11] += 12u;
            Single(13, F(4) + F(13));
            Store(0, r[10]);
            Store(13, r[9]);
        } while (!s.cr6.eq);
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &m, float_triplet_transfer::NativeServices &native,
           Registers &s) {
    Geometry g{m, native, s};
    switch (entry) {
    case 0x82bddf08u:
        g.Cube();
        return true;
    case 0x82bddfc8u:
        g.Contains();
        return true;
    case 0x82bde040u:
        g.Corners();
        return true;
    case 0x82bde108u:
        g.ReverseWinding();
        return true;
    case 0x82bde140u:
        g.Area();
        return true;
    case 0x82bde1b8u:
        g.Expand();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::geometry_primitives61
