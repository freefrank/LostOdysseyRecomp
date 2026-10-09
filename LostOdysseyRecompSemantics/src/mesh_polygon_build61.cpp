#include "lo_semantics/mesh_polygon_build61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_polygon_plane61.h"
#include "lo_semantics/mesh_polygon_triangulate61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_polygon_build61 {
namespace {
using recovery_abi::Address;
struct Builder {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    static std::uint32_t Shift(std::uint64_t v, unsigned n) { return Address(v) << n; }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void FCompare(double a, double b) {
        bool u = std::isnan(a) || std::isnan(b);
        s.cr6 = {std::uint8_t(!u && a < b), std::uint8_t(!u && a > b), std::uint8_t(!u && a == b),
                 std::uint8_t(u)};
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
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
    void FloatStore(unsigned i, std::uint64_t p) {
        Store(p, std::bit_cast<std::uint32_t>(float(F(i))));
    }
    void Neg(unsigned i) { s.fpr_bits[i] ^= 0x8000000000000000ull; }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.edge.engine.sort.guest, s);
            break;
        case 0x82bd2a08u:
        case 0x82bd2c08u:
            (void)object_sort_support61::Apply(e, m, {d.edge.engine.sort.guest, d.edge.engine.fp},
                                               s);
            break;
        case 0x82bb9318u:
            (void)mesh_polygon_collect61::Apply(e, m, d, s);
            break;
        case 0x82b7bc40u:
            crt_reader_chain61::ApplySupport_B7BC40(m, d.edge.engine.sort.accepted, s);
            break;
        case 0x82bc65f8u:
        case 0x82bd92c0u:
            (void)mesh_geometry_math61::Apply(e, m, d.edge.engine.fp, s);
            break;
        case 0x82bd9390u:
        case 0x82bc3880u:
            (void)mesh_polygon_plane61::Apply(e, m, d.edge.engine.fp, s);
            break;
        case 0x82bb8c08u:
            (void)mesh_polygon_triangulate61::Apply(e, m, d.lifetime, s);
            break;
        }
    }
    void Call(unsigned reg, GuestAddress cont) {
        s.ctr = s.r[reg];
        s.lr = cont;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bb9ab0u;
        for (unsigned i = 20; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        Gradual();
        recovery_abi::WriteU64(m, Address(r[1] - 112), s.fpr_bits[31]);
        auto old = r[1];
        r[1] -= 272;
        Store(r[1], old);
        Body();
        r[1] += 272;
        Gradual();
        s.fpr_bits[31] = recovery_abi::ReadU64(m, Address(r[1] - 112));
        for (unsigned i = 20; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Failure() {
        s.r[3] = s.r[1] + 128;
        Lower(0x82bd2c08u, 0x82bb9b68u);
        s.r[3] = s.r[1] + 112;
        Lower(0x82bd2c08u, 0x82bb9b70u);
        s.r[3] = 0;
    }
    void ReleaseOld(unsigned offset, GuestAddress first) {
        auto &r = s.r;
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + offset);
        Compare(r[11]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, first);
            r[11] = Word(r[31] + 4);
            r[4] = Word(r[11] + offset);
            r[11] = Word(r[3]);
            r[11] = Word(r[11] + 12);
            Call(11, first + 24);
            r[11] = Word(r[31] + 4);
            Store(r[11] + offset, r[21]);
        }
    }
    void SourcePlane() {
        auto &r = s.r;
        r[10] = Word(r[29]);
        r[3] = r[1] + 144;
        r[11] = Word(r[31] + 4);
        r[29] += 4;
        r[8] = Shift(r[10], 1);
        r[10] += r[8];
        r[9] = Word(r[11] + 8);
        r[10] = Shift(r[10], 2);
        r[11] = Word(r[11] + 16);
        r[10] += r[9];
        r[8] = Word(r[10] + 4);
        r[9] = Word(r[10] + 8);
        r[10] = Word(r[10]);
        r[6] = Shift(r[8], 1);
        r[5] = Shift(r[9], 1);
        r[7] = Shift(r[10], 1);
        r[9] += r[5];
        r[6] += r[8];
        r[10] += r[7];
        r[8] = Shift(r[9], 2);
        r[9] = Shift(r[6], 2);
        r[10] = Shift(r[10], 2);
        r[6] = r[8] + r[11];
        r[5] = r[9] + r[11];
        r[4] = r[10] + r[11];
        Lower(0x82bd92c0u, 0x82bb9d44u);
    }
    void FlipPlane(bool reverseFirst) {
        auto &r = s.r;
        if (reverseFirst) {
            r[4] = r[25];
            r[3] = r[27];
            Lower(0x82bc3880u, 0x82bb9eb8u);
            r[10] = Word(r[31] + 4);
        } else {
            r[10] = Word(r[31] + 4);
            r[4] = r[25];
            r[3] = r[27];
        }
        r[11] = Word(r[10] + 40);
        r[11] += r[28];
        r[11] += 12;
        Load(0, r[11]);
        Neg(0);
        FloatStore(0, r[11]);
        Load(0, r[11] + 4);
        Load(13, r[11] + 8);
        Neg(0);
        Neg(13);
        FloatStore(0, r[11] + 4);
        FloatStore(13, r[11] + 8);
        r[11] = Word(r[10] + 40);
        r[11] += r[28];
        Load(0, r[11] + 24);
        Neg(0);
        FloatStore(0, r[11] + 24);
        if (!reverseFirst)
            Lower(0x82bc3880u, 0x82bb9df4u);
    }
    void OrientToSource() {
        auto &r = s.r;
        r[26] = Word(r[29]);
        r[29] += 4;
        r[24] = r[21];
        r[30] = r[21];
        Compare(r[26]);
        if (!s.cr6.eq)
            do {
                SourcePlane();
                Compare(r[30]);
                if (s.cr6.eq) {
                    r[11] = Word(r[31] + 4);
                    Load(11, r[1] + 152);
                    Load(0, r[1] + 148);
                    Load(13, r[1] + 144);
                    r[11] = Word(r[11] + 40);
                    r[11] += r[28];
                    Load(12, r[11] + 20);
                    Single(12, F(12) * F(11));
                    Load(11, r[11] + 16);
                    Load(10, r[11] + 12);
                    Single(0, F(11) * F(0) + F(12));
                    Single(0, F(10) * F(13) + F(0));
                    FCompare(F(0), F(31));
                    if (s.cr6.lt)
                        r[24] = 1;
                }
                ++r[30];
                Compare(r[30], r[26]);
            } while (s.cr6.lt);
        r[11] = Address(r[24]) & 255u;
        Compare(r[11]);
        if (!s.cr6.eq)
            FlipPlane(false);
    }
    void ExpandPlane() {
        auto &r = s.r;
        r[7] = Word(r[31] + 4);
        r[6] = r[21];
        r[11] = Word(r[7] + 12);
        Compare(r[11]);
        if (s.cr6.gt) {
            r[8] = Address(r[7]);
            r[9] = r[21];
            do {
                r[11] = Word(r[8] + 40);
                r[10] = Word(r[8] + 16);
                r[11] += r[28];
                r[10] += r[9];
                Load(10, r[11] + 16);
                Load(0, r[10] + 4);
                Single(0, F(0) * F(10));
                Load(12, r[10] + 8);
                Load(10, r[11] + 20);
                Load(11, r[10]);
                Load(9, r[11] + 12);
                Load(13, r[11] + 24);
                Single(0, F(12) * F(10) + F(0));
                Single(0, -(F(11) * F(9) + F(0)));
                FCompare(F(0), F(13));
                if (s.cr6.lt) {
                    r[11] = Word(r[7] + 40);
                    r[11] += r[28];
                    FloatStore(0, r[11] + 24);
                }
                r[11] = Word(r[7] + 12);
                ++r[6];
                r[9] += 12;
                Compare(r[6], r[11]);
            } while (s.cr6.lt);
        }
        r[11] = Word(r[7] + 40);
        Load(11, r[1] + 96);
        Load(0, r[1] + 92);
        r[11] += r[28];
        Load(13, r[1] + 88);
        Load(12, r[11] + 20);
        Single(12, F(12) * F(11));
        Load(11, r[11] + 16);
        Load(10, r[11] + 12);
        Load(9, r[11] + 24);
        Single(0, F(11) * F(0) + F(12));
        Single(0, F(10) * F(13) + F(0));
        Single(0, F(0) + F(9));
        FCompare(F(0), F(31));
        if (s.cr6.gt)
            FlipPlane(true);
    }
    void Polygons() {
        auto &r = s.r;
        r[11] = 0xffffffff82000000ull;
        r[28] = r[21];
        r[22] = r[20];
        Load(31, r[11] + 3664);
        do {
            r[9] = Word(r[31] + 4);
            r[23] = r[10] + 4;
            r[11] = r[21];
            r[9] = Word(r[9] + 40);
            r[9] += r[28];
            Store(r[9] + 4, r[25]);
            r[27] = Word(r[10]);
            r[10] = Word(r[31] + 4);
            Compare(r[27]);
            r[10] = Word(r[10] + 40);
            m.WriteU16(Address(r[10]) + Address(r[28]), std::uint16_t(r[27]));
            if (!s.cr6.eq) {
                r[10] = r[23];
                do {
                    r[9] = Word(r[10]);
                    r[10] += 4;
                    m.WriteU8(Address(r[11]) + Address(r[25]), std::uint8_t(r[9]));
                    ++r[11];
                    Compare(r[11], r[27]);
                } while (s.cr6.lt);
            }
            r[11] = Word(r[31] + 4);
            r[5] = r[25];
            r[4] = r[27];
            r[10] = Word(r[11] + 40);
            r[6] = Word(r[11] + 16);
            r[11] = r[10] + r[28];
            r[3] = r[11] + 12;
            Lower(0x82bd9390u, 0x82bb9cc4u);
            OrientToSource();
            ExpandPlane();
            --r[22];
            r[11] = Shift(r[27], 2);
            r[25] += r[27];
            r[10] = r[11] + r[23];
            r[28] += 36;
            Compare(r[22]);
        } while (!s.cr6.eq);
        Compare(r[20]);
        if (!s.cr6.eq)
            ProjectionRanges();
    }
    void ProjectionRanges() {
        auto &r = s.r;
        r[10] = 0xffffffff82000000ull;
        r[11] = 0xffffffff82000000ull;
        r[9] = r[21];
        r[6] = r[20];
        Load(12, r[10] + 3428);
        Load(13, r[11] + 3596);
        do {
            r[11] = Word(r[31] + 4);
            r[10] = Word(r[11] + 40);
            r[7] = Word(r[11] + 12);
            r[8] = Word(r[11] + 16);
            r[11] = r[10] + r[9];
            Compare(r[7]);
            Gradual();
            FloatStore(13, r[11] + 28);
            r[11] = Word(r[31] + 4);
            r[11] = Word(r[11] + 40);
            r[11] += r[9];
            FloatStore(12, r[11] + 32);
            if (!s.cr6.eq)
                do {
                    r[11] = Word(r[31] + 4);
                    r[10] = r[8];
                    --r[7];
                    r[8] += 12;
                    r[11] = Word(r[11] + 40);
                    Load(0, r[10] + 4);
                    Load(11, r[10] + 8);
                    r[11] += r[9];
                    Load(10, r[10]);
                    Load(8, r[11] + 16);
                    Single(0, F(8) * F(0));
                    Load(8, r[11] + 20);
                    Load(7, r[11] + 12);
                    Load(9, r[11] + 28);
                    Single(0, F(8) * F(11) + F(0));
                    Single(0, F(7) * F(10) + F(0));
                    FCompare(F(0), F(9));
                    if (s.cr6.lt) {
                        r[11] = Word(r[31] + 4);
                        r[11] = Word(r[11] + 40);
                        r[11] += r[9];
                        FloatStore(0, r[11] + 28);
                    }
                    r[11] = Word(r[31] + 4);
                    r[11] = Word(r[11] + 40);
                    r[11] += r[9];
                    Load(11, r[11] + 32);
                    FCompare(F(0), F(11));
                    if (s.cr6.gt)
                        FloatStore(0, r[11] + 32);
                    Compare(r[7]);
                } while (!s.cr6.eq);
            --r[6];
            r[9] += 36;
            Compare(r[6]);
        } while (!s.cr6.eq);
    }
    void Body() {
        auto &r = s.r;
        r[31] = r[3];
        r[21] = 0;
        r[11] = Word(r[31] + 4);
        Store(r[11] + 36, r[21]);
        ReleaseOld(44, 0x82bb9adcu);
        ReleaseOld(40, 0x82bb9b10u);
        r[3] = r[1] + 112;
        Lower(0x82bd2a08u, 0x82bb9b38u);
        r[3] = r[1] + 128;
        Lower(0x82bd2a08u, 0x82bb9b40u);
        r[6] = r[1] + 128;
        r[5] = r[31];
        r[4] = r[1] + 112;
        r[3] = r[1] + 80;
        Lower(0x82bb9318u, 0x82bb9b54u);
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            Failure();
            return;
        }
        r[11] = Word(r[31] + 4);
        r[20] = Word(r[1] + 80);
        Store(r[11] + 36, r[20]);
        Lower(0x82bd0798u, 0x82bb9b90u);
        r[11] = Word(r[31] + 4);
        r[5] = 6;
        r[10] = Word(r[3]);
        r[11] = Word(r[11] + 36);
        r[9] = Word(r[10]);
        r[10] = Shift(r[11], 3);
        r[11] += r[10];
        r[4] = Shift(r[11], 2);
        Call(9, 0x82bb9bb8u);
        r[11] = Word(r[31] + 4);
        Store(r[11] + 40, r[3]);
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 40);
        Compare(r[11]);
        if (s.cr6.eq) {
            Failure();
            return;
        }
        r[11] = Word(r[31] + 4);
        r[4] = 0;
        r[10] = Word(r[11] + 36);
        r[3] = Word(r[11] + 40);
        r[11] = Shift(r[10], 3);
        r[11] += r[10];
        r[5] = Shift(r[11], 2);
        Lower(0x82b7bc40u, 0x82bb9bf0u);
        r[4] = r[1] + 88;
        r[3] = Word(r[31] + 4);
        Lower(0x82bc65f8u, 0x82bb9bfcu);
        r[30] = Word(r[1] + 116);
        Lower(0x82bd0798u, 0x82bb9c04u);
        r[11] = Word(r[3]);
        r[5] = 49;
        r[4] = r[30] - r[20];
        r[11] = Word(r[11]);
        Call(11, 0x82bb9c1cu);
        r[11] = Word(r[31] + 4);
        Store(r[11] + 44, r[3]);
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 44);
        Compare(r[11]);
        if (s.cr6.eq) {
            Failure();
            return;
        }
        r[10] = Word(r[1] + 120);
        r[25] = r[11];
        r[29] = Word(r[1] + 136);
        Compare(r[20]);
        if (!s.cr6.eq)
            Polygons();
        r[3] = r[31];
        Lower(0x82bb8c08u, 0x82bba000u);
        r[31] = r[3];
        r[3] = r[1] + 128;
        Lower(0x82bd2c08u, 0x82bba00cu);
        r[3] = r[1] + 112;
        Lower(0x82bd2c08u, 0x82bba014u);
        r[3] = r[31];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82bb9aa8u)
        return false;
    Builder{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_polygon_build61
