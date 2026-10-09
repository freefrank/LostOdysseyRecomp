#include "lo_semantics/mesh_polygon_collect61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/mesh_vertex_normals61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_polygon_collect61 {
namespace {
using recovery_abi::Address;
struct Collect {
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
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        auto f = crt_reader_float61::Dependencies{d.edge.engine.sort.guest, d.edge.engine.fp};
        switch (e) {
        case 0x82656eb8u:
            (void)mesh_vertex_normals61::Apply(e, m, {d.lifetime, d.edge.engine.sort.accepted}, s);
            break;
        case 0x82bc3f20u:
        case 0x82bc38e8u:
        case 0x82bc3ec0u:
            (void)mesh_triangle_links61::Apply(e, m, d, s);
            break;
        case 0x82b7e504u:
            Probe();
            break;
        case 0x82b7bc40u:
            crt_reader_chain61::ApplySupport_B7BC40(m, d.edge.engine.sort.accepted, s);
            break;
        case 0x82bd2a08u:
        case 0x82bd2c08u:
            (void)object_sort_support61::Apply(e, m, f, s);
            break;
        case 0x82bb8498u:
        case 0x82bc2a18u:
            (void)mesh_boundary_walk61::Apply(e, m, f, s);
            break;
        case 0x82bd2870u:
            (void)reader_buffer_growth61::Apply(e, m, f, s);
            break;
        case 0x82b7a0b0u:
            (void)crt_copy_full_context::Apply(e, m, s);
            break;
        case 0x82b9d328u:
            (void)diagnostic_format_routes61::Apply(e, m, d.edge.diagnostics, s);
            break;
        }
    }
    void Probe() { ProbeStack(m, s); }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bb9320u;
        for (unsigned i = 18; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        r[31] = r[1] - 288;
        auto old = r[1];
        r[1] -= 288;
        Store(r[1], old);
        Body();
        r[1] = r[31] + 288;
        for (unsigned i = 18; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void FailLinks(bool args) {
        if (args)
            s.r[3] = s.r[31] + 80;
        Lower(0x82bc3ec0u, 0x82bb939cu);
        s.r[3] = 0;
    }
    void Pair(unsigned a, unsigned b, GuestAddress first) {
        auto &r = s.r;
        r[11] = Word(r[31] + 96);
        Compare(r[9], r[11]);
        if (s.cr6.eq) {
            r[4] = 1;
            r[3] = r[31] + 96;
            Lower(0x82bd2870u, first);
            r[9] = Word(r[31] + 100);
        }
        r[10] = Word(r[31] + 104);
        r[11] = Shift(r[9], 2);
        Store(r[11] + r[10], r[a]);
        r[11] = Word(r[31] + 100);
        r[10] = Word(r[31] + 96);
        ++r[11];
        Compare(r[11], r[10]);
        Store(r[31] + 100, r[11]);
        if (s.cr6.eq) {
            r[4] = 1;
            r[3] = r[31] + 96;
            Lower(0x82bd2870u, first + 52);
            r[11] = Word(r[31] + 100);
        }
        r[10] = Word(r[31] + 104);
        r[11] = Shift(r[11], 2);
        Store(r[11] + r[10], r[b]);
        r[11] = Word(r[31] + 100);
        r[9] = r[11] + 1;
        Store(r[31] + 100, r[9]);
    }
    void GatherBoundary() {
        auto &r = s.r;
        r[11] = Word(r[31] + 116);
        r[25] = r[19];
        Compare(r[11]);
        if (s.cr6.eq)
            return;
        r[9] = Word(r[31] + 100);
        r[26] = r[19];
        do {
            r[11] = Word(r[31] + 120);
            Compare(r[22]);
            r[11] = Word(r[26] + r[11]);
            r[10] = Shift(r[11], 1);
            r[10] += r[11];
            if (!s.cr6.eq) {
                r[10] = Shift(r[10], 2);
                r[10] += r[22];
                r[30] = Word(r[10]);
                r[27] = Word(r[10] + 4);
                r[28] = Word(r[10] + 8);
            } else {
                r[10] = Shift(r[10], 1);
                r[30] = m.ReadU16(Address(r[10]));
                r[27] = m.ReadU16(Address(r[10] + 2));
                r[28] = m.ReadU16(Address(r[10] + 4));
            }
            r[10] = Shift(r[11], 1);
            r[11] += r[10];
            r[29] = Shift(r[11], 2);
            r[11] = Word(r[31] + 84);
            r[11] = Word(r[29] + r[11]);
            r[11] = Address(r[11]) & 0x20000000u;
            Compare(r[11]);
            if (!s.cr6.eq)
                Pair(30, 27, 0x82bb94b8u);
            r[11] = Word(r[31] + 84);
            r[11] += r[29];
            r[11] = Word(r[11] + 4);
            r[11] = Address(r[11]) & 0x20000000u;
            Compare(r[11]);
            if (!s.cr6.eq)
                Pair(30, 28, 0x82bb9538u);
            r[11] = Word(r[31] + 84);
            r[11] += r[29];
            r[11] = Word(r[11] + 8);
            r[11] = Address(r[11]) & 0x20000000u;
            Compare(r[11]);
            if (!s.cr6.eq)
                Pair(27, 28, 0x82bb95b8u);
            r[11] = Word(r[31] + 116);
            ++r[25];
            r[26] += 4;
            Compare(r[25], r[11]);
        } while (s.cr6.lt);
    }
    void OutputPolygon() {
        auto &r = s.r;
        r[11] = Word(r[23]);
        r[10] = Word(r[23] + 4);
        r[29] = Word(r[31] + 136);
        Compare(r[10], r[11]);
        if (s.cr6.eq) {
            r[4] = 1;
            r[3] = r[23];
            Lower(0x82bd2870u, 0x82bb9668u);
        }
        r[11] = Word(r[23] + 4);
        --r[30];
        r[10] = Word(r[23] + 8);
        Compare(r[29]);
        r[11] = Shift(r[11], 2);
        Store(r[11] + r[10], r[30]);
        r[11] = Word(r[23] + 4);
        ++r[11];
        Store(r[23] + 4, r[11]);
        if (!s.cr6.eq) {
            Compare(r[30]);
            if (!s.cr6.eq) {
                r[10] = Word(r[23]);
                r[11] += r[30];
                Compare(r[11], r[10]);
                if (s.cr6.gt) {
                    r[4] = r[30];
                    r[3] = r[23];
                    Lower(0x82bd2870u, 0x82bb96b4u);
                }
                r[11] = Word(r[23] + 4);
                r[5] = Shift(r[30], 2);
                r[10] = Word(r[23] + 8);
                r[4] = r[29];
                r[11] = Shift(r[11], 2);
                r[3] = r[11] + r[10];
                Lower(0x82b7a0b0u, 0x82bb96d0u);
                r[11] = Word(r[23] + 4);
                r[11] += r[30];
                Store(r[23] + 4, r[11]);
            }
        }
        r[11] = Word(r[18]);
        Compare(r[24]);
        ++r[11];
        Store(r[18], r[11]);
        if (!s.cr6.eq)
            OutputTriangles();
    }
    void OutputTriangles() {
        auto &r = s.r;
        r[11] = Word(r[24]);
        r[10] = Word(r[24] + 4);
        r[30] = Word(r[31] + 116);
        Compare(r[10], r[11]);
        if (s.cr6.eq) {
            r[4] = 1;
            r[3] = r[24];
            Lower(0x82bd2870u, 0x82bb9710u);
        }
        r[11] = Word(r[24] + 4);
        r[10] = Word(r[24] + 8);
        r[11] = Shift(r[11], 2);
        Store(r[11] + r[10], r[30]);
        r[11] = Word(r[24] + 4);
        r[10] = Word(r[31] + 120);
        ++r[11];
        r[4] = Word(r[31] + 116);
        r[29] = r[10];
        r[30] = r[4];
        Compare(r[10]);
        Store(r[24] + 4, r[11]);
        if (s.cr6.eq)
            return;
        Compare(r[4]);
        if (s.cr6.eq)
            return;
        r[10] = Word(r[24]);
        r[11] += r[4];
        Compare(r[11], r[10]);
        if (s.cr6.gt) {
            r[3] = r[24];
            Lower(0x82bd2870u, 0x82bb9764u);
        }
        r[11] = Word(r[24] + 4);
        r[5] = Shift(r[30], 2);
        r[10] = Word(r[24] + 8);
        r[4] = r[29];
        r[11] = Shift(r[11], 2);
        r[3] = r[11] + r[10];
        Lower(0x82b7a0b0u, 0x82bb9780u);
        r[11] = Word(r[24] + 4);
        r[11] += r[30];
        Store(r[24] + 4, r[11]);
    }
    void Cleanup(GuestAddress first) {
        constexpr unsigned offsets[]{128, 96, 112};
        for (unsigned i = 0; i < 3; ++i) {
            s.r[3] = s.r[31] + offsets[i];
            Lower(0x82bd2c08u, first + 8 * i);
        }
    }
    void Body() {
        auto &r = s.r;
        r[11] = Word(r[5] + 4);
        r[19] = 0;
        r[18] = r[3];
        r[3] = r[31] + 80;
        r[23] = r[4];
        r[24] = r[6];
        r[21] = Word(r[11] + 4);
        r[10] = Word(r[11] + 16);
        r[22] = Word(r[11] + 8);
        r[11] = 0xffffffff82000000ull;
        Store(r[31] + 152, r[19]);
        Store(r[31] + 144, r[21]);
        Store(r[31] + 156, r[10]);
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[0] =
            std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(r[11] + 14588))));
        Store(r[31] + 148, r[22]);
        Store(r[31] + 160,
              std::bit_cast<std::uint32_t>(float(std::bit_cast<double>(s.fpr_bits[0]))));
        Lower(0x82656eb8u, 0x82bb936cu);
        r[4] = r[31] + 144;
        r[3] = r[31] + 80;
        Lower(0x82bc3f20u, 0x82bb9378u);
        r[11] = Address(r[3]) & 255u;
        r[3] = r[31] + 80;
        Compare(r[11]);
        if (s.cr6.eq) {
            FailLinks(false);
            return;
        }
        Lower(0x82bc38e8u, 0x82bb938cu);
        Compare(r[3]);
        if (!s.cr6.eq) {
            FailLinks(true);
            return;
        }
        r[11] = 0 - r[21];
        r[12] = Address(r[11]) & 0xfffffff0u;
        Lower(0x82b7e504u, 0x82bb93b4u);
        r[11] = Word(r[1]);
        auto next = r[1] + r[12];
        Store(next, r[11]);
        r[1] = next;
        s.xer_ca = Address(r[1]) > 0xffffffafu;
        r[20] = r[1] + 80;
        auto v = std::int32_t(r[20]);
        s.cr0 = {std::uint8_t(v < 0), std::uint8_t(v > 0), std::uint8_t(v == 0), s.xer_so};
        if (s.cr0.eq) {
            FailLinks(true);
            return;
        }
        r[5] = r[21];
        r[4] = 0;
        r[3] = r[20];
        Lower(0x82b7bc40u, 0x82bb93d4u);
        Store(r[18], r[19]);
        while (true) {
            r[30] = r[19];
            Compare(r[21]);
            if (!s.cr6.eq)
                do {
                    r[11] = m.ReadU8(Address(r[30]) + Address(r[20]));
                    Compare(r[11]);
                    if (s.cr6.eq)
                        break;
                    ++r[30];
                    Compare(r[30], r[21]);
                } while (s.cr6.lt);
            Compare(r[30], r[21]);
            if (s.cr6.eq)
                break;
            r[3] = r[31] + 112;
            Lower(0x82bd2a08u, 0x82bb940cu);
            r[6] = r[20];
            r[5] = r[30];
            r[4] = Word(r[31] + 84);
            r[3] = r[31] + 112;
            Lower(0x82bb8498u, 0x82bb9420u);
            r[3] = r[31] + 96;
            Lower(0x82bd2a08u, 0x82bb9428u);
            GatherBoundary();
            r[3] = r[31] + 128;
            Lower(0x82bd2a08u, 0x82bb9624u);
            r[4] = r[31] + 96;
            r[3] = r[31] + 128;
            Lower(0x82bc2a18u, 0x82bb9630u);
            r[11] = Address(r[3]) & 255u;
            Compare(r[11]);
            if (s.cr6.eq) {
                r[11] = 0xffffffff820d0000ull;
                r[5] = 162;
                r[4] = r[11] + 25688;
                r[3] = 0;
                Lower(0x82b9d328u, 0x82bb97bcu);
                r[30] = r[3];
                Cleanup(0x82bb97c8u);
                r[3] = r[31] + 80;
                Lower(0x82bc3ec0u, 0x82bb97e0u);
                r[3] = r[30];
                return;
            }
            r[30] = Word(r[31] + 132);
            Compare(r[30]);
            if (!s.cr6.eq)
                OutputPolygon();
            Cleanup(0x82bb9794u);
        }
        r[3] = r[31] + 80;
        Lower(0x82bc3ec0u, 0x82bb97f4u);
        r[3] = 1;
    }
};
} // namespace
void ProbeStack(GuestMemory &m, Registers &s) {
    auto &r = s.r;
    r[11] = 0 - r[12];
    r[0] = r[11] + 4095;
    auto v = std::int32_t(r[0]);
    s.xer_ca = (v < 0) && ((Address(r[0]) & 4095u) != 0);
    r[0] = std::uint64_t(std::int64_t(v >> 12));
    auto n = std::int32_t(r[0]);
    s.cr0 = {std::uint8_t(n < 0), std::uint8_t(n > 0), std::uint8_t(n == 0), s.xer_so};
    if (!s.cr0.gt)
        return;
    r[11] = r[1];
    s.ctr = r[0];
    do {
        auto p = r[11] - 4096;
        r[0] = m.ReadU32(Address(p));
        r[11] = p;
        --s.ctr;
    } while (Address(s.ctr));
}

bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Collect c{m, d, s};
    if (e == 0x82bb9318u) {
        c.Run();
        return true;
    }
    if (e == 0x82b7e504u) {
        c.Probe();
        return true;
    }
    return false;
}
} // namespace lo::semantic::gpu::mesh_polygon_collect61
