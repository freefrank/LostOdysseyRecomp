#include "ppc_context.h"

#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/integer_leaf.h"
#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/mesh_convex_check61.h"
#include "lo_semantics/mesh_convex_hull61.h"
#include "lo_semantics/mesh_cook_hull61.h"
#include "lo_semantics/mesh_indexed_cook61.h"
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
namespace indexed_cook_smoke {
using Registers = mesh_convex_hull61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Owner = 0x30000, Source = Owner + 156, Input = 0x32000, Positions = 0x33000,
                       Count = 0x34000, Polygons = 0x35000, Triangles = 0x36000,
                       AllocatorTable = 0x37000, PolygonData = 0x38000, TriangleData = 0x39000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr std::array<test::Region, 9> Regions{{{0, 0x180000},
                                               {0x82000000, 0x10000},
                                               {0x83214000, 0x3000},
                                               {0x832df000, 0x1000},
                                               {0x821ba000, 0x1000},
                                               {0x8201f000, 0x1000},
                                               {0x820d0000, 0x10000},
                                               {0x82210000, 0x10000},
                                               {0x832dc000, 0x1000}}};
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

void Check(bool narrow) {
    test::GuestWindow w(Regions);
    w.Fill(0);
    auto m = w.Memory();
    Environment env(w);
    m.WriteU32(0x83216624, AllocatorTable);
    m.WriteU32(AllocatorTable, Allocate | 1);
    m.WriteU32(AllocatorTable + 12, Free | 3);
    m.WriteU32(Owner + 108, 0xa5);
    m.WriteU32(Source + 80, 0x40000);
    m.WriteU32(0x40004, 0x41000);
    m.WriteU32(0x41000, 0x55);
    m.WriteU32(Input, 4);
    m.WriteU32(Input + 4, 4);
    m.WriteU32(Input + 8, 20);
    m.WriteU32(Input + 12, narrow ? 8 : 16);
    m.WriteU32(Input + 16, Positions);
    m.WriteU32(Input + 20, TriangleData);
    m.WriteU32(Input + 24, narrow ? 2 : 0);
    constexpr float xyz[]{0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 1};
    constexpr unsigned ids[]{0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3};
    for (unsigned i = 0; i < 12; ++i)
        m.WriteU32(Positions + 20 * (i / 3) + 4 * (i % 3), std::bit_cast<std::uint32_t>(xyz[i]));
    for (unsigned i = 0; i < 12; ++i) {
        auto p = TriangleData + (narrow ? 8 : 16) * (i / 3) + (narrow ? 2 : 4) * (i % 3);
        if (narrow)
            m.WriteU16(p, ids[i]);
        else
            m.WriteU32(p, ids[i]);
    }
    for (unsigned i = 0; i < 184; ++i)
        m.WriteU8(0x83214e88 + i, constants[i]);
    auto f = [&](unsigned p, float v) { m.WriteU32(p, std::bit_cast<std::uint32_t>(v)); };
    f(0x8201f9f0, .5f);
    f(0x82000f20, 1.f / 3.f);
    f(0x82000d64, -std::numeric_limits<float>::max());
    f(0x82000e0c, std::numeric_limits<float>::max());
    f(0x82007784, 1.f);
    f(0x821baa74, 2.f);
    f(0x82000b58, .01f);
    f(0x82000dac, .1f);
    f(0x820038fc, .1f);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = Input;
    auto initial = s;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_indexed_cook61::Apply(0x82b9e8a0u, m, env.Deps(), s);
    if (s.r[3] != 1 || m.ReadU32(Source + 12) != 4 || m.ReadU32(Source + 4) != 4 ||
        m.ReadU32(Source + 36) != 4 || m.ReadU32(Owner + 108) != 0xa4)
        throw std::runtime_error("indexed cooking result");
    for (unsigned i = 14; i < 32; ++i)
        if (s.r[i] != initial.r[i])
            throw std::runtime_error("indexed cooking preserved GPR");
    if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr)) {
        std::fprintf(stderr, "sp %llx/%llx lr %llx/%llx\n", (unsigned long long)s.r[1],
                     (unsigned long long)initial.r[1], (unsigned long long)s.lr,
                     (unsigned long long)Address(initial.lr));
        throw std::runtime_error("indexed cooking stack/LR");
    }
    for (unsigned off = 108; off <= 128; off += 4)
        if (m.ReadU32(Source + off) != 0x55)
            throw std::runtime_error("adapter metadata");
    std::set<GuestAddress> retained{m.ReadU32(Source + 8), m.ReadU32(Source + 16),
                                    m.ReadU32(Source + 40), m.ReadU32(Source + 44)};
    if (env.guest.live != retained)
        throw std::runtime_error("indexed cooking retained ownership");
}
} // namespace indexed_cook_smoke
int main() {
    try {
        const char *p = std::getenv("LO_MESH_MATH_CONSTANTS");
        if (!p)
            throw std::runtime_error("atan2 constants required");
        std::ifstream f(p, std::ios::binary);
        f.read(reinterpret_cast<char *>(indexed_cook_smoke::constants.data()), 184);
        if (f.gcount() != 184)
            throw std::runtime_error("constants size");
        indexed_cook_smoke::Check(false);
        indexed_cook_smoke::Check(true);
        std::puts("PASS indexed cook strided u32/u16 tetrahedron paths; counts, ABI and retained "
                  "ownership");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
