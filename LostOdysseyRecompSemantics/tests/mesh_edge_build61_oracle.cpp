#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_edge_build61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <set>
namespace mesh_edge_oracle {
using Registers = mesh_edge_build61::Registers;
using Machine = diagnostic_lock61::MachineState;
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
    mesh_edge_build61::Dependencies Deps() {
        return {{{guest, accepted.Deps().sort.accepted}, accepted.fp},
                {{accepted.Deps().sort.accepted, diagnostic}, sync, machine}};
    }
};
Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    auto d = env.Deps();
    switch (e) {
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
        (void)crt_reader_bucket_sort61::Apply(e, m, d.engine.sort, s);
        break;
    case 0x82b7bc40u:
        crt_reader_chain61::ApplySupport_B7BC40(m, d.engine.sort.accepted, s);
        break;
    case 0x82b7a0b0u:
        (void)crt_copy_full_context::Apply(e, m, s);
        break;
    case 0x82b9d328u:
        (void)diagnostic_format_routes61::Apply(e, m, d.diagnostics, s);
        break;
    default:
        throw std::runtime_error("edge lower");
    }
}
void Check(unsigned mode) {
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(0x83216624, AllocatorTable);
        m.WriteU32(AllocatorTable, Allocate | 1);
        m.WriteU32(AllocatorTable + 12, Free | 3);
        if (mode == 2)
            m.WriteU32(Owner + 12, 0x45678);
        if (mode == 3)
            for (unsigned i = 0; i < 6; ++i)
                m.WriteU32(Owner + 4 * i, 0xa5a5a5a5);
        unsigned tri[]{0, 1, 2, 2, 1, 3};
        for (unsigned i = 0; i < 6; ++i)
            if ((mode == 1 || mode == 5))
                m.WriteU16(Input + 2 * i, tri[i]);
            else
                m.WriteU32(Input + 4 * i, tri[i]);
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = 2;
    s.r[5] = (mode == 1 || mode == 5) ? 0 : Input;
    s.r[6] = (mode == 1 || mode == 5) ? Input : 0;
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    if (mode == 3)
        __imp__sub_82BBD4C0(c, before.Bytes());
    else if (mode >= 4)
        __imp__sub_82BBD1E0(c, before.Bytes());
    else
        __imp__sub_82BBCE58(c, before.Bytes());
    original = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    if (!mesh_edge_build61::Apply(mode == 3   ? 0x82bbd4c0u
                                  : mode >= 4 ? 0x82bbd1e0u
                                              : 0x82bbce58u,
                                  m, actual.Deps(), s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live)
        throw std::runtime_error("edge Full72/RAM/callback mismatch mode " + std::to_string(mode));
    if (mode == 3) {
        for (unsigned off : {0u, 4u, 12u, 16u, 20u})
            if (m.ReadU32(Owner + off))
                throw std::runtime_error("edge descriptor zero");
        if (m.ReadU32(Owner + 8) != 0xa5a5a5a5)
            throw std::runtime_error("edge untouched triangle count");
        return;
    }
    if (mode == 2) {
        if (s.r[3] != 1 || !actual.guest.events.empty() || m.ReadU32(Owner + 12) != 0x45678)
            throw std::runtime_error("edge reuse");
        return;
    }
    if (s.r[3] != 1 || m.ReadU32(Owner) != 5 || m.ReadU32(Owner + 8) != 2 ||
        actual.guest.live.size() != (mode >= 4 ? 4u : 2u) ||
        !actual.guest.live.contains(m.ReadU32(Owner + 4)) ||
        !actual.guest.live.contains(m.ReadU32(Owner + 12)))
        throw std::runtime_error("edge counts/ownership");
    unsigned pairs[]{0, 1, 0, 2, 1, 2, 1, 3, 2, 3};
    unsigned map[]{0, 2, 1, 2, 3, 4};
    for (unsigned i = 0; i < 10; ++i)
        if (m.ReadU32(m.ReadU32(Owner + 4) + 4 * i) != pairs[i])
            throw std::runtime_error("unique normalized edge pairs");
    for (unsigned i = 0; i < 6; ++i)
        if (m.ReadU32(m.ReadU32(Owner + 12) + 4 * i) != map[i])
            throw std::runtime_error("triangle side mapping");
    if (mode >= 4) {
        unsigned counts[]{1, 1, 2, 1, 1}, offsets[]{0, 1, 2, 4, 5}, triangles[]{0, 0, 0, 1, 1, 1};
        auto records = m.ReadU32(Owner + 16), items = m.ReadU32(Owner + 20);
        if (!actual.guest.live.contains(records) || !actual.guest.live.contains(items))
            throw std::runtime_error("edge adjacency ownership");
        for (unsigned i = 0; i < 5; ++i)
            if (m.ReadU16(records + 8 * i + 2) != counts[i] ||
                m.ReadU32(records + 8 * i + 4) != offsets[i])
                throw std::runtime_error("edge degree/prefix records");
        for (unsigned i = 0; i < 6; ++i)
            if (m.ReadU32(items + 4 * i) != triangles[i])
                throw std::runtime_error("edge incident triangle list");
    }
}
} // namespace mesh_edge_oracle
void MeshEdgeIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    mesh_edge_oracle::original->guest.CallIndirect(e, *mesh_edge_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void MeshEdgeLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    mesh_edge_oracle::Lower(e, *mesh_edge_oracle::memory, *mesh_edge_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void MeshEdgeSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mesh_edge_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void MeshEdgeRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mesh_edge_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 6; ++i)
            mesh_edge_oracle::Check(i);
        std::puts("PASS mesh-edge-build61 6 original-chain/shared-concrete-sort cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
