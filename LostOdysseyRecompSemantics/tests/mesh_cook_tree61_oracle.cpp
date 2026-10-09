// Original orchestration with shared recovered tree/geometry lowers. Actual
// getter and strategy targets; synthetic allocator only. No whole-chain proof.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/accessor_family.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/grid_transform_buffer61.h"
#include "lo_semantics/grid_transform_support61.h"
#include "lo_semantics/integer_leaf.h"
#include "lo_semantics/mesh_cook_tree61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/owned_tree_construct61.h"
#include "lo_semantics/owned_tree_mesh_build61.h"
#include "lo_semantics/owned_tree_reorder_support61.h"
#include "lo_semantics/record_snapshot_gather61.h"
#include "lo_semantics/transform_owner_initialize61.h"
#include "lo_semantics/tree_mesh_callbacks61.h"
#include "lo_semantics/tree_mesh_lifetime61.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <limits>
#include <set>
namespace cook_tree_oracle {
using Full = owned_tree_mesh_build61::Registers;
constexpr GuestAddress Owner = 0x30000, Settings = 0x31000, Source = 0x32000, Triangles = 0x33000,
                       Vertices = 0x34000, Table = 0x35000;
constexpr GuestAddress Allocate = 0x2a00, Release = 0x2a04, TriangleBounds = 0x2b00,
                       BoxBounds = 0x2b04, Split = 0x2b08, Bind = 0x2b0c;
constexpr std::array<test::Region, 7> Regions{{{0, 0x180000},
                                               {0x82000000, 0x10000},
                                               {0x8201f000, 0x1000},
                                               {0x82218000, 0x1000},
                                               {0x820d5000, 0x2000},
                                               {0x821ba000, 0x1000},
                                               {0x83216000, 0xca000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
struct Guest final : manager_release_context61::GuestServices {
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
        if (target == 0x82b9f188u || target == 0x82b9f190u) {
            s.r[3] = ReadField(m, Address(s.r[3]), target == 0x82b9f188u ? 168 : 160,
                               IntegerWidth::Word);
            return;
        }
        if (owned_tree_mesh_build61::Apply(target, m, {*this, fp}, s))
            return;
        if (tree_mesh_callbacks61::Apply(target, m, {*this, fp}, s))
            return;
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
        } else if (target == Split)
            s.r[3] = Address(s.r[5]) > 1 ? 1 : 0;
        else if (target == TriangleBounds || target == BoxBounds) {
            std::array<float, 3> lo;
            lo.fill(std::numeric_limits<float>::infinity());
            std::array<float, 3> hi;
            hi.fill(-std::numeric_limits<float>::infinity());
            const auto payload = m.ReadU32(Address(s.r[3]) + 72);
            for (unsigned i = 0; i < Address(s.r[5]); ++i) {
                auto index = m.ReadU32(Address(s.r[4]) + 4 * i);
                if (target == TriangleBounds) {
                    auto triangles = m.ReadU32(payload + 16), vertices = m.ReadU32(payload + 20);
                    for (unsigned v = 0; v < 3; ++v) {
                        auto vertex = m.ReadU32(triangles + index * 12 + v * 4);
                        for (unsigned axis = 0; axis < 3; ++axis) {
                            float x =
                                std::bit_cast<float>(m.ReadU32(vertices + 12 * vertex + 4 * axis));
                            lo[axis] = std::min(lo[axis], x);
                            hi[axis] = std::max(hi[axis], x);
                        }
                    }
                } else
                    for (unsigned axis = 0; axis < 3; ++axis) {
                        lo[axis] = std::min(lo[axis], std::bit_cast<float>(m.ReadU32(
                                                          payload + 24 * index + 4 * axis)));
                        hi[axis] = std::max(hi[axis], std::bit_cast<float>(m.ReadU32(
                                                          payload + 24 * index + 12 + 4 * axis)));
                    }
            }
            for (unsigned axis = 0; axis < 3; ++axis) {
                m.WriteU32(Address(s.r[6]) + 4 * axis, std::bit_cast<std::uint32_t>(lo[axis]));
                m.WriteU32(Address(s.r[6]) + 12 + 4 * axis, std::bit_cast<std::uint32_t>(hi[axis]));
            }
        } else if (target == Bind) {
            const auto tree = Address(s.r[4]);
            m.WriteU32(Address(s.r[3]) + 4, m.ReadU32(tree + 16));
            m.WriteU32(Address(s.r[3]) + 8, m.ReadU32(tree + 20));
            s.r[3] = 1;
        } else
            throw std::runtime_error("unexpected mesh builder indirect boundary");
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
    mesh_cook_tree61::Dependencies Deps() {
        return {{guest, accepted.fp}, {{accepted.Deps().sort.accepted, diagnostic}, sync, machine}};
    }
};
Environment *original = nullptr;
GuestMemory *original_memory = nullptr;
Native fp;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Full &s) {
    if (e == 0x82bd20f0u)
        (void)owned_tree_reorder_support61::Apply(e, m, env.Deps().tree, s);
    else if (e == 0x82bd1278u)
        (void)grid_transform_support61::Apply(e, m, fp, s);
    else if (e == 0x82b9cb60u) {
        lo::semantic::integer_leaf::Registers leaf{};
        (void)lo::semantic::integer_leaf::Apply(e, leaf);
        s.r[3] = leaf.r3;
        s.r[11] = leaf.r11;
    } else
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
        m.WriteU32(Owner, 0x820d5d30u);
        m.WriteU32(0x820d5d30u + 48, 0x82b9f188u);
        m.WriteU32(0x820d5d30u + 52, 0x82b9f190u);
        m.WriteU32(Owner + 8, 0x820d6c34u);
        m.WriteU32(0x820d6c34u + 8, 0x82bd22a8u);
        m.WriteU32(Owner + 160, 9);
        m.WriteU32(Owner + 168, 27);
        m.WriteU32(Owner + 164, Triangles);
        m.WriteU32(Owner + 172, Vertices);
        m.WriteU8(0x832dc188u, mode == 5 ? 1 : 0);
        for (unsigned i = 0; i < (mode >= 3 ? 27u : 6u); ++i) {
            m.WriteU32(Triangles + 4 * i, i);
            for (unsigned j = 0; j < 3; ++j)
                m.WriteU32(Vertices + 12 * i + 4 * j,
                           std::bit_cast<std::uint32_t>(float(i * 2 + j)));
        }
        m.WriteU32(0x820d6c54 + 4, TriangleBounds | 1);
        m.WriteU32(0x820d6c54 + 20, Split | 3);
        m.WriteU32(0x820d6300 + 4, BoxBounds | 1);
        m.WriteU32(0x820d6300 + 20, Split | 3);
        m.WriteU32(0x820d6ebc + 4, Bind | 1);
        if (mode >= 3) {
            constexpr std::array<GuestAddress, 5> triangle{0x82bd88e8u, 0x82bd8bf8u, 0x82bd8ac0u,
                                                           0x82bd8b40u, 0x82bb3b88u};
            constexpr std::array<GuestAddress, 5> boxes{0x82bd8ee0u, 0x82bb3b60u, 0x82bd8848u,
                                                        0x82bd8888u, 0x82bb3b88u};
            for (unsigned i = 0; i < 5; ++i) {
                m.WriteU32(0x820d6c58 + 4 * i, triangle[i]);
                m.WriteU32(0x820d6304 + 4 * i, boxes[i]);
            }
            constexpr GuestAddress cleanup[]{0x82bddac0u, 0x82bddcd8u, 0x82bddd38u, 0x82bddd98u};
            constexpr GuestAddress bind[]{0x82bdd058u, 0x82bdd1e8u, 0x82bdbd90u, 0x82bdc208u};
            for (unsigned i = 0; i < 4; ++i) {
                m.WriteU32(0x820d6e7cu + 32 * i, cleanup[i]);
                m.WriteU32(0x820d6e80u + 32 * i, bind[i]);
            }
            m.WriteU32(0x8221864cu, std::bit_cast<std::uint32_t>(32767.f));
            m.WriteU32(0x82007784u, std::bit_cast<std::uint32_t>(1.f));
            m.WriteU8(0x83216670u, 1);
            m.WriteU32(0x82000e0cu,
                       std::bit_cast<std::uint32_t>(std::numeric_limits<float>::infinity()));
            m.WriteU32(0x82000d64u,
                       std::bit_cast<std::uint32_t>(-std::numeric_limits<float>::infinity()));
            m.WriteU32(0x82000f20u, std::bit_cast<std::uint32_t>(1.f / 3.f));
            m.WriteU32(0x8201f9f0u, std::bit_cast<std::uint32_t>(0.5f));
        }
        m.WriteU32(0x83216624, Table);
        m.WriteU32(Table, Allocate | 1);
        m.WriteU32(Table + 12, Release | 3);
        m.WriteU32(0x821baa74, std::bit_cast<std::uint32_t>(2.f));
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
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
    original = &expected;
    original_memory = &om;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82B9E6A8(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    original_memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!mesh_cook_tree61::Apply(0x82b9e6a8u, m, actual.Deps(), s))
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
    if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr) || s.r[3] != (mode ? 1u : 0u))
        throw std::runtime_error("mesh builder frame/result");
    const auto tree = Owner + 8;
    const auto map = m.ReadU32(tree + 24), strategy = m.ReadU32(tree + 16),
               flat = m.ReadU32(strategy + 8);
    if (m.ReadU32(Owner + 92) != 9 || m.ReadU32(Owner + 96) != 27 ||
        m.ReadU32(Owner + 100) != Triangles || m.ReadU32(Owner + 104) != Vertices ||
        m.ReadU32(tree + 20) != 2 || m.ReadU32(strategy + 4) != 1 ||
        ((m.ReadU32(map) & 15u) + 1u) + ((m.ReadU32(map + 4) & 15u) + 1u) != 9u ||
        !actual.guest.live.contains(flat - 4))
        throw std::runtime_error("cooked tree layout");
    s.r[3] = tree;
    s.r[4] = 0;
    (void)tree_mesh_lifetime61::Apply(0x82bd2268u, m, actual.Deps().tree, s);
    if (!actual.guest.live.empty())
        throw std::runtime_error("tree teardown ownership");
}
} // namespace cook_tree_oracle
void CookTreeIndirect(std::uint32_t t, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cook_tree_oracle::original->guest.CallIndirect(t, *cook_tree_oracle::original_memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void CookTreeLower(std::uint32_t t, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cook_tree_oracle::Lower(t, *cook_tree_oracle::original_memory, *cook_tree_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void CookTreeSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cook_tree_oracle::original_memory;
    for (unsigned i = first; i < 32; ++i)
        WriteU64(m, Address(s.r[1] - 16u - 8u * (31u - i)), s.r[i]);
    m.WriteU32(Address(s.r[1] - 8u), Address(s.r[12]));
}
void CookTreeRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cook_tree_oracle::original_memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = ReadU64(m, Address(s.r[1] - 16u - 8u * (31u - i)));
    s.r[12] = m.ReadU32(Address(s.r[1] - 8u));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode : {5u, 6u})
            cook_tree_oracle::Check(mode);
        std::puts(
            "PASS mesh-cook-tree61 2 original-upper concrete nine-triangle build/teardown modes");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
