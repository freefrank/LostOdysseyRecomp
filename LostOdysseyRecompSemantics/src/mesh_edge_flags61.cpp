#include "lo_semantics/mesh_edge_flags61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_edge_flags61 {
namespace {
using recovery_abi::Address;
struct Flags {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    static std::uint32_t Shift(std::uint64_t v, unsigned bits) { return Address(v) << bits; }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void FloatCompare(double a, double b) {
        bool u = std::isnan(a) || std::isnan(b);
        s.cr6 = {std::uint8_t(!u && a < b), std::uint8_t(!u && a > b), std::uint8_t(!u && a == b),
                 std::uint8_t(u)};
    }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Single(unsigned i, double v) {
        Gradual();
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(v)));
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        if (e == 0x82bd0798u)
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.engine.sort.guest, s);
        else if (e == 0x82b7bc40u)
            crt_reader_chain61::ApplySupport_B7BC40(m, d.engine.sort.accepted, s);
        else if (e == 0x82b9d328u)
            (void)diagnostic_format_routes61::Apply(e, m, d.diagnostics, s);
        else
            (void)mesh_geometry_math61::Apply(e, m, d.engine.fp, s);
    }
    void Call(GuestAddress cont) {
        s.ctr = s.r[11];
        s.lr = cont;
        d.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Diagnostic(unsigned line, unsigned text, GuestAddress cont) {
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        r[5] = line;
        r[4] = r[11] + 25852;
        r[11] = 0xffffffff820d0000ull;
        r[3] = r[11] + text;
        Lower(0x82b9d328u, cont);
    }
    void TrianglePair() {
        auto &r = s.r;
        r[11] = Word(r[20]);
        Compare(r[29]);
        r[11] = Shift(r[11], 2);
        r[11] += r[15];
        r[10] = Word(r[11]);
        r[9] = Word(r[11] + 4);
        r[11] = Shift(r[10], 1);
        r[8] = Shift(r[9], 1);
        r[11] += r[10];
        r[10] = r[9] + r[8];
        if (!s.cr6.eq) {
            r[9] = Shift(r[11], 2);
            r[8] = Shift(r[10], 2);
            r[11] += 2;
            r[10] += 2;
            r[9] += r[29];
            r[8] += r[29];
            r[11] = Shift(r[11], 2);
            r[10] = Shift(r[10], 2);
            r[26] = Word(r[9]);
            r[24] = Word(r[9] + 4);
            r[25] = Word(r[11] + r[29]);
            r[23] = Word(r[10] + r[29]);
            r[22] = Word(r[8]);
            r[21] = Word(r[8] + 4);
        } else {
            r[7] = Word(r[1] + 364);
            Compare(r[7]);
            if (!s.cr6.eq) {
                r[9] = Shift(r[11], 1);
                r[8] = Shift(r[10], 1);
                r[11] += 2;
                r[10] += 2;
                r[9] += r[7];
                r[8] += r[7];
                r[11] = Shift(r[11], 1);
                r[10] = Shift(r[10], 1);
                r[26] = m.ReadU16(Address(r[9]));
                r[24] = m.ReadU16(Address(r[9] + 2));
                r[25] = m.ReadU16(Address(r[11] + r[7]));
                r[23] = m.ReadU16(Address(r[10] + r[7]));
                r[22] = m.ReadU16(Address(r[8]));
                r[21] = m.ReadU16(Address(r[8] + 2));
            }
        }
        r[10] = Word(r[18]);
        r[11] = Word(r[18] + 4);
        constexpr unsigned first[]{26, 26, 26, 26, 24, 24}, second[]{24, 24, 25, 25, 25, 25},
            other[]{25, 25, 24, 24, 26, 26};
        bool found = false;
        for (unsigned i = 0; i < 6; ++i) {
            Compare(r[first[i]], r[(i & 1) ? 11 : 10]);
            if (!s.cr6.eq)
                continue;
            Compare(r[second[i]], r[(i & 1) ? 10 : 11]);
            if (i == 5)
                r[28] = r[26];
            if (s.cr6.eq) {
                r[28] = r[other[i]];
                found = true;
                break;
            }
        }
        if (!found)
            r[28] = r[14];
        r[9] = Shift(r[23], 1);
        r[10] = Shift(r[21], 1);
        r[11] = Shift(r[22], 1);
        r[9] += r[23];
        r[10] += r[21];
        r[11] += r[22];
        r[9] = Shift(r[9], 2);
        r[10] = Shift(r[10], 2);
        r[11] = Shift(r[11], 2);
        r[31] = r[9] + r[27];
        r[30] = r[10] + r[27];
        r[29] = r[11] + r[27];
        r[3] = r[1] + 96;
        r[6] = r[31];
        r[5] = r[30];
        r[4] = r[29];
        Lower(0x82bd92c0u, 0x82bbd874u);
        r[11] = Shift(r[28], 1);
        Load(11, r[1] + 104);
        r[11] += r[28];
        Load(0, r[1] + 100);
        Load(13, r[1] + 96);
        r[11] = Shift(r[11], 2);
        r[11] += r[27];
        Load(12, r[11] + 8);
        Single(12, F(12) * F(11));
        Load(11, r[11] + 4);
        Load(10, r[11]);
        Single(0, F(11) * F(0) + F(12));
        Single(0, F(10) * F(13) + F(0));
        Load(13, r[1] + 108);
        Single(0, F(0) + F(13));
        FloatCompare(F(0), F(29));
        if (s.cr6.lt)
            Angle();
        r[28] = Word(r[1] + 340);
        r[29] = Word(r[1] + 356);
        r[30] = Word(r[1] + 80);
    }
    void Angle() {
        auto &r = s.r;
        r[9] = Shift(r[25], 1);
        Load(4, r[31]);
        r[11] = Shift(r[26], 1);
        Load(3, r[30]);
        r[10] = Shift(r[24], 1);
        Load(8, r[29]);
        r[9] += r[25];
        Load(7, r[29] + 4);
        r[8] = r[26] + r[11];
        Load(6, r[29] + 8);
        r[10] += r[24];
        Load(2, r[30] + 4);
        r[11] = Shift(r[9], 2);
        Load(1, r[30] + 8);
        r[10] = Shift(r[10], 2);
        Load(31, r[31] + 4);
        r[9] = Shift(r[8], 2);
        Load(30, r[31] + 8);
        r[11] += r[27];
        r[10] += r[27];
        r[9] += r[27];
        Load(13, r[11]);
        Load(10, r[10] + 4);
        Load(0, r[9]);
        Load(12, r[9] + 4);
        Single(13, F(0) - F(13));
        Single(10, F(12) - F(10));
        Load(11, r[10]);
        Load(5, r[11] + 4);
        Single(11, F(0) - F(11));
        Load(0, r[9] + 8);
        Single(12, F(12) - F(5));
        Load(9, r[10] + 8);
        Load(5, r[11] + 8);
        Single(9, F(0) - F(9));
        Single(0, F(0) - F(5));
        Single(5, F(10) * F(13));
        Single(26, F(9) * F(12));
        Single(25, F(0) * F(11));
        Single(12, F(12) * F(11) - F(5));
        Single(0, F(10) * F(0) - F(26));
        Single(13, F(9) * F(13) - F(25));
        Single(11, F(12) * F(12));
        Single(11, F(0) * F(0) + F(11));
        Single(11, F(13) * F(13) + F(11));
        FloatCompare(F(11), F(29));
        if (!s.cr6.eq) {
            Single(11, std::sqrt(F(11)));
            Single(11, F(28) / F(11));
            Single(0, F(0) * F(11));
            Single(13, F(13) * F(11));
            Single(12, F(12) * F(11));
        }
        Single(9, F(7) - F(31));
        Single(10, F(8) - F(4));
        Single(7, F(7) - F(2));
        Single(11, F(6) - F(30));
        Single(6, F(6) - F(1));
        Single(8, F(8) - F(3));
        Single(4, F(7) * F(10));
        Single(5, F(6) * F(9));
        Single(3, F(11) * F(8));
        Single(9, F(9) * F(8) - F(4));
        Single(11, F(7) * F(11) - F(5));
        Single(10, F(6) * F(10) - F(3));
        Single(8, F(9) * F(9));
        Single(8, F(11) * F(11) + F(8));
        Single(8, F(10) * F(10) + F(8));
        FloatCompare(F(8), F(29));
        if (!s.cr6.eq) {
            Single(8, std::sqrt(F(8)));
            Single(8, F(28) / F(8));
            Single(11, F(11) * F(8));
            Single(10, F(10) * F(8));
            Single(9, F(9) * F(8));
        }
        Single(8, F(9) * F(0));
        Single(7, F(13) * F(11));
        Single(5, F(9) * F(12));
        Single(6, F(10) * F(12));
        Single(12, F(12) * F(11) - F(8));
        Single(8, F(10) * F(0) - F(7));
        Single(0, F(11) * F(0) + F(5));
        Single(9, F(9) * F(13) - F(6));
        Single(2, F(10) * F(13) + F(0));
        Single(0, F(12) * F(12));
        Single(0, F(8) * F(8) + F(0));
        Single(0, F(9) * F(9) + F(0));
        Single(1, std::sqrt(F(0)));
        Lower(0x822da388u, 0x82bbda18u);
        Single(0, F(1));
        s.fpr_bits[0] &= 0x7fffffffffffffffull;
        FloatCompare(F(0), F(27));
        if (s.cr6.gt)
            r[19] = 1;
    }
    void TagEdges() {
        auto &r = s.r;
        r[11] = Word(r[28] + 8);
        r[8] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[9] = 0;
            do {
                r[11] = Word(r[28] + 12);
                r[11] += r[9];
                for (unsigned i = 0; i < 3; ++i) {
                    r[10] = Word(r[11] + 4 * i);
                    r[7] = Address(r[10]) & 0x80000000u;
                    Compare(r[7]);
                    if (s.cr6.eq) {
                        r[7] = Address(r[10]) & 0x0fffffffu;
                        r[7] = m.ReadU8(Address(r[7] + r[30]));
                        Compare(r[7]);
                        if (!s.cr6.eq) {
                            r[10] |= 0x80000000u;
                            Store(r[11] + 4 * i, r[10]);
                        }
                    }
                }
                r[11] = Word(r[28] + 8);
                ++r[8];
                r[9] += 12;
                Compare(r[8], r[11]);
            } while (s.cr6.lt);
        }
        r[11] = Word(r[28]);
        r[9] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[11] = 0;
            do {
                r[10] = m.ReadU8(Address(r[9] + r[30]));
                Compare(r[10]);
                if (!s.cr6.eq) {
                    r[10] = Word(r[28] + 16);
                    r[8] = m.ReadU16(Address(r[10] + r[11]));
                    r[8] |= 1;
                    m.WriteU16(Address(r[10] + r[11]), std::uint16_t(r[8]));
                }
                r[10] = Word(r[28]);
                ++r[9];
                r[11] += 8;
                Compare(r[9], r[10]);
            } while (s.cr6.lt);
        }
        Lower(0x82bd0798u, 0x82bbdb40u);
        r[11] = Word(r[3]);
        r[4] = r[30];
        r[11] = Word(r[11] + 12);
        Call(0x82bbdb54u);
    }
    void MarkVertices() {
        auto &r = s.r;
        r[5] = Word(r[1] + 348);
        r[9] = 0;
        Compare(r[5]);
        if (!s.cr6.eq) {
            r[4] = Word(r[1] + 364);
            r[11] = r[29] + 8;
            r[8] = Word(r[1] + 80);
            r[7] = Word(r[1] + 80);
            r[10] = r[4] + 4;
            r[6] = Word(r[1] + 80);
            do {
                Compare(r[29]);
                if (!s.cr6.eq) {
                    r[8] = Word(r[11] - 8);
                    r[7] = Word(r[11] - 4);
                    r[6] = Word(r[11]);
                } else {
                    Compare(r[4]);
                    if (!s.cr6.eq) {
                        r[8] = m.ReadU16(Address(r[10] - 4));
                        r[7] = m.ReadU16(Address(r[10] - 2));
                        r[6] = m.ReadU16(Address(r[10]));
                    }
                }
                for (unsigned reg : {8u, 7u, 6u}) {
                    Compare(r[reg], r[9]);
                    if (s.cr6.gt)
                        r[9] = r[reg];
                }
                --r[5];
                r[11] += 12;
                r[10] += 6;
                Compare(r[5]);
            } while (!s.cr6.eq);
        }
        r[31] = r[9] + 1;
        Lower(0x82bd0798u, 0x82bbdbe8u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = r[31];
        r[11] = Word(r[11]);
        Call(0x82bbdc00u);
        r[30] = r[3];
        Compare(r[30]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[5] = r[31];
        r[4] = 0;
        Lower(0x82b7bc40u, 0x82bbdc18u);
        r[11] = Word(r[28] + 8);
        r[27] = Word(r[1] + 364);
        r[4] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[6] = Word(r[1] + 104);
            r[5] = r[27] + 4;
            r[7] = Word(r[1] + 100);
            r[9] = r[29] + 8;
            r[8] = Word(r[1] + 96);
            s.xer_ca = Address(r[29]) <= 0xfffffff8u;
            r[3] = 0xfffffffffffffff8ull - r[29];
            r[31] = 1;
            do {
                Compare(r[29]);
                if (!s.cr6.eq) {
                    r[8] = Word(r[9] - 8);
                    r[7] = Word(r[9] - 4);
                    r[6] = Word(r[9]);
                } else {
                    Compare(r[27]);
                    if (!s.cr6.eq) {
                        r[8] = m.ReadU16(Address(r[5] - 4));
                        r[7] = m.ReadU16(Address(r[5] - 2));
                        r[6] = m.ReadU16(Address(r[5]));
                    }
                }
                r[10] = Word(r[28] + 12);
                r[11] = r[3] + r[9];
                r[11] += r[10];
                r[10] = Word(r[11]);
                r[10] = Address(r[10]) & 0x80000000u;
                Compare(r[10]);
                if (!s.cr6.eq) {
                    m.WriteU8(Address(r[7] + r[30]), std::uint8_t(r[31]));
                    m.WriteU8(Address(r[8] + r[30]), std::uint8_t(r[31]));
                }
                r[10] = Word(r[11] + 4);
                r[10] = Address(r[10]) & 0x80000000u;
                Compare(r[10]);
                if (!s.cr6.eq) {
                    m.WriteU8(Address(r[6] + r[30]), std::uint8_t(r[31]));
                    m.WriteU8(Address(r[7] + r[30]), std::uint8_t(r[31]));
                }
                r[11] = Word(r[11] + 8);
                r[11] = Address(r[11]) & 0x80000000u;
                Compare(r[11]);
                if (!s.cr6.eq) {
                    m.WriteU8(Address(r[6] + r[30]), std::uint8_t(r[31]));
                    m.WriteU8(Address(r[8] + r[30]), std::uint8_t(r[31]));
                }
                r[11] = Word(r[28] + 8);
                ++r[4];
                r[5] += 6;
                r[9] += 12;
                Compare(r[4], r[11]);
            } while (s.cr6.lt);
        }
        r[11] = Word(r[28] + 8);
        r[3] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[5] = Word(r[1] + 104);
            r[8] = r[27] + 4;
            r[6] = Word(r[1] + 100);
            r[9] = r[29] + 8;
            r[7] = Word(r[1] + 96);
            s.xer_ca = Address(r[29]) <= 0xfffffff8u;
            r[4] = 0xfffffffffffffff8ull - r[29];
            do {
                Compare(r[29]);
                if (!s.cr6.eq) {
                    r[7] = Word(r[9] - 8);
                    r[6] = Word(r[9] - 4);
                    r[5] = Word(r[9]);
                } else {
                    Compare(r[27]);
                    if (!s.cr6.eq) {
                        r[7] = m.ReadU16(Address(r[8] - 4));
                        r[6] = m.ReadU16(Address(r[8] - 2));
                        r[5] = m.ReadU16(Address(r[8]));
                    }
                }
                r[10] = Word(r[28] + 12);
                r[11] = r[9] + r[4];
                r[11] += r[10];
                for (unsigned i = 0; i < 3; ++i) {
                    r[10] = Word(r[11] + 4 * i);
                    r[31] = Address(r[10]) & 0x40000000u;
                    Compare(r[31]);
                    if (s.cr6.eq) {
                        r[31] = m.ReadU8(Address(r[7 - i] + r[30]));
                        Compare(r[31]);
                        if (!s.cr6.eq) {
                            r[10] |= 0x40000000u;
                            Store(r[11] + 4 * i, r[10]);
                        }
                    }
                }
                r[11] = Word(r[28] + 8);
                ++r[3];
                r[8] += 6;
                r[9] += 12;
                Compare(r[3], r[11]);
            } while (s.cr6.lt);
        }
        Lower(0x82bd0798u, 0x82bbddc8u);
        r[11] = Word(r[3]);
        r[4] = r[30];
        r[11] = Word(r[11] + 12);
        Call(0x82bbdddcu);
        r[3] = 1;
    }
    void Build() {
        auto &r = s.r;
        r[29] = r[5];
        Store(r[1] + 348, r[4]);
        r[28] = r[3];
        Store(r[1] + 364, r[6]);
        r[27] = r[7];
        Gradual();
        s.fpr_bits[27] = s.fpr_bits[1];
        Compare(r[29]);
        Store(r[1] + 356, r[29]);
        Store(r[1] + 340, r[28]);
        if (s.cr6.eq) {
            Compare(r[6]);
            if (s.cr6.eq) {
                Diagnostic(303, 26144, 0x82bbd544u);
                return;
            }
        }
        Compare(r[27]);
        if (s.cr6.eq) {
            Diagnostic(304, 26144, 0x82bbd574u);
            return;
        }
        constexpr unsigned offsets[]{0, 4, 16, 20}, regs[]{17, 18, 31, 15},
            lines[]{307, 310, 313, 316}, texts[]{26088, 26028, 25960, 25888};
        constexpr GuestAddress sites[]{0x82bbd5a8u, 0x82bbd5dcu, 0x82bbd610u, 0x82bbd644u};
        for (unsigned i = 0; i < 4; ++i) {
            r[regs[i]] = Word(r[28] + offsets[i]);
            Compare(r[regs[i]]);
            if (s.cr6.eq) {
                Diagnostic(lines[i], texts[i], sites[i]);
                return;
            }
        }
        Lower(0x82bd0798u, 0x82bbd658u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = r[17];
        r[11] = Word(r[11]);
        Call(0x82bbd670u);
        r[30] = r[3];
        Compare(r[30]);
        Store(r[1] + 80, r[30]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[10] = 0xffffffff82000000ull;
        r[26] = Word(r[1] + 80);
        r[11] = 0xffffffff82000000ull;
        r[24] = Word(r[1] + 80);
        r[25] = Word(r[1] + 80);
        r[16] = r[30];
        r[22] = Word(r[1] + 80);
        r[20] = r[31] + 4;
        r[21] = Word(r[1] + 80);
        r[14] = ~std::uint64_t{0};
        Load(28, r[10] + 30596);
        r[23] = Word(r[1] + 80);
        Load(29, r[11] + 3664);
        do {
            r[11] = m.ReadU16(Address(r[20] - 2));
            --r[17];
            r[19] = 0;
            Compare(r[11], 1);
            if (s.cr6.eq)
                r[19] = 1;
            else {
                Compare(r[11], 2);
                if (s.cr6.eq)
                    TrianglePair();
            }
            m.WriteU8(Address(r[16]), std::uint8_t(r[19]));
            r[20] += 8;
            r[18] += 8;
            Compare(r[17]);
            ++r[16];
        } while (!s.cr6.eq);
        TagEdges();
        MarkVertices();
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82bbd4e8u)
        return false;
    Flags f{m, d, s};
    auto &r = s.r;
    r[12] = s.lr;
    s.lr = 0x82bbd4f0u;
    for (unsigned i = 14; i < 32; ++i)
        recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
    m.WriteU32(Address(r[1] - 8), Address(r[12]));
    r[12] = r[1] - 152;
    s.lr = 0x82bbd4f8u;
    f.Gradual();
    for (unsigned i = 25; i < 32; ++i)
        recovery_abi::WriteU64(m, Address(r[12] - 8 * (32 - i)), s.fpr_bits[i]);
    auto old = r[1];
    r[1] -= 320;
    m.WriteU32(Address(r[1]), Address(old));
    f.Build();
    r[1] += 320;
    r[12] = r[1] - 152;
    f.Gradual();
    for (unsigned i = 25; i < 32; ++i)
        s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(r[12] - 8 * (32 - i)));
    for (unsigned i = 14; i < 32; ++i)
        r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
    r[12] = m.ReadU32(Address(r[1] - 8));
    s.lr = r[12];
    return true;
}
} // namespace lo::semantic::gpu::mesh_edge_flags61
