#include "ppc_context.h"

#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/integer_leaf.h"
#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/mesh_convex_check61.h"
#include "lo_semantics/mesh_convex_hull61.h"
#include "lo_semantics/mesh_cook_hull61.h"
#include "lo_semantics/mesh_hull_incremental61.h"
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
namespace incremental_smoke {
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
    f(0x82000e50, 0);
    f(0x82000dac, .1f);
    f(0x820d5fb8, -.001f);
    f(0x820d5fbc, .001f);
    constexpr float xyz[]{0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 1};
    for (unsigned i = 0; i < 12; ++i)
        f(Positions + 4 * i, xyz[i]);
    const char *bundle = std::getenv("LO_HULL_INCREMENTAL_CONSTANTS");
    if (!bundle)
        throw std::runtime_error("hull incremental constants required");
    std::ifstream privateConstants(bundle, std::ios::binary);
    std::array<unsigned char, 188> data{};
    privateConstants.read(reinterpret_cast<char *>(data.data()), data.size());
    if (privateConstants.gcount() != 188)
        throw std::runtime_error("hull incremental constants size");
    unsigned offset = 0;
    for (auto region : std::array<test::Region, 16>{{{0x83214d80, 120},
                                                     {0x83215508, 8},
                                                     {0x82000f28, 8},
                                                     {0x82000de0, 4},
                                                     {0x82000b7c, 4},
                                                     {0x82000e44, 4},
                                                     {0x82000dc0, 4},
                                                     {0x820d57f0, 4},
                                                     {0x820009c8, 4},
                                                     {0x82000d7c, 4},
                                                     {0x82000b58, 4},
                                                     {0x822183e8, 4},
                                                     {0x82218644, 4},
                                                     {0x82000e40, 4},
                                                     {0x82000d6c, 4},
                                                     {0x82000da4, 4}}})
        for (unsigned i = 0; i < region.size; ++i)
            m.WriteU8(region.base + i, data[offset++]);
    auto st = sort_engine61_oracle::Initial(0);
    auto call = [&](unsigned e, std::initializer_list<unsigned> args) {
        unsigned j = 3;
        for (auto a : args)
            st.r[j++] = a;
        PPCFPSCRRegister{}.setcsr(st.cached_fp_control);
        if (!mesh_hull_incremental61::Apply(e, m, env.Deps(), st))
            throw std::runtime_error("incremental dispatch");
    };
    call(0x82b9fd50, {Input, Positions, Positions + 12, Positions + 24});
    if (std::bit_cast<float>(m.ReadU32(Input)) != 0 ||
        std::bit_cast<float>(m.ReadU32(Input + 4)) != 0 ||
        std::bit_cast<float>(m.ReadU32(Input + 8)) != 1)
        throw std::runtime_error("triangle normal");
    m.WriteU32(TriangleData, 0);
    m.WriteU32(TriangleData + 4, 1);
    m.WriteU32(TriangleData + 8, 2);
    st.fpr_bits[1] = std::bit_cast<std::uint64_t>(.01);
    call(0x82b9feb8, {Positions, TriangleData, Positions + 36});
    if (st.r[3] != 1)
        throw std::runtime_error("face visibility");
    call(0x82ba0030, {Positions, 0, 1, 2, 3});
    if (st.r[3] != 1)
        throw std::runtime_error("tetra volume predicate");
    call(0x82b9ff70, {TriangleData, 1, 0});
    if (st.r[3] != TriangleData + 20)
        throw std::runtime_error("undirected edge slot");
    f(Input, 1);
    f(Input + 4, 0);
    f(Input + 8, 0);
    m.WriteU32(Count, 0x52000);
    for (unsigned i = 0; i < 4; ++i)
        m.WriteU32(0x52000 + 4 * i, i == 1 ? 0 : 1);
    call(0x82ba1290, {Positions, 4, Input, Count});
    if (st.r[3] != 0)
        throw std::runtime_error("filtered support tie order");
    m.WriteU32(0x52004, 1);
    call(0x82ba1290, {Positions, 4, Input, Count});
    if (st.r[3] != 1)
        throw std::runtime_error("support extremum");
    st.fpr_bits[1] = std::bit_cast<std::uint64_t>(.5);
    call(0x822a2fe0, {});
    if (std::abs(std::bit_cast<double>(st.fpr_bits[1]) - std::sin(.5)) > 1e-12)
        throw std::runtime_error("guest sine polynomial");
    st.fpr_bits[1] = std::bit_cast<std::uint64_t>(.5);
    call(0x822a2f08, {});
    if (std::abs(std::bit_cast<double>(st.fpr_bits[1]) - std::cos(.5)) > 1e-12)
        throw std::runtime_error("guest cosine polynomial");
    for (unsigned i = 0; i < 4; ++i)
        m.WriteU32(0x52000 + 4 * i, 1);
    call(0x82ba3be0, {0x53000, Positions, 4, Count});
    std::set<unsigned> simplex;
    for (unsigned i = 0; i < 4; ++i)
        simplex.insert(m.ReadU32(0x53000 + 4 * i));
    if (simplex != std::set<unsigned>{0, 1, 2, 3})
        throw std::runtime_error("tetrahedron simplex support");
    m.WriteU32(0x832df548, 0x50000);
    m.WriteU32(0x50000, 0x51000);
    m.WriteU32(0x51008, Allocate | 1);
    m.WriteU32(0x51014, Free | 3);
    call(0x82ba3868, {PolygonData, 0, 1, 2});
    if (st.r[3] != PolygonData || m.ReadU32(0x832dc424) != 1 || m.ReadU32(PolygonData + 24) != 0 ||
        m.ReadU32(PolygonData + 12) != 0xffffffffu)
        throw std::runtime_error("face registry");
    f(PolygonData + 32, 2);
    st.fpr_bits[1] = std::bit_cast<std::uint64_t>(1.);
    call(0x82ba1cd8, {});
    if (st.r[3] != PolygonData)
        throw std::runtime_error("farthest face");
    st.r[3] = 0x832dc420;
    (void)mesh_hull_preprocess61::Apply(0x82ba0d68, m, env.Deps(), st);
    if (!env.guest.live.empty())
        throw std::runtime_error("face registry ownership");
    constexpr unsigned faces[4][3]{{0, 2, 1}, {0, 1, 3}, {1, 2, 3}, {2, 0, 3}};
    for (unsigned i = 0; i < 4; ++i) {
        auto p = 0x60000 + 64 * i;
        env.guest.live.insert(p);
        call(0x82ba3868, {p, faces[i][0], faces[i][1], faces[i][2]});
    }
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned slot = 0; slot < 3; ++slot) {
            auto a = faces[i][(slot + 1) % 3], b = faces[i][(slot + 2) % 3];
            for (unsigned j = 0; j < 4; ++j)
                if (j != i) {
                    bool hasA = false, hasB = false;
                    for (auto v : faces[j]) {
                        hasA |= v == a;
                        hasB |= v == b;
                    }
                    if (hasA && hasB)
                        m.WriteU32(0x60000 + 64 * i + 12 + 4 * slot, j);
                }
        }
    call(0x82ba3970, {0x60000, 4});
    auto registry = m.ReadU32(0x832dc420);
    unsigned live = 0;
    for (unsigned i = 0; i < m.ReadU32(0x832dc424); ++i) {
        auto face = m.ReadU32(registry + 4 * i);
        if (!face)
            continue;
        ++live;
        for (unsigned slot = 0; slot < 3; ++slot) {
            auto neighborID = m.ReadU32(face + 12 + 4 * slot),
                 neighbor = m.ReadU32(registry + 4 * neighborID);
            if (!neighbor)
                throw std::runtime_error("extrusion dead neighbor");
            auto a = m.ReadU32(face + 4 * ((slot + 1) % 3)),
                 b = m.ReadU32(face + 4 * ((slot + 2) % 3));
            call(0x82b9ff70, {neighbor, a, b});
            if (m.ReadU32(Address(st.r[3])) != i)
                throw std::runtime_error("extrusion reciprocal link");
        }
    }
    if (live != 6 || m.ReadU32(registry) != 0)
        throw std::runtime_error("extruded tetrahedron face count");
    for (unsigned i = 0; i < m.ReadU32(0x832dc424); ++i) {
        auto face = m.ReadU32(registry + 4 * i);
        if (face) {
            st.r[4] = face;
            env.guest.CallIndirect(Free, m, st);
        }
    }
    st.r[3] = 0x832dc420;
    (void)mesh_hull_preprocess61::Apply(0x82ba0d68, m, env.Deps(), st);
    if (!env.guest.live.empty())
        throw std::runtime_error("extrusion cleanup");
    const auto saved = st;
    st.r[3] = Positions;
    st.r[4] = 4;
    st.r[5] = Count;
    st.r[6] = Count + 4;
    st.r[7] = 0;
    if (!mesh_hull_preprocess61::Apply(0x82ba4a88, m, env.Deps(), st) || st.r[3] != 1 ||
        m.ReadU32(Count + 4) != 4)
        throw std::runtime_error("plain tetrahedron hull");
    for (unsigned r = 14; r < 32; ++r)
        if (st.r[r] != saved.r[r])
            throw std::runtime_error("hull preserved register");
    if (st.r[1] != saved.r[1] || st.lr != saved.lr)
        throw std::runtime_error("hull stack/link preservation");
    st.r[4] = m.ReadU32(Count);
    env.guest.CallIndirect(Free, m, st);
    if (!env.guest.live.empty())
        throw std::runtime_error("plain hull cleanup");
    for (unsigned i = 0; i < 8; ++i)
        for (unsigned axis = 0; axis < 3; ++axis)
            f(Positions + 12 * i + 4 * axis, float((i >> axis) & 1u));
    st.r[3] = Positions;
    st.r[4] = 8;
    st.r[5] = Count;
    st.r[6] = Count + 4;
    st.r[7] = 0;
    if (!mesh_hull_preprocess61::Apply(0x82ba4a88, m, env.Deps(), st) || st.r[3] != 1 ||
        m.ReadU32(Count + 4) != 12)
        throw std::runtime_error("plain cube hull");
    st.r[4] = m.ReadU32(Count);
    env.guest.CallIndirect(Free, m, st);
    if (!env.guest.live.empty())
        throw std::runtime_error("cube hull cleanup");
    f(0x820a6b8c, 1e-6f);
    f(0x82000e10, .01f);
    f(0x82000d64, -std::numeric_limits<float>::max());
    f(0x82000e0c, std::numeric_limits<float>::max());
    m.WriteU32(Input, 1);
    m.WriteU32(Input + 4, 8);
    m.WriteU32(Input + 8, Positions);
    m.WriteU32(Input + 12, 12);
    f(Input + 16, .001f);
    m.WriteU32(Input + 24, 0);
    st.r[3] = Owner;
    st.r[4] = Input;
    st.r[5] = Polygons;
    if (!mesh_hull_preprocess61::Apply(0x82ba5cf8, m, env.Deps(), st) || st.r[3] != 0 ||
        m.ReadU32(Polygons + 4) != 8 || m.ReadU32(Polygons + 12) != 12 ||
        m.ReadU32(Polygons + 16) != 36)
        throw std::runtime_error("prepared cube output");
    for (auto offset : {8u, 20u}) {
        st.r[4] = m.ReadU32(Polygons + offset);
        env.guest.CallIndirect(Free, m, st);
    }
    if (!env.guest.live.empty())
        throw std::runtime_error("prepared cube cleanup");
}
} // namespace incremental_smoke
int main() {
    try {
        incremental_smoke::Check();
        std::puts("PASS incremental primitives, support/simplex, plain tetrahedron/cube hulls and "
                  "prepared cube output");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
