#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/mesh_polygon_triangulate61.h"
#include "lo_semantics/recovery_abi.h"
#include <set>
namespace polygon_oracle {
using Registers = mesh_polygon_triangulate61::Registers;
constexpr GuestAddress Owner = 0x30000, Source = 0x31000, Polygons = 0x32000, Indices = 0x33000,
                       Positions = 0x34000, Old = 0x35000, AllocatorTable = 0x37000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr std::array<test::Region, 5> Regions{
    {{0, 0x120000}, {0x82000000, 0x10000}, {0x8201f000, 0x1000}, {0x83216000, 0x1000}, {0x832df000, 0x1000}}};
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

struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
} native;
Guest *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Guest &g, Registers &s) {
    if (e == 0x82bd0798u)
        (void)crt_close_recursive_buffer_context::Apply(e, m, g, s);
    else
        (void)mesh_geometry_math61::Apply(e, m, native, s);
}
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    constexpr float points[]{-1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1,
                             -1, -1, 1,  1, -1, 1,  1, 1, 1,  -1, 1, 1};
    constexpr unsigned faces[]{0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 5, 4,
                               3, 2, 6, 7, 0, 3, 7, 4, 1, 2, 6, 5};
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(0x83216624, AllocatorTable);
        m.WriteU32(AllocatorTable, Allocate | 1);
        m.WriteU32(AllocatorTable + 12, Free | 3);
        m.WriteU32(Owner + 4, Source);
        m.WriteU32(Source + 4, 7);
        m.WriteU32(Source + 8, mode == 2 ? Old : 0);
        m.WriteU32(Source + 12, 8);
        m.WriteU32(Source + 16, Positions);
        m.WriteU32(Source + 36, mode == 0 ? 0 : 6);
        m.WriteU32(Source + 40, Polygons);
        for (unsigned i = 0; i < 6; ++i) {
            m.WriteU16(Polygons + 36 * i, 4);
            m.WriteU32(Polygons + 36 * i + 4, Indices + 4 * i);
        }
        for (unsigned i = 0; i < 24; ++i) {
            m.WriteU8(Indices + i, faces[i]);
            m.WriteU32(Positions + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        }
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x82000f20, std::bit_cast<std::uint32_t>(1.f / 3.f));
        m.WriteU32(0x8201f9f0, std::bit_cast<std::uint32_t>(0.5f));
    };
    seed(before);
    seed(after);
    Guest expected, actual;
    if (mode == 2) {
        expected.live.insert(Old);
        actual.live.insert(Old);
    }
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    s.r[3] = Owner;
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BB8C08(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!mesh_polygon_triangulate61::Apply(0x82bb8c08u, m, {actual, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events ||
        expected.live != actual.live || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("polygon Full72/RAM/CSR/callback mismatch mode " +
                                 std::to_string(mode));
    if (mode == 0) {
        if (s.r[3] || !actual.events.empty() || m.ReadU32(Source + 4) != 7)
            throw std::runtime_error("empty polygon changed mesh");
        return;
    }
    auto tris = m.ReadU32(Source + 8);
    if (s.r[3] != 1 || m.ReadU32(Source + 4) != 12 || actual.live.size() != 1 ||
        !actual.live.contains(tris) || actual.events.size() != (mode == 2 ? 2u : 1u))
        throw std::runtime_error("polygon counts/ownership");
    for (unsigned t = 0; t < 12; ++t) {
        unsigned a = m.ReadU32(tris + 12 * t), b = m.ReadU32(tris + 12 * t + 4),
                 c = m.ReadU32(tris + 12 * t + 8);
        unsigned f = t / 2, k = t % 2;
        if (a != faces[4 * f] || std::set<unsigned>{b, c} !=
                                     std::set<unsigned>{faces[4 * f + k + 1], faces[4 * f + k + 2]})
            throw std::runtime_error("polygon fan connectivity");
        float u[3], v[3];
        for (unsigned j = 0; j < 3; ++j) {
            u[j] = points[3 * b + j] - points[3 * a + j];
            v[j] = points[3 * c + j] - points[3 * a + j];
        }
        float n[]{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        if (n[0] * points[3 * a] + n[1] * points[3 * a + 1] + n[2] * points[3 * a + 2] <= 0)
            throw std::runtime_error("polygon inward orientation");
    }
}
} // namespace polygon_oracle
void PolygonIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    polygon_oracle::original->CallIndirect(e, *polygon_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void PolygonLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    polygon_oracle::Lower(e, *polygon_oracle::memory, *polygon_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void PolygonSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *polygon_oracle::memory;
    for (unsigned i = 28; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void PolygonRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *polygon_oracle::memory;
    for (unsigned i = 28; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}

int main() {
    try {
        for (unsigned i = 0; i < 3; ++i)
            polygon_oracle::Check(i);
        std::puts(
            "PASS mesh-polygon-triangulate61 3 original-upper/shared-concrete-geometry cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
