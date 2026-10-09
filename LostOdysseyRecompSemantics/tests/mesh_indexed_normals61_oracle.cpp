#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_sort_float61.h"
#include "lo_semantics/mesh_indexed_channels61.h"
#include "lo_semantics/mesh_indexed_normals61.h"
#include "lo_semantics/mesh_indexed_workspace61.h"
#include "lo_semantics/mesh_polygon_collect61.h"
#include "lo_semantics/mesh_vertex_dedup61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <set>
namespace normals_oracle {
using Registers = mesh_indexed_channels61::Registers;

constexpr GuestAddress Owner = 0x30000, Input = 0x31000, AllocatorTable = 0x32000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr auto Regions = [] {
    auto r = sort_engine61_oracle::EngineRegions;
    r[1].size = 0x10000;
    return r;
}();
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    std::set<GuestAddress> live;
    unsigned allocations = 0;
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
    mesh_indexed_channels61::Dependencies Deps() {
        return {{guest, accepted.Deps().sort.accepted}, accepted.fp};
    }
};
Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    switch (e) {
    case 0x82bd0798u:
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
        break;
    case 0x82b7bc40u:
        crt_reader_chain61::ApplySupport_B7BC40(m, env.Deps().sort.accepted, s);
        break;
    case 0x82bb3c00u:
        (void)mesh_indexed_channels61::Apply(e, m, env.Deps(), s);
        break;
    default:
        throw std::runtime_error("indexed normals lower");
    }
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
        m.WriteU32(0x821baa74, std::bit_cast<std::uint32_t>(2.f));
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(Owner + 212, mode == 3 ? 0 : 4);
        m.WriteU32(Owner + 224, 2);
        m.WriteU32(Owner + 228, 6);
        m.WriteU32(Owner + 236, 0x60000);
        m.WriteU32(Owner + 248, 0x61000);
        m.WriteU32(Owner + 252, 0x62000);
        m.WriteU8(Owner + 282, mode != 2);
        m.WriteU8(Owner + 283, mode != 1 && mode != 2);
        m.WriteU32(Owner + 156, std::bit_cast<std::uint32_t>(2.f));
        constexpr float points[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU32(0x60000 + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        constexpr unsigned ids[]{0, 1, 2, 0, 2, 3};
        for (unsigned i = 0; i < 6; ++i) {
            m.WriteU32(0x61000 + 48 * (i / 3) + 12 + 4 * (i % 3), i);
            m.WriteU32(0x62000 + 12 * i, mode == 4 && i == 1 ? 0 : ids[i]);
        }
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    expected.guest.live = {0x60000, 0x61000, 0x62000};
    actual.guest.live = expected.guest.live;
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    auto om = before.Memory(), m = after.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BBED20(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_indexed_normals61::Apply(0x82bbed20u, m, actual.Deps(), s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "normals%u Full%d RAM%d events%d host%d\n", mode, a == b,
                     before.EqualCommitted(after), expected.guest.events == actual.guest.events,
                     host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("indexed normals original mismatch");
    }
    if (s.r[3] != (mode == 3 ? 0u : 1u))
        throw std::runtime_error("normal result");
    if (mode != 2 && mode != 3) {
        constexpr float normals[]{0, 0, 1, 1, 0, 0};
        auto output = m.ReadU32(Owner + 152);
        if (m.ReadU32(Owner + 148) != (mode == 1 ? 0u : 6u))
            throw std::runtime_error("normal output count");
        for (unsigned i = 0; i < 6; ++i) {
            float want = mode == 4 && i < 3 ? 0 : normals[i];
            auto word = m.ReadU32(0x61000 + 48 * (i / 3) + 32 + 4 * (i % 3));
            if (std::bit_cast<float>(word) != want)
                throw std::runtime_error("face normal vector");
            if (mode != 1 && m.ReadU32(output + 4 * i) != word)
                throw std::runtime_error("normal output vector");
        }
        std::array<unsigned, 4> degree{}, prefix{};
        std::vector<unsigned> adjacency[4];
        constexpr unsigned ids[]{0, 1, 2, 0, 2, 3};
        for (unsigned i = 0; i < 6; ++i)
            adjacency[mode == 4 && i == 1 ? 0 : ids[i]].push_back(i / 3);
        unsigned offset = 0;
        for (unsigned i = 0; i < 4; ++i) {
            if (m.ReadU32(m.ReadU32(Owner + 264) + 4 * i) != adjacency[i].size() ||
                m.ReadU32(m.ReadU32(Owner + 268) + 4 * i) != offset)
                throw std::runtime_error("normal incidence prefix/count");
            for (auto face : adjacency[i]) {
                if (m.ReadU32(m.ReadU32(Owner + 272) + 4 * offset) != face)
                    throw std::runtime_error("normal incident faces");
                ++offset;
            }
        }
    }
    auto os = crt_full_oracle::FromPpc(c);
    os.r[3] = Owner;
    s.r[3] = Owner;
    PPCFPSCRRegister{}.setcsr(os.cached_fp_control);
    (void)mesh_indexed_workspace61::Apply(0x82bbf590u, om, expected.Deps(), os);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_indexed_workspace61::Apply(0x82bbf590u, m, actual.Deps(), s);
    if (!actual.guest.live.empty() || !expected.guest.live.empty() ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events)
        throw std::runtime_error("normal complete teardown");
    original = nullptr;
    memory = nullptr;
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
void NormalsSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *normals_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void NormalsRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *normals_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 5; ++i)
            normals_oracle::Check(i);
        std::puts("PASS mesh-indexed-normals61 5 original-upper/shared-concrete-buffer cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
