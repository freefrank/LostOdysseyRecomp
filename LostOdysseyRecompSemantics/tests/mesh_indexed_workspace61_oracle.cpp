#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_sort_float61.h"
#include "lo_semantics/mesh_indexed_workspace61.h"
#include "lo_semantics/mesh_polygon_collect61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <set>
namespace workspace_oracle {
using Registers = mesh_indexed_workspace61::Registers;

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
    mesh_indexed_workspace61::Dependencies Deps() {
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
    case 0x82bd2a08u:
    case 0x82bd2c08u:
        (void)object_sort_support61::Apply(e, m, {env.guest, env.accepted.fp}, s);
        break;
    case 0x82bd2a28u:
        (void)crt_reader_sort_float61::Apply(e, m, {env.guest, env.accepted.fp}, s);
        break;
    case 0x82b7a0b0u:
        (void)crt_copy_full_context::Apply(e, m, s);
        break;
    case 0x82b7bc40u:
        crt_reader_chain61::ApplySupport_B7BC40(m, env.Deps().sort.accepted, s);
        break;
    default:
        throw std::runtime_error("workspace lower");
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
        m.WriteU32(Input, 4);
        m.WriteU32(Input + 4, mode == 3 ? 0 : 4);
        m.WriteU32(Input + 8, 2);
        m.WriteU32(Input + 12, mode == 2 ? 0 : 1);
        m.WriteU32(Input + 16, 0x34000);
        m.WriteU32(Input + 20, mode == 2 ? 0 : 0x35000);
        m.WriteU32(Input + 24, 0x36000);
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU8(Input + 28 + i, (i == 1 ? mode == 1 : 1));
        for (unsigned p : {0x34000u, 0x35000u, 0x36000u})
            for (unsigned i = 0; i < 12; ++i)
                m.WriteU32(p + 4 * i, std::bit_cast<std::uint32_t>(float(i + 1)));
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    auto om = before.Memory(), m = after.Memory();
    original = &expected;
    memory = &om;
    auto equal = [&]() {
        return crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) ==
                   crt_full_oracle::Snapshot(s) &&
               before.EqualCommitted(after) && expected.guest.events == actual.guest.events &&
               expected.guest.live == actual.guest.live;
    };
    auto run = [&](unsigned e) {
        PPCFPSCRRegister{}.setcsr(c.fpscr.getcsr());
        switch (e) {
        case 0x82bbdf60u:
            __imp__sub_82BBDF60(c, before.Bytes());
            break;
        case 0x82bbf628u:
            __imp__sub_82BBF628(c, before.Bytes());
            break;
        default:
            __imp__sub_82BBF590(c, before.Bytes());
        }
        auto host = PPCFPSCRRegister{}.getcsr();
        PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
        (void)mesh_indexed_workspace61::Apply(e, m, actual.Deps(), s);
        if (!equal() || host != PPCFPSCRRegister{}.getcsr()) {
            auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
                 b = crt_full_oracle::Snapshot(s);
            std::fprintf(stderr, "workspace%u %x Full%d RAM%d events%d host%d\n", mode, e, a == b,
                         before.EqualCommitted(after), expected.guest.events == actual.guest.events,
                         host == PPCFPSCRRegister{}.getcsr());
            for (unsigned i = 0; i < a.size(); ++i)
                if (a[i] != b[i])
                    std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                                 (unsigned long long)b[i]);
            throw std::runtime_error("workspace original mismatch");
        }
    };
    run(0x82bbdf60u);
    if (mode == 2) {
        for (unsigned i = 0; i < 13; ++i) {
            auto p = 0x60000 + 256 * i;
            om.WriteU32(Owner + 16 * i + 8, p);
            m.WriteU32(Owner + 16 * i + 8, p);
            expected.guest.live.insert(p);
            actual.guest.live.insert(p);
        }
        for (unsigned off : {236u, 240u, 244u, 248u, 252u, 264u, 268u, 272u, 256u}) {
            auto p = 0x70000 + off * 4;
            om.WriteU32(Owner + off, p);
            m.WriteU32(Owner + off, p);
            expected.guest.live.insert(p);
            actual.guest.live.insert(p);
        }
    }
    c.r3.u64 = Owner;
    c.r4.u64 = Input;
    s.r[3] = Owner;
    s.r[4] = Input;
    run(0x82bbf628u);
    if (s.r[3] != (mode == 3 ? 0u : 1u) || m.ReadU32(Owner + 212) != 4 ||
        m.ReadU32(Owner + 216) != 2 || m.ReadU32(Owner + 220) != (mode == 2 ? 0u : 1u))
        throw std::runtime_error("workspace channel counts");
    for (unsigned i = 0; i < 12; ++i)
        if (m.ReadU8(Owner + 280 + i) != m.ReadU8(Input + 28 + i))
            throw std::runtime_error("workspace option bytes");
    auto pos = m.ReadU32(Owner + 236), attr = m.ReadU32(Owner + 240);
    for (unsigned i = 0; i < 12; ++i)
        if (m.ReadU32(pos + 4 * i) != m.ReadU32(0x34000 + 4 * i))
            throw std::runtime_error("position copy");
    for (unsigned i = 0; i < 6; ++i) {
        auto value = mode == 2 || (mode != 1 && i % 3 == 2) ? 0 : m.ReadU32(0x35000 + 4 * i);
        if (m.ReadU32(attr + 4 * i) != value)
            throw std::runtime_error("attribute copy/zero z");
    }
    if (actual.guest.live.size() != (mode == 3 ? 3u : mode == 2 ? 4u : 5u))
        throw std::runtime_error("workspace ownership");
    c.r3.u64 = Owner;
    s.r[3] = Owner;
    run(0x82bbf590u);
    if (!actual.guest.live.empty())
        throw std::runtime_error("workspace complete teardown");
    original = nullptr;
    memory = nullptr;
}
} // namespace workspace_oracle
void WorkspaceIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    workspace_oracle::original->guest.CallIndirect(e, *workspace_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void WorkspaceLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    workspace_oracle::Lower(e, *workspace_oracle::memory, *workspace_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void WorkspaceSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *workspace_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void WorkspaceRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *workspace_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 4; ++i)
            workspace_oracle::Check(i);
        std::puts(
            "PASS mesh-indexed-workspace61 4 original-local-chain/shared-concrete-buffer cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
