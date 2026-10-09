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
namespace cook_stream_oracle {
using Registers = mesh_cook_stream61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Owner = 0x30000, Source = Owner + 156, Input = 0x32000, Positions = 0x33000,
                       Count = 0x34000, Polygons = 0x35000, Triangles = 0x36000,
                       AllocatorTable = 0x37000, PolygonData = 0x38000, TriangleData = 0x39000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr GuestAddress Writer = 0x40000, Table = 0x41000, Buffer = 0x42000;
constexpr std::array<test::Region, 11> Regions{{{0, 0x180000},
                                                {0x82000000, 0x10000},
                                                {0x83214000, 0x3000},
                                                {0x832df000, 0x1000},
                                                {0x821ba000, 0x1000},
                                                {0x8201f000, 0x1000},
                                                {0x832dc000, 0x1000},
                                                {0x820d5000, 0x2000},
                                                {0x82218000, 0x1000},
                                                {0x82051000, 0x1000},
                                                {0x82048000, 0x1000}}};
std::array<unsigned char, 184> constants{};
std::array<unsigned char, 144> normal_constants{};
std::array<unsigned char, 128> mass_constants{};
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

Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    auto d = env.Deps();
    mesh_stream_write61::Dependencies stream{env.guest, env.accepted.fp};
    switch (e) {
    case 0x82b9cb70u:
        (void)serialization_control61::Apply(e, m, stream, s);
        break;
    case 0x82bada00u:
    case 0x82bb44a8u:
        (void)crt_reader_object_chain61::Apply(e, m, {env.guest, d.edge.engine.sort.accepted}, s);
        break;
    case 0x82bb3008u:
    case 0x82bb32d8u:
        (void)mesh_cache_lifetime61::Apply(e, m, d.lifetime, s);
        break;
    case 0x82bb3220u:
        (void)mesh_geometry_stream61::Apply(e, m, d, s);
        break;
    case 0x82b9f418u:
        (void)mesh_mass_cache61::Apply(e, m, {env.accepted.fp, d.edge.diagnostics}, s);
        break;
    case 0x82bb3408u:
    case 0x82bb36f0u:
    case 0x82bb3420u:
        (void)mesh_support_stream61::Apply(e, m, stream, s);
        break;
    default:
        if (!mesh_stream_write61::Apply(e, m, stream, s))
            throw std::runtime_error("cook stream lower");
        break;
    }
}
void Check(unsigned mode) {

    const unsigned normalMode = mode;
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
        m.WriteU32(Owner + 12, Source);
        m.WriteU32(Source + 4, 12);
        m.WriteU32(Source + 8, Input);
        m.WriteU32(Source + 12, 8);
        m.WriteU32(Source + 16, Positions);
        m.WriteU32(Count, 99);

        m.WriteU32(0x832dc180, (mode & 1) ? 0 : 1);
        m.WriteU32(Writer, Table);
        m.WriteU32(Writer + 8, 8192);
        m.WriteU32(Writer + 12, Buffer);
        m.WriteU32(Table + 28, 0x82bde331);
        m.WriteU32(Table + 32, 0x82bde379);
        m.WriteU32(Table + 36, 0x82bde3c1);
        m.WriteU32(Table + 48, 0x82bde49b);
        m.WriteU32(Table + 40, 0x82bde409);
        m.WriteU32(Table + 44, 0x82bde451);
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
        for (unsigned i = 0; i < 72; ++i)
            m.WriteU8(0x820d6a78 + i, mass_constants[i]);
        constexpr GuestAddress massExtra[]{0x82000fe8, 0x82001010, 0x82051430, 0x82000f70,
                                           0x82048090, 0x82000f28, 0x820d6ac0};
        for (unsigned i = 0; i < 7; ++i)
            for (unsigned j = 0; j < 8; ++j)
                m.WriteU8(massExtra[i] + j, mass_constants[72 + 8 * i + j]);
        m.WriteU32(0x832df548, 1);
        m.WriteU32(0x83216158, 17);
        m.WriteU32(0x82000e40, std::bit_cast<std::uint32_t>(-1.f));
        m.WriteU32(Owner + 292, std::bit_cast<std::uint32_t>(-1.f));
        // Concrete prepared one-record flat strategy and identity triangle map.
        m.WriteU32(Owner + 8, 0x45000);
        m.WriteU32(0x45018, 0x82bd1bf9);
        m.WriteU32(Owner + 16, 0);
        m.WriteU32(Owner + 24, 0x46000);
        m.WriteU32(Owner + 28, 1);
        m.WriteU32(Owner + 36, 12);
        m.WriteU32(Owner + 40, 0x49000);
        m.WriteU32(0x46000, 0x47000);
        m.WriteU32(0x47014, 0x82bdd869);
        m.WriteU32(0x46004, 1);
        m.WriteU32(0x46008, 0x48000);
        for (unsigned i = 0; i < 6; ++i)
            m.WriteU32(0x48000 + 4 * i, std::bit_cast<std::uint32_t>(i < 3 ? 0.f : 1.f));
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU32(0x49000 + 4 * i, i);
        for (unsigned i = 0; i < 6; ++i)
            m.WriteU32(0x820d5d70 + 28 + 4 * i, 0x82b9e528 + 64 * i);
        constexpr unsigned fields[]{152, 136, 140, 144, 148, 112, 116, 120, 124, 128, 132};
        for (unsigned i = 0; i < 11; ++i)
            m.WriteU32(Owner + fields[i], std::bit_cast<std::uint32_t>(float(i) + 0.25f));
        if (mode) {
            m.WriteU32(Owner + 288, 0x4a000);
            m.WriteU32(0x4a004, 7);
            m.WriteU32(0x4a008, 3);
            m.WriteU32(0x4a018, 0x4b000);
            m.WriteU32(0x4a01c, 0x4c000);
            for (unsigned i = 0; i < 3; ++i) {
                m.WriteU8(0x4b000 + i, 10 + i);
                m.WriteU8(0x4c000 + i, 20 + i);
            }
        }
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    expected.guest.live.insert(Input);
    actual.guest.live.insert(Input);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = Writer;
    s.r[5] = normalMode;
    s.r[6] = Triangles;
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82B9F6F0(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_cook_stream61::Apply(0x82b9f6f0u, m, actual.Deps(), s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("polygon collect Full72/RAM/CSR/events mode " +
                                 std::to_string(mode));

    std::set<GuestAddress> owned;
    for (unsigned off : {8, 20, 40, 44, 48, 56, 60, 64, 68})
        owned.insert(m.ReadU32(Source + off));
    if (s.r[3] != 1 || actual.guest.live != owned)
        throw std::runtime_error("cook stream retained mesh ownership");
    unsigned pos = 0;
    auto read = [&](unsigned n) {
        unsigned v = 0;
        for (unsigned i = 0; i < n; ++i) {
            unsigned b = m.ReadU8(Buffer + pos++);
            if (mode)
                v |= b << (8 * i);
            else
                v = (v << 8) | b;
        }
        return v;
    };
    for (unsigned b : {'N', 'X', 'S'})
        if (read(1) != b)
            throw std::runtime_error("NXS tag");
    if (read(1) != mode)
        throw std::runtime_error("NXS endian");
    for (unsigned b : {'C', 'V', 'X', 'M'})
        if (read(1) != b)
            throw std::runtime_error("CVXM tag");
    if (read(4) != 17 || read(4) != 0)
        throw std::runtime_error("aggregate version/reserved");
    if (m.ReadU32(Buffer + pos + 4) != 0x434c484cu || m.ReadU32(Buffer + pos + 16) != 0x4356484cu)
        throw std::runtime_error("nested CLHL/CVHL");
    pos += 12 + (mode ? 798 : 598) + 68;
    unsigned treeLength = read(4);
    if (treeLength != 84 || m.ReadU8(Buffer + pos) != 'O' || m.ReadU8(Buffer + pos + 1) != 'P' ||
        m.ReadU8(Buffer + pos + 2) != 'C')
        throw std::runtime_error("prepared tree envelope length/tag " + std::to_string(treeLength));
    pos += treeLength;
    constexpr unsigned fields[]{152, 136, 140, 144, 148, 112, 116, 120, 124, 128, 132};
    for (unsigned off : fields)
        if (read(4) != m.ReadU32(Owner + off))
            throw std::runtime_error("aggregate scalar fields");
    for (unsigned i = 0; i < 13; ++i)
        if (read(4) != m.ReadU32(Owner + 292 + 4 * i))
            throw std::runtime_error("aggregate mass block");
    if (std::bit_cast<float>(m.ReadU32(Owner + 292)) != 8.f)
        throw std::runtime_error("aggregate integrated cube volume");
    if (mode) {
        if (m.ReadU32(Buffer + pos + 4) != 0x5355504du ||
            m.ReadU32(Buffer + pos + 16) != 0x47415553u)
            throw std::runtime_error("SUPM/GAUS sections");
        pos += 24;
        if (read(4) != 7 || read(4) != 3)
            throw std::runtime_error("support counts");
        for (unsigned i = 0; i < 3; ++i)
            if (read(1) != 10 + i)
                throw std::runtime_error("support first bytes");
        for (unsigned i = 0; i < 3; ++i)
            if (read(1) != 20 + i)
                throw std::runtime_error("support second bytes");
    }
    if (pos != m.ReadU32(Writer + 4))
        throw std::runtime_error("aggregate final size " + std::to_string(pos));
}
} // namespace cook_stream_oracle
void CookStreamIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cook_stream_oracle::original->guest.CallIndirect(e, *cook_stream_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void CookStreamLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cook_stream_oracle::Lower(e, *cook_stream_oracle::memory, *cook_stream_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void CookStreamSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cook_stream_oracle::memory;
    for (unsigned i = 27; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void CookStreamRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cook_stream_oracle::memory;
    for (unsigned i = 27; i < 32; ++i)
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
        f.read(reinterpret_cast<char *>(cook_stream_oracle::constants.data()), 184);
        if (f.gcount() != 184)
            throw std::runtime_error("private constant size");
        const char *np = std::getenv("LO_NORMAL_ENCODING_CONSTANTS");
        if (!np)
            throw std::runtime_error("private normal constants required");
        std::ifstream nf(np, std::ios::binary);
        nf.read(reinterpret_cast<char *>(cook_stream_oracle::normal_constants.data()), 144);
        if (nf.gcount() != 144)
            throw std::runtime_error("normal constant size");
        const char *mp = std::getenv("LO_MASS_CONSTANTS");
        if (!mp)
            throw std::runtime_error("private mass constants required");
        std::ifstream mf(mp, std::ios::binary);
        mf.read(reinterpret_cast<char *>(cook_stream_oracle::mass_constants.data()), 128);
        if (mf.gcount() != 128)
            throw std::runtime_error("mass constant size");
        for (unsigned i = 0; i < 2; ++i)
            cook_stream_oracle::Check(i);
        std::puts("PASS mesh-cook-stream61 2 original-upper/concrete-serialization-graph cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
