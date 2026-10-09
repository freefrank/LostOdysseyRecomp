#include "lo_semantics/mesh_convex_hull61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_convex_check61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/mesh_vertex_dedup61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_convex_hull61 {
namespace {
using recovery_abi::Address;
struct Hull {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    static std::uint32_t Shift(std::uint64_t x, unsigned n) { return Address(x) << n; }
    void Integer(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Signed(std::uint64_t a, std::uint64_t b = 0) {
        auto x = std::int32_t(a), y = std::int32_t(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Compare(double a, double b) {
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void F(unsigned i, double v) { s.fpr_bits[i] = std::bit_cast<std::uint64_t>(v); }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(p));
    }
    void Store(unsigned i, std::uint64_t p) {
        recovery_abi::WriteU64(m, Address(p), s.fpr_bits[i]);
    }
    void SingleLoad(unsigned i, std::uint64_t p) {
        Gradual();
        F(i, double(std::bit_cast<float>(Word(p))));
    }
    void SingleStore(unsigned i, std::uint64_t p) {
        Word(p, std::bit_cast<std::uint32_t>(float(F(i))));
    }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.edge.engine.sort.guest, s);
            break;
        case 0x82bc2d28u:
            mesh_vertex_dedup61::Initialize(m, s);
            break;
        case 0x82bc2dd0u:
        case 0x82bc38e0u:
        case 0x82bb8580u:
            (void)mesh_vertex_dedup61::Apply(e, m, d.edge.engine, s);
            break;
        case 0x82bb86c8u:
        case 0x82bb88a8u:
        case 0x82bb8e88u:
            (void)mesh_convex_check61::Apply(e, m, d.edge.engine.fp, s);
            break;
        case 0x82b7e504u:
            mesh_polygon_collect61::ProbeStack(m, s);
            break;
        case 0x82b7bc40u:
            crt_reader_chain61::ApplySupport_B7BC40(m, d.edge.engine.sort.accepted, s);
            break;
        case 0x82bb9aa8u:
            (void)mesh_polygon_build61::Apply(e, m, d, s);
            break;
        case 0x82bc65f8u:
            (void)mesh_geometry_math61::Apply(e, m, d.edge.engine.fp, s);
            break;
        }
    }
    void Allocator(GuestAddress c) { Lower(0x82bd0798u, c); }
    void Call(unsigned reg, GuestAddress cont) {
        s.ctr = s.r[reg];
        s.lr = cont;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Free(unsigned reg, bool indirect, int offset, GuestAddress a, GuestAddress c) {
        auto &r = s.r;
        Allocator(a);
        r[11] = Word(r[3]);
        r[4] = indirect ? Word(r[reg] + offset) : r[reg] + offset;
        r[11] = Word(r[11] + 12);
        Call(11, c);
    }
    // Stable unique points are promoted to individually owned double triples.
    bool PreparePoints() {
        auto &r = s.r;
        r[30] = r[4];
        r[19] = r[3];
        r[3] = r[31] + 112;
        r[5] = Word(r[30]);
        r[4] = Word(r[30] + 4);
        Lower(0x82bc2d28u, 0x82bba05cu);
        r[4] = 0;
        r[3] = r[31] + 112;
        Lower(0x82bc2dd0u, 0x82bba068u);
        r[11] = Address(r[3]) & 255u;
        Integer(r[11]);
        if (s.cr6.eq)
            return false;
        r[10] = Word(r[30]);
        r[11] = r[31] + 144;
        r[9] = Word(r[30] + 4);
        r[16] = 0;
        r[8] = Word(r[30] + 8);
        r[7] = Word(r[30] + 12);
        r[15] = Word(r[31] + 120);
        r[28] = Word(r[31] + 124);
        Word(r[11], r[10]);
        Word(r[11] + 4, r[9]);
        Word(r[11] + 8, r[8]);
        Word(r[11] + 12, r[7]);
        Word(r[31] + 80, r[15]);
        Word(r[31] + 100, r[16]);
        Word(r[31] + 148, r[28]);
        Allocator(0x82bba0b4u);
        r[11] = Word(r[3]);
        r[30] = r[15] + 4;
        r[5] = 1;
        r[4] = Shift(r[30], 2);
        r[11] = Word(r[11]);
        Call(11, 0x82bba0d0u);
        r[18] = r[3];
        Signed(r[30]);
        if (s.cr6.gt) {
            r[29] = r[18];
            do {
                Allocator(0x82bba0e4u);
                r[11] = Word(r[3]);
                r[5] = 1;
                r[4] = 24;
                r[11] = Word(r[11]);
                Call(11, 0x82bba0fcu);
                --r[30];
                Word(r[29], r[3]);
                r[29] += 4;
                Integer(r[30]);
            } while (!s.cr6.eq);
        }
        Signed(r[15]);
        if (s.cr6.gt) {
            r[8] = r[18];
            r[11] = r[28] + 8;
            r[9] = r[15];
            do {
                r[10] = Word(r[8]);
                SingleLoad(0, r[11] - 8);
                --r[9];
                r[8] += 4;
                Integer(r[9]);
                Store(0, r[10]);
                SingleLoad(0, r[11] - 4);
                Store(0, r[10] + 8);
                SingleLoad(0, r[11]);
                r[11] += 12;
                Store(0, r[10] + 16);
            } while (!s.cr6.eq);
        }
        return true;
    }
    // Bounds scale a deterministic tiny perturbation before tetrahedral insertion.
    void PerturbPoints() {
        auto &r = s.r;
        r[11] = Word(r[18]);
        Signed(r[15], 1);
        Load(7, r[11]);
        Load(8, r[11] + 8);
        F(11, F(7));
        Load(9, r[11] + 16);
        F(12, F(8));
        F(13, F(9));
        if (s.cr6.gt) {
            r[11] = r[18] + 4;
            r[9] = r[15] - 1;
            do {
                r[10] = Word(r[11]);
                for (unsigned axis = 0; axis < 3; ++axis) {
                    Load(0, r[10] + 8 * axis);
                    Compare(F(11 + axis), F(0));
                    if (s.cr6.lt)
                        F(11 + axis, F(0));
                    Compare(F(7 + axis), F(0));
                    if (s.cr6.gt)
                        F(7 + axis, F(0));
                }
                --r[9];
                r[11] += 4;
                Integer(r[9]);
            } while (!s.cr6.eq);
        }
        F(0, F(11) - F(7));
        F(11, F(12) - F(8));
        F(10, F(13) - F(9));
        F(13, F(0));
        Compare(F(0), F(11));
        if (s.cr6.lt)
            F(13, F(11));
        Compare(F(13), F(10));
        if (s.cr6.lt)
            F(13, F(10));
        r[11] = 0xffffffff820d0000ull;
        r[6] = 367;
        Signed(r[15]);
        Load(12, r[11] + 25760);
        r[11] = 0xffffffff82000000ull;
        F(13, F(13) * F(12));
        Load(31, r[11] + 3952);
        if (s.cr6.gt) {
            r[10] = 2146959360;
            r[11] = 0xffffffff820d0000ull;
            r[9] = r[10] | 41965;
            r[10] = 715128832;
            r[7] = r[18];
            r[8] = r[15];
            Load(12, r[11] + 25752);
            r[10] |= 7473;
            do {
                r[6] = std::uint64_t(std::int64_t(std::int32_t(r[6])) *
                                     std::int64_t(std::int32_t(r[9])));
                r[11] = Word(r[7]);
                r[6] += r[10];
                Load(6, r[11]);
                Load(5, r[11] + 8);
                Load(4, r[11] + 16);
                --r[8];
                r[5] = std::uint64_t(std::int64_t(std::int32_t(r[6])) *
                                     std::int64_t(std::int32_t(r[9])));
                r[4] = r[6];
                r[6] = r[5] + r[10];
                r[7] += 4;
                r[5] = r[4] & 65535u;
                r[4] = r[6];
                Integer(r[8]);
                recovery_abi::WriteU64(m, Address(r[31] + 264), r[5]);
                r[5] = std::uint64_t(std::int64_t(std::int32_t(r[6])) *
                                     std::int64_t(std::int32_t(r[9])));
                r[6] = r[5] + r[10];
                r[5] = r[4] & 65535u;
                recovery_abi::WriteU64(m, Address(r[31] + 256), r[5]);
                r[5] = r[6] & 65535u;
                recovery_abi::WriteU64(m, Address(r[31] + 88), r[5]);
                Load(3, r[31] + 264);
                F(3, double(std::int64_t(s.fpr_bits[3])));
                F(3, -(F(3) * F(12) - F(31)));
                F(6, F(3) * F(13) + F(6));
                Store(6, r[11]);
                Load(6, r[31] + 256);
                F(6, double(std::int64_t(s.fpr_bits[6])));
                F(6, -(F(6) * F(12) - F(31)));
                F(6, F(6) * F(13) + F(5));
                Store(6, r[11] + 8);
                Load(6, r[31] + 88);
                F(6, double(std::int64_t(s.fpr_bits[6])));
                F(6, -(F(6) * F(12) - F(31)));
                F(6, F(6) * F(13) + F(4));
                Store(6, r[11] + 16);
            } while (!s.cr6.eq);
        }
        r[11] = 0xffffffff820d0000ull;
        r[10] = Shift(r[15], 2);
        r[9] = 4;
        r[8] = r[10] + r[18];
        Load(12, r[11] + 25744);
        r[11] = 0xffffffff820d0000ull;
        for (unsigned offset : {160u, 200u, 240u})
            Store(12, r[31] + offset);
        Load(13, r[11] + 25736);
        r[11] = r[31] + 192;
        for (unsigned offset : {168u, 176u, 184u, 192u, 208u, 216u, 224u, 232u, 248u})
            Store(13, r[31] + offset);
        do {
            r[10] = Word(r[8]);
            Load(13, r[11] - 32);
            Load(12, r[11]);
            --r[9];
            Load(6, r[11] + 32);
            F(13, F(13) * F(0) + F(7));
            F(12, F(12) * F(11) + F(8));
            r[8] += 4;
            F(6, F(6) * F(10) + F(9));
            r[11] += 8;
            Store(13, r[10]);
            Integer(r[9]);
            Store(12, r[10] + 8);
            Store(6, r[10] + 16);
        } while (!s.cr6.eq);
    }
    void AllocateWork() {
        auto &r = s.r;
        Allocator(0x82bba374u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = 1804;
        r[11] = Word(r[11]);
        Call(11, 0x82bba38cu);
        r[23] = r[3];
        Allocator(0x82bba394u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = 5412;
        r[11] = Word(r[11]);
        Call(11, 0x82bba3acu);
        r[11] = 12;
        r[10] = r[23] + 4;
        Word(r[23], r[3]);
        do {
            r[9] = Word(r[23]);
            r[9] += r[11];
            r[11] += 12;
            Signed(r[11], 5412);
            Word(r[10], r[9]);
            r[10] += 4;
        } while (s.cr6.lt);
        r[11] = r[15] + 6;
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[30] = Shift(r[11], 1);
        Allocator(0x82bba3e8u);
        r[11] = Word(r[3]);
        r[29] = Shift(r[30], 2);
        r[5] = 1;
        r[4] = r[29];
        r[11] = Word(r[11]);
        Call(11, 0x82bba404u);
        r[17] = r[3];
        r[11] = r[16];
        Signed(r[30]);
        if (s.cr6.gt) {
            r[10] = r[17];
            do {
                r[9] = r[11];
                ++r[11];
                Signed(r[11], r[30]);
                Word(r[10], r[9]);
                r[10] += 4;
            } while (s.cr6.lt);
        }
        Allocator(0x82bba434u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = r[29];
        r[11] = Word(r[11]);
        Call(11, 0x82bba44cu);
        r[27] = r[3];
        Allocator(0x82bba454u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = Shift(r[30], 4);
        r[11] = Word(r[11]);
        Call(11, 0x82bba46cu);
        Signed(r[30], 1);
        Word(r[27], r[3]);
        if (s.cr6.gt) {
            r[10] = 16;
            r[9] = r[27] + 4;
            r[11] = r[30] - 1;
            do {
                r[8] = Word(r[27]);
                --r[11];
                r[8] += r[10];
                r[10] += 16;
                Integer(r[11]);
                Word(r[9], r[8]);
                r[9] += 4;
            } while (!s.cr6.eq);
        }
        r[11] = Word(r[27]);
        r[10] = r[15] + 1;
        r[9] = r[15] + 2;
        r[8] = r[15] + 3;
        Word(r[11], r[15]);
        r[11] = Word(r[27]);
        Word(r[11] + 4, r[10]);
        r[11] = Word(r[27]);
        Word(r[11] + 8, r[9]);
        r[11] = Word(r[27]);
        Word(r[11] + 12, r[8]);
        Allocator(0x82bba4d4u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = r[29];
        r[11] = Word(r[11]);
        Call(11, 0x82bba4ecu);
        r[26] = r[3];
        Allocator(0x82bba4f4u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = Shift(r[30], 5);
        r[11] = Word(r[11]);
        Call(11, 0x82bba50cu);
        Signed(r[30], 1);
        Word(r[26], r[3]);
        if (s.cr6.gt) {
            r[10] = 32;
            r[9] = r[26] + 4;
            r[11] = r[30] - 1;
            do {
                r[8] = Word(r[26]);
                --r[11];
                r[8] += r[10];
                r[10] += 32;
                Integer(r[11]);
                Word(r[9], r[8]);
                r[9] += 4;
            } while (!s.cr6.eq);
        }
        r[11] = 0xffffffff82000000ull;
        r[10] = Word(r[26]);
        r[21] = 1;
        r[25] = 1;
        r[22] = r[16];
        Signed(r[15]);
        Load(2, r[11] + 4072);
        r[11] = 0xffffffff820d0000ull;
        Store(2, r[10]);
        r[20] = ~std::uint64_t(0);
        Load(0, r[11] + 25728);
        r[11] = Word(r[26]);
        Store(2, r[11] + 8);
        r[11] = Word(r[26]);
        Store(2, r[11] + 16);
        r[11] = Word(r[26]);
        Store(0, r[11] + 24);
    }
    // Remove duplicate cavity faces by replacing a matched face with the last.
    // Ordered vertex triples retain the guest's exact matching and capacity rule.
    bool CavityFaces() {
        auto &r = s.r;
        r[3] -= 4;
        r[11] = Shift(r[5], 2);
        --r[24];
        --r[25];
        r[6] = r[16];
        r[7] = r[11] + r[23];
        Word(r[3], r[4]);
        do {
            Word(r[31] + 88, r[16]);
            Signed(r[6]);
            if (s.cr6.eq) {
                r[11] = 1;
                Word(r[31] + 88, r[11]);
            }
            r[11] = r[31] + 92;
            r[9] = 2;
            do {
                r[10] = Word(r[11] - 4);
                ++r[10];
                Signed(r[10], r[6]);
                Word(r[11], r[10]);
                if (s.cr6.eq) {
                    ++r[10];
                    Word(r[11], r[10]);
                }
                --r[9];
                r[11] += 4;
                Integer(r[9]);
            } while (!s.cr6.eq);
            bool matched = false;
            Signed(r[5], 2);
            if (s.cr6.gt) {
                r[9] = r[16];
                Signed(r[5]);
                if (!s.cr6.lt) {
                    r[10] = r[23];
                    do {
                        r[11] = r[16];
                        for (;;) {
                            Signed(r[11], 12);
                            if (!s.cr6.lt) {
                                matched = true;
                                break;
                            }
                            r[29] = r[31] + 88;
                            r[15] = Word(r[8] + r[27]);
                            r[14] = Word(r[10]);
                            r[29] = Word(r[11] + r[29]);
                            r[14] = Word(r[11] + r[14]);
                            r[29] = Shift(r[29], 2);
                            r[29] = Word(r[29] + r[15]);
                            Signed(r[29], r[14]);
                            if (!s.cr6.eq)
                                break;
                            r[11] += 4;
                        }
                        if (matched)
                            break;
                        ++r[9];
                        r[10] += 4;
                        Signed(r[9], r[5]);
                    } while (!s.cr6.gt);
                }
            }
            if (matched) {
                r[10] = Shift(r[9], 2);
                r[11] = r[16];
                do {
                    r[9] = Word(r[7]);
                    r[29] = Word(r[10] + r[23]);
                    r[9] = Word(r[9] + r[11]);
                    Word(r[29] + r[11], r[9]);
                    r[11] += 4;
                    Signed(r[11], 12);
                } while (s.cr6.lt);
                --r[5];
                r[7] -= 4;
            } else {
                ++r[5];
                r[7] += 4;
                Signed(r[5], 450);
                if (s.cr6.gt) {
                    r[15] = Word(r[31] + 80);
                    r[28] = r[16];
                    r[25] = Word(r[31] + 100);
                    return false;
                }
                r[11] = r[16];
                do {
                    r[10] = r[31] + 88;
                    r[9] = Word(r[8] + r[27]);
                    r[29] = Word(r[7]);
                    r[10] = Word(r[11] + r[10]);
                    r[10] = Shift(r[10], 2);
                    r[10] = Word(r[10] + r[9]);
                    Word(r[29] + r[11], r[10]);
                    r[11] += 4;
                    Signed(r[11], 12);
                } while (s.cr6.lt);
            }
            ++r[6];
            Signed(r[6], 4);
        } while (s.cr6.lt);
        r[11] = Word(r[8] + r[27]);
        r[15] = Word(r[31] + 80);
        Word(r[11], r[20]);
        return true;
    }
    // Circumsphere of a cavity face and the inserted point, using the guest's
    // determinant evaluation order (double precision, no host geometry library).
    void NewSphere() {
        auto &r = s.r;
        r[5] = Word(r[28]);
        r[10] = r[31] + 184;
        r[6] = Word(r[30]);
        r[3] = 3;
        do {
            r[9] = Word(r[6]);
            Store(2, r[10]);
            r[11] = r[5];
            r[7] = Shift(r[9], 2);
            r[8] = r[10] - 24;
            r[9] = 3;
            r[7] = Word(r[7] + r[18]);
            r[7] -= r[5];
            do {
                Load(13, r[11]);
                --r[9];
                Load(0, r[11] + r[7]);
                r[11] += 8;
                F(12, F(0) - F(13));
                Store(12, r[8]);
                F(0, F(13) + F(0));
                Load(13, r[10]);
                Integer(r[9]);
                r[8] += 8;
                F(0, F(0) * F(12));
                F(0, F(0) * F(31) + F(13));
                Store(0, r[10]);
            } while (!s.cr6.eq);
            --r[3];
            r[6] += 4;
            r[10] += 32;
            Integer(r[3]);
        } while (!s.cr6.eq);
        Load(11, r[31] + 208);
        r[11] = Word(r[4]);
        Load(9, r[31] + 224);
        r[10] = r[16];
        Load(13, r[31] + 200);
        F(3, F(9) * F(11));
        F(30, F(9) * F(13));
        Load(4, r[31] + 248);
        Load(12, r[31] + 232);
        F(29, F(4) * F(11));
        Load(0, r[31] + 240);
        F(6, F(12) * F(11));
        Load(10, r[31] + 192);
        r[9] = Shift(r[11], 2);
        Load(5, r[31] + 216);
        r[11] = r[16];
        Load(8, r[31] + 160);
        Load(7, r[31] + 176);
        r[9] = Word(r[9] + r[26]);
        F(11, F(0) * F(10) - F(3));
        F(3, F(12) * F(5));
        F(12, F(12) * F(10) - F(30));
        F(30, F(9) * F(5));
        Load(9, r[31] + 184);
        F(6, F(0) * F(13) - F(6));
        F(13, F(4) * F(13));
        F(0, F(0) * F(5) - F(29));
        Load(5, r[31] + 168);
        F(10, F(4) * F(10) - F(30));
        F(4, F(11) * F(5));
        F(30, F(3) - F(13));
        F(3, F(13) - F(3));
        F(13, F(0) * F(5));
        F(11, F(11) * F(9));
        F(5, F(10) * F(5));
        F(4, F(6) * F(8) - F(4));
        F(6, F(6) * F(9) - F(13));
        F(0, F(0) * F(8) - F(11));
        F(11, F(3) * F(8) - F(5));
        F(13, F(12) * F(7) + F(4));
        F(8, F(30) * F(7) + F(6));
        F(10, F(10) * F(7) + F(0));
        F(12, F(12) * F(9) + F(11));
        F(0, F(1) / F(13));
        F(13, F(8) * F(0));
        Store(13, r[9]);
        r[9] = Word(r[4]);
        F(13, F(10) * F(0));
        F(0, F(12) * F(0));
        r[9] = Shift(r[9], 2);
        r[9] = Word(r[9] + r[26]);
        Store(13, r[9] + 8);
        r[9] = Word(r[4]);
        r[9] = Shift(r[9], 2);
        r[9] = Word(r[9] + r[26]);
        Store(0, r[9] + 16);
        r[9] = Word(r[4]);
        r[9] = Shift(r[9], 2);
        r[9] = Word(r[9] + r[26]);
        Store(2, r[9] + 24);
        do {
            r[9] = Word(r[4]);
            r[8] = Word(r[28]);
            r[9] = Shift(r[9], 2);
            r[7] = Word(r[30]);
            Load(0, r[8] + r[11]);
            r[9] = Word(r[9] + r[26]);
            Load(13, r[9] + r[11]);
            r[11] += 8;
            F(0, F(0) - F(13));
            Load(13, r[9] + 24);
            Signed(r[11], 24);
            F(0, F(0) * F(0) + F(13));
            Store(0, r[9] + 24);
            r[9] = Word(r[4]);
            r[8] = Word(r[10] + r[7]);
            r[9] = Shift(r[9], 2);
            r[9] = Word(r[9] + r[27]);
            Word(r[9] + r[10], r[8]);
            r[10] += 4;
        } while (s.cr6.lt);
        r[11] = Word(r[4]);
        ++r[25];
        r[4] += 4;
        r[11] = Shift(r[11], 2);
        ++r[24];
        r[11] = Word(r[11] + r[27]);
        Word(r[11] + 12, r[22]);
    }
    bool InsertPoints() {
        auto &r = s.r;
        if (!s.cr6.gt)
            return true;
        r[11] = 0xffffffff82000000ull;
        r[28] = r[18];
        Load(1, r[11] + 3880);
        do {
            r[5] = r[20];
            r[4] = r[20];
            r[24] = r[16];
            r[30] = r[16];
            Signed(r[21]);
            if (s.cr6.gt) {
                r[11] = Shift(r[25], 2);
                r[3] = r[11] + r[17];
                do {
                    ++r[4];
                    r[11] = Shift(r[4], 2);
                    r[11] += r[27];
                    r[10] = Word(r[11]);
                    r[10] = Word(r[10]);
                    Signed(r[10]);
                    while (s.cr6.lt) {
                        r[11] += 4;
                        ++r[4];
                        r[10] = Word(r[11]);
                        r[10] = Word(r[10]);
                        Signed(r[10]);
                    }
                    r[8] = Shift(r[4], 2);
                    r[9] = Word(r[28]);
                    r[10] = r[16];
                    r[11] = Word(r[26] + r[8]);
                    r[9] -= r[11];
                    Load(0, r[11] + 24);
                    bool outside = false;
                    do {
                        Load(13, r[9] + r[11]);
                        Load(12, r[11]);
                        F(13, F(13) - F(12));
                        F(0, -(F(13) * F(13) - F(0)));
                        Compare(F(0), F(2));
                        if (s.cr6.lt) {
                            outside = true;
                            break;
                        }
                        ++r[10];
                        r[11] += 8;
                        Signed(r[10], 3);
                    } while (s.cr6.lt);
                    if (!outside && !CavityFaces())
                        return false;
                    ++r[30];
                    Signed(r[30], r[21]);
                } while (s.cr6.lt);
                Signed(r[5]);
                if (!s.cr6.lt) {
                    r[11] = Shift(r[25], 2);
                    r[30] = r[23];
                    r[4] = r[11] + r[17];
                    r[29] = r[5] + 1;
                    do {
                        r[11] = Word(r[30]);
                        r[11] = Word(r[11]);
                        Signed(r[11], r[15]);
                        if (!s.cr6.lt)
                            NewSphere();
                        --r[29];
                        r[30] += 4;
                        Integer(r[29]);
                    } while (!s.cr6.eq);
                }
            }
            ++r[22];
            r[21] += r[24];
            r[28] += 4;
            Signed(r[22], r[15]);
        } while (s.cr6.lt);
        return true;
    }
    void ExtractFaces() {
        auto &r = s.r;
        r[10] = Word(r[19] + 4);
        r[11] = r[20];
        Signed(r[21]);
        Word(r[10] + 4, r[16]);
        if (s.cr6.gt) {
            r[8] = r[21];
            do {
                ++r[11];
                r[10] = Shift(r[11], 2);
                r[10] += r[27];
                r[9] = Word(r[10]);
                r[9] = Word(r[9]);
                Signed(r[9]);
                while (s.cr6.lt) {
                    r[10] += 4;
                    ++r[11];
                    r[9] = Word(r[10]);
                    r[9] = Word(r[9]);
                    Signed(r[9]);
                }
                r[10] = Shift(r[11], 2);
                r[10] = Word(r[10] + r[27]);
                r[10] = Word(r[10] + 4);
                Signed(r[10], r[15]);
                if (s.cr6.lt) {
                    r[10] = Word(r[19] + 4);
                    r[9] = Word(r[10] + 4);
                    ++r[9];
                    Word(r[10] + 4, r[9]);
                }
                --r[8];
                Integer(r[8]);
            } while (!s.cr6.eq);
        }
        r[11] = Word(r[19] + 4);
        r[10] = 357913941;
        r[30] = Word(r[11] + 4);
        Integer(r[30], r[10]);
        bool overflow = s.cr6.gt;
        if (!overflow) {
            r[11] = Shift(r[30], 1);
            r[10] = std::uint64_t(-5);
            r[11] += r[30];
            r[11] = Shift(r[11], 2);
            Integer(r[11], r[10]);
            r[29] = r[11] + 4;
            overflow = s.cr6.gt;
        }
        if (overflow)
            r[29] = ~std::uint64_t(0);
        Allocator(0x82bbaa2cu);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = r[29];
        r[11] = Word(r[11]);
        Call(11, 0x82bbaa44u);
        Integer(r[3]);
        if (!s.cr6.eq) {
            r[25] = r[3] + 4;
            Word(r[3], r[30]);
        } else
            r[25] = r[16];
        r[11] = Word(r[19] + 4);
        r[8] = r[20];
        Signed(r[21]);
        Word(r[11] + 4, r[16]);
        if (s.cr6.gt) {
            r[7] = r[21];
            do {
                ++r[8];
                r[11] = Shift(r[8], 2);
                r[11] += r[27];
                r[10] = Word(r[11]);
                r[10] = Word(r[10]);
                Signed(r[10]);
                while (s.cr6.lt) {
                    r[11] += 4;
                    ++r[8];
                    r[10] = Word(r[11]);
                    r[10] = Word(r[10]);
                    Signed(r[10]);
                }
                r[10] = Shift(r[8], 2);
                r[11] = Word(r[10] + r[27]);
                r[11] = Word(r[11] + 4);
                Signed(r[11], r[15]);
                if (s.cr6.lt) {
                    r[11] = Word(r[19] + 4);
                    r[9] = Word(r[10] + r[27]);
                    r[11] = Word(r[11] + 4);
                    r[6] = Word(r[9] + 4);
                    r[9] = Shift(r[11], 1);
                    r[11] += r[9];
                    r[11] = Shift(r[11], 2);
                    r[11] += r[25];
                    Word(r[11], r[6]);
                    r[9] = Word(r[10] + r[27]);
                    r[9] = Word(r[9] + 8);
                    Word(r[11] + 4, r[9]);
                    r[10] = Word(r[10] + r[27]);
                    r[10] = Word(r[10] + 12);
                    Word(r[11] + 8, r[10]);
                    r[11] = Word(r[19] + 4);
                    r[10] = Word(r[11] + 4);
                    ++r[10];
                    Word(r[11] + 4, r[10]);
                }
                --r[7];
                Integer(r[7]);
            } while (!s.cr6.eq);
        }
        r[28] = 1;
    }
    void ReleaseWork() {
        auto &r = s.r;
        r[11] = Word(r[26]);
        Integer(r[11]);
        if (!s.cr6.eq) {
            Free(26, true, 0, 0x82bbab68u, 0x82bbab7cu);
            Word(r[26], r[16]);
        }
        Free(26, false, 0, 0x82bbab84u, 0x82bbab98u);
        r[11] = Word(r[27]);
        Integer(r[11]);
        if (!s.cr6.eq) {
            Free(27, true, 0, 0x82bbaba8u, 0x82bbabbcu);
            Word(r[27], r[16]);
        }
        Free(27, false, 0, 0x82bbabc4u, 0x82bbabd8u);
        Integer(r[17]);
        if (!s.cr6.eq)
            Free(17, false, 0, 0x82bbabe4u, 0x82bbabf8u);
        r[11] = Word(r[23]);
        Integer(r[11]);
        if (!s.cr6.eq) {
            Free(23, true, 0, 0x82bbac08u, 0x82bbac1cu);
            Word(r[23], r[16]);
        }
        Free(23, false, 0, 0x82bbac24u, 0x82bbac38u);
        r[29] = r[15] + 4;
        Signed(r[29]);
        if (s.cr6.gt) {
            r[30] = r[18];
            do {
                r[11] = Word(r[30]);
                Integer(r[11]);
                if (!s.cr6.eq) {
                    Free(30, true, 0, 0x82bbac58u, 0x82bbac6cu);
                    Word(r[30], r[16]);
                }
                --r[29];
                r[30] += 4;
                Integer(r[29]);
            } while (!s.cr6.eq);
        }
        Free(18, false, 0, 0x82bbac84u, 0x82bbac98u);
    }
    bool PublishTriangles() {
        auto &r = s.r;
        Allocator(0x82bbace8u);
        r[11] = Word(r[19] + 4);
        r[10] = Word(r[3]);
        r[5] = 0;
        r[11] = Word(r[11] + 4);
        r[9] = Word(r[10]);
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[4] = Shift(r[11], 2);
        Call(9, 0x82bbad10u);
        r[11] = Word(r[19] + 4);
        Word(r[11] + 8, r[3]);
        r[11] = Word(r[19] + 4);
        r[10] = Word(r[11] + 8);
        Integer(r[10]);
        if (s.cr6.eq)
            return false;
        r[11] = Word(r[11] + 4);
        r[9] = r[16];
        Integer(r[11]);
        if (s.cr6.gt) {
            r[10] = r[16];
            r[11] = r[25] + 8;
            do {
                r[8] = Word(r[19] + 4);
                ++r[9];
                r[7] = Word(r[11] - 8);
                r[8] = Word(r[8] + 8);
                Word(r[8] + r[10], r[7]);
                r[8] = Word(r[19] + 4);
                r[7] = Word(r[11] - 4);
                r[8] = Word(r[8] + 8);
                r[8] += r[10];
                Word(r[8] + 4, r[7]);
                r[8] = Word(r[19] + 4);
                r[7] = Word(r[11]);
                r[11] += 12;
                r[8] = Word(r[8] + 8);
                r[8] += r[10];
                r[10] += 12;
                Word(r[8] + 8, r[7]);
                r[8] = Word(r[19] + 4);
                r[8] = Word(r[8] + 4);
                Integer(r[9], r[8]);
            } while (s.cr6.lt);
        }
        Integer(r[25]);
        if (!s.cr6.eq)
            Free(25, false, -4, 0x82bbada0u, 0x82bbadb4u);
        r[11] = Word(r[19] + 4);
        r[5] = 1;
        r[3] = r[11] + 4;
        r[28] = Word(r[11] + 8);
        r[4] = r[28];
        Lower(0x82bb8580u, 0x82bbadccu);
        r[11] = m.ReadU8(Address(r[31] + 156));
        Integer(r[11]);
        if (!s.cr6.eq) {
            r[11] = Word(r[19] + 4);
            r[7] = 1;
            r[6] = r[28];
            r[4] = Word(r[31] + 148);
            r[3] = r[15];
            r[5] = Word(r[11] + 4);
            Lower(0x82bb86c8u, 0x82bbadf4u);
        }
        return true;
    }
    void CompactVertices() {
        auto &r = s.r;
        r[11] = 0 - r[15];
        r[12] = Address(r[11]) & 0xfffffff0u;
        Lower(0x82b7e504u, 0x82bbae00u);
        r[11] = Word(r[1]);
        r[5] = r[15];
        r[4] = 0;
        r[1] += r[12];
        Word(r[1], r[11]);
        r[29] = r[1] + 80;
        r[3] = r[29];
        Lower(0x82b7bc40u, 0x82bbae1cu);
        r[11] = Word(r[19] + 4);
        r[6] = r[16];
        Word(r[11] + 12, r[16]);
        r[11] = Word(r[19] + 4);
        r[11] = Word(r[11] + 4);
        Integer(r[11]);
        if (s.cr6.gt) {
            r[7] = r[16];
            r[5] = 1;
            do {
                r[11] = Word(r[19] + 4);
                r[11] = Word(r[11] + 8);
                r[11] += r[7];
                r[10] = Word(r[11]);
                r[9] = Word(r[11] + 4);
                r[11] = Word(r[11] + 8);
                r[9] &= 65535u;
                r[8] = r[11] & 65535u;
                r[11] = r[10] & 65535u;
                r[10] = m.ReadU8(Address(r[11] + r[29]));
                Integer(r[10]);
                if (s.cr6.eq) {
                    r[10] = Word(r[19] + 4);
                    m.WriteU8(Address(r[11] + r[29]), std::uint8_t(r[5]));
                    r[11] = Word(r[10] + 12);
                    ++r[11];
                    Word(r[10] + 12, r[11]);
                }
                for (unsigned reg : {9u, 8u}) {
                    r[10] = r[reg] & 65535u;
                    r[11] = m.ReadU8(Address(r[10] + r[29]));
                    Integer(r[11]);
                    if (s.cr6.eq) {
                        r[11] = Word(r[19] + 4);
                        m.WriteU8(Address(r[10] + r[29]), std::uint8_t(r[5]));
                        r[10] = Word(r[11] + 12);
                        ++r[10];
                        Word(r[11] + 12, r[10]);
                    }
                }
                r[11] = Word(r[19] + 4);
                ++r[6];
                r[7] += 12;
                r[11] = Word(r[11] + 4);
                Integer(r[6], r[11]);
            } while (s.cr6.lt);
        }
        r[11] = Shift(r[15], 2);
        r[11] = 0 - r[11];
        r[12] = Address(r[11]) & 0xfffffff0u;
        Lower(0x82b7e504u, 0x82bbaef4u);
        r[11] = Word(r[1]);
        r[1] += r[12];
        Word(r[1], r[11]);
        r[30] = r[1] + 80;
        Allocator(0x82bbaf04u);
        r[11] = Word(r[19] + 4);
        r[5] = 47;
        r[10] = Word(r[3]);
        r[11] = Word(r[11] + 12);
        r[9] = Word(r[10]);
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[4] = Shift(r[11], 2);
        Call(9, 0x82bbaf2cu);
        r[11] = Word(r[19] + 4);
        r[8] = r[16];
        r[6] = r[16];
        Signed(r[15]);
        Word(r[11] + 16, r[3]);
        if (s.cr6.gt) {
            r[11] = Word(r[31] + 148);
            r[9] = r[16];
            r[7] = r[30];
            r[11] += 8;
            do {
                r[10] = m.ReadU8(Address(r[6] + r[29]));
                Integer(r[10]);
                if (!s.cr6.eq) {
                    r[10] = Word(r[19] + 4);
                    r[5] = r[8];
                    SingleLoad(0, r[11] - 8);
                    ++r[8];
                    r[10] = Word(r[10] + 16);
                    Word(r[7], r[5]);
                    r[10] += r[9];
                    r[9] += 12;
                    SingleStore(0, r[10]);
                    SingleLoad(0, r[11] - 4);
                    SingleStore(0, r[10] + 4);
                    SingleLoad(0, r[11]);
                    SingleStore(0, r[10] + 8);
                }
                ++r[6];
                r[7] += 4;
                r[11] += 12;
                Signed(r[6], r[15]);
            } while (s.cr6.lt);
        }
        r[11] = Word(r[19] + 4);
        r[10] = r[16];
        r[11] = Word(r[11] + 4);
        Integer(r[11]);
        if (s.cr6.gt) {
            r[11] = r[16];
            do {
                r[9] = Word(r[19] + 4);
                ++r[10];
                r[9] = Word(r[9] + 8);
                r[8] = Word(r[9] + r[11]);
                r[8] = Shift(r[8], 2);
                r[8] = Word(r[8] + r[30]);
                Word(r[9] + r[11], r[8]);
                r[9] = Word(r[19] + 4);
                r[9] = Word(r[9] + 8);
                r[9] += r[11];
                r[8] = Word(r[9] + 4);
                r[8] = Shift(r[8], 2);
                r[8] = Word(r[8] + r[30]);
                Word(r[9] + 4, r[8]);
                r[9] = Word(r[19] + 4);
                r[9] = Word(r[9] + 8);
                r[9] += r[11];
                r[11] += 12;
                r[8] = Word(r[9] + 8);
                r[8] = Shift(r[8], 2);
                r[8] = Word(r[8] + r[30]);
                Word(r[9] + 8, r[8]);
                r[9] = Word(r[19] + 4);
                r[9] = Word(r[9] + 4);
                Integer(r[10], r[9]);
            } while (s.cr6.lt);
        }
    }
    void Fail() {
        s.r[3] = s.r[31] + 112;
        Lower(0x82bc38e0u, 0x82bbacccu);
        s.r[3] = 0;
    }
    void Body() {
        auto &r = s.r;
        if (!PreparePoints()) {
            Fail();
            return;
        }
        PerturbPoints();
        AllocateWork();
        if (InsertPoints())
            ExtractFaces();
        ReleaseWork();
        r[11] = Address(r[28]) & 255u;
        Integer(r[11]);
        if (s.cr6.eq) {
            Integer(r[25]);
            if (!s.cr6.eq)
                Free(25, false, -4, 0x82bbacb0u, 0x82bbacc4u);
            Fail();
            return;
        }
        if (!PublishTriangles()) {
            Fail();
            return;
        }
        CompactVertices();
        r[11] = Word(r[19] + 4);
        r[6] = 1;
        r[4] = r[28];
        r[3] = r[11] + 4;
        r[5] = Word(r[11] + 16);
        Lower(0x82bb88a8u, 0x82bbb040u);
        r[11] = Address(r[3]) & 255u;
        Integer(r[11]);
        if (s.cr6.eq) {
            Fail();
            return;
        }
        r[11] = m.ReadU8(Address(r[31] + 157));
        Integer(r[11]);
        if (!s.cr6.eq) {
            r[3] = r[19];
            Lower(0x82bb9aa8u, 0x82bbb060u);
            r[11] = Address(r[3]) & 255u;
            Integer(r[11]);
            if (s.cr6.eq) {
                Fail();
                return;
            }
        }
        r[3] = Word(r[19] + 4);
        r[4] = r[3] + 24;
        Lower(0x82bc65f8u, 0x82bbb078u);
        r[3] = r[19];
        Lower(0x82bb8e88u, 0x82bbb080u);
        r[30] = r[3];
        r[3] = r[31] + 112;
        Lower(0x82bc38e0u, 0x82bbb08cu);
        r[3] = r[30];
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bba030u;
        for (unsigned i = 14; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        Gradual();
        for (unsigned i = 29; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 176 + 8 * (i - 29)), s.fpr_bits[i]);
        auto old = r[1];
        r[31] = r[1] - 448;
        r[1] -= 448;
        Word(r[1], old);
        Body();
        r[1] = r[31] + 448;
        Gradual();
        for (unsigned i = 29; i < 32; ++i)
            s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(r[1] - 176 + 8 * (i - 29)));
        for (unsigned i = 14; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82bba028u)
        return false;
    Hull{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_convex_hull61
