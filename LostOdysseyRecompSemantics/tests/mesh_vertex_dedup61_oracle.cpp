#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_polygon_collect61.h"
#include "lo_semantics/mesh_vertex_dedup61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <set>
namespace dedup_oracle {
using Registers = mesh_vertex_dedup61::Registers;

constexpr GuestAddress Owner = 0x30000, Input = 0x31000, AllocatorTable = 0x32000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr auto Regions = sort_engine61_oracle::EngineRegions;
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
    mesh_vertex_dedup61::Dependencies Deps() {
        return {{guest, accepted.Deps().sort.accepted}, accepted.fp};
    }
};
Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    switch (e) {
    case 0x82b7e504u:
        mesh_polygon_collect61::ProbeStack(m, s);
        break;
    case 0x82bc2d28u:
        mesh_vertex_dedup61::Initialize(m, s);
        break;
    case 0x82bd0798u:
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
        break;
    case 0x82bd2c50u:
        (void)object_sort_support61::Apply(e, m, {env.guest, env.accepted.fp}, s);
        break;
    case 0x82bd2c78u:
        (void)crt_reader_follow61::Apply(e, m, env.guest, s);
        break;
    case 0x82bd2df0u:
        (void)crt_reader_bucket_sort61::Apply(e, m, env.Deps().sort, s);
        break;
    default:
        throw std::runtime_error("dedup lower");
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
        m.WriteU32(Owner, 6);
        m.WriteU32(Owner + 4, Input);
        constexpr float points[]{1, 2, 0, 0, 1, 1, 1, 2, 0, 0, 0, 0, 0, 1, 1, -1, 2, 0};
        for (unsigned i = 0; i < 18; ++i)
            m.WriteU32(Input + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        if (mode == 2) {
            m.WriteU32(Owner + 12, 0x70000);
            m.WriteU32(Owner + 16, 0x71000);
        }
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    if (mode == 2) {
        expected.guest.live = {0x70000, 0x71000};
        actual.guest.live = expected.guest.live;
    }
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = mode == 1 ? 0 : 0x33000;
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BC2DD0(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_vertex_dedup61::Apply(0x82bc2dd0u, m, actual.Deps(), s);
    auto equal = [&]() {
        return crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) ==
                   crt_full_oracle::Snapshot(s) &&
               before.EqualCommitted(after) && expected.guest.events == actual.guest.events &&
               expected.guest.live == actual.guest.live;
    };
    if (!equal() || host != PPCFPSCRRegister{}.getcsr()) {
        auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
             b = crt_full_oracle::Snapshot(s);
        std::fprintf(stderr, "dedup%u Full%d RAM%d events%d host%d\n", mode, a == b,
                     before.EqualCommitted(after), expected.guest.events == actual.guest.events,
                     host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("dedup original mismatch");
    }
    if (s.r[3] != 1 || m.ReadU32(Owner + 8) != 4 || actual.guest.live.size() != 2)
        throw std::runtime_error("unique count/ownership");
    // Mode 1 sorts coordinate words: negative float bits follow positive bits.
    auto points = m.ReadU32(Owner + 12), map = m.ReadU32(Owner + 16);
    constexpr unsigned expected_map[]{1, 3, 1, 0, 3, 2};
    for (unsigned i = 0; i < 6; ++i) {
        auto index = m.ReadU32(map + 4 * i);
        if (index != expected_map[i]) {
            for (unsigned k = 0; k < 6; ++k)
                std::fprintf(stderr, "%u ", m.ReadU32(map + 4 * k));
            std::fprintf(stderr, "\n");
            throw std::runtime_error("stable unique remap");
        }
        for (unsigned axis = 0; axis < 3; ++axis)
            if (m.ReadU32(points + 12 * index + 4 * axis) != m.ReadU32(Input + 12 * i + 4 * axis))
                throw std::runtime_error("unique vertex content");
    }
    if (mode != 1 &&
        (m.ReadU32(0x33000) != points || m.ReadU32(0x33004) != 4 || m.ReadU32(0x33008) != map))
        throw std::runtime_error("borrowed result aliases");
    c.r3.u64 = Owner;
    s.r[3] = Owner;
    PPCFPSCRRegister{}.setcsr(c.fpscr.getcsr());
    __imp__sub_82BC38E0(c, before.Bytes());
    (void)mesh_vertex_dedup61::Apply(0x82bc38e0u, m, actual.Deps(), s);
    if (!equal() || !actual.guest.live.empty() || m.ReadU32(Owner + 12) || m.ReadU32(Owner + 16))
        throw std::runtime_error("dedup tail cleanup");
    original = nullptr;
    memory = nullptr;
}

void CheckInput(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    constexpr float points[]{1, 2, 0, 0, 1, 1, 1, 2, 0, 0, 0, 0, 0, 1, 1, -1, 2, 0};
    const unsigned count = mode == 2 ? 2 : mode == 3 ? 0 : 6;
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(0x83216624, AllocatorTable);
        m.WriteU32(AllocatorTable, Allocate | 1);
        m.WriteU32(AllocatorTable + 12, Free | 3);
        m.WriteU32(Owner, count);
        for (unsigned i = 0; i < 18; ++i)
            m.WriteU32(Input + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = Input;
    s.r[5] = mode == 0 ? 1 : 0;
    auto om = before.Memory(), m = after.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BB8580(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_vertex_dedup61::Apply(0x82bb8580u, m, actual.Deps(), s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "input%u Full%d RAM%d events%d host%d\n", mode, a == b,
                     before.EqualCommitted(after), expected.guest.events == actual.guest.events,
                     host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("input uniqueness original mismatch");
    }
    if (s.r[3] != (mode < 2 ? 0u : 1u) || m.ReadU32(Owner) != (mode == 0 ? 4 : count) ||
        !actual.guest.live.empty() || !expected.guest.live.empty())
        throw std::runtime_error("input uniqueness/count/cleanup");
    constexpr unsigned order[]{3, 0, 5, 1};
    for (unsigned i = 0; i < (mode == 0 ? 4 : count); ++i)
        for (unsigned axis = 0; axis < 3; ++axis)
            if (m.ReadU32(Input + 12 * i + 4 * axis) !=
                std::bit_cast<std::uint32_t>(points[3 * (mode == 0 ? order[i] : i) + axis]))
                throw std::runtime_error("in-place unique vertices");
    original = nullptr;
    memory = nullptr;
}

} // namespace dedup_oracle
void DedupIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    dedup_oracle::original->guest.CallIndirect(e, *dedup_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void DedupLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    dedup_oracle::Lower(e, *dedup_oracle::memory, *dedup_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void DedupSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *dedup_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void DedupRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *dedup_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 3; ++i)
            dedup_oracle::Check(i);
        for (unsigned i = 0; i < 4; ++i)
            dedup_oracle::CheckInput(i);
        std::puts(
            "PASS mesh-vertex-dedup61 7 original-chain/shared-concrete-sort and teardown cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
