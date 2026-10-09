#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/mesh_geometry_stream61.h"
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
namespace geometry_stream_oracle {
using Registers = mesh_geometry_stream61::Registers;
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
std::array<unsigned char, 184> constants{};
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
    auto d = env.Deps();
    mesh_stream_write61::Dependencies stream{env.guest, env.accepted.fp};
    switch (e) {
    case 0x82bd0798u:
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
        break;
    case 0x82b9cb70u:
        (void)serialization_control61::Apply(e, m, stream, s);
        break;
    case 0x82bb3130u:
        (void)mesh_cache_build61::Apply(e, m, d, s);
        break;
    case 0x82bbcc28u:
        (void)mesh_valence_stream61::Apply(e, m, stream, s);
        break;
    case 0x82bbb728u:
        (void)mesh_polygon_topology61::Apply(e, m, d, s);
        break;
    case 0x82bb9aa8u:
        (void)mesh_polygon_build61::Apply(e, m, d, s);
        break;
    case 0x82bb9160u:
        (void)mesh_vertex_normals61::Apply(e, m, {d.lifetime, d.edge.engine.sort.accepted}, s);
        break;
    case 0x82bb8fa0u:
        (void)mesh_normal_encode61::Apply(e, m, env.accepted.fp, s);
        break;
    case 0x82badfa0u:
    case 0x82bd8668u:
    case 0x82bd8360u:
    case 0x82bd83a0u:
        (void)mesh_valence_stream61::Apply(e, m, stream, s);
        break;
    default:
        if (!mesh_stream_write61::Apply(e, m, stream, s))
            throw std::runtime_error("stream lower");
    }
}
void Check(unsigned mode) {
    const bool wrapped = mode >= 4;
    const unsigned normalMode = wrapped ? mode - 4 : mode >> 1;
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
        m.WriteU16(Owner + 8, normalMode);
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
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    expected.guest.live.insert(Input);
    actual.guest.live.insert(Input);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = Writer;
    s.r[5] = Owner;
    s.r[6] = Triangles;
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (wrapped)
        __imp__sub_82BB3220(c, before.Bytes());
    else
        __imp__sub_82BBC110(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_geometry_stream61::Apply(wrapped ? 0x82bb3220u : 0x82bbc110u, m, actual.Deps(), s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("polygon collect Full72/RAM/CSR/events mode " +
                                 std::to_string(mode));

    auto records = m.ReadU32(Source + 40), bytes = m.ReadU32(Source + 44),
         tris = m.ReadU32(Source + 8), map = m.ReadU32(Source + 48), pairs = m.ReadU32(Source + 56),
         normals = m.ReadU32(Source + 60), inc = m.ReadU32(Source + 64),
         owners = m.ReadU32(Source + 68), vertexNormals = m.ReadU32(Source + 20);
    std::set<GuestAddress> owned{records, bytes, tris,   map,          pairs,
                                 normals, inc,   owners, vertexNormals};
    auto cache = m.ReadU32(Owner + 16);
    if (wrapped) {
        owned.insert(cache);
        owned.insert(m.ReadU32(cache + 12));
        owned.insert(m.ReadU32(cache + 16));
        if (m.ReadU32(Source + 84) != cache + 4)
            throw std::runtime_error("published valence cache");
    }
    if (s.r[3] != 1 || actual.guest.live != owned)
        throw std::runtime_error("serialized geometry retained ownership");
    unsigned pos = 0;
    bool little = mode & 1;
    auto number = [&](unsigned n) {
        unsigned v = 0;
        for (unsigned i = 0; i < n; ++i) {
            auto b = m.ReadU8(Buffer + pos++);
            if (little)
                v |= unsigned(b) << (8 * i);
            else
                v = (v << 8) | b;
        }
        return v;
    };
    if (wrapped) {
        for (unsigned b : {'I', 'C', 'E'})
            if (number(1) != b)
                throw std::runtime_error("wrapper ICE");
        if (number(1) != unsigned(little))
            throw std::runtime_error("wrapper endian");
        for (unsigned b : {'C', 'L', 'H', 'L'})
            if (number(1) != b)
                throw std::runtime_error("CLHL tag");
        if (number(4) != 0)
            throw std::runtime_error("CLHL version");
    }
    for (unsigned b : {'I', 'C', 'E'})
        if (number(1) != b)
            throw std::runtime_error("ICE header");
    if (number(1) != unsigned(little))
        throw std::runtime_error("endian flag");
    for (unsigned b : {'C', 'V', 'H', 'L'})
        if (number(1) != b)
            throw std::runtime_error("CVHL tag");
    for (unsigned v : {5, 8, 12, 12, 6, 24, 24})
        if (number(4) != v)
            throw std::runtime_error("CVHL counts");
    for (unsigned i = 0; i < 24; ++i)
        if (number(4) != m.ReadU32(Positions + 4 * i))
            throw std::runtime_error("vertex stream");
    if (number(4) != 7)
        throw std::runtime_error("triangle maximum");
    for (unsigned i = 0; i < 36; ++i)
        if (number(1) != m.ReadU32(tris + 4 * i))
            throw std::runtime_error("triangle stream");
    if (number(2) != (normalMode))
        throw std::runtime_error("normal mode");
    if (normalMode) {
        for (unsigned i = 0; i < 24; ++i)
            if (number(4) != m.ReadU32(vertexNormals + 4 * i))
                throw std::runtime_error("raw vertex normals");
    } else {
        for (unsigned i = 0; i < 8; ++i)
            (void)number(2);
    }
    for (unsigned i = 0; i < 3; ++i)
        if (number(4) != m.ReadU32(Source + 24 + 4 * i))
            throw std::runtime_error("geometry center");
    for (unsigned i = 0; i < 6; ++i) {
        auto rec = records + 36 * i;
        if (number(2) != 4 || number(2) != m.ReadU16(rec + 2) ||
            number(4) != m.ReadU32(rec + 4) - bytes || number(4) != m.ReadU32(rec + 8) - map)
            throw std::runtime_error("relocated polygon record");
        for (unsigned j = 12; j < 36; j += 4)
            if (number(4) != m.ReadU32(rec + j))
                throw std::runtime_error("polygon plane/projections");
    }
    for (unsigned i = 0; i < 24; ++i)
        if (number(1) != m.ReadU8(bytes + i))
            throw std::runtime_error("polygon vertex bytes");
    if (number(4) != 11)
        throw std::runtime_error("edge maximum");
    for (unsigned i = 0; i < 24; ++i)
        if (number(1) != m.ReadU16(map + 2 * i))
            throw std::runtime_error("polygon edge IDs");
    if (number(4) != 0 || number(4) != 0)
        throw std::runtime_error("reserved words");
    for (unsigned i = 0; i < 24; ++i)
        if (number(1) != m.ReadU8(pairs + i))
            throw std::runtime_error("edge endpoints");
    if (normalMode) {
        for (unsigned i = 0; i < 36; ++i)
            if (number(4) != m.ReadU32(normals + 4 * i))
                throw std::runtime_error("raw edge normals");
    } else {
        for (unsigned i = 0; i < 12; ++i)
            (void)number(2);
    }
    for (unsigned field : {0, 2, 4}) {
        unsigned max = 0;
        for (unsigned i = 0; i < 12; ++i)
            max = std::max(max, field == 4 ? m.ReadU32(inc + 8 * i + field)
                                           : unsigned(m.ReadU16(inc + 8 * i + field)));
        if (number(4) != max)
            throw std::runtime_error("incidence maximum");
        for (unsigned i = 0; i < 12; ++i)
            if (number(1) !=
                (field == 4 ? m.ReadU32(inc + 8 * i + field) : m.ReadU16(inc + 8 * i + field)))
                throw std::runtime_error("incidence array");
    }
    for (unsigned i = 0; i < 24; ++i)
        if (number(1) != m.ReadU8(owners + i))
            throw std::runtime_error("incidence owner bytes");
    if (pos != (normalMode ? 798u : 598u) + (wrapped ? 12u : 0u))
        throw std::runtime_error("CVHL size");
    if (wrapped) {
        for (unsigned b : {'I', 'C', 'E'})
            if (number(1) != b)
                throw std::runtime_error("valence ICE");
        if (number(1) != unsigned(little))
            throw std::runtime_error("valence endian");
        for (unsigned b : {'V', 'A', 'L', 'E'})
            if (number(1) != b)
                throw std::runtime_error("VALE tag");
        if (number(4) != 2 || number(4) != 8 || number(4) != 36)
            throw std::runtime_error("VALE counts");
        auto degrees = m.ReadU32(cache + 12), neighbors = m.ReadU32(cache + 16);
        unsigned max = 0;
        for (unsigned i = 0; i < 8; ++i)
            max = std::max(max, unsigned(m.ReadU16(degrees + 4 * i)));
        if (number(4) != max)
            throw std::runtime_error("VALE max degree");
        for (unsigned i = 0; i < 8; ++i)
            if (number(1) != m.ReadU16(degrees + 4 * i))
                throw std::runtime_error("VALE degrees");
        for (unsigned i = 0; i < 36; ++i)
            if (number(1) != m.ReadU8(neighbors + i))
                throw std::runtime_error("VALE neighbors");
    }
    if (pos != m.ReadU32(Writer + 4))
        throw std::runtime_error("stream total bytes");
}
} // namespace geometry_stream_oracle
void GeometryStreamIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    geometry_stream_oracle::original->guest.CallIndirect(e, *geometry_stream_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void GeometryStreamLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    geometry_stream_oracle::Lower(e, *geometry_stream_oracle::memory,
                                  *geometry_stream_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void GeometryStreamSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *geometry_stream_oracle::memory;
    for (unsigned i = 22; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void GeometryStreamRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *geometry_stream_oracle::memory;
    for (unsigned i = 22; i < 32; ++i)
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
        f.read(reinterpret_cast<char *>(geometry_stream_oracle::constants.data()), 184);
        if (f.gcount() != 184)
            throw std::runtime_error("private constant size");
        const char *np = std::getenv("LO_NORMAL_ENCODING_CONSTANTS");
        if (!np)
            throw std::runtime_error("private normal constants required");
        std::ifstream nf(np, std::ios::binary);
        nf.read(reinterpret_cast<char *>(geometry_stream_oracle::normal_constants.data()), 144);
        if (nf.gcount() != 144)
            throw std::runtime_error("normal constant size");
        for (unsigned i = 0; i < 6; ++i)
            geometry_stream_oracle::Check(i);
        std::puts("PASS mesh-geometry-stream61 6 original-CVHL/CLHL shared-concrete-lower "
                  "endian/normal cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
