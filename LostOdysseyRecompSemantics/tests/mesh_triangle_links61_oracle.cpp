#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/mesh_triangle_links61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <cstdlib>
#include <fstream>
#include <set>
namespace links_oracle {
using Registers = mesh_triangle_links61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Owner = 0x30000, Input = 0x31000, Output = 0x32000, Descriptor = 0x33000,
                       Count = 0x34000, Records = 0x35000, AllocatorTable = 0x37000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr std::array<test::Region, 4> Regions{
    {{0, 0x120000}, {0x83214000, 0x3000}, {0x832df000, 0x1000}, {0x82000000, 0x10000}}};
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
    mesh_triangle_links61::Dependencies Deps() { return {EdgeDeps(), {guest, accepted.fp}}; }
};

Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    auto d = env.Deps();
    if (e == 0x82bd0798u)
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
    else if (e == 0x82bd2c50u)
        (void)object_sort_support61::Apply(e, m, {env.guest, env.accepted.fp}, s);
    else if (e == 0x82bd2c78u)
        (void)crt_reader_follow61::Apply(e, m, env.guest, s);
    else if (e == 0x82bd2df0u)
        (void)crt_reader_bucket_sort61::Apply(e, m, d.edge.engine.sort, s);
    else if (e == 0x82bbd4c0u)
        (void)mesh_edge_build61::Apply(e, m, d.edge, s);
    else if (e == 0x82bbddf0u)
        (void)mesh_cache_build61::Apply(e, m, d, s);
    else if (e == 0x82bbd4e0u)
        (void)mesh_cook_storage61::Apply(e, m, d.lifetime, s);
    else
        (void)diagnostic_format_routes61::Apply(e, m, d.edge.diagnostics, s);
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
        m.WriteU32(Owner, 2);
        m.WriteU32(Owner + 4, Output);
        m.WriteU32(Descriptor + ((mode == 4 || mode == 8) ? 8 : 4), Input);
        constexpr unsigned ix[]{0, 1, 2, 2, 1, 3};
        for (unsigned i = 0; i < 6; ++i) {
            if (mode == 4 || mode == 8)
                m.WriteU16(Input + 2 * i, ix[i]);
            else
                m.WriteU32(Input + 4 * i, ix[i]);
            m.WriteU32(Output + 4 * i, 0xffffffffu);
        }
        m.WriteU32(Output + 4, 1);
        m.WriteU32(Output + 20, 0x80000000u);
        m.WriteU32(Descriptor, 2);
        if (mode >= 7)
            m.WriteU32(Owner + 4, 0);
        if (mode == 9) {
            m.WriteU32(Descriptor + 12, 0x39000);
            m.WriteU32(Descriptor + 16, std::bit_cast<std::uint32_t>(0.1f));
        }
        constexpr float points[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0};
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU32(0x39000 + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x82000dac, std::bit_cast<std::uint32_t>(0.1f));
        for (unsigned i = 0; i < 184; ++i)
            m.WriteU8(0x83214e88 + i, constants[i]);
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    if (mode == 6) {
        expected.guest.live.insert(Output - 4);
        actual.guest.live.insert(Output - 4);
    }
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    constexpr GuestAddress entries[]{0x82bc3970u, 0x82bd9218u, 0x82bd9218u, 0x82bc3b10u,
                                     0x82bc3b10u, 0x82bc38e8u, 0x82bc3ec0u, 0x82bc3f20u,
                                     0x82bc3f20u, 0x82bc3f20u};
    if (mode == 0) {
        s.r[3] = 9;
        s.r[4] = 2;
        s.r[5] = 5;
        s.r[6] = 1;
        s.r[7] = Output;
        s.r[8] = Count;
        s.r[9] = Records;
    }
    if (mode == 1 || mode == 2) {
        s.r[3] = Input;
        s.r[4] = mode == 1 ? 2 : 7;
        s.r[5] = 1;
    }
    if (mode == 3 || mode == 4) {
        s.r[3] = 0;
        s.r[4] = 1;
        s.r[5] = 1;
        s.r[6] = 2;
        s.r[7] = Output;
        s.r[8] = Descriptor;
    }
    if (mode >= 7)
        s.r[4] = Descriptor;
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    switch (mode) {
    case 0:
        __imp__sub_82BC3970(c, before.Bytes());
        break;
    case 1:
    case 2:
        __imp__sub_82BD9218(c, before.Bytes());
        break;
    case 3:
    case 4:
        __imp__sub_82BC3B10(c, before.Bytes());
        break;
    case 5:
        __imp__sub_82BC38E8(c, before.Bytes());
        break;
    case 7:
    case 8:
    case 9:
        __imp__sub_82BC3F20(c, before.Bytes());
        break;
    default:
        __imp__sub_82BC3EC0(c, before.Bytes());
        break;
    }
    auto csr = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_triangle_links61::Apply(entries[mode], m, actual.Deps(), s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("links Full72/RAM/callback mode " + std::to_string(mode));
    if (mode == 0) {
        constexpr unsigned want[]{2, 9, 1, 5, 9, 1, 2, 5, 1};
        if (m.ReadU32(Count) != 3)
            throw std::runtime_error("edge count");
        for (unsigned i = 0; i < 9; ++i)
            if (m.ReadU32(Records + 4 * i) != want[i])
                throw std::runtime_error("edge records");
        for (unsigned i = 3; i < 6; ++i)
            if (m.ReadU32(Output + 4 * i) != 0xffffffffu)
                throw std::runtime_error("initial links");
    } else if (mode < 3) {
        if (s.r[3] != (mode == 1 ? 2u : 255u))
            throw std::runtime_error("side index");
    } else if (mode < 5) {
        if (s.r[3] != 1 || m.ReadU32(Output + 8) != 1 || m.ReadU32(Output + 12) != 0x80000000u)
            throw std::runtime_error("reciprocal link packing");
    } else if (mode == 5) {
        if (s.r[3] != 4)
            throw std::runtime_error("boundary count");
    } else if (mode >= 7) {
        auto links = m.ReadU32(Owner + 4);
        unsigned tag = mode == 9 ? 0x20000000u : 0;
        unsigned want[]{0xffffffffu, 0xffffffffu, 1u, 0x80000000u, 0xffffffffu, 0xffffffffu};
        if (mode == 9) {
            want[2] &= ~tag;
            want[3] &= ~tag;
        }
        if (s.r[3] != 1 || actual.guest.live.size() != 1 ||
            !actual.guest.live.contains(links - 4) || m.ReadU32(links - 4) != 2)
            throw std::runtime_error("built link ownership");
        for (unsigned i = 0; i < 6; ++i)
            if (m.ReadU32(links + 4 * i) != want[i])
                throw std::runtime_error("built adjacency and boundary flags");
    } else if (!actual.guest.live.empty() || m.ReadU32(Owner + 4) ||
               actual.guest.events.size() != 1)
        throw std::runtime_error("prefix release");
}
} // namespace links_oracle
void LinksIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    links_oracle::original->guest.CallIndirect(e, *links_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void LinksLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    links_oracle::Lower(e, *links_oracle::memory, *links_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void LinksSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *links_oracle::memory;
    for (unsigned i = 26; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void LinksRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *links_oracle::memory;
    for (unsigned i = 26; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}

void LinksSave24(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *links_oracle::memory;
    for (unsigned i = 24; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void LinksRestore24(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *links_oracle::memory;
    for (unsigned i = 24; i < 32; ++i)
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
        f.read(reinterpret_cast<char *>(links_oracle::constants.data()), 184);
        if (f.gcount() != 184)
            throw std::runtime_error("private constants size");
        for (unsigned i = 0; i < 10; ++i)
            links_oracle::Check(i);
        std::puts("PASS mesh-triangle-links61 10 original-chain/shared-concrete-sort cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
