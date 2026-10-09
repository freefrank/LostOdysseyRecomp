#include "ppc_context.h"

#include "lo_semantics/mesh_indexed_cook61.h"
#include "lo_semantics/owned_tree_mesh_build61.h"
#include "lo_semantics/tree_mesh_callbacks61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/crt_reader_object_chain61.h"
#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/mesh_cache_lifetime61.h"
#include "lo_semantics/mesh_cook_stream61.h"
#include "lo_semantics/mesh_mass_cache61.h"
#include "lo_semantics/mesh_normal_encode61.h"
#include "lo_semantics/mesh_polygon_plane61.h"
#include "lo_semantics/mesh_polygon_triangulate61.h"
#include "lo_semantics/mesh_support_stream61.h"
#include "lo_semantics/mesh_valence_stream61.h"
#include "lo_semantics/mesh_vertex_normals61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/serialization_control61.h"
#include "lo_semantics/tree_envelope_write61.h"
#include "lo_semantics/tree_flat_write61.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
namespace cook_main_smoke {
using Registers = mesh_cook_stream61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Owner = 0x30000, Source = Owner + 156, Input = 0x32000, Positions = 0x33000,
                       Count = 0x34000, Polygons = 0x35000, Triangles = 0x36000,
                       AllocatorTable = 0x37000, PolygonData = 0x38000, TriangleData = 0x39000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr GuestAddress Writer = 0x40000, Table = 0x41000, Buffer = 0x42000;
constexpr std::array<test::Region, 13> Regions{{{0, 0x180000},
                                                {0x82000000, 0x10000},
                                                {0x83214000, 0x3000},
                                                {0x832df000, 0x1000},
                                                {0x821ba000, 0x1000},
                                                {0x8201f000, 0x1000},
                                                {0x832dc000, 0x1000},
                                                {0x820d5000, 0x2000},
                                                {0x82218000, 0x1000},
                                                {0x82051000, 0x1000},
                                                {0x82048000, 0x1000},
                                                {0x820d2000, 0x2000},
                                                {0x82bc9000, 0x1000}}};
std::array<unsigned char, 184> constants{};
std::array<unsigned char, 144> normal_constants{};
std::array<unsigned char, 128> mass_constants{};
std::array<unsigned char, 1448> power_constants{};
std::array<unsigned char, 8> bounds_constants{};
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
        if (e == 0x82b9f188u || e == 0x82b9f190u) {
            s.r[3] = m.ReadU32(Address(s.r[3]) + (e == 0x82b9f188u ? 168 : 160));
            return;
        }
        if (owned_tree_mesh_build61::Apply(e, m, {*this, native}, s) ||
            tree_mesh_callbacks61::Apply(e, m, {*this, native}, s))
            return;
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
            if (!mesh_stream_write61::Apply(e, m, {*this, native}, s) &&
                !tree_envelope_write61::Apply(e, m, {*this, native}, s) &&
                !tree_flat_write61::Apply(e, m, {*this, native}, s) &&
                !growable_output61::Apply(e, m, {*this, native}, s))
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
    mesh_cook_stream61::Dependencies Deps() { return {EdgeDeps(), {guest, accepted.fp}}; }
};

void Check() {
    test::GuestWindow w(Regions);
    w.Fill(0);
    auto m = w.Memory();
    Environment env(w);
    for (unsigned i = 0; i < 128; ++i)
        m.WriteU8(0x83214e08 + i, normal_constants[i]);
    constexpr GuestAddress extra[]{0x820d60a8, 0x820d6454, 0x82000e40, 0x822181c4};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            m.WriteU8(extra[i] + j, normal_constants[128 + 4 * i + j]);
    m.WriteU32(0x8201f9f0, std::bit_cast<std::uint32_t>(0.5f));
    m.WriteU32(0x82000f20, std::bit_cast<std::uint32_t>(1.f / 3.f));
    m.WriteU32(0x82000d64, std::bit_cast<std::uint32_t>(-std::numeric_limits<float>::max()));
    m.WriteU32(0x82000e0c, std::bit_cast<std::uint32_t>(std::numeric_limits<float>::max()));
    m.WriteU32(0x82000e50, 0);
    m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
    m.WriteU32(0x82000dac, std::bit_cast<std::uint32_t>(0.1f));
    m.WriteU32(0x820038fc, std::bit_cast<std::uint32_t>(0.1f));
    m.WriteU32(0x821baa74, std::bit_cast<std::uint32_t>(2.f));
    for (unsigned i = 0; i < 184; ++i)
        m.WriteU8(0x83214e88 + i, constants[i]);
    for (unsigned i = 0; i < 72; ++i)
        m.WriteU8(0x820d6a78 + i, mass_constants[i]);
    constexpr GuestAddress massExtra[]{0x82000fe8, 0x82001010, 0x82051430, 0x82000f70,
                                       0x82048090, 0x82000f28, 0x820d6ac0};
    for (unsigned i = 0; i < 7; ++i)
        for (unsigned j = 0; j < 8; ++j)
            m.WriteU8(massExtra[i] + j, mass_constants[72 + 8 * i + j]);

    unsigned offset = 0;
    for (auto region : std::array<test::Region, 6>{{{0x82000e00, 768},
                                                    {0x820d2f68, 512},
                                                    {0x83215500, 40},
                                                    {0x822181a0, 112},
                                                    {0x83214fc0, 8},
                                                    {0x820d5e30, 8}}})
        for (unsigned i = 0; i < region.size; ++i)
            m.WriteU8(region.base + i, power_constants[offset++]);
    for (unsigned i = 0; i < 4; ++i) {
        m.WriteU8(0x820d6a18 + i, bounds_constants[i]);
        m.WriteU8(0x82000d70 + i, bounds_constants[i + 4]);
    }
    constexpr unsigned cases[]{0x82bc9994, 0x82bc99a0, 0x82bc99b8, 0x82bc99d4, 0x82bc99f4};
    for (unsigned i = 0; i < 5; ++i)
        m.WriteU32(0x82bc9984 + 4 * i, cases[i]);
    m.WriteU32(0x83216624, AllocatorTable);
    m.WriteU32(AllocatorTable, Allocate | 1);
    m.WriteU32(AllocatorTable + 12, Free | 3);
    m.WriteU32(0x832df548, 0x50000);
    m.WriteU32(0x50000, 0x51000);
    m.WriteU32(0x51008, Allocate | 1);
    m.WriteU32(0x51014, Free | 3);
    m.WriteU32(0x832dc414, 1);
    m.WriteU32(0x83216158, 17);
    m.WriteU8(0x83216670, 1);
    m.WriteU32(0x820d5d30 + 48, 0x82b9f188);
    m.WriteU32(0x820d5d30 + 52, 0x82b9f190);
    m.WriteU32(0x820d6c34 + 8, 0x82bd22a8);
    m.WriteU32(0x820d6c34 + 24, 0x82bd1bf8);
    constexpr std::array<GuestAddress, 5> triangle{0x82bd88e8, 0x82bd8bf8, 0x82bd8ac0, 0x82bd8b40,
                                                   0x82bb3b88},
        boxes{0x82bd8ee0, 0x82bb3b60, 0x82bd8848, 0x82bd8888, 0x82bb3b88};
    for (unsigned i = 0; i < 5; ++i) {
        m.WriteU32(0x820d6c58 + 4 * i, triangle[i]);
        m.WriteU32(0x820d6304 + 4 * i, boxes[i]);
    }
    constexpr GuestAddress cleanup[]{0x82bddac0, 0x82bddcd8, 0x82bddd38, 0x82bddd98},
        bind[]{0x82bdd058, 0x82bdd1e8, 0x82bdbd90, 0x82bdc208},
        write[]{0x82bddb20, 0x82bdb660, 0x82bdd868, 0x82bdc838};
    for (unsigned i = 0; i < 4; ++i) {
        m.WriteU32(0x820d6e7c + 32 * i, cleanup[i]);
        m.WriteU32(0x820d6e80 + 32 * i, bind[i]);
        m.WriteU32(0x820d6e90 + 32 * i, write[i]);
    }
    m.WriteU32(Writer, Table);
    m.WriteU32(Writer + 8, 8192);
    m.WriteU32(Writer + 12, Buffer);
    constexpr unsigned writers[]{0x82bde330, 0x82bde378, 0x82bde3c0,
                                 0x82bde408, 0x82bde450, 0x82bde498};
    for (unsigned i = 0; i < 6; ++i) {
        m.WriteU32(Table + 28 + 4 * i, writers[i]);
        m.WriteU32(0x820d5d70 + 28 + 4 * i, 0x82b9e528 + 64 * i);
    }
    m.WriteU32(Input, 4);
    m.WriteU32(Input + 4, 4);
    m.WriteU32(Input + 8, 12);
    m.WriteU32(Input + 12, 12);
    m.WriteU32(Input + 16, Positions);
    m.WriteU32(Input + 20, TriangleData);
    m.WriteU32(Input + 24, 0);
    constexpr float xyz[]{0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 1};
    constexpr unsigned ix[]{0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3};
    for (unsigned i = 0; i < 12; ++i) {
        m.WriteU32(Positions + 4 * i, std::bit_cast<std::uint32_t>(xyz[i]));
        m.WriteU32(TriangleData + 4 * i, ix[i]);
    }
    auto st = sort_engine61_oracle::Initial(0);
    auto initial = st;
    st.r[3] = Input;
    st.r[4] = Writer;
    PPCFPSCRRegister{}.setcsr(st.cached_fp_control);
    (void)mesh_indexed_cook61::Apply(0x82b9c7d8u, m, env.Deps(), st);
    if (st.r[3] != 1 || m.ReadU32(Writer + 4) < 100 || !env.guest.live.empty())
        throw std::runtime_error("main cook output/ownership");
    if (m.ReadU8(Buffer) != 'N' || m.ReadU8(Buffer + 1) != 'X' || m.ReadU8(Buffer + 2) != 'S')
        throw std::runtime_error("NXS envelope");
    if (st.r[1] != initial.r[1] || st.lr != Address(initial.lr))
        throw std::runtime_error("main ABI");
    std::printf("PASS indexed cook main tetrahedron -> %u bytes NXS/CVXM; zero live allocations\n",
                m.ReadU32(Writer + 4));
}
} // namespace cook_main_smoke
int main() {
    try {
        auto load = [](const char *env, auto &out) {
            const char *p = std::getenv(env);
            if (!p)
                throw std::runtime_error(env);
            std::ifstream f(p, std::ios::binary);
            f.read(reinterpret_cast<char *>(out.data()), out.size());
            if (f.gcount() != std::streamsize(out.size()))
                throw std::runtime_error("private bundle size");
        };
        using namespace cook_main_smoke;
        load("LO_MESH_MATH_CONSTANTS", constants);
        load("LO_NORMAL_ENCODING_CONSTANTS", normal_constants);
        load("LO_MASS_CONSTANTS", mass_constants);
        load("LO_POWER_CONSTANTS", power_constants);
        load("LO_BOUNDS_CONSTANTS", bounds_constants);
        Check();
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
