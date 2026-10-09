#include "lo_semantics/mesh_polygon_triangulate61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_polygon_triangulate61 {
namespace {
using recovery_abi::Address;
struct Triangulator {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    static std::uint32_t Shift(std::uint64_t v, unsigned b) { return Address(v) << b; }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Single(unsigned i, double x) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(x)));
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        if (e == 0x82bd0798u)
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.guest, s);
        else
            (void)mesh_geometry_math61::Apply(e, m, d.fp, s);
    }
    void Call(unsigned reg, GuestAddress cont) {
        s.ctr = s.r[reg];
        s.lr = cont;
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bb8c10u;
        for (unsigned i = 28; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        Gradual();
        recovery_abi::WriteU64(m, Address(r[1] - 48), s.fpr_bits[31]);
        auto old = r[1];
        r[1] -= 160;
        Store(r[1], old);
        Body();
        r[1] += 160;
        Gradual();
        s.fpr_bits[31] = recovery_abi::ReadU64(m, Address(r[1] - 48));
        for (unsigned i = 28; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Body() {
        auto &r = s.r;
        r[30] = r[3];
        r[8] = Word(r[30] + 4);
        r[11] = Word(r[8] + 36);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[10] = Word(r[8] + 40);
        Compare(r[10]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[28] = 0;
        Compare(r[11]);
        r[31] = r[28];
        // One fan contributes vertex_count - 2 triangles per polygon.
        if (!s.cr6.eq)
            do {
                r[9] = m.ReadU16(Address(r[10]));
                --r[11];
                r[10] += 36;
                r[9] += r[31];
                Compare(r[11]);
                r[31] = r[9] - 2;
            } while (!s.cr6.eq);
        r[11] = Word(r[8] + 8);
        Compare(r[11]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bb8c74u);
            r[11] = Word(r[30] + 4);
            r[4] = Word(r[11] + 8);
            r[11] = Word(r[3]);
            r[11] = Word(r[11] + 12);
            Call(11, 0x82bb8c8cu);
            r[11] = Word(r[30] + 4);
            Store(r[11] + 8, r[28]);
        }
        Lower(0x82bd0798u, 0x82bb8c98u);
        r[10] = Word(r[3]);
        r[11] = Shift(r[31], 1);
        r[5] = 0;
        r[11] += r[31];
        r[10] = Word(r[10]);
        r[4] = Shift(r[11], 2);
        Call(10, 0x82bb8cb8u);
        r[11] = Word(r[30] + 4);
        Store(r[11] + 8, r[3]);
        r[11] = Word(r[30] + 4);
        r[11] = Word(r[11] + 8);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = Word(r[30] + 4);
        r[5] = r[28];
        Store(r[11] + 4, r[31]);
        r[11] = Word(r[30] + 4);
        r[10] = r[11];
        r[11] = Word(r[11] + 36);
        r[9] = Word(r[10] + 8);
        Compare(r[11]);
        if (s.cr6.gt) {
            r[6] = r[28];
            do {
                r[11] = Word(r[30] + 4);
                r[11] = Word(r[11] + 40);
                r[11] += r[6];
                r[10] = m.ReadU16(Address(r[11]));
                r[7] = Word(r[11] + 4);
                r[8] = r[10] - 2;
                Compare(r[8]);
                if (!s.cr6.eq) {
                    r[11] = 2;
                    do {
                        r[4] = m.ReadU8(Address(r[7]));
                        r[3] = r[11] - 1;
                        r[31] = (r[31] & 0xffffffff00000000ull) | (Address(r[11]) / Address(r[10]));
                        r[29] = (r[29] & 0xffffffff00000000ull) | (Address(r[3]) / Address(r[10]));
                        r[31] = std::uint64_t(std::int64_t(std::int32_t(r[31])) *
                                              std::int64_t(std::int32_t(r[10])));
                        Store(r[9], r[4]);
                        r[4] = std::uint64_t(std::int64_t(std::int32_t(r[29])) *
                                             std::int64_t(std::int32_t(r[10])));
                        r[4] = r[3] - r[4];
                        r[3] = r[11] - r[31];
                        --r[8];
                        ++r[11];
                        r[4] = m.ReadU8(Address(r[4]) + Address(r[7]));
                        Compare(r[8]);
                        Store(r[9] + 4, r[4]);
                        r[4] = m.ReadU8(Address(r[3]) + Address(r[7]));
                        Store(r[9] + 8, r[4]);
                        r[9] += 12;
                    } while (!s.cr6.eq);
                }
                r[11] = Word(r[30] + 4);
                ++r[5];
                r[6] += 36;
                r[11] = Word(r[11] + 36);
                Compare(r[5], r[11]);
            } while (s.cr6.lt);
        }
        r[4] = r[1] + 80;
        r[3] = Word(r[30] + 4);
        Lower(0x82bc65f8u, 0x82bb8d90u);
        r[11] = Word(r[30] + 4);
        r[10] = Word(r[11] + 4);
        r[31] = Word(r[11] + 16);
        Compare(r[10]);
        if (s.cr6.gt) {
            r[11] = 0xffffffff82000000ull;
            r[29] = r[28];
            Load(31, r[11] + 3664);
            // Flip a fan triangle only if its plane points toward the centroid.
            do {
                r[11] = Word(r[30] + 4);
                r[3] = r[1] + 96;
                r[11] = Word(r[11] + 8);
                r[11] += r[29];
                r[9] = Word(r[11] + 4);
                r[10] = Word(r[11] + 8);
                r[11] = Word(r[11]);
                r[7] = Shift(r[9], 1);
                r[6] = Shift(r[10], 1);
                r[8] = Shift(r[11], 1);
                r[10] += r[6];
                r[7] += r[9];
                r[11] += r[8];
                r[9] = Shift(r[10], 2);
                r[10] = Shift(r[7], 2);
                r[11] = Shift(r[11], 2);
                r[6] = r[9] + r[31];
                r[5] = r[10] + r[31];
                r[4] = r[11] + r[31];
                Lower(0x82bd92c0u, 0x82bb8e00u);
                Load(11, r[1] + 96);
                Load(12, r[1] + 80);
                Single(12, F(11) * F(12));
                Load(13, r[1] + 100);
                Load(0, r[1] + 84);
                Load(11, r[1] + 88);
                Load(10, r[1] + 104);
                Single(0, F(0) * F(13) + F(12));
                Load(13, r[1] + 108);
                Single(0, F(11) * F(10) + F(0));
                Single(0, F(0) + F(13));
                bool u = std::isnan(F(0)) || std::isnan(F(31));
                s.cr6 = {std::uint8_t(!u && F(0) < F(31)), std::uint8_t(!u && F(0) > F(31)),
                         std::uint8_t(!u && F(0) == F(31)), std::uint8_t(u)};
                if (s.cr6.gt) {
                    r[11] = Word(r[30] + 4);
                    r[11] = Word(r[11] + 8);
                    r[11] += r[29];
                    r[10] = Word(r[11] + 8);
                    r[9] = Word(r[11] + 4);
                    Store(r[11] + 4, r[10]);
                    Store(r[11] + 8, r[9]);
                }
                r[11] = Word(r[30] + 4);
                ++r[28];
                r[29] += 12;
                r[11] = Word(r[11] + 4);
                Compare(r[28], r[11]);
            } while (s.cr6.lt);
        }
        r[3] = 1;
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82bb8c08u)
        return false;
    Triangulator{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_polygon_triangulate61
