#include "lo_semantics/mesh_bounds_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_bounds_math61 {
namespace {
using recovery_abi::Address;
struct Sphere {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &fp;
    Registers &s;
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Move(unsigned i, unsigned j) { s.fpr_bits[i] = s.fpr_bits[j]; }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(v)));
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] =
            std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(m.ReadU32(Address(p)))));
    }
    void Store(unsigned i, std::uint64_t p) {
        m.WriteU32(Address(p), std::bit_cast<std::uint32_t>(float(F(i))));
    }
    void Compare(double a, double b) {
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
    }
    void Integer(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Extreme(std::uint64_t p) {
        Load(0, p);
        Compare(F(0), F(6));
        if (s.cr6.lt) {
            Load(5, p + 4);
            Move(6, 0);
            Load(4, p + 8);
        }
        Compare(F(0), F(10));
        if (s.cr6.gt) {
            Load(10, p);
            Load(9, p + 4);
            Load(8, p + 8);
        }
        Load(0, p + 4);
        Compare(F(0), F(2));
        if (s.cr6.lt) {
            Load(28, p);
            Move(2, 0);
            Load(27, p + 8);
        }
        Compare(F(0), F(3));
        if (s.cr6.gt) {
            Load(30, p);
            Load(3, p + 4);
            Load(29, p + 8);
        }
        Load(0, p + 8);
        Compare(F(0), F(31));
        if (s.cr6.lt) {
            Load(24, p);
            Move(31, 0);
            Load(23, p + 4);
        }
        Compare(F(0), F(1));
        if (s.cr6.gt) {
            Load(26, p);
            Load(25, p + 4);
            Load(1, p + 8);
        }
    }
    void SeedDiameter() {
        // Compare squared distances between the three extreme-point pairs.
        Single(0, F(8) - F(4));
        Single(13, F(9) - F(5));
        Single(12, F(10) - F(6));
        Move(7, 0);
        Move(11, 0);
        Single(22, F(13) * F(13));
        Move(21, 12);
        Move(20, 12);
        Single(13, F(3) - F(2));
        Single(0, F(29) - F(27));
        Single(12, F(30) - F(28));
        Single(11, F(11) * F(7) + F(22));
        Move(7, 0);
        Move(22, 0);
        Move(19, 12);
        Single(0, F(1) - F(31));
        Single(11, F(21) * F(20) + F(11));
        Single(21, F(13) * F(13));
        Single(13, F(25) - F(23));
        Move(20, 12);
        Single(12, F(26) - F(24));
        Single(7, F(7) * F(22) + F(21));
        Single(13, F(13) * F(13));
        Single(7, F(20) * F(19) + F(7));
        Single(0, F(0) * F(0) + F(13));
        Compare(F(7), F(11));
        Single(0, F(12) * F(12) + F(0));
        if (s.cr6.gt) {
            Move(11, 7);
            Move(6, 28);
            Move(5, 2);
            Move(4, 27);
            Move(10, 30);
            Move(9, 3);
            Move(8, 29);
        }
        Compare(F(0), F(11));
        if (s.cr6.gt) {
            Move(6, 24);
            Move(5, 23);
            Move(4, 31);
            Move(10, 26);
            Move(9, 25);
            Move(8, 1);
        }
        auto &r = s.r;
        Single(0, F(9) + F(5));
        r[11] = 0xffffffff82020000ull;
        Single(13, F(8) + F(4));
        Integer(r[4]);
        Single(12, F(10) + F(6));
        Load(7, r[11] - 1552);
        Single(0, F(0) * F(7));
        Store(0, r[3] + 4);
        Single(13, F(13) * F(7));
        Store(13, r[3] + 8);
        Single(12, F(12) * F(7));
        Store(12, r[3]);
        Single(11, F(9) - F(0));
        Single(9, F(8) - F(13));
        Single(10, F(10) - F(12));
        Single(0, F(11) * F(11));
        Single(0, F(9) * F(9) + F(0));
        Single(11, F(10) * F(10) + F(0));
        Single(0, std::sqrt(F(11)));
        Store(0, r[3] + 12);
    }
    void Enclose() {
        auto &r = s.r;
        r[9] = 0xffffffff82000000ull;
        r[11] = r[5] + 8;
        r[10] = r[4];
        Load(6, r[9] + 30596);
        do {
            Load(10, r[3] + 4);
            Load(13, r[11] - 4);
            Single(13, F(13) - F(10));
            Load(9, r[3] + 8);
            Load(0, r[11]);
            Single(0, F(0) - F(9));
            Load(8, r[3]);
            Load(12, r[11] - 8);
            Single(12, F(12) - F(8));
            Single(13, F(13) * F(13));
            Single(0, F(0) * F(0) + F(13));
            Single(0, F(12) * F(12) + F(0));
            Compare(F(0), F(11));
            if (s.cr6.gt) {
                Single(13, std::sqrt(F(0)));
                Load(12, r[3] + 12);
                Single(0, F(13) + F(12));
                Single(12, F(6) / F(13));
                Single(0, F(0) * F(7));
                Store(0, r[3] + 12);
                Load(5, r[11] - 8);
                Single(13, F(13) - F(0));
                Single(9, F(0) * F(9));
                Single(11, F(0) * F(0));
                Single(5, F(13) * F(5));
                Single(8, F(0) * F(8) + F(5));
                Single(8, F(8) * F(12));
                Store(8, r[3]);
                Load(8, r[11] - 4);
                Single(8, F(8) * F(13));
                Single(0, F(0) * F(10) + F(8));
                Single(0, F(0) * F(12));
                Store(0, r[3] + 4);
                Load(0, r[11]);
                Single(0, F(0) * F(13) + F(9));
                Single(0, F(0) * F(12));
                Store(0, r[3] + 8);
            }
            --r[10];
            r[11] += 12;
            Integer(r[10]);
        } while (!s.cr6.eq);
    }
    void Body() {
        auto &r = s.r;
        Integer(r[5]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = 0xffffffff82000000ull;
        Load(4, r[1] - 120);
        Load(5, r[1] - 124);
        r[9] = 0;
        Load(8, r[1] - 120); // signed count branch, retained through the initialization
        auto n = std::bit_cast<std::int32_t>(Address(r[4]));
        s.cr6 = {std::uint8_t(n < 4), std::uint8_t(n > 4), std::uint8_t(n == 4), s.xer_so};
        Load(9, r[1] - 124);
        Load(27, r[1] - 120);
        Load(31, r[11] + 3596);
        r[11] = 0xffffffff82000000ull;
        Load(28, r[1] - 128);
        Move(2, 31);
        Load(29, r[1] - 120);
        Move(6, 31);
        Load(30, r[1] - 128);
        Load(23, r[1] - 124);
        Load(1, r[11] + 3428);
        Load(24, r[1] - 128);
        Move(3, 1);
        Load(25, r[1] - 124);
        Move(10, 1);
        Load(26, r[1] - 128);
        if (!s.cr6.lt) {
            r[10] = r[4] - 4;
            r[11] = r[5] + 20;
            r[10] = (Address(r[10]) >> 2) + 1;
            r[9] = Address(r[10]) << 2;
            do {
                for (unsigned i = 0; i < 4; ++i)
                    Extreme(r[11] - 20 + 12 * i);
                --r[10];
                r[11] += 48;
                Integer(r[10]);
            } while (!s.cr6.eq);
        }
        Integer(r[9], r[4]);
        if (s.cr6.lt) {
            r[11] = Address(r[9]) << 1;
            r[10] = r[4] - r[9];
            r[11] += r[9];
            r[11] = Address(r[11]) << 2;
            r[11] += r[5] + 8;
            do {
                Extreme(r[11] - 8);
                --r[10];
                r[11] += 12;
                Integer(r[10]);
            } while (!s.cr6.eq);
        }
        SeedDiameter();
        if (!s.cr6.eq)
            Enclose();
        r[3] = 1;
    }
    void SaveSupport(GuestAddress continuation) {
        auto &r = s.r;
        r[12] = s.lr;
        m.WriteU32(Address(r[1] - 8), Address(r[12]));
        r[12] = r[1] - 8;
        s.lr = continuation;
        for (unsigned i = 23; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[12] - 8 * (32 - i)), s.fpr_bits[i]);
    }
    void RestoreSupport(GuestAddress continuation) {
        auto &r = s.r;
        r[12] = r[1] - 8;
        s.lr = continuation;
        for (unsigned i = 23; i < 32; ++i)
            s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(r[12] - 8 * (32 - i)));
        r[12] = m.ReadU32(Address(r[1] - 8));
        s.lr = r[12];
    }
    void Radius(unsigned x, unsigned y, unsigned z) {
        Single(11, F(x) * F(x));
        Single(11, F(y) * F(y) + F(11));
        Single(11, F(z) * F(z) + F(11));
        Single(10, std::sqrt(F(11)));
        Load(11, s.r[11] + 27160);
        Single(11, F(10) + F(11));
        Store(11, s.r[3] + 12);
    }
    void Pair() {
        auto &r = s.r;
        Load(0, r[5]);
        r[11] = 0xffffffff82020000ull;
        Load(13, r[4]);
        Single(13, F(0) - F(13));
        Load(11, r[4] + 8);
        Load(12, r[5] + 8);
        Single(12, F(12) - F(11));
        Load(0, r[5] + 4);
        Load(11, r[4] + 4);
        Single(11, F(0) - F(11));
        Load(0, r[11] - 1552);
        r[11] = 0xffffffff820d0000ull;
        Single(13, F(13) * F(0));
        Single(12, F(12) * F(0));
        Single(0, F(11) * F(0));
        Radius(13, 12, 0);
        Load(11, r[4]);
        Single(13, F(11) + F(13));
        Load(10, r[4] + 4);
        Load(11, r[4] + 8);
        Single(0, F(0) + F(10));
        Single(12, F(11) + F(12));
        Store(13, r[3]);
        Store(0, r[3] + 4);
        Store(12, r[3] + 8);
    }
    void Triangle() {
        SaveSupport(0x82bc9610u);
        auto &r = s.r;
        // Edge vectors and their cross product; retain staged products because
        // changing evaluation order changes binary32 circumcenter rounding.
        Load(13, r[4]);
        r[11] = 0xffffffff821c0000ull;
        Load(9, r[4] + 4);
        Load(10, r[4] + 8);
        Load(0, r[5]);
        Load(12, r[5] + 4);
        Single(0, F(0) - F(13));
        Load(11, r[5] + 8);
        Single(12, F(12) - F(9));
        Load(8, r[6]);
        Single(11, F(11) - F(10));
        Load(7, r[6] + 4);
        Single(13, F(8) - F(13));
        Load(6, r[6] + 8);
        Single(9, F(7) - F(9));
        Single(10, F(6) - F(10));
        Single(2, F(12) * F(12));
        Single(8, F(12) * F(13));
        Single(5, F(9) * F(11));
        Single(3, F(9) * F(0));
        Single(6, F(10) * F(12));
        Single(4, F(10) * F(0));
        Single(7, F(11) * F(13));
        Single(1, F(9) * F(9));
        Single(2, F(11) * F(11) + F(2));
        Single(29, F(3) - F(8));
        Single(28, F(6) - F(5));
        Single(31, F(6) - F(5));
        Single(30, F(7) - F(4));
        Single(27, F(7) - F(4));
        Single(7, F(7) - F(4));
        Single(1, F(10) * F(10) + F(1));
        Single(26, F(3) - F(8));
        Single(6, F(6) - F(5));
        Single(2, F(0) * F(0) + F(2));
        Single(4, F(29) * F(13));
        Single(24, F(11) * F(28));
        Single(8, F(3) - F(8));
        Single(5, F(30) * F(10));
        Single(23, F(27) * F(0));
        Single(3, F(9) * F(31));
        Single(1, F(13) * F(13) + F(1));
        Single(25, F(26) * F(12));
        Single(10, F(10) * F(31) - F(4));
        Single(0, F(26) * F(0) - F(24));
        Single(9, F(29) * F(9) - F(5));
        Single(12, F(28) * F(12) - F(23));
        Single(13, F(30) * F(13) - F(3));
        Single(11, F(27) * F(11) - F(25));
        Single(10, F(10) * F(2));
        Single(0, F(0) * F(1));
        Single(9, F(9) * F(2));
        Single(12, F(12) * F(1));
        Single(13, F(13) * F(2));
        Single(11, F(11) * F(1));
        Single(10, F(0) + F(10));
        Single(0, F(7) * F(7));
        Single(13, F(12) + F(13));
        Single(11, F(11) + F(9));
        Single(0, F(8) * F(8) + F(0));
        Single(12, F(6) * F(6) + F(0));
        // Divide the cross-product numerator by twice the squared normal.
        Load(0, r[11] - 21900);
        r[11] = 0xffffffff82000000ull;
        Single(12, F(12) * F(0));
        Load(0, r[11] + 30596);
        r[11] = 0xffffffff820d0000ull;
        Single(0, F(0) / F(12));
        Single(12, F(10) * F(0));
        Single(13, F(13) * F(0));
        Single(0, F(11) * F(0));
        Radius(12, 13, 0);
        Load(11, r[4]);
        Single(0, F(11) + F(0));
        Load(10, r[4] + 4);
        Load(11, r[4] + 8);
        Single(12, F(12) + F(10));
        Store(0, r[3]);
        Single(13, F(11) + F(13));
        Store(12, r[3] + 4);
        Store(13, r[3] + 8);
        RestoreSupport(0x82bc9770u);
    }
    void Tetrahedron() {
        SaveSupport(0x82bc9790u);
        auto &r = s.r;
        // Three edges from the first point. Cofactors solve the equal-distance
        // equations; the determinant and each numerator keep guest rounding.
        Load(10, r[4] + 4);
        r[11] = 0xffffffff821c0000ull;
        Load(7, r[6] + 4);
        Single(7, F(7) - F(10));
        Load(12, r[4]);
        Load(0, r[7]);
        Load(9, r[5]);
        Single(13, F(0) - F(12));
        Load(8, r[6]);
        Single(0, F(9) - F(12));
        Load(11, r[5] + 4);
        Single(9, F(8) - F(12));
        Load(6, r[4] + 8);
        Single(8, F(11) - F(10));
        Load(3, r[7] + 8);
        Single(11, F(3) - F(6));
        Load(5, r[6] + 8);
        Single(12, F(5) - F(6));
        Load(4, r[7] + 4);
        Load(2, r[5] + 8);
        Single(10, F(4) - F(10));
        Single(1, F(7) * F(7));
        Single(6, F(2) - F(6));
        Single(29, F(7) * F(13));
        Single(5, F(0) * F(0));
        Single(4, F(13) * F(8));
        Single(31, F(11) * F(9));
        Single(28, F(11) * F(8));
        Single(2, F(10) * F(12));
        Single(26, F(12) * F(12) + F(1));
        Single(27, F(6) * F(13));
        Single(1, F(7) * F(6));
        Single(5, F(6) * F(6) + F(5));
        Single(29, F(10) * F(9) - F(29));
        Single(23, F(10) * F(10));
        Single(3, F(11) * F(7));
        Single(30, F(12) * F(13) - F(31));
        Single(28, F(10) * F(6) - F(28));
        Single(31, F(6) * F(9));
        Single(6, F(9) * F(8));
        Single(9, F(9) * F(9) + F(26));
        Single(26, -(F(10) * F(0) - F(4)));
        Single(4, F(4) * F(12));
        Single(27, F(11) * F(0) - F(27));
        Single(5, F(8) * F(8) + F(5));
        Single(8, F(12) * F(8) - F(1));
        Single(25, F(3) - F(2));
        Single(24, -(F(12) * F(0) - F(31)));
        Single(7, F(7) * F(0) - F(6));
        Single(28, F(28) * F(9));
        Single(10, F(31) * F(10) + F(4));
        Single(27, F(27) * F(9));
        Single(9, F(26) * F(9));
        Single(26, F(11) * F(11) + F(23));
        Single(30, F(30) * F(5));
        Single(29, F(29) * F(5));
        Single(5, F(25) * F(5));
        Single(10, F(3) * F(0) + F(10));
        Single(12, F(13) * F(13) + F(26));
        Single(13, -(F(1) * F(13) - F(10)));
        Single(13, -(F(6) * F(11) - F(13)));
        Single(11, F(7) * F(12));
        Single(10, -(F(2) * F(0) - F(13)));
        Load(0, r[11] - 21900);
        Single(13, F(24) * F(12));
        r[11] = 0xffffffff82000000ull;
        Single(12, F(8) * F(12));
        Single(11, F(11) + F(9));
        Single(10, F(10) * F(0));
        Load(0, r[11] + 30596);
        Single(13, F(13) + F(27));
        Single(12, F(12) + F(28));
        Single(11, F(11) + F(29));
        Single(0, F(0) / F(10));
        Single(13, F(13) + F(30));
        Single(10, F(12) + F(5));
        Single(12, F(11) * F(0));
        Single(13, F(13) * F(0));
        Single(0, F(10) * F(0));
        Single(11, F(13) * F(13));
        Single(11, F(12) * F(12) + F(11));
        Single(11, F(0) * F(0) + F(11));
        r[11] = 0xffffffff820d0000ull;
        Single(10, std::sqrt(F(11)));
        Load(11, r[11] + 27160);
        Single(11, F(10) + F(11));
        Store(11, r[3] + 12);
        Load(11, r[4]);
        Single(0, F(11) + F(0));
        Load(10, r[4] + 4);
        Load(11, r[4] + 8);
        Single(13, F(13) + F(10));
        Single(12, F(11) + F(12));
        Store(0, r[3]);
        Store(13, r[3] + 4);
        Store(12, r[3] + 8);
        RestoreSupport(0x82bc9918u);
    }
    void ReadSphere(std::uint64_t p) {
        Load(0, p + 12);
        Load(13, p);
        Load(12, p + 4);
        Load(11, p + 8);
    }
    void Recursive() {
        // Move an outside point to the support prefix, solve the earlier points
        // with that additional boundary constraint, then resume the scan.
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bc9930u;
        for (unsigned i = 26; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        m.WriteU32(Address(r[1] - 8), Address(r[12]));
        Gradual();
        recovery_abi::WriteU64(m, Address(r[1] - 64), s.fpr_bits[31]);
        auto old = r[1];
        r[1] -= 192;
        m.WriteU32(Address(r[1]), Address(old));
        r[11] = 0xffffffff82000000ull;
        r[28] = r[6];
        r[27] = r[3];
        r[30] = r[4];
        r[26] = r[5];
        Load(0, r[11] + 3440);
        r[11] = 0xffffffff82000000ull;
        Integer(r[28], 4);
        Load(31, r[11] + 3664);
        Move(11, 31);
        Move(12, 31);
        Move(13, 31);
        bool complete = false;
        if (!s.cr6.gt) {
            r[12] = 0xffffffff82bc9984ull;
            r[0] = Address(r[28]) << 2;
            r[0] = m.ReadU32(Address(r[12] + r[0]));
            s.ctr = r[0];
            switch (Address(r[28])) {
            case 0:
                Move(13, 31);
                Move(12, 31);
                Move(11, 31);
                break;
            case 1:
                r[11] = m.ReadU32(Address(r[30] - 4));
                r[10] = 0xffffffff820d0000ull;
                Load(0, r[10] + 27160);
                Load(13, r[11]);
                Load(12, r[11] + 4);
                Load(11, r[11] + 8);
                break;
            case 2:
                r[3] = r[1] + 80;
                r[5] = m.ReadU32(Address(r[30] - 8));
                r[4] = m.ReadU32(Address(r[30] - 4));
                s.lr = 0x82bc99d4u;
                Pair();
                ReadSphere(r[3]);
                break;
            case 3:
                r[3] = r[1] + 96;
                r[6] = m.ReadU32(Address(r[30] - 12));
                r[5] = m.ReadU32(Address(r[30] - 8));
                r[4] = m.ReadU32(Address(r[30] - 4));
                s.lr = 0x82bc9abcu;
                Triangle();
                ReadSphere(r[3]);
                break;
            case 4:
                r[3] = r[1] + 112;
                r[7] = m.ReadU32(Address(r[30] - 16));
                r[6] = m.ReadU32(Address(r[30] - 12));
                r[5] = m.ReadU32(Address(r[30] - 8));
                r[4] = m.ReadU32(Address(r[30] - 4));
                s.lr = 0x82bc9ad8u;
                Tetrahedron();
                r[11] = r[3];
                ReadSphere(r[11]);
                complete = true;
                break;
            }
        }
        if (!complete) {
            r[31] = 0;
            Integer(r[26]);
            if (!s.cr6.eq) {
                r[29] = r[30];
                do {
                    r[11] = m.ReadU32(Address(r[29]));
                    Load(10, r[11] + 4);
                    Single(10, F(10) - F(12));
                    Load(9, r[11] + 8);
                    Single(9, F(9) - F(11));
                    Load(8, r[11]);
                    Single(8, F(8) - F(13));
                    Single(10, F(10) * F(10));
                    Single(10, F(9) * F(9) + F(10));
                    Single(10, F(8) * F(8) + F(10));
                    Single(10, -(F(0) * F(0) - F(10)));
                    Compare(F(10), F(31));
                    if (s.cr6.gt) {
                        s.ctr = r[31];
                        Integer(r[31]);
                        if (!s.cr6.eq) {
                            r[11] = r[29];
                            do {
                                r[10] = r[11] - 4;
                                r[9] = m.ReadU32(Address(r[11]));
                                r[8] = m.ReadU32(Address(r[10]));
                                m.WriteU32(Address(r[10]), Address(r[9]));
                                m.WriteU32(Address(r[11]), Address(r[8]));
                                r[11] -= 4;
                                --s.ctr;
                            } while (Address(s.ctr));
                        }
                        r[6] = r[28] + 1;
                        r[5] = r[31];
                        r[4] = r[30] + 4;
                        r[3] = r[1] + 112;
                        s.lr = 0x82bc9a68u;
                        Recursive();
                        ReadSphere(r[3]);
                    }
                    ++r[31];
                    r[29] += 4;
                    Integer(r[31], r[26]);
                } while (s.cr6.lt);
            }
        }
        Gradual();
        Store(11, r[27] + 8);
        r[3] = r[27];
        Store(12, r[27] + 4);
        Store(13, r[27]);
        Store(0, r[27] + 12);
        r[1] += 192;
        s.fpr_bits[31] = recovery_abi::ReadU64(m, Address(r[1] - 64));
        for (unsigned i = 26; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = m.ReadU32(Address(r[1] - 8));
        s.lr = r[12];
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        m.WriteU32(Address(r[1] - 8), Address(r[12]));
        r[12] = r[1] - 8;
        s.lr = 0x82bc9050u;
        for (unsigned i = 19; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[12] - 8 * (32 - i)), s.fpr_bits[i]);
        Body();
        r[12] = r[1] - 8;
        s.lr = r[3] ? 0x82bc9574u : 0x82bc9064u;
        for (unsigned i = 19; i < 32; ++i)
            s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(r[12] - 8 * (32 - i)));
        r[12] = m.ReadU32(Address(r[1] - 8));
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, float_triplet_transfer::NativeServices &fp,
           Registers &s) {
    Sphere x{m, fp, s};
    switch (e) {
    case 0x82bc9928u:
        x.Recursive();
        break;
    case 0x82bc9040u:
        x.Run();
        break;
    case 0x82bc9580u:
        x.Pair();
        break;
    case 0x82bc9600u:
        x.Triangle();
        break;
    case 0x82bc9780u:
        x.Tetrahedron();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_bounds_math61
