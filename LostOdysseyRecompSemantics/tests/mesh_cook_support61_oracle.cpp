// Original orchestration/constructors/destructors; shared concrete cube sampling.
// Synthetic allocator, actual projection targets. No whole-game proof.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/cube_projection_table61.h"
#include "lo_semantics/mesh_cook_support61.h"
#include "lo_semantics/mesh_support_stream61.h"
#include "lo_semantics/owned_tree_mesh_build61.h"
#include "lo_semantics/owned_tree_reorder_support61.h"
#include "lo_semantics/projection_extrema61.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <limits>
#include <set>
namespace cook_support_oracle {
using Full = owned_tree_mesh_build61::Registers;
constexpr GuestAddress Owner = 0x30000, Settings = 0x31000, Source = 0x32000, Triangles = 0x33000,
                       Vertices = 0x34000, Table = 0x35000;
constexpr GuestAddress Allocate = 0x2a00, Release = 0x2a04, TriangleBounds = 0x2b00,
                       BoxBounds = 0x2b04, Split = 0x2b08, Bind = 0x2b0c;
constexpr std::array<test::Region, 7> Regions{{{0, 0x180000},
                                               {0x82000000, 0x20000},
                                               {0x820d5000, 0x2000},
                                               {0x82bb3000, 0x1000},
                                               {0x83216000, 0x1000},
                                               {0x832df000, 0x1000},
                                               {0x821ba000, 0x1000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
struct Guest final : manager_release_context61::GuestServices {
    bool original_side = false;
    void DestroyOriginalOrRecovered(GuestMemory &, Full &);
    unsigned allocations = 0;
    std::set<GuestAddress> live;
    std::vector<std::array<std::uint64_t, 73>> events;
    void CallDirect(GuestAddress, GuestMemory &, Full &) override {
        throw std::runtime_error("unexpected mesh builder direct boundary");
    }
    void CallIndirect(GuestAddress target, GuestMemory &m, Full &s) override {
        std::array<std::uint64_t, 73> e{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), e.begin());
        e[72] = target;
        events.push_back(e);
        Native fp;
        if (cube_projection_table61::Apply(target, m, {*this, fp}, s))
            return;
        if (projection_extrema61::Apply(target, m, fp, s))
            return;
        if (target == 0x822d3068u)
            return;
        if (target == 0x82bc63c8u) {
            DestroyOriginalOrRecovered(m, s);
            return;
        }
        if (target == Allocate) {
            if (s.r[4] > 4096u)
                throw std::runtime_error("oversized mesh fixture allocation");
            auto p = 0x90000u + 0x1000u * allocations++;
            live.insert(p);
            s.r[3] = p;
        } else if (target == Release) {
            if (!live.erase(Address(s.r[4])))
                throw std::runtime_error("mesh builder double/unowned release");
            s.r[3] = 0;
        } else
            throw std::runtime_error("unexpected support callback");
        s.r[8] ^= 0x123456789abcdef0ull;
        s.fpr_bits[7] ^= 0x100u;
        s.cr7.lt ^= 1u;
    }
};
struct Sync final : diagnostic_lock61::SynchronizationServices {
    std::uint32_t LoadReservedWord(GuestAddress, GuestMemory &) override {
        throw std::runtime_error("unexpected diagnostic reservation");
    }
    bool CompareExchangeWord(GuestAddress, std::uint32_t, std::uint32_t, GuestMemory &) override {
        throw std::runtime_error("unexpected diagnostic CAS");
    }
    void EnterCriticalSection(GuestMemory &, Full &, diagnostic_lock61::MachineState &) override {
        throw std::runtime_error("unexpected diagnostic lock");
    }
    void LeaveCriticalSection(GuestMemory &, Full &, diagnostic_lock61::MachineState &) override {
        throw std::runtime_error("unexpected diagnostic unlock");
    }
};
struct DiagnosticGuest final : crt_narrow_formatter61::GuestServices {
    void CallIndirect(GuestAddress, GuestMemory &, Full &) override {
        throw std::runtime_error("unexpected diagnostic output");
    }
    void CallOutput(GuestMemory &, Full &) override {
        throw std::runtime_error("unexpected formatter output");
    }
};

struct Environment {
    sort_engine61_oracle::Environment accepted;
    Guest guest;
    Sync sync;
    DiagnosticGuest diagnostic;
    diagnostic_lock61::MachineState machine{0x020a8020u, 0xcafebabe11223344ull};
    explicit Environment(test::GuestWindow &w) : accepted(w) {}
    mesh_cook_support61::Dependencies Deps() {
        return {{guest, accepted.fp}, {{accepted.Deps().sort.accepted, diagnostic}, sync, machine}};
    }
};
Environment *original = nullptr;
GuestMemory *original_memory = nullptr;
test::GuestWindow *original_window = nullptr;
Native fp;
void Guest::DestroyOriginalOrRecovered(GuestMemory &m, Full &s) {
    if (original_side) {
        PPCContext c{};
        crt_full_oracle::ToPpc(c, s);
        __imp__sub_82BC63C8(c, original_window->Bytes());
        s = crt_full_oracle::FromPpc(c);
    } else
        (void)mesh_cook_support61::Apply(0x82bc63c8u, m, original->Deps(), s);
}
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Full &s) {
    if (e == 0x82bd0798u)
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
    else if (e == 0x82bb38a0u)
        (void)cube_projection_table61::Apply(e, m, {env.guest, fp}, s);
    else if (e == 0x82bb3408u || e == 0x82bb3420u)
        (void)mesh_support_stream61::Apply(e, m, {env.guest, fp}, s);
    else
        (void)diagnostic_format_routes61::Apply(e, m, env.Deps().diagnostics, s);
}
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    const auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Owner + 168, mode == 0 ? 8 : 33);
        m.WriteU32(Owner + 192, 6);
        m.WriteU32(Owner + 172, Vertices);
        m.WriteU32(0x820d6284, 0x82bb3430);
        m.WriteU32(0x820d6288, 0x82bb34f8);
        m.WriteU32(0x820d628c, 0x822d3068);
        m.WriteU32(0x820d6940, 0x82bc63c8);
        for (unsigned i = 0; i < 6; ++i)
            m.WriteU32(0x82bb3988 + 4 * i, i < 2 ? 0x82bb39a0u : i < 4 ? 0x82bb39f8u : 0x82bb3a50u);
        for (unsigned i = 0; i < 33; ++i)
            for (unsigned a = 0; a < 3; ++a)
                m.WriteU32(Vertices + 12 * i + 4 * a,
                           std::bit_cast<std::uint32_t>(float(i % 3 == a)));
        m.WriteU32(0x82000e0c, 0x7f7fffff);
        m.WriteU32(0x82000e40, std::bit_cast<std::uint32_t>(-1.f));
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x8201f9f0, std::bit_cast<std::uint32_t>(.5f));
        m.WriteU32(0x83216624, Table);
        m.WriteU32(Table, Allocate | 1);
        m.WriteU32(Table + 12, Release | 3);
        if (mode >= 2) {
            m.WriteU32(Owner + 288, 0x60000);
            m.WriteU32(0x60000, 0x820d6940);
            if (mode == 2) {
                m.WriteU32(0x60000 + 24, 0x61000);
                m.WriteU32(0x60000 + 28, 0x62000);
            } else {
                m.WriteU32(0x60000 + 12, 0x61000);
                m.WriteU32(0x60000 + 24, 0x61000);
                m.WriteU32(0x60000 + 28, 0x61010);
            }
        }
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    if (mode >= 2) {
        expected.guest.live = {0x60000, 0x61000};
        actual.guest.live = expected.guest.live;
        if (mode == 2) {
            expected.guest.live.insert(0x62000);
            actual.guest.live.insert(0x62000);
        }
    }
    expected.guest.original_side = true;
    Full s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = Owner;
    s.r[4] = Settings;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    const auto initial = s;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    auto om = before.Memory();
    original_window = &before;
    original = &expected;
    original_memory = &om;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82B9EB58(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    original = &actual;
    original_memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!mesh_cook_support61::Apply(0x82b9eb58u, m, actual.Deps(), s))
        throw std::runtime_error("missing mesh builder");
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "mesh build mode%u Full%d RAM%d events%d ownership%d host%d\n", mode,
                     a == b, before.EqualCommitted(after),
                     expected.guest.events == actual.guest.events,
                     expected.guest.live == actual.guest.live, host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "Full[%u] %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        for (auto reg : Regions)
            for (std::size_t i = 0; i < reg.size; ++i)
                if (before.Bytes()[reg.base + i] != after.Bytes()[reg.base + i]) {
                    std::fprintf(stderr, "RAM %08llx %02x/%02x\n",
                                 (unsigned long long)(reg.base + i), before.Bytes()[reg.base + i],
                                 after.Bytes()[reg.base + i]);
                    break;
                }
        for (std::size_t i = 0;
             i < std::min(expected.guest.events.size(), actual.guest.events.size()); ++i)
            if (expected.guest.events[i] != actual.guest.events[i]) {
                for (unsigned j = 0; j < 73; ++j)
                    if (expected.guest.events[i][j] != actual.guest.events[i][j])
                        std::fprintf(stderr, "event%zu field%u %llx/%llx\n", i, j,
                                     (unsigned long long)expected.guest.events[i][j],
                                     (unsigned long long)actual.guest.events[i][j]);
                break;
            }
        throw std::runtime_error("mesh builder state mismatch");
    }
    if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr) || s.r[3] != 1u)
        throw std::runtime_error("mesh builder frame/result");
    auto storage = m.ReadU32(Owner + 288);
    if (mode == 0) {
        if (storage || !actual.guest.live.empty())
            throw std::runtime_error("small mesh support");
    } else {
        if (!storage || actual.guest.live.size() != 3 || m.ReadU32(storage + 32) != Owner + 156 ||
            m.ReadU32(storage + 4) != 16 || m.ReadU32(storage + 8) != 1536)
            throw std::runtime_error("support dimensions/ownership");
        for (unsigned o : {24u, 28u}) {
            auto data = m.ReadU32(storage + o);
            for (unsigned i = 0; i < 1536; ++i)
                if (m.ReadU8(data + i) >= 33)
                    throw std::runtime_error("support index range");
        }
        s.r[3] = storage;
        s.r[4] = 1;
        (void)mesh_cook_support61::Apply(0x82bc63c8u, m, actual.Deps(), s);
        if (!actual.guest.live.empty())
            throw std::runtime_error("support teardown");
    }
    original = nullptr;
}
} // namespace cook_support_oracle
void CookSupportIndirect(std::uint32_t t, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cook_support_oracle::original->guest.CallIndirect(t, *cook_support_oracle::original_memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void CookSupportLower(std::uint32_t t, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cook_support_oracle::Lower(t, *cook_support_oracle::original_memory,
                               *cook_support_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode : {0u, 1u, 2u, 3u})
            cook_support_oracle::Check(mode);
        std::puts("PASS mesh-cook-support61 4 original-local-chain "
                  "small/large/replaced-split/replaced-combined cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
