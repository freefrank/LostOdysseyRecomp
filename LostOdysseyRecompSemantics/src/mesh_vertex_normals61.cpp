#include "lo_semantics/mesh_vertex_normals61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_vertex_normals61 {
namespace {
using recovery_abi::Address;
struct Normals {
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
            d.lifetime.fp.SetHostFpControl(s.cached_fp_control);
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
    void FloatStore(unsigned i, std::uint64_t p) {
        Store(p, std::bit_cast<std::uint32_t>(float(F(i))));
    }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        if (e == 0x82bd0798u)
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.lifetime.guest, s);
        else if (e == 0x82b7bc40u)
            crt_reader_chain61::ApplySupport_B7BC40(m, d.memory, s);
        else if (e == 0x82bc3128u)
            (void)mesh_geometry_math61::Apply(e, m, d.lifetime.fp, s);
        else
            (void)Apply(e, m, d, s);
    }
    void Call(unsigned reg, GuestAddress cont) {
        s.ctr = s.r[reg];
        s.lr = cont;
        d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Enter(unsigned first, unsigned frame, GuestAddress save = 0) {
        auto &r = s.r;
        r[12] = s.lr;
        if (save)
            s.lr = save;
        Store(r[1] - 8, r[12]);
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        auto old = r[1];
        r[1] -= frame;
        Store(r[1], old);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
    }
    void Release() {
        Enter(30, 112);
        auto &r = s.r;
        r[31] = r[3];
        r[30] = 0;
        for (unsigned i = 0; i < 2; ++i) {
            r[11] = Word(r[31] + 4 * i);
            Compare(r[11]);
            if (!s.cr6.eq) {
                Lower(0x82bd0798u, i ? 0x82bc30f4u : 0x82bc30ccu);
                r[11] = Word(r[3]);
                r[4] = Word(r[31] + 4 * i);
                r[11] = Word(r[11] + 12);
                Call(11, i ? 0x82bc3108u : 0x82bc30e0u);
                Store(r[31] + 4 * i, r[30]);
            }
        }
        Leave(30, 112);
    }
    void Build() {
        auto &r = s.r;
        // The 23..31 frame and adjacent FP spills are observable to callers.
        r[12] = s.lr;
        s.lr = 0x82bc3258u;
        for (unsigned i = 23; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        Gradual();
        recovery_abi::WriteU64(m, Address(r[1] - 96), s.fpr_bits[30]);
        recovery_abi::WriteU64(m, Address(r[1] - 88), s.fpr_bits[31]);
        auto old = r[1];
        r[1] -= 192;
        Store(r[1], old);
        BuildBody();
        r[1] += 192;
        Gradual();
        s.fpr_bits[30] = recovery_abi::ReadU64(m, Address(r[1] - 96));
        s.fpr_bits[31] = recovery_abi::ReadU64(m, Address(r[1] - 88));
        for (unsigned i = 23; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    // Preserve the two index-width paths and the implicit 0,1,2 fallback.
    void FaceIndices() {
        auto &r = s.r;
        r[10] = Word(r[27] + 12);
        Compare(r[10]);
        if (!s.cr6.eq) {
            r[11] = r[10] + r[3];
            r[6] = Word(r[11] + r[8]);
        } else {
            r[11] = Word(r[27] + 16);
            Compare(r[11]);
            r[6] = s.cr6.eq ? 0 : m.ReadU16(Address(r[11]) + Address(r[4]));
        }
        Compare(r[10]);
        if (!s.cr6.eq) {
            r[11] = r[10] + r[3];
            r[11] += r[8];
            r[7] = Word(r[11] + 4);
        } else {
            r[11] = Word(r[27] + 16);
            Compare(r[11]);
            if (!s.cr6.eq) {
                r[11] += r[4];
                r[7] = m.ReadU16(Address(r[11] + 2));
            } else
                r[7] = 1;
        }
        Compare(r[10]);
        if (!s.cr6.eq) {
            r[11] = r[10] + r[3];
            r[11] += r[8];
            r[11] = Word(r[11] + 8);
        } else {
            r[11] = Word(r[27] + 16);
            Compare(r[11]);
            if (!s.cr6.eq) {
                r[11] += r[4];
                r[11] = m.ReadU16(Address(r[11] + 4));
            } else
                r[11] = 2;
        }
    }
    void FaceNormal() {
        auto &r = s.r;
        r[10] = Shift(r[11], 1);
        r[9] = Shift(r[6], 1);
        r[11] += r[10];
        r[10] = Shift(r[7], 1);
        r[9] += r[6];
        r[7] += r[10];
        r[10] = Shift(r[9], 2);
        r[11] = Shift(r[11], 2);
        r[9] = Shift(r[7], 2);
        r[11] += r[5];
        r[10] += r[5];
        r[9] += r[5];
        // Negative cross product, component-specific staging retained.
        Load(0, r[11] + 8);
        Load(13, r[11] + 4);
        Load(12, r[10] + 4);
        Load(11, r[9] + 8);
        Single(12, F(12) - F(13));
        Single(11, F(11) - F(0));
        Load(10, r[10] + 8);
        Load(9, r[9] + 4);
        Single(0, F(10) - F(0));
        Single(13, F(9) - F(13));
        Single(12, F(12) * F(11));
        Single(0, F(13) * F(0) - F(12));
        FloatStore(0, r[8] - 8);
        Load(0, r[11]);
        Load(13, r[11] + 8);
        Load(12, r[10] + 8);
        Load(11, r[9]);
        Single(12, F(12) - F(13));
        Single(11, F(11) - F(0));
        Load(10, r[10]);
        Load(9, r[9] + 8);
        Single(0, F(10) - F(0));
        Single(13, F(9) - F(13));
        Single(12, F(12) * F(11));
        Single(0, F(0) * F(13) - F(12));
        FloatStore(0, r[8] - 4);
        Load(0, r[11] + 4);
        Load(13, r[11]);
        Load(12, r[9] + 4);
        Load(11, r[10]);
        Single(12, F(12) - F(0));
        Single(11, F(11) - F(13));
        Load(10, r[9]);
        Load(9, r[10] + 4);
        Single(13, F(10) - F(13));
        Single(0, F(9) - F(0));
        Single(12, F(12) * F(11));
        Single(0, F(13) * F(0) - F(12));
        FloatStore(0, r[8]);
        Load(13, r[8] - 4);
        Single(11, F(13) * F(13));
        Load(0, r[8] - 8);
        Load(12, r[8]);
        Single(11, F(0) * F(0) + F(11));
        Single(11, F(12) * F(12) + F(11));
        FCompare(F(11), F(31));
        if (!s.cr6.eq) {
            Single(11, std::sqrt(F(11)));
            Single(11, F(30) / F(11));
            Single(0, F(0) * F(11));
            FloatStore(0, r[8] - 8);
            Single(0, F(13) * F(11));
            FloatStore(0, r[8] - 4);
            Single(0, F(12) * F(11));
            FloatStore(0, r[8]);
        }
    }
    void VertexIndices() {
        auto &r = s.r;
        r[10] = Word(r[27] + 12);
        Compare(r[10]);
        if (!s.cr6.eq) {
            r[11] = r[10] + r[31];
            r[28] = Word(r[11] + r[24]);
        } else {
            r[11] = Word(r[27] + 16);
            Compare(r[11]);
            r[28] = s.cr6.eq ? 0 : m.ReadU16(Address(r[11]) + Address(r[25]));
        }
        Store(r[1] + 80, r[28]);
        Compare(r[10]);
        if (!s.cr6.eq) {
            r[11] = r[10] + r[31];
            r[11] += r[24];
            r[29] = Word(r[11] + 4);
        } else {
            r[11] = Word(r[27] + 16);
            Compare(r[11]);
            if (!s.cr6.eq) {
                r[11] += r[25];
                r[29] = m.ReadU16(Address(r[11] + 2));
            } else
                r[29] = 1;
        }
        Store(r[1] + 88, r[29]);
        Compare(r[10]);
        if (!s.cr6.eq) {
            r[11] = r[10] + r[31];
            r[11] += r[24];
            r[30] = Word(r[11] + 8);
        } else {
            r[11] = Word(r[27] + 16);
            Compare(r[11]);
            if (!s.cr6.eq) {
                r[11] += r[25];
                r[30] = m.ReadU16(Address(r[11] + 4));
            } else
                r[30] = 2;
        }
        r[11] = m.ReadU8(Address(r[27] + 20));
        Store(r[1] + 84, r[30]);
        Compare(r[11]);
    }
    void Weighted() {
        auto &r = s.r;
        r[5] = r[28];
        r[3] = Word(r[27] + 4);
        r[4] = r[1] + 80;
        Lower(0x82bc3128u, 0x82bc3620u);
        r[11] = Shift(r[28], 1);
        Load(0, r[31] - 8);
        r[5] = r[30];
        r[11] += r[28];
        Single(0, F(1) * F(0));
        Load(13, r[31] - 4);
        r[4] = r[1] + 80;
        r[11] = Shift(r[11], 2);
        Load(12, r[31]);
        Single(13, F(1) * F(13));
        r[11] += r[26];
        Single(12, F(1) * F(12));
        Load(11, r[11]);
        Single(0, F(0) + F(11));
        FloatStore(0, r[11]);
        Load(0, r[11] + 4);
        Load(11, r[11] + 8);
        Single(0, F(13) + F(0));
        FloatStore(0, r[11] + 4);
        Single(13, F(12) + F(11));
        FloatStore(13, r[11] + 8);
        r[3] = Word(r[27] + 4);
        Lower(0x82bc3128u, 0x82bc367cu);
        r[11] = Shift(r[30], 1);
        Load(0, r[31] - 8);
        Single(0, F(1) * F(0));
        Load(13, r[31] - 4);
        r[11] += r[30];
        Load(12, r[31]);
        Single(13, F(1) * F(13));
        r[5] = r[29];
        r[11] = Shift(r[11], 2);
        Single(12, F(1) * F(12));
        r[4] = r[1] + 80;
        r[11] += r[26];
        Load(11, r[11]);
        Single(0, F(11) + F(0));
        FloatStore(0, r[11]);
        Load(0, r[11] + 4);
        Load(11, r[11] + 8);
        Single(0, F(13) + F(0));
        Single(13, F(12) + F(11));
        FloatStore(0, r[11] + 4);
        FloatStore(13, r[11] + 8);
        r[3] = Word(r[27] + 4);
        Lower(0x82bc3128u, 0x82bc36d8u);
        r[11] = Shift(r[29], 1);
        Load(0, r[31] - 8);
        r[11] += r[29];
        Single(0, F(1) * F(0));
        Load(13, r[31] - 4);
        r[11] = Shift(r[11], 2);
        Single(13, F(1) * F(13));
        Load(12, r[31]);
        r[11] += r[26];
        Single(12, F(1) * F(12));
        Load(11, r[11]);
        Single(0, F(11) + F(0));
        FloatStore(0, r[11]);
        Load(0, r[11] + 4);
        Single(0, F(13) + F(0));
        FloatStore(0, r[11] + 4);
        Load(0, r[11] + 8);
        Single(0, F(12) + F(0));
        FloatStore(0, r[11] + 8);
    }
    void Uniform() {
        auto &r = s.r;
        r[11] = Shift(r[28], 1);
        Load(0, r[31] - 8);
        r[10] = Shift(r[30], 1);
        r[11] += r[28];
        r[10] += r[30];
        r[11] = Shift(r[11], 2);
        r[10] = Shift(r[10], 2);
        r[11] += r[26];
        r[10] += r[26];
        r[9] = Shift(r[29], 1);
        r[9] += r[29];
        Load(13, r[11]);
        Single(0, F(13) + F(0));
        FloatStore(0, r[11]);
        Load(0, r[31] - 4);
        r[9] = Shift(r[9], 2);
        Load(13, r[11] + 4);
        Single(0, F(13) + F(0));
        FloatStore(0, r[11] + 4);
        Load(13, r[31]);
        r[9] += r[26];
        Load(0, r[11] + 8);
        Single(0, F(0) + F(13));
        FloatStore(0, r[11] + 8);
        Load(0, r[31] - 8);
        Load(13, r[10]);
        Single(0, F(13) + F(0));
        FloatStore(0, r[10]);
        Load(0, r[31] - 4);
        Load(13, r[10] + 4);
        Single(0, F(13) + F(0));
        FloatStore(0, r[10] + 4);
        Load(13, r[10] + 8);
        Load(0, r[31]);
        Single(0, F(0) + F(13));
        FloatStore(0, r[10] + 8);
        Load(0, r[31] - 8);
        Load(13, r[9]);
        Single(0, F(13) + F(0));
        FloatStore(0, r[9]);
        Load(0, r[31] - 4);
        Load(13, r[9] + 4);
        Single(0, F(0) + F(13));
        FloatStore(0, r[9] + 4);
        Load(0, r[9] + 8);
        Load(13, r[31]);
        Single(0, F(0) + F(13));
        FloatStore(0, r[9] + 8);
    }
    void NormalizeVertices() {
        auto &r = s.r;
        r[11] = Word(r[27]);
        r[10] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[11] = r[26] + 8;
            do {
                Load(12, r[11] - 4);
                Single(0, F(12) * F(12));
                Load(11, r[11] - 8);
                Load(13, r[11]);
                Single(0, F(11) * F(11) + F(0));
                Single(0, F(13) * F(13) + F(0));
                FCompare(F(0), F(31));
                if (!s.cr6.eq) {
                    Single(0, std::sqrt(F(0)));
                    Single(0, F(30) / F(0));
                    Single(11, F(11) * F(0));
                    FloatStore(11, r[11] - 8);
                    Single(12, F(12) * F(0));
                    FloatStore(12, r[11] - 4);
                    Single(0, F(13) * F(0));
                    FloatStore(0, r[11]);
                }
                r[9] = Word(r[27]);
                ++r[10];
                r[11] += 12;
                Compare(r[10], r[9]);
            } while (s.cr6.lt);
        }
    }
    void BuildBody() {
        auto &r = s.r;
        r[27] = r[4];
        r[31] = r[3];
        r[11] = Word(r[27] + 4);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = Word(r[27] + 24);
        Compare(r[11]);
        if (!s.cr6.eq)
            r[30] = r[11];
        else {
            Lower(0x82bd0798u, 0x82bc32a4u);
            r[11] = Word(r[27] + 8);
            r[9] = Word(r[3]);
            r[5] = 45;
            r[10] = Shift(r[11], 1);
            r[11] += r[10];
            r[4] = Shift(r[11], 2);
            r[11] = Word(r[9]);
            Call(11, 0x82bc32c8u);
            r[30] = r[3];
        }
        Compare(r[30]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = Word(r[27] + 28);
        Compare(r[11]);
        if (!s.cr6.eq)
            r[26] = r[11];
        else {
            Lower(0x82bd0798u, 0x82bc32ecu);
            r[11] = Word(r[27]);
            r[9] = Word(r[3]);
            r[5] = 46;
            r[10] = Shift(r[11], 1);
            r[11] += r[10];
            r[4] = Shift(r[11], 2);
            r[11] = Word(r[9]);
            Call(11, 0x82bc3310u);
            r[26] = r[3];
        }
        Compare(r[26]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = Word(r[27] + 24);
        Compare(r[11]);
        if (s.cr6.eq)
            Store(r[31], r[30]);
        r[11] = Word(r[27] + 28);
        Compare(r[11]);
        if (s.cr6.eq)
            Store(r[31] + 4, r[26]);
        r[11] = Word(r[27] + 8);
        r[10] = 0xffffffff82000000ull;
        r[5] = Word(r[27] + 4);
        r[31] = 0;
        Compare(r[11]);
        r[11] = 0xffffffff82000000ull;
        Load(30, r[10] + 30596);
        Load(31, r[11] + 3664);
        if (s.cr6.gt) {
            r[4] = 0;
            r[8] = r[30] + 8;
            s.xer_ca = Address(r[30]) <= 0xfffffff8u;
            r[3] = 0xfffffffffffffff8ull - r[30];
            do {
                FaceIndices();
                FaceNormal();
                r[11] = Word(r[27] + 8);
                ++r[31];
                r[4] += 6;
                r[8] += 12;
                Compare(r[31], r[11]);
            } while (s.cr6.lt);
        }
        r[11] = Word(r[27]);
        r[4] = 0;
        r[3] = r[26];
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[5] = Shift(r[11], 2);
        Lower(0x82b7bc40u, 0x82bc3544u);
        r[11] = Word(r[27] + 8);
        r[23] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[25] = 0;
            r[31] = r[30] + 8;
            s.xer_ca = Address(r[30]) <= 0xfffffff8u;
            r[24] = 0xfffffffffffffff8ull - r[30];
            do {
                VertexIndices();
                if (!s.cr6.eq)
                    Weighted();
                else
                    Uniform();
                r[11] = Word(r[27] + 8);
                ++r[23];
                r[25] += 6;
                r[31] += 12;
                Compare(r[23], r[11]);
            } while (s.cr6.lt);
        }
        NormalizeVertices();
        r[3] = 1;
    }
    void Mesh() {
        Enter(30, 160);
        MeshBody();
        Leave(30, 160);
    }
    void MeshBody() {
        auto &r = s.r;
        r[31] = r[3];
        r[30] = 0;
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 20);
        Compare(r[11]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bb9190u);
            r[11] = Word(r[31] + 4);
            r[4] = Word(r[11] + 20);
            r[11] = Word(r[3]);
            r[11] = Word(r[11] + 12);
            Call(11, 0x82bb91a8u);
            r[11] = Word(r[31] + 4);
            Store(r[11] + 20, r[30]);
        }
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 12);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        Lower(0x82bd0798u, 0x82bb91c4u);
        r[11] = Word(r[31] + 4);
        r[9] = Word(r[3]);
        r[5] = 48;
        r[11] = Word(r[11] + 12);
        r[9] = Word(r[9]);
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[4] = Shift(r[11], 2);
        Call(9, 0x82bb91ecu);
        r[11] = Word(r[31] + 4);
        Store(r[11] + 20, r[3]);
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 20);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[10] = r[1] + 96;
        r[11] = Word(r[31] + 4);
        r[3] = r[1] + 80;
        for (unsigned i = 0; i < 4; ++i)
            recovery_abi::WriteU64(m, Address(r[10] + 8 * i), r[30]);
        constexpr unsigned offsets[]{12, 16, 4, 8};
        for (unsigned i = 0; i < 4; ++i) {
            r[10] = Word(r[11] + offsets[i]);
            Store(r[1] + 96 + 4 * i, r[10]);
        }
        r[10] = 1;
        m.WriteU8(Address(r[1] + 116), std::uint8_t(r[10]));
        r[11] = Word(r[11] + 20);
        Store(r[1] + 124, r[11]);
        Lower(0x82656eb8u, 0x82bb9254u);
        r[4] = r[1] + 96;
        r[3] = r[1] + 80;
        Lower(0x82bc3250u, 0x82bb9260u);
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = r[1] + 80;
            Lower(0x82bc30a0u, 0x82bb9274u);
            r[3] = 0;
            return;
        }
        r[11] = Word(r[31] + 4);
        r[10] = r[30];
        r[11] = Word(r[11] + 12);
        Compare(r[11]);
        if (s.cr6.gt) {
            r[11] = r[30];
            do {
                r[9] = Word(r[31] + 4);
                ++r[10];
                r[9] = Word(r[9] + 20);
                Load(0, Address(r[9]) + Address(r[11]));
                s.fpr_bits[0] ^= 0x8000000000000000ull;
                FloatStore(0, Address(r[9]) + Address(r[11]));
                r[9] = Word(r[31] + 4);
                r[9] = Word(r[9] + 20);
                r[9] += r[11];
                Load(0, r[9] + 4);
                s.fpr_bits[0] ^= 0x8000000000000000ull;
                FloatStore(0, r[9] + 4);
                r[9] = Word(r[31] + 4);
                r[9] = Word(r[9] + 20);
                r[9] += r[11];
                r[11] += 12;
                Load(0, r[9] + 8);
                s.fpr_bits[0] ^= 0x8000000000000000ull;
                FloatStore(0, r[9] + 8);
                r[9] = Word(r[31] + 4);
                r[9] = Word(r[9] + 12);
                Compare(r[10], r[9]);
            } while (s.cr6.lt);
        }
        r[3] = r[1] + 80;
        Lower(0x82bc30a0u, 0x82bb92f8u);
        r[3] = 1;
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Normals n{m, d, s};
    switch (e) {
    case 0x82656eb8u:
        s.r[11] = 0;
        m.WriteU32(Address(s.r[3]), 0);
        m.WriteU32(Address(s.r[3] + 4), 0);
        return true;
    case 0x82bc30a0u:
        n.Release();
        return true;
    case 0x82bc3250u:
        n.Build();
        return true;
    case 0x82bb9160u:
        n.Mesh();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_vertex_normals61
