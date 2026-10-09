#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/mesh_polygon_build61.h"
#include "lo_semantics/mesh_polygon_plane61.h"
#include "lo_semantics/mesh_polygon_triangulate61.h"
#include "lo_semantics/mesh_vertex_normals61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
namespace polygon_build_oracle {
using Registers = mesh_polygon_build61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Owner = 0x30000, Source = 0x31000, Input = 0x32000, Positions = 0x33000,
                       Count = 0x34000, Polygons = 0x35000, Triangles = 0x36000,
                       AllocatorTable = 0x37000, PolygonData = 0x38000, TriangleData = 0x39000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr std::array<test::Region, 6> Regions{{{0, 0x180000},
                                               {0x82000000, 0x10000},
                                               {0x83214000, 0x3000},
                                               {0x832df000, 0x1000},
                                               {0x821ba000, 0x1000},
                                               {0x8201f000, 0x1000}}};
std::array<unsigned char, 184> constants{};
struct Guest final : manager_release_context61::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    std::set<GuestAddress> live;
    unsigned allocations = 0;
    void CallDirect(GuestAddress, GuestMemory &, Registers &) override {
        throw std::runtime_error("cache build direct boundary");
    }
    void CallIndirect(GuestAddress e, GuestMemory &, Registers &s) override {
        std::array<std::uint64_t, 73> ev{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), ev.begin());
        ev.back() = e;
        events.push_back(ev);
        if (e == Allocate) {
            if (s.r[4] > 4096)
                throw std::runtime_error("edge allocation size");
            s.r[3] = 0x90000 + 4096 * allocations++;
            live.insert(std::uint32_t(s.r[3]));
        } else if (e == Free) {
            if (!live.erase(std::uint32_t(s.r[4])))
                throw std::runtime_error("edge unknown free");
            s.r[3] = 0;
        } else
            throw std::runtime_error("edge allocator callback");
        s.r[8] ^= 0x1234u;
        s.cr7.eq ^= 1;
    }
};
struct Sync final : diagnostic_lock61::SynchronizationServices {
    std::uint32_t LoadReservedWord(GuestAddress, GuestMemory &) override {
        throw std::runtime_error("unexpected diagnostic reservation");
    }
    bool CompareExchangeWord(GuestAddress, std::uint32_t, std::uint32_t, GuestMemory &) override {
        throw std::runtime_error("unexpected diagnostic CAS");
    }
    void EnterCriticalSection(GuestMemory &, Registers &, Machine &) override {
        throw std::runtime_error("unexpected diagnostic lock");
    }
    void LeaveCriticalSection(GuestMemory &, Registers &, Machine &) override {
        throw std::runtime_error("unexpected diagnostic unlock");
    }
};
struct DiagnosticGuest final : crt_narrow_formatter61::GuestServices {
    void CallIndirect(GuestAddress, GuestMemory &, Registers &) override {
        throw std::runtime_error("unexpected diagnostic output");
    }
    void CallOutput(GuestMemory &, Registers &) override {
        throw std::runtime_error("unexpected formatter output");
    }
};

struct Environment {
    sort_engine61_oracle::Environment accepted;
    Guest guest;
    Sync sync;
    DiagnosticGuest diagnostic;
    Machine machine{0x020a8020u, 0xcafebabe11223344ull};
    explicit Environment(test::GuestWindow &w) : accepted(w) {}
    mesh_edge_build61::Dependencies EdgeDeps() {
        return {{{guest, accepted.Deps().sort.accepted}, accepted.fp},
                {{accepted.Deps().sort.accepted, diagnostic}, sync, machine}};
    }
    mesh_polygon_build61::Dependencies Deps() { return {EdgeDeps(), {guest, accepted.fp}}; }
};

Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    auto d = env.Deps();
    switch (e) {
    case 0x82bd0798u:
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
        break;
    case 0x82bd2a08u:
    case 0x82bd2c08u:
        (void)object_sort_support61::Apply(e, m, {env.guest, env.accepted.fp}, s);
        break;
    case 0x82bb9318u:
        (void)mesh_polygon_collect61::Apply(e, m, d, s);
        break;
    case 0x82b7bc40u:
        crt_reader_chain61::ApplySupport_B7BC40(m, d.edge.engine.sort.accepted, s);
        break;
    case 0x82bc65f8u:
    case 0x82bd92c0u:
        (void)mesh_geometry_math61::Apply(e, m, env.accepted.fp, s);
        break;
    case 0x82bd9390u:
    case 0x82bc3880u:
        (void)mesh_polygon_plane61::Apply(e, m, env.accepted.fp, s);
        break;
    case 0x82bb8c08u:
        (void)mesh_polygon_triangulate61::Apply(e, m, d.lifetime, s);
        break;
    default:
        throw std::runtime_error("polygon build lower");
    }
}
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(0x83216624, AllocatorTable);
        m.WriteU32(AllocatorTable, Allocate | 1);
        m.WriteU32(AllocatorTable + 12, Free | 3);
        m.WriteU32(Owner + 4, Source);
        m.WriteU32(Source + 4, mode == 2 ? 1 : 12);
        m.WriteU32(Source + 8, Input);
        m.WriteU32(Source + 12, 8);
        m.WriteU32(Source + 16, Positions);
        m.WriteU32(Count, 99);
        if (mode == 1) {
            m.WriteU32(Source + 36, 9);
            m.WriteU32(Source + 40, 0x3a000);
            m.WriteU32(Source + 44, 0x3b000);
        }
        m.WriteU32(0x8201f9f0, std::bit_cast<std::uint32_t>(0.5f));
        m.WriteU32(0x82000f20, std::bit_cast<std::uint32_t>(1.f / 3.f));
        m.WriteU32(0x82000d64, std::bit_cast<std::uint32_t>(-std::numeric_limits<float>::max()));
        m.WriteU32(0x82000e0c, std::bit_cast<std::uint32_t>(std::numeric_limits<float>::max()));
        m.WriteU32(Polygons, 128);
        m.WriteU32(Polygons + 8, PolygonData);
        m.WriteU32(Polygons + 12, std::bit_cast<std::uint32_t>(2.f));
        m.WriteU32(Triangles, 128);
        m.WriteU32(Triangles + 8, TriangleData);
        m.WriteU32(Triangles + 12, std::bit_cast<std::uint32_t>(2.f));
        constexpr float points[]{-1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1,
                                 -1, -1, 1,  1, -1, 1,  1, 1, 1,  -1, 1, 1};
        constexpr unsigned ix[]{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                                3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
        for (unsigned i = 0; i < 24; ++i)
            m.WriteU32(Positions + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        for (unsigned i = 0; i < 36; ++i)
            m.WriteU32(Input + 4 * i, ix[i]);
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x82000dac, std::bit_cast<std::uint32_t>(0.1f));
        m.WriteU32(0x820038fc, std::bit_cast<std::uint32_t>(0.1f));
        m.WriteU32(0x821baa74, std::bit_cast<std::uint32_t>(2.f));
        for (unsigned i = 0; i < 184; ++i)
            m.WriteU8(0x83214e88 + i, constants[i]);
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    expected.guest.live.insert(Input);
    actual.guest.live.insert(Input);
    if (mode == 1) {
        for (unsigned p : {0x3a000u, 0x3b000u}) {
            expected.guest.live.insert(p);
            actual.guest.live.insert(p);
        }
    }
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = Polygons;
    s.r[5] = Owner;
    s.r[6] = mode == 1 ? 0 : Triangles;
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BB9AA8(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_polygon_build61::Apply(0x82bb9aa8u, m, actual.Deps(), s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("polygon collect Full72/RAM/CSR/events mode " +
                                 std::to_string(mode));

    if (mode == 2) {
        if (s.r[3] || m.ReadU32(Source + 36) || m.ReadU32(Source + 40) || m.ReadU32(Source + 44) ||
            actual.guest.live != std::set<GuestAddress>{Input})
            throw std::runtime_error("open mesh polygon failure");
        return;
    }
    auto records = m.ReadU32(Source + 40), bytes = m.ReadU32(Source + 44),
         tris = m.ReadU32(Source + 8);
    if (s.r[3] != 1 || m.ReadU32(Source + 36) != 6 || m.ReadU32(Source + 4) != 12 ||
        actual.guest.live != std::set<GuestAddress>{records, bytes, tris})
        throw std::runtime_error("built polygons ownership/counts");
    const std::set<std::set<unsigned>> expectedFaces{{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 4, 5},
                                                     {2, 3, 6, 7}, {0, 3, 4, 7}, {1, 2, 5, 6}};
    std::set<std::set<unsigned>> faces;
    for (unsigned i = 0; i < 6; ++i) {
        auto rec = records + 36 * i;
        if (m.ReadU16(rec) != 4 || m.ReadU32(rec + 4) != bytes + 4 * i)
            throw std::runtime_error("polygon record degree/slice");
        std::set<unsigned> face;
        for (unsigned j = 0; j < 4; ++j)
            face.insert(m.ReadU8(bytes + 4 * i + j));
        faces.insert(face);
        float len = 0;
        for (unsigned j = 0; j < 3; ++j) {
            float n = std::bit_cast<float>(m.ReadU32(rec + 12 + 4 * j));
            len += n * n;
        }
        if (std::abs(len - 1.f) > 1e-6f || std::bit_cast<float>(m.ReadU32(rec + 24)) != -1.f ||
            std::bit_cast<float>(m.ReadU32(rec + 28)) != -1.f ||
            std::bit_cast<float>(m.ReadU32(rec + 32)) != 1.f)
            throw std::runtime_error("outward plane and projections");
        for (unsigned t = 0; t < 2; ++t)
            for (unsigned j = 0; j < 3; ++j)
                if (!face.contains(m.ReadU32(tris + 24 * i + 12 * t + 4 * j)))
                    throw std::runtime_error("rebuilt triangle membership");
    }
    if (faces != expectedFaces)
        throw std::runtime_error("built face membership");
}
} // namespace polygon_build_oracle
void PolygonBuildIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    polygon_build_oracle::original->guest.CallIndirect(e, *polygon_build_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void PolygonBuildLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    polygon_build_oracle::Lower(e, *polygon_build_oracle::memory, *polygon_build_oracle::original,
                                s);
    crt_full_oracle::ToPpc(c, s);
}
void PolygonBuildSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *polygon_build_oracle::memory;
    for (unsigned i = 20; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void PolygonBuildRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *polygon_build_oracle::memory;
    for (unsigned i = 20; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}

int main() {
    try {
        const char *p = std::getenv("LO_MESH_MATH_CONSTANTS");
        if (!p)
            throw std::runtime_error("private constants required");
        std::ifstream f(p, std::ios::binary);
        f.read(reinterpret_cast<char *>(polygon_build_oracle::constants.data()), 184);
        if (f.gcount() != 184)
            throw std::runtime_error("private constant size");
        for (unsigned i = 0; i < 3; ++i)
            polygon_build_oracle::Check(i);
        std::puts("PASS mesh-polygon-build61 3 original-upper/shared-concrete-polygon-chain cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
