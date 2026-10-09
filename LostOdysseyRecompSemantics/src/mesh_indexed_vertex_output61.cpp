#include "lo_semantics/mesh_indexed_vertex_output61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/mesh_indexed_channels61.h"
#include "lo_semantics/mesh_indexed_remap61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_indexed_vertex_output61 {
namespace {
using recovery_abi::Address;
struct Output {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    static std::uint32_t Shift(std::uint64_t v, unsigned n) { return Address(v) << n; }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void FCompare(double a, double b) {
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(v)));
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Store(unsigned i, std::uint64_t p) { Word(p, std::bit_cast<std::uint32_t>(float(F(i)))); }
    void Lower(GuestAddress e, GuestAddress c) {
        s.lr = c;
        switch (e) {
        case 0x82bd2870u:
            (void)reader_buffer_growth61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82bd2a08u:
        case 0x82bd2c08u:
            (void)object_sort_support61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82bb3c00u:
            (void)mesh_indexed_channels61::Apply(e, m, d, s);
            break;
        case 0x82bbfea8u:
            (void)mesh_indexed_remap61::Apply(e, m, d, s);
            break;
        case 0x822da388u:
            (void)mesh_geometry_math61::Apply(e, m, d.fp, s);
            break;
        }
    }
    void Capacity(unsigned desc) {
        auto &r = s.r;
        r[11] = Word(r[desc]);
        r[10] = Word(r[desc] + 4);
        Compare(r[10], r[11]);
    }
    void Grow(unsigned desc, GuestAddress c) {
        auto &r = s.r;
        if (s.cr6.eq) {
            r[4] = 1;
            r[3] = r[desc];
            Lower(0x82bd2870u, c);
        }
    }
    void Push(unsigned desc, unsigned value) {
        auto &r = s.r;
        r[11] = Word(r[desc] + 4);
        r[10] = Word(r[desc] + 8);
        r[11] = Shift(r[11], 2);
        Word(r[11] + r[10], r[value]);
    }
    void Increment(unsigned desc) {
        auto &r = s.r;
        r[11] = Word(r[desc] + 4);
        ++r[11];
        Word(r[desc] + 4, r[11]);
    }
    void BufferedFloat(bool earlyShift) {
        auto &r = s.r;
        r[11] = Word(r[30] + 4);
        r[10] = Word(r[1] + 80);
        if (earlyShift)
            r[11] = Shift(r[11], 2);
        r[9] = Word(r[30] + 8);
        if (!earlyShift)
            r[11] = Shift(r[11], 2);
        Word(r[11] + r[9], r[10]);
    }
    void FirstAttribute() {
        auto &r = s.r;
        Compare(r[10]); // The caller has already loaded the pointer, then tuple IDs.
        if (s.cr6.eq)
            return;
        r[11] = m.ReadU8(Address(r[31] + 286));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[30] = r[31] + 48;
            Capacity(30);
            Grow(30, 0x82bc0258u);
            Push(30, 29);
            Increment(30);
            return;
        }
        r[11] = Shift(r[29], 1);
        r[30] = r[31] + 96;
        r[11] += r[29];
        r[29] = Shift(r[11], 2);
        r[11] = Word(r[30]);
        Load(0, r[29] + r[10]);
        r[10] = Word(r[30] + 4);
        Store(0, r[1] + 80);
        Compare(r[10], r[11]);
        Grow(30, 0x82bc02a0u);
        BufferedFloat(true);
        r[11] = Word(r[30] + 4);
        r[10] = Word(r[30]);
        ++r[11];
        Compare(r[11], r[10]);
        Word(r[30] + 4, r[11]);
        r[11] = Word(r[31] + 240);
        r[11] += r[29];
        Load(0, r[11] + 4);
        Store(0, r[1] + 80);
        Grow(30, 0x82bc02e8u);
        BufferedFloat(false);
        Increment(30);
        r[11] = m.ReadU8(Address(r[31] + 281));
        Compare(r[11]);
        if (s.cr6.eq)
            return;
        r[11] = Word(r[31] + 240);
        r[10] = Word(r[30] + 4);
        r[11] += r[29];
        Load(0, r[11] + 8);
        r[11] = Word(r[30]);
        Store(0, r[1] + 80);
        Compare(r[10], r[11]);
        Grow(30, 0x82bc0340u);
        BufferedFloat(false);
        Increment(30);
    }
    void SecondAttribute() {
        auto &r = s.r;
        r[10] = Word(r[31] + 244);
        Compare(r[10]);
        if (s.cr6.eq)
            return;
        r[11] = m.ReadU8(Address(r[31] + 287));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[30] = r[31] + 64;
            Capacity(30);
            Grow(30, 0x82bc0398u);
            Push(30, 26);
            Increment(30);
        } else {
            r[11] = Shift(r[26], 1);
            r[3] = r[31] + 112;
            r[11] += r[26];
            r[11] = Shift(r[11], 2);
            r[4] = r[11] + r[10];
            Lower(0x82bb3c00u, 0x82bc03d0u);
        }
    }
    void WeightedNormal() {
        auto &r = s.r;
        r[10] = Word(r[11] + 12);
        r[7] = r[20];
        r[9] = Word(r[11] + 16);
        r[6] = r[20];
        r[3] = Shift(r[10], 1);
        r[8] = Word(r[11] + 20);
        r[11] = Word(r[31] + 252);
        r[4] = Shift(r[9], 1);
        r[10] += r[3];
        r[5] = Shift(r[8], 1);
        r[10] = Shift(r[10], 2);
        r[9] += r[4];
        r[8] += r[5];
        r[9] = Shift(r[9], 2);
        r[8] = Shift(r[8], 2);
        r[10] = Word(r[10] + r[11]);
        Compare(r[28], r[10]);
        Word(r[1] + 104, r[10]);
        r[10] = Word(r[9] + r[11]);
        r[11] = Word(r[8] + r[11]);
        Word(r[1] + 108, r[10]);
        Word(r[1] + 112, r[11]);
        if (s.cr6.eq) {
            r[7] = 2;
            r[6] = 1;
        } else {
            Compare(r[28], r[10]);
            if (s.cr6.eq) {
                r[7] = 2;
                r[6] = r[20];
            } else {
                Compare(r[28], r[11]);
                if (s.cr6.eq) {
                    r[7] = r[20];
                    r[6] = 1;
                }
            }
        }
        r[11] = Shift(r[7], 2);
        r[10] = Word(r[31] + 236);
        r[9] = r[1] + 104;
        r[8] = Shift(r[6], 2);
        r[7] = r[1] + 104;
        r[9] = Word(r[11] + r[9]);
        r[11] = Shift(r[28], 1);
        r[8] = Word(r[8] + r[7]);
        r[6] = Shift(r[9], 1);
        r[11] += r[28];
        r[7] = Shift(r[8], 1);
        r[9] += r[6];
        r[11] = Shift(r[11], 2);
        r[8] += r[7];
        r[9] = Shift(r[9], 2);
        r[11] += r[10];
        r[9] += r[10];
        r[8] = Shift(r[8], 2);
        r[10] += r[8];
        Load(7, r[11] + 4);
        Load(13, r[9] + 4);
        Load(8, r[11]);
        Single(13, F(13) - F(7));
        Load(0, r[9]);
        Single(0, F(0) - F(8));
        Load(7, r[11]);
        Load(11, r[10]);
        Load(8, r[11] + 8);
        Single(11, F(11) - F(7));
        Load(12, r[9] + 8);
        Load(7, r[11] + 8);
        Single(12, F(12) - F(8));
        Load(9, r[10] + 8);
        Load(8, r[11] + 4);
        Single(9, F(9) - F(7));
        Load(10, r[10] + 4);
        Single(10, F(10) - F(8));
        Single(7, F(13) * F(11));
        Single(8, F(9) * F(0));
        Single(5, F(10) * F(13));
        Single(6, F(10) * F(12));
        Single(10, F(10) * F(0) - F(7));
        Single(8, F(12) * F(11) - F(8));
        Single(12, F(9) * F(12) + F(5));
        Single(13, F(9) * F(13) - F(6));
        Single(2, F(11) * F(0) + F(12));
        Single(0, F(8) * F(8));
        Single(0, F(10) * F(10) + F(0));
        Single(0, F(13) * F(13) + F(0));
        Single(1, std::sqrt(F(0)));
        Lower(0x822da388u, 0x82bc05fcu);
        r[11] = Word(r[31] + 248);
        Single(0, F(1));
        r[11] += r[30];
        Load(13, r[11] + 32);
        Load(12, r[11] + 36);
        Load(11, r[11] + 40);
        Single(13, F(13) * F(0));
        Single(12, F(12) * F(0));
        Single(0, F(11) * F(0));
        Single(31, F(13) + F(31));
        Single(30, F(12) + F(30));
    }
    void SmoothNormal() {
        auto &r = s.r;
        r[11] = m.ReadU8(Address(r[31] + 282));
        Compare(r[11]);
        if (s.cr6.eq)
            return;
        r[11] = m.ReadU8(Address(r[31] + 284));
        s.fpr_bits[31] = s.fpr_bits[28];
        s.fpr_bits[30] = s.fpr_bits[28];
        Store(31, r[1] + 88);
        s.fpr_bits[29] = s.fpr_bits[28];
        Store(30, r[1] + 92);
        Store(29, r[1] + 96);
        r[24] = r[20];
        r[23] = r[20];
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[30] = r[31] + 160;
            r[23] = Word(r[31] + 164);
            Capacity(30);
            Grow(30, 0x82bc042cu);
            Push(30, 20);
            Increment(30);
        }
        r[11] = Word(r[31] + 264);
        r[25] = Shift(r[28], 2);
        r[26] = r[20];
        r[11] = Word(r[25] + r[11]);
        Compare(r[11]);
        if (s.cr6.gt) {
            do {
                r[10] = Word(r[31] + 268);
                r[9] = Word(r[31] + 272);
                r[11] = Word(r[31] + 248);
                r[10] = Word(r[25] + r[10]);
                r[10] += r[26];
                r[10] = Shift(r[10], 2);
                r[29] = Word(r[10] + r[9]);
                r[10] = Shift(r[29], 1);
                r[10] += r[29];
                r[30] = Shift(r[10], 4);
                r[11] += r[30];
                r[10] = Word(r[11] + 28);
                r[10] &= r[22];
                Compare(r[10]);
                if (!s.cr6.eq) {
                    r[10] = m.ReadU8(Address(r[31] + 290));
                    Compare(r[10]);
                    if (!s.cr6.eq)
                        WeightedNormal();
                    else {
                        Load(0, r[11] + 32);
                        Load(13, r[11] + 36);
                        Single(31, F(0) + F(31));
                        Load(0, r[11] + 40);
                        Single(30, F(13) + F(30));
                    }
                    r[11] = m.ReadU8(Address(r[31] + 284));
                    Single(29, F(0) + F(29));
                    ++r[24];
                    Compare(r[11]);
                    if (!s.cr6.eq) {
                        r[30] = r[31] + 160;
                        Capacity(30);
                        Grow(30, 0x82bc0674u);
                        Push(30, 29);
                        Increment(30);
                    }
                }
                r[11] = Word(r[31] + 264);
                ++r[26];
                r[11] = Word(r[25] + r[11]);
                Compare(r[26], r[11]);
            } while (s.cr6.lt);
            Store(29, r[1] + 96);
            Store(30, r[1] + 92);
            Store(31, r[1] + 88);
        }
        r[11] = m.ReadU8(Address(r[31] + 284));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[11] = Word(r[31] + 168);
            r[10] = Shift(r[23], 2);
            Word(r[10] + r[11], r[24]);
            r[11] = Word(r[31] + 276);
            ++r[11];
            Word(r[31] + 276, r[11]);
        }
        Single(0, F(30) * F(30));
        Single(0, F(29) * F(29) + F(0));
        Single(0, F(31) * F(31) + F(0));
        FCompare(F(0), F(28));
        if (!s.cr6.eq) {
            Single(0, std::sqrt(F(0)));
            Single(0, F(27) / F(0));
            Single(13, F(0) * F(31));
            Store(13, r[1] + 88);
            Single(13, F(30) * F(0));
            Store(13, r[1] + 92);
            Single(0, F(29) * F(0));
            Store(0, r[1] + 96);
        }
        r[4] = r[1] + 88;
        r[3] = r[31] + 128;
        Lower(0x82bb3c00u, 0x82bc0714u);
    }
    void Position() {
        auto &r = s.r;
        r[10] = Word(r[31] + 236);
        Compare(r[10]);
        if (s.cr6.eq)
            return;
        r[11] = m.ReadU8(Address(r[31] + 285));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[30] = r[31] + 32;
            Capacity(30);
            Grow(30, 0x82bc074cu);
            Push(30, 28);
            Increment(30);
        } else {
            r[11] = Shift(r[28], 1);
            r[3] = r[31] + 80;
            r[11] += r[28];
            r[11] = Shift(r[11], 2);
            r[4] = r[11] + r[10];
            Lower(0x82bb3c00u, 0x82bc0784u);
        }
    }
    void EmitFaces() {
        auto &r = s.r;
        r[30] = r[31] + 16;
        Capacity(30);
        Grow(30, 0x82bc07b0u);
        r[11] = Word(r[30] + 4);
        Compare(r[17]);
        r[10] = Word(r[30] + 8);
        r[11] = Shift(r[11], 2);
        Word(r[11] + r[10], r[17]);
        Increment(30);
        if (s.cr6.eq)
            return;
        r[26] = r[17];
        do {
            r[11] = Word(r[31] + 4);
            r[9] = Word(r[31]);
            r[28] = Word(r[27]);
            Compare(r[11], r[9]);
            r[10] = Word(r[31] + 248);
            r[11] = Shift(r[28], 1);
            r[11] += r[28];
            r[30] = Shift(r[11], 4);
            r[29] = Word(r[30] + r[10]);
            Grow(31, 0x82bc080cu);
            Push(31, 29);
            r[11] = Word(r[31] + 4);
            r[10] = Word(r[31] + 248);
            ++r[11];
            r[9] = Word(r[31]);
            r[10] += r[30];
            Compare(r[11], r[9]);
            Word(r[31] + 4, r[11]);
            r[29] = Word(r[10] + 4);
            Grow(31, 0x82bc084cu);
            Push(31, 29);
            r[11] = Word(r[31] + 4);
            r[10] = Word(r[31] + 248);
            ++r[11];
            r[9] = Word(r[31]);
            r[10] += r[30];
            Compare(r[11], r[9]);
            Word(r[31] + 4, r[11]);
            r[30] = Word(r[10] + 8);
            Grow(31, 0x82bc088cu);
            Push(31, 30);
            r[10] = Word(r[31] + 4);
            r[11] = Word(r[31] + 256);
            ++r[10];
            Compare(r[11]);
            Word(r[31] + 4, r[10]);
            if (!s.cr6.eq) {
                r[10] = Word(r[31] + 260);
                r[10] = Shift(r[10], 2);
                Word(r[10] + r[11], r[28]);
                r[11] = Word(r[31] + 260);
                ++r[11];
                Word(r[31] + 260, r[11]);
            }
            --r[26];
            r[27] += 4;
            Compare(r[26]);
        } while (!s.cr6.eq);
    }
    void Body() {
        auto &r = s.r;
        r[31] = r[3];
        r[27] = r[4];
        r[17] = r[5];
        r[30] = r[6];
        r[29] = r[7];
        r[11] = Word(r[31] + 248);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[18] = r[31] + 176;
        Capacity(18);
        Grow(18, 0x82bc0170u);
        Push(18, 30);
        r[11] = Word(r[18] + 4);
        r[10] = Word(r[18]);
        ++r[11];
        Compare(r[11], r[10]);
        Word(r[18] + 4, r[11]);
        Grow(18, 0x82bc01a4u);
        r[11] = Word(r[18] + 4);
        r[3] = r[1] + 128;
        r[10] = Word(r[18] + 8);
        r[11] = Shift(r[11], 2);
        Word(r[11] + r[10], r[29]);
        Increment(18);
        Lower(0x82bd2a08u, 0x82bc01c8u);
        r[6] = r[1] + 128;
        r[5] = r[17];
        r[4] = r[27];
        r[3] = r[31];
        Lower(0x82bbfea8u, 0x82bc01dcu);
        r[21] = Word(r[1] + 136);
        r[20] = 0;
        Compare(r[3]);
        if (!s.cr6.eq) {
            r[10] = 0xffffffff82000000ull;
            r[11] = 0xffffffff82000000ull;
            r[19] = r[3];
            Load(27, r[10] + 30596);
            Load(28, r[11] + 3664);
            do {
                r[11] = r[21] + 4;
                r[10] = Word(r[31] + 240);
                r[28] = Word(r[21]);
                Compare(r[10]);
                r[29] = Word(r[11]);
                r[11] += 4;
                r[26] = Word(r[11]);
                r[11] += 4;
                r[21] = r[11] + 4;
                r[22] = Word(r[11]);
                FirstAttribute();
                SecondAttribute();
                SmoothNormal();
                Position();
                --r[19];
                Compare(r[19]);
            } while (!s.cr6.eq);
        }
        EmitFaces();
        Capacity(18);
        Grow(18, 0x82bc08f8u);
        r[11] = Word(r[18] + 4);
        r[3] = r[1] + 128;
        r[10] = Word(r[18] + 8);
        r[11] = Shift(r[11], 2);
        Word(r[10] + r[11], r[20]);
        Increment(18);
        Lower(0x82bd2c08u, 0x82bc091cu);
        r[3] = r[17];
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bc0110u;
        for (unsigned i = 17; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        r[12] = r[1] - 128;
        s.lr = 0x82bc0118u;
        Gradual();
        for (unsigned i = 27; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[12] - 8 * (32 - i)), s.fpr_bits[i]);
        auto old = r[1];
        r[1] -= 320;
        Word(r[1], old);
        Body();
        r[1] += 320;
        r[12] = r[1] - 128;
        Gradual();
        for (unsigned i = 27; i < 32; ++i)
            s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(r[12] - 8 * (32 - i)));
        for (unsigned i = 17; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82bc0108u)
        return false;
    Output{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_indexed_vertex_output61
