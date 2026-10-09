#include "lo_semantics/mesh_polygon_plane61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_polygon_plane61 {
namespace {
using recovery_abi::Address;
struct Polygon {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &native;
    Registers &s;
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            native.SetHostFpControl(s.cached_fp_control);
        }
    }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Set(unsigned i, double v) { s.fpr_bits[i] = std::bit_cast<std::uint64_t>(v); }
    void Single(unsigned i, double v) { Set(i, double(float(v))); }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        Set(i, double(std::bit_cast<float>(m.ReadU32(Address(p)))));
    }
    void Store(unsigned i, std::uint64_t p) {
        m.WriteU32(Address(p), std::bit_cast<std::uint32_t>(float(F(i))));
    }
    static std::uint32_t Shift(std::uint64_t v, unsigned n) { return Address(v) << n; }
    void Reverse() {
        auto &r = s.r;
        Compare(r[3]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        Compare(r[4]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[7] = Address(r[3]) >> 1;
        r[10] = 0;
        Compare(r[7]);
        if (!s.cr6.eq) {
            r[11] = r[3] + r[4] - 1;
            do {
                r[9] = r[10] + r[4];
                r[6] = m.ReadU8(Address(r[11]));
                ++r[10];
                Compare(r[10], r[7]);
                r[8] = m.ReadU8(Address(r[9]));
                m.WriteU8(Address(r[9]), std::uint8_t(r[6]));
                m.WriteU8(Address(r[11]), std::uint8_t(r[8]));
                --r[11];
            } while (s.cr6.lt);
        }
        r[3] = 1;
    }
    void SaveFp() {
        Gradual();
        for (unsigned i = 21; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(s.r[12] - 8 * (32 - i)), s.fpr_bits[i]);
    }
    void RestoreFp() {
        Gradual();
        for (unsigned i = 21; i < 32; ++i)
            s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(s.r[12] - 8 * (32 - i)));
    }
    void Plane() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bd9398u;
        for (unsigned i = 26; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        m.WriteU32(Address(r[1] - 8), Address(r[12]));
        r[12] = r[1] - 56;
        s.lr = 0x82bd93a0u;
        SaveFp();
        bool ok = Body();
        r[12] = r[1] - 56;
        s.lr = ok ? 0x82bd972cu : 0x82bd973cu;
        RestoreFp();
        for (unsigned i = 26; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = m.ReadU32(Address(r[1] - 8));
        s.lr = r[12];
    }
    void FourEdges() {
        auto &r = s.r;
        // Four consecutive Newell edges, with the guest's FP staging preserved.
        r[10] = m.ReadU8(Address(r[11]));
        --r[31];
        r[9] = m.ReadU8(Address(r[3]) + Address(r[5]));
        r[7] = std::rotl(Address(r[10]), 1);
        r[3] = m.ReadU8(Address(r[11] + 1));
        r[8] = std::rotl(Address(r[9]), 1);
        r[29] = m.ReadU8(Address(r[11] + 2));
        r[30] = r[10];
        r[10] += r[7];
        r[8] += r[9];
        r[9] = Shift(r[10], 2);
        r[10] = Shift(r[8], 2);
        r[8] = r[9] + r[6];
        r[9] = std::rotl(Address(r[3]), 1);
        r[7] = r[10] + r[6];
        r[10] = std::rotl(Address(r[30]), 1);
        r[9] += r[3];
        r[3] = r[30] + r[10];
        Load(8, r[8] + 4);
        r[10] = Shift(r[9], 2);
        Load(11, r[8] + 8);
        Load(9, r[7] + 4);
        Set(25, F(8));
        Load(7, r[8]);
        r[10] += r[6];
        Load(6, r[7]);
        Single(22, F(9) - F(8));
        Single(21, F(6) + F(7));
        Load(10, r[7] + 8);
        Set(27, F(7));
        r[9] = Shift(r[3], 2);
        Set(23, F(11));
        r[8] = m.ReadU8(Address(r[11] + 3));
        Single(7, F(6) - F(7));
        Load(29, r[10]);
        Single(8, F(8) + F(9));
        r[3] = r[9] + r[6];
        Single(26, F(11) + F(10));
        Load(6, r[10] + 8);
        Single(24, F(10) - F(11));
        r[9] = std::rotl(Address(r[29]), 1);
        Set(28, F(29));
        r[7] = r[29];
        Single(10, F(25) + F(1));
        Load(1, r[10] + 8);
        Load(5, r[3] + 8);
        r[9] += r[29];
        Load(4, r[3] + 4);
        Single(6, F(6) + F(5));
        Single(11, F(27) + F(2));
        Load(2, r[10] + 4);
        Single(9, F(23) + F(31));
        Load(31, r[10] + 4);
        Single(5, F(5) - F(1));
        Load(3, r[3]);
        Single(12, F(7) * F(8) + F(12));
        r[9] = Shift(r[9], 2);
        Set(8, F(29));
        r[3] = r[11] + r[28];
        Set(1, F(31));
        r[9] += r[6];
        Single(2, F(4) - F(2));
        Single(0, F(22) * F(26) + F(0));
        Single(7, F(29) + F(3));
        Single(4, F(31) + F(4));
        Load(31, r[10] + 8);
        Single(3, F(3) - F(28));
        r[10] = m.ReadU8(Address(r[11] + 1));
        Single(13, F(21) * F(24) + F(13));
        Load(28, r[9] + 4);
        Single(9, F(31) + F(9));
        r[11] += 4;
        Single(11, F(8) + F(11));
        Single(10, F(1) + F(10));
        Load(1, r[9]);
        Set(29, F(1));
        Single(8, F(6) * F(2) + F(0));
        Load(2, r[9] + 8);
        Set(31, F(2));
        Single(6, F(4) * F(3) + F(12));
        Single(7, F(5) * F(7) + F(13));
        r[30] = std::rotl(Address(r[10]), 1);
        Set(0, F(1));
        Set(13, F(28));
        Compare(r[31]);
        r[10] += r[30];
        Set(12, F(2));
        r[30] = std::rotl(Address(r[8]), 1);
        Set(27, F(28));
        r[10] = Shift(r[10], 2);
        r[8] += r[30];
        r[9] = r[10] + r[6];
        r[10] = std::rotl(Address(r[7]), 1);
        r[7] += r[10];
        r[10] = Shift(r[8], 2);
        Single(5, F(0) + F(11));
        Single(4, F(13) + F(10));
        Load(0, r[9] + 8);
        r[10] += r[6];
        Single(3, F(12) + F(9));
        Load(13, r[9] + 4);
        r[8] = Shift(r[7], 2);
        Load(12, r[9]);
        Single(28, F(13) - F(28));
        Single(1, F(12) + F(1));
        r[8] += r[6];
        Single(2, F(0) + F(2));
        Load(26, r[10]);
        Single(12, F(12) - F(29));
        Single(31, F(0) - F(31));
        Load(0, r[10] + 8);
        Single(29, F(13) + F(27));
        Load(13, r[10] + 4);
        Set(25, F(26));
        Load(11, r[8] + 8);
        Load(10, r[8] + 4);
        Single(27, F(11) + F(0));
        Load(9, r[8]);
        Single(11, F(11) - F(0));
        Single(24, F(10) - F(13));
        Single(10, F(10) + F(13));
        Single(26, F(26) + F(9));
        Single(0, F(28) * F(2) + F(8));
        Load(28, r[10] + 8);
        Set(8, F(13));
        Single(13, F(1) * F(31) + F(7));
        Single(12, F(12) * F(29) + F(6));
        Set(23, F(25));
        Single(9, F(9) - F(25));
        Single(31, F(28) + F(3));
        Single(0, F(24) * F(27) + F(0));
        Single(1, F(8) + F(4));
        Single(13, F(11) * F(26) + F(13));
        Single(2, F(23) + F(5));
        Single(12, F(10) * F(9) + F(12));
    }
    void OneEdge() {
        auto &r = s.r;
        r[11] = m.ReadU8(Address(r[27]) + Address(r[5]));
        r[10] = m.ReadU8(Address(r[3]) + Address(r[5]));
        r[3] = r[27];
        r[8] = std::rotl(Address(r[11]), 1);
        r[9] = std::rotl(Address(r[10]), 1);
        r[11] += r[8];
        r[10] += r[9];
        r[11] = Shift(r[11], 2);
        r[10] = Shift(r[10], 2);
        r[11] += r[6];
        r[10] += r[6];
        ++r[27];
        Compare(r[27], r[4]);
        Load(8, r[11] + 4);
        Load(9, r[10] + 4);
        Set(3, F(8));
        Load(10, r[10] + 8);
        Single(29, F(9) - F(8));
        Load(7, r[11]);
        Single(9, F(8) + F(9));
        Load(11, r[11] + 8);
        Set(5, F(7));
        Load(6, r[10]);
        Single(4, F(10) + F(11));
        Single(8, F(6) + F(7));
        Single(11, F(10) - F(11));
        Load(10, r[11] + 8);
        Single(7, F(6) - F(7));
        Single(31, F(10) + F(31));
        Single(1, F(3) + F(1));
        Single(2, F(5) + F(2));
        Single(0, F(29) * F(4) + F(0));
        Single(13, F(8) * F(11) + F(13));
        Single(12, F(7) * F(9) + F(12));
    }
    bool Body() {
        auto &r = s.r;
        r[26] = r[3];
        for (unsigned i = 4; i <= 6; ++i) {
            Compare(r[i]);
            if (s.cr6.eq) {
                r[3] = 0;
                return false;
            }
        }
        r[11] = 0xffffffff82000000ull;
        r[3] = r[4] - 1;
        r[27] = 0;
        auto count = std::int32_t(r[4]);
        s.cr6 = {std::uint8_t(count < 4), std::uint8_t(count > 4), std::uint8_t(count == 4),
                 s.xer_so};
        Load(30, r[11] + 3664);
        for (unsigned f : {2u, 1u, 31u, 0u, 13u, 12u})
            Set(f, F(30));
        if (!s.cr6.lt) {
            r[10] = r[4] - 4;
            r[11] = r[5];
            r[10] = Address(r[10]) >> 2;
            s.xer_ca = Address(r[5]) <= 3;
            r[28] = 3 - r[5];
            r[31] = r[10] + 1;
            r[27] = Shift(r[31], 2);
            do {
                FourEdges();
            } while (!s.cr6.eq);
        }
        Compare(r[27], r[4]);
        if (s.cr6.lt)
            do {
                OneEdge();
            } while (s.cr6.lt);
        Gradual();
        Single(11, F(13) * F(13));
        Single(11, F(12) * F(12) + F(11));
        Single(11, F(0) * F(0) + F(11));
        bool u = std::isnan(F(11)) || std::isnan(F(30));
        s.cr6 = {std::uint8_t(!u && F(11) < F(30)), std::uint8_t(!u && F(11) > F(30)),
                 std::uint8_t(!u && F(11) == F(30)), std::uint8_t(u)};
        if (!s.cr6.eq) {
            Single(10, std::sqrt(F(11)));
            r[11] = 0xffffffff82000000ull;
            Load(11, r[11] + 30596);
            Single(11, F(11) / F(10));
            Single(0, F(11) * F(0));
            Single(13, F(11) * F(13));
            Single(12, F(11) * F(12));
        }
        r[11] = Address(r[4]);
        Gradual();
        Single(11, F(1) * F(13));
        Store(13, r[26] + 4);
        r[3] = 1;
        Store(0, r[26]);
        Store(12, r[26] + 8);
        recovery_abi::WriteU64(m, Address(r[1] - 160), r[11]);
        Single(13, F(31) * F(12) + F(11));
        Single(0, F(2) * F(0) + F(13));
        s.fpr_bits[13] = recovery_abi::ReadU64(m, Address(r[1] - 160));
        Set(13, double(std::bit_cast<std::int64_t>(s.fpr_bits[13])));
        Single(13, F(13));
        Single(0, F(0) / F(13));
        s.fpr_bits[0] ^= 0x8000000000000000ull;
        Store(0, r[26] + 12);
        return true;
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, float_triplet_transfer::NativeServices &n,
           Registers &s) {
    Polygon p{m, n, s};
    if (e == 0x82bd9390u) {
        p.Plane();
        return true;
    }
    if (e == 0x82bc3880u) {
        p.Reverse();
        return true;
    }
    return false;
}
} // namespace lo::semantic::gpu::mesh_polygon_plane61
