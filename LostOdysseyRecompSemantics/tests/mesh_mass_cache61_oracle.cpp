#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/mesh_geometry_stream61.h"
#include "lo_semantics/mesh_mass_cache61.h"
#include "lo_semantics/mesh_normal_encode61.h"
#include "lo_semantics/mesh_polygon_plane61.h"
#include "lo_semantics/mesh_polygon_triangulate61.h"
#include "lo_semantics/mesh_valence_stream61.h"
#include "lo_semantics/mesh_vertex_normals61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/serialization_control61.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
namespace mass_cache_oracle {
using Registers = mesh_mass_cache61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Owner = 0x30000, Source = 0x31000, Input = 0x32000, Positions = 0x33000,
                       Count = 0x34000, Polygons = 0x35000, Triangles = 0x36000,
                       AllocatorTable = 0x37000, PolygonData = 0x38000, TriangleData = 0x39000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr GuestAddress Writer = 0x40000, Table = 0x41000, Buffer = 0x42000;
constexpr std::array<test::Region, 9> Regions{{{0, 0x180000},
                                               {0x82000000, 0x10000},
                                               {0x83214000, 0x3000},
                                               {0x832df000, 0x1000},
                                               {0x821ba000, 0x1000},
                                               {0x8201f000, 0x1000},
                                               {0x832dc000, 0x1000},
                                               {0x820d6000, 0x1000},
                                               {0x82218000, 0x1000}}};
std::array<unsigned char, 128> mass_constants{};
std::array<unsigned char, 144> normal_constants{};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
} native;
struct Guest final : manager_release_context61::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    std::set<GuestAddress> live;
    unsigned allocations = 0;
    void CallDirect(GuestAddress, GuestMemory &, Registers &) override {
        throw std::runtime_error("cache build direct boundary");
    }
    void CallIndirect(GuestAddress e, GuestMemory &m, Registers &s) override {
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
        } else {
            if (!growable_output61::Apply(e, m, {*this, native}, s))
                throw std::runtime_error("stream callback " + std::to_string(e) + " lr " +
                                         std::to_string(s.lr));
            return;
        }
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
    mesh_geometry_stream61::Dependencies Deps() { return {EdgeDeps(), {guest, accepted.fp}}; }
};

Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    if (e == 0x82bcd8a8u)
        (void)mesh_mass_math61::Apply(e, m, env.accepted.fp, s);
    else if (e == 0x82b7dfc0u)
        mesh_mass_cache61::Classify(m, env.accepted.fp, s);
    else
        (void)diagnostic_format_routes61::Apply(e, m, env.Deps().edge.diagnostics, s);
}
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    // Only mass integration constant pages are used here.
    constexpr std::array<test::Region, 6> pages{{{0, 0x180000},
                                                 {0x82000000, 0x10000},
                                                 {0x820d6000, 0x1000},
                                                 {0x82051000, 0x1000},
                                                 {0x82048000, 0x1000},
                                                 {0x832df000, 0x1000}}};
    test::GuestWindow before(pages), after(pages);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        for (unsigned i = 0; i < 72; ++i)
            m.WriteU8(0x820d6a78 + i, mass_constants[i]);
        constexpr GuestAddress extra[]{0x82000fe8, 0x82001010, 0x82051430, 0x82000f70,
                                       0x82048090, 0x82000f28, 0x820d6ac0};
        for (unsigned i = 0; i < 7; ++i)
            for (unsigned j = 0; j < 8; ++j)
                m.WriteU8(extra[i] + j, mass_constants[72 + 8 * i + j]);
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x832df548, mode == 2 ? 0 : 1);
        m.WriteU32(Owner + 292, std::bit_cast<std::uint32_t>(mode == 1 ? 17.f : -1.f));
        m.WriteU32(Owner + 160, 12);
        m.WriteU32(Owner + 164, Input);
        m.WriteU32(Owner + 168, 8);
        m.WriteU32(Owner + 172, Positions);
        constexpr float points[]{0, 1, 2, 2, 1, 2, 2, 3, 2, 0, 3, 2,
                                 0, 1, 4, 2, 1, 4, 2, 3, 4, 0, 3, 4};
        constexpr unsigned indices[]{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                                     3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
        for (unsigned i = 0; i < 24; ++i)
            m.WriteU32(Positions + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        for (unsigned i = 0; i < 36; ++i)
            m.WriteU32(Input + 4 * i, indices[i]);
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    auto om = before.Memory();
    memory = &om;
    original = &expected;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82B9F418(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    memory = nullptr;
    original = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_mass_cache61::Apply(0x82b9f418u, m,
                                   {actual.accepted.fp, actual.Deps().edge.diagnostics}, s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("mass cache Full72/RAM/CSR mode " + std::to_string(mode));
    if (s.r[3] != (mode == 2 ? 0 : Owner + 292))
        throw std::runtime_error("mass cache result");
    auto f = [&](GuestAddress p) { return std::bit_cast<float>(m.ReadU32(p)); };
    if (mode == 0) {
        if (f(Owner + 292) != 8)
            throw std::runtime_error("cached volume");
        double center[]{1, 2, 3};
        for (unsigned i = 0; i < 3; ++i)
            if (f(Owner + 332 + 4 * i) != center[i])
                throw std::runtime_error("cached center");
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned j = 0; j < 3; ++j) {
                float inertia = float(i == j ? 16. / 3 + 8 * (14 - center[i] * center[i])
                                             : -8 * center[i] * center[j]);
                if (f(Owner + 296 + 4 * (3 * i + j)) != inertia)
                    throw std::runtime_error("cached origin inertia");
            }
    } else if (f(Owner + 292) != (mode == 1 ? 17 : -1))
        throw std::runtime_error("cache preserved");
}
} // namespace mass_cache_oracle
void MassCacheLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    mass_cache_oracle::Lower(e, *mass_cache_oracle::memory, *mass_cache_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void MassCacheSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mass_cache_oracle::memory;
    for (unsigned i = 29; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void MassCacheRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mass_cache_oracle::memory;
    for (unsigned i = 29; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        const char *path = std::getenv("LO_MASS_CONSTANTS");
        if (!path)
            throw std::runtime_error("private mass constants required");
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(mass_cache_oracle::mass_constants.data()), 128);
        if (f.gcount() != 128)
            throw std::runtime_error("private mass constant size");
        for (unsigned i = 0; i < 3; ++i)
            mass_cache_oracle::Check(i);
        std::puts("PASS mesh-mass-cache61 3 original-upper/shared-concrete-math cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
