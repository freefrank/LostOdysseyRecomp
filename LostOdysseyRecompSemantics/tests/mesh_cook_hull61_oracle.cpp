#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/integer_leaf.h"
#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/mesh_convex_check61.h"
#include "lo_semantics/mesh_convex_hull61.h"
#include "lo_semantics/mesh_cook_hull61.h"
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
namespace cook_hull_oracle {
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

Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    auto d = env.Deps();
    switch (e) {
    case 0x82bba028u:
        (void)mesh_convex_hull61::Apply(e, m, d, s);
        break;
    case 0x82bb3130u:
        (void)mesh_cache_build61::Apply(e, m, d, s);
        break;
    case 0x82bb3008u:
    case 0x82bb32d8u:
        (void)mesh_cache_lifetime61::Apply(e, m, d.lifetime, s);
        break;
    case 0x82b7e504u:
        mesh_polygon_collect61::ProbeStack(m, s);
        break;
    case 0x82b7a0b0u:
        (void)crt_copy_full_context::Apply(e, m, s);
        break;
    case 0x82b9c298u:
        (void)diagnostic_format_routes61::Apply(e, m, d.edge.diagnostics, s);
        break;
    case 0x82b9cb60u: {
        lo::semantic::integer_leaf::Registers leaf{};
        (void)lo::semantic::integer_leaf::Apply(e, leaf);
        s.r[3] = leaf.r3;
        s.r[11] = leaf.r11;
        break;
    }
    default:
        throw std::runtime_error("cook hull lower");
    }
}
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    unsigned n = 8;
    unsigned stride = mode == 3 ? 20 : 12;
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(0x83216624, AllocatorTable);
        m.WriteU32(AllocatorTable, Allocate | 1);
        m.WriteU32(AllocatorTable + 12, Free | 3);
        m.WriteU32(Owner + 4, Source);
        m.WriteU32(Owner + 12, Source);
        m.WriteU32(Owner + 108, 0xa5);
        m.WriteU32(Input, n);
        m.WriteU32(Input + 4, Positions);
        m.WriteU8(Input + 12, 1);
        m.WriteU8(Input + 13, 1);
        if (mode >= 2) {
            m.WriteU32(Input + 8, stride);
            m.WriteU32(Input + 16, Positions);
        }
        constexpr float cube[]{-1, -1, -1, 1, -1, -1, 1, 1,  -1, -1, 1,  -1, -1, -1,
                               1,  1,  -1, 1, 1,  1,  1, -1, 1,  1,  -1, -1, -1};
        constexpr float tetra[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
        for (unsigned i = 0; i < 3 * n; ++i)
            m.WriteU32(Positions + stride * (i / 3) + 4 * (i % 3),
                       std::bit_cast<std::uint32_t>(cube[i]));
        for (unsigned i = 0; i < 184; ++i)
            m.WriteU8(0x83214e88 + i, constants[i]);
        for (unsigned i = 0; i < 40; ++i)
            m.WriteU8(0x820d6480 + i, hull_constants[i]);
        auto f = [&](unsigned p, float v) { m.WriteU32(p, std::bit_cast<std::uint32_t>(v)); };
        f(0x8201f9f0, .5f);
        f(0x82000f20, 1.f / 3.f);
        f(0x82000d64, -std::numeric_limits<float>::max());
        f(0x82000e0c, std::numeric_limits<float>::max());
        f(0x82007784, 1.f);
        f(0x832dc184, .1f);
        f(0x82000dac, .1f);
        f(0x820038fc, .1f);
        f(0x821baa74, 2.f);
        f(0x82215748, .001f);
        f(0x82000b58, .01f);
        recovery_abi::WriteU64(m, 0x82000f70, std::bit_cast<std::uint64_t>(.5));
        recovery_abi::WriteU64(m, 0x82000f28, std::bit_cast<std::uint64_t>(1.));
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = Input;
    if (mode == 1) {
        s.r[3] = Source;
        s.r[4] = n;
        s.r[5] = Positions;
    }
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(.1);
    auto om = before.Memory(), m = after.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode == 0)
        __imp__sub_82BB3350(c, before.Bytes());
    else if (mode == 1)
        __imp__sub_82B9E3F8(c, before.Bytes());
    else
        __imp__sub_82B9E7B0(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    memory = nullptr;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_cook_hull61::Apply(mode == 0   ? 0x82bb3350u
                                  : mode == 1 ? 0x82b9e3f8u
                                              : 0x82b9e7b0u,
                                  m, actual.Deps(), s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "hull%u Full%d RAM%d events%d live%d host%d\n", mode, a == b,
                     before.EqualCommitted(after), expected.guest.events == actual.guest.events,
                     expected.guest.live == actual.guest.live, host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        for (unsigned i = 0; i < std::min(expected.guest.events.size(), actual.guest.events.size());
             ++i)
            if (expected.guest.events[i] != actual.guest.events[i]) {
                std::fprintf(stderr, "first event %u\n", i);
                for (unsigned k = 0; k < 73; ++k)
                    if (expected.guest.events[i][k] != actual.guest.events[i][k])
                        std::fprintf(stderr, "ev%u %llx/%llx\n", k,
                                     (unsigned long long)expected.guest.events[i][k],
                                     (unsigned long long)actual.guest.events[i][k]);
                break;
            }
        throw std::runtime_error("hull original mismatch");
    }
    if (s.r[3] != 1 || m.ReadU32(Source + 12) != 8u || m.ReadU32(Source + 4) != 12u ||
        m.ReadU32(Source + 36) != 6u)
        throw std::runtime_error("hull geometry counts");
    std::set<GuestAddress> retained{m.ReadU32(Source + 8), m.ReadU32(Source + 16),
                                    m.ReadU32(Source + 40), m.ReadU32(Source + 44)};
    if (mode == 0) {
        auto cache = m.ReadU32(Owner + 16);
        retained.insert(cache);
        retained.insert(m.ReadU32(cache + 12));
        retained.insert(m.ReadU32(cache + 16));
    }
    if (mode >= 2 && m.ReadU32(Owner + 108) != 0xa5)
        throw std::runtime_error("owner convex flag");
    if (actual.guest.live != retained)
        throw std::runtime_error("hull retained ownership");
    // Independently check closed triangular topology and outward hull halfspaces.
    auto tris = m.ReadU32(Source + 8), verts = m.ReadU32(Source + 16),
         polys = m.ReadU32(Source + 40);
    std::map<std::pair<unsigned, unsigned>, unsigned> edges;
    for (unsigned i = 0; i < m.ReadU32(Source + 4); ++i) {
        unsigned ids[]{m.ReadU32(tris + 12 * i), m.ReadU32(tris + 12 * i + 4),
                       m.ReadU32(tris + 12 * i + 8)};
        for (unsigned j = 0; j < 3; ++j) {
            auto a = ids[j], b = ids[(j + 1) % 3];
            if (a >= m.ReadU32(Source + 12) || a == b)
                throw std::runtime_error("hull index");
            ++edges[std::minmax(a, b)];
        }
    }
    for (auto [edge, degree] : edges)
        if (degree != 2)
            throw std::runtime_error("hull closed edges");
    for (unsigned i = 0; i < m.ReadU32(Source + 36); ++i) {
        auto p = polys + 36 * i;
        float plane[4];
        for (unsigned j = 0; j < 4; ++j)
            plane[j] = std::bit_cast<float>(m.ReadU32(p + 12 + 4 * j));
        if (m.ReadU16(p) != 4)
            throw std::runtime_error("hull polygon degree");
        for (unsigned j = 0; j < n; ++j) {
            float distance = plane[3];
            for (unsigned k = 0; k < 3; ++k)
                distance +=
                    plane[k] * std::bit_cast<float>(m.ReadU32(Positions + stride * j + 4 * k));
            if (distance > .0001f)
                throw std::runtime_error("hull enclosure");
        }
    }
    auto originalState = crt_full_oracle::FromPpc(c);
    if (mode == 0) {
        originalState.r[3] = Owner;
        s.r[3] = Owner;
        PPCFPSCRRegister{}.setcsr(originalState.cached_fp_control);
        (void)mesh_cache_lifetime61::Apply(0x82bb32d8u, om, expected.Deps().lifetime,
                                           originalState);
        PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
        (void)mesh_cache_lifetime61::Apply(0x82bb32d8u, m, actual.Deps().lifetime, s);
    }
    originalState.r[3] = Source;
    s.r[3] = Source;
    PPCFPSCRRegister{}.setcsr(originalState.cached_fp_control);
    (void)mesh_auxiliary_storage61::Apply(0x82bc85f0u, om, expected.Deps().lifetime, originalState);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_auxiliary_storage61::Apply(0x82bc85f0u, m, actual.Deps().lifetime, s);
    if (!expected.guest.live.empty() || !actual.guest.live.empty() ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events)
        throw std::runtime_error("hull teardown");
}
} // namespace cook_hull_oracle
void CookHullIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cook_hull_oracle::original->guest.CallIndirect(e, *cook_hull_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void CookHullLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cook_hull_oracle::Lower(e, *cook_hull_oracle::memory, *cook_hull_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void CookHullSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cook_hull_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void CookHullRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cook_hull_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
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
        f.read(reinterpret_cast<char *>(cook_hull_oracle::constants.data()), 184);
        if (f.gcount() != 184)
            throw std::runtime_error("private constant size");
        const char *hp = std::getenv("LO_HULL_CONSTANTS");
        if (!hp)
            throw std::runtime_error("hull constants required");
        std::ifstream hf(hp, std::ios::binary);
        hf.read(reinterpret_cast<char *>(cook_hull_oracle::hull_constants.data()), 40);
        if (hf.gcount() != 40)
            throw std::runtime_error("hull constants size");
        for (unsigned i = 0; i < 4; ++i)
            cook_hull_oracle::Check(i);
        std::puts("PASS mesh-cook-hull61 4 original-upper/shared-concrete-hull and teardown cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
