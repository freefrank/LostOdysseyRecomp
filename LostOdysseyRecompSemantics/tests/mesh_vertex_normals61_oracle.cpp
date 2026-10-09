#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/mesh_vertex_normals61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <cstdlib>
#include <fstream>
#include <set>
namespace normals_oracle {
using Registers = mesh_vertex_normals61::Registers;
constexpr GuestAddress Owner = 0x30000, Source = 0x31000, Descriptor = 0x32000, Indices = 0x33000,
                       Positions = 0x34000, Old = 0x35000, Faces = 0x36000, Vertices = 0x38000,
                       AllocatorTable = 0x37000, Allocate = 0x2000, Free = 0x2004;
constexpr std::array<test::Region, 4> Regions{
    {{0, 0x120000}, {0x82000000, 0x10000}, {0x83214000, 0x3000}, {0x832df000, 0x1000}}};
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

struct Environment {
    sort_engine61_oracle::Environment accepted;
    Guest guest;
    explicit Environment(test::GuestWindow &w) : accepted(w) {}
    mesh_vertex_normals61::Dependencies Deps() {
        return {{guest, accepted.fp}, accepted.Deps().sort.accepted};
    }
};
Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    if (e == 0x82bd0798u)
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
    else if (e == 0x82b7bc40u)
        crt_reader_chain61::ApplySupport_B7BC40(m, env.Deps().memory, s);
    else
        (void)mesh_geometry_math61::Apply(e, m, env.accepted.fp, s);
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
        m.WriteU32(Source + 4, 2);
        m.WriteU32(Source + 8, Indices);
        m.WriteU32(Source + 12, 4);
        m.WriteU32(Source + 16, Positions);
        m.WriteU32(Source + 20, Old);
        m.WriteU32(Descriptor, mode == 2 ? 3 : 4);
        m.WriteU32(Descriptor + 4, mode == 3 ? 0 : Positions);
        m.WriteU32(Descriptor + 8, mode == 2 ? 1 : 2);
        m.WriteU32(Descriptor + 12, (mode == 1 || mode == 2) ? 0 : Indices);
        m.WriteU32(Descriptor + 16, mode == 1 ? Indices : 0);
        m.WriteU8(Descriptor + 20, mode == 4 ? 1 : 0);
        if (mode == 1 || mode == 2) {
            m.WriteU32(Descriptor + 24, Faces);
            m.WriteU32(Descriptor + 28, Vertices);
        }
        constexpr float points[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
        constexpr unsigned ix[]{0, 1, 2, 0, 3, 1};
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU32(Positions + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        for (unsigned i = 0; i < 6; ++i)
            if (mode == 1)
                m.WriteU16(Indices + 2 * i, ix[i]);
            else
                m.WriteU32(Indices + 4 * i, ix[i]);
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        for (unsigned i = 0; i < 184; ++i)
            m.WriteU8(0x83214e88 + i, constants[i]);
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    if (mode == 0) {
        expected.guest.live.insert(Old);
        actual.guest.live.insert(Old);
    }
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = Descriptor;
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode == 0)
        __imp__sub_82BB9160(c, before.Bytes());
    else {
        __imp__sub_82656EB8(c, before.Bytes());
        __imp__sub_82BC3250(c, before.Bytes());
    }
    auto csr = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode == 0)
        (void)mesh_vertex_normals61::Apply(0x82bb9160u, m, actual.Deps(), s);
    else {
        (void)mesh_vertex_normals61::Apply(0x82656eb8u, m, actual.Deps(), s);
        (void)mesh_vertex_normals61::Apply(0x82bc3250u, m, actual.Deps(), s);
    }
    auto eq = [&]() {
        return crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) ==
                   crt_full_oracle::Snapshot(s) &&
               before.EqualCommitted(after) && expected.guest.events == actual.guest.events &&
               expected.guest.live == actual.guest.live;
    };
    if (!eq() || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("normals Full72/RAM/CSR/callback mismatch mode " +
                                 std::to_string(mode));
    if (mode == 3) {
        if (s.r[3] || !actual.guest.events.empty())
            throw std::runtime_error("invalid normal input");
        return;
    }
    auto out = mode == 0 ? m.ReadU32(Source + 20) : mode == 4 ? m.ReadU32(Owner + 4) : Vertices;
    float sign = mode == 0 ? 1.f : -1.f, q = std::sqrt(0.5f);
    for (unsigned i = 0; i < (mode == 2 ? 3u : 4u); ++i) {
        float want[]{0, 0, sign};
        if (mode != 2) {
            want[1] = sign * (i == 2 ? 0.f : i == 3 ? 1.f : q);
            want[2] = sign * (i == 3 ? 0.f : i == 2 ? 1.f : q);
        }
        for (unsigned j = 0; j < 3; ++j)
            if (std::abs(std::bit_cast<float>(m.ReadU32(out + 12 * i + 4 * j)) - want[j]) > 2e-6f)
                throw std::runtime_error("independent normal direction");
    }
    if (s.r[3] != 1 || actual.guest.live.size() != (mode == 0 ? 1u : mode == 4 ? 2u : 0u))
        throw std::runtime_error("normal output ownership");
    if (mode == 4) {
        s.r[3] = Owner;
        crt_full_oracle::ToPpc(c, s);
        original = &expected;
        memory = &om;
        __imp__sub_82BC30A0(c, before.Bytes());
        original = nullptr;
        memory = nullptr;
        (void)mesh_vertex_normals61::Apply(0x82bc30a0u, m, actual.Deps(), s);
        if (!eq() || !actual.guest.live.empty() || m.ReadU32(Owner) || m.ReadU32(Owner + 4))
            throw std::runtime_error("normal owner cleanup");
    }
}
} // namespace normals_oracle
void NormalsIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    normals_oracle::original->guest.CallIndirect(e, *normals_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void NormalsLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    normals_oracle::Lower(e, *normals_oracle::memory, *normals_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void NormalsSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *normals_oracle::memory;
    for (unsigned i = 23; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void NormalsRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *normals_oracle::memory;
    for (unsigned i = 23; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}

int main() {
    try {
        const char *p = std::getenv("LO_MESH_MATH_CONSTANTS");
        if (!p)
            throw std::runtime_error("private math constants required");
        std::ifstream f(p, std::ios::binary);
        f.read(reinterpret_cast<char *>(normals_oracle::constants.data()), 184);
        if (f.gcount() != 184)
            throw std::runtime_error("private constant block size");
        for (unsigned i = 0; i < 5; ++i)
            normals_oracle::Check(i);
        std::puts("PASS mesh-vertex-normals61 5 original-chain/shared-concrete-angle cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
