#include "ppc_context.h"

#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/integer_leaf.h"
#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/mesh_convex_check61.h"
#include "lo_semantics/mesh_convex_hull61.h"
#include "lo_semantics/mesh_cook_hull61.h"
#include "lo_semantics/mesh_hull_preprocess61.h"
#include "lo_semantics/mesh_polygon_plane61.h"
#include "lo_semantics/mesh_polygon_triangulate61.h"
#include "lo_semantics/mesh_vertex_dedup61.h"
#include "lo_semantics/mesh_vertex_normals61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <set>
namespace preprocess_smoke {
using Registers = mesh_convex_hull61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Owner = 0x30000, Source = Owner + 156, Input = 0x32000, Positions = 0x33000,
                       Count = 0x34000, Polygons = 0x35000, Triangles = 0x36000,
                       AllocatorTable = 0x37000, PolygonData = 0x38000, TriangleData = 0x39000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr std::array<test::Region, 10> Regions{{{0, 0x180000},
                                                {0x82000000, 0x10000},
                                                {0x83214000, 0x3000},
                                                {0x832df000, 0x1000},
                                                {0x821ba000, 0x1000},
                                                {0x8201f000, 0x1000},
                                                {0x820d0000, 0x10000},
                                                {0x82210000, 0x10000},
                                                {0x832dc000, 0x1000},
                                                {0x820a6000, 0x1000}}};
std::array<unsigned char, 184> constants{};
std::array<unsigned char, 40> hull_constants{};
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
            if (s.r[4] > 8192)
                throw std::runtime_error("edge allocation size");
            s.r[3] = 0x90000 + 8192 * allocations++;
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
    mesh_convex_hull61::Dependencies Deps() { return {EdgeDeps(), {guest, accepted.fp}}; }
};

void Check() {
    test::GuestWindow w(Regions);
    w.Fill(0);
    auto m = w.Memory();
    Environment env(w);
    auto f = [&](unsigned p, float v) { m.WriteU32(p, std::bit_cast<std::uint32_t>(v)); };
    f(0x82007784, 1);
    f(0x82000e0c, std::numeric_limits<float>::max());
    f(0x82000d64, -std::numeric_limits<float>::max());
    f(0x8201f9f0, .5f);
    f(0x820a6b8c, .000001f);
    f(0x82000d7c, .01f);
    f(0x82000e10, .01f);
    constexpr float xyz[]{0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    for (unsigned i = 0; i < 15; ++i)
        f(Positions + 4 * i, xyz[i]);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[4] = 5;
    s.r[5] = Positions;
    s.r[6] = 12;
    s.r[7] = Count;
    s.r[8] = PolygonData;
    s.r[10] = Input;
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(.001);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_hull_preprocess61::Apply(0x82ba0230u, m, env.Deps(), s);
    if (s.r[3] != 1 || m.ReadU32(Count) != 4 || std::bit_cast<float>(m.ReadU32(Input)) != 2 ||
        std::bit_cast<float>(m.ReadU32(PolygonData + 12)) != 1)
        throw std::runtime_error("normalized dedup");
    s.r[4] = 3;
    s.r[5] = Positions;
    s.r[6] = 12;
    s.r[7] = Count;
    s.r[8] = PolygonData;
    s.r[10] = Input;
    (void)mesh_hull_preprocess61::Apply(0x82ba0230u, m, env.Deps(), s);
    if (s.r[3] != 1 || m.ReadU32(Count) != 8 || std::bit_cast<float>(m.ReadU32(Input)) != 1 ||
        std::bit_cast<float>(m.ReadU32(PolygonData + 8)) >= 0 ||
        std::bit_cast<float>(m.ReadU32(PolygonData + 56)) <= 0)
        throw std::runtime_error("degenerate box fallback");
    m.WriteU32(0x832df548, 0x50000);
    m.WriteU32(0x50000, 0x51000);
    m.WriteU32(0x51008, Allocate | 1);
    m.WriteU32(0x51014, Free | 3);
    constexpr unsigned ids[]{2, 0, 2, 3, 0, 1};
    for (unsigned i = 0; i < 6; ++i)
        m.WriteU32(TriangleData + 4 * i, ids[i]);
    s.r[4] = Positions;
    s.r[5] = 5;
    s.r[6] = PolygonData;
    s.r[7] = Count;
    s.r[8] = TriangleData;
    s.r[9] = 6;
    (void)mesh_hull_preprocess61::Apply(0x82ba0998u, m, env.Deps(), s);
    constexpr unsigned want[]{0, 1, 0, 2, 1, 3};
    for (unsigned i = 0; i < 6; ++i)
        if (m.ReadU32(TriangleData + 4 * i) != want[i])
            throw std::runtime_error("first-use remap");
    if (m.ReadU32(Count) != 4 || !env.guest.live.empty() ||
        std::bit_cast<float>(m.ReadU32(PolygonData + 4)) != 1)
        throw std::runtime_error("compaction ownership");
    for (unsigned off : {0, 4, 8})
        m.WriteU32(Input + off, 0);
    s.r[3] = Input;
    s.r[4] = (std::uint64_t(7) << 32) | 9;
    s.r[5] = std::uint64_t(11) << 32;
    (void)mesh_hull_preprocess61::Apply(0x82ba2280u, m, env.Deps(), s);
    auto triangle = m.ReadU32(Input);
    if (m.ReadU32(Input + 4) != 1 || m.ReadU32(Input + 8) != 16 || m.ReadU32(triangle) != 7 ||
        m.ReadU32(triangle + 4) != 9 || m.ReadU32(triangle + 8) != 11)
        throw std::runtime_error("triangle array append");
    s.r[3] = Input;
    (void)mesh_hull_preprocess61::Apply(0x82ba0d68u, m, env.Deps(), s);
    if (!env.guest.live.empty() || m.ReadU32(Input) || m.ReadU32(Input + 4) || m.ReadU32(Input + 8))
        throw std::runtime_error("triangle array release");
}
} // namespace preprocess_smoke
int main() {
    try {
        preprocess_smoke::Check();
        std::puts("PASS hull preprocess normalization/dedup, degenerate box, first-use remap, "
                  "triangle array lifecycle");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
