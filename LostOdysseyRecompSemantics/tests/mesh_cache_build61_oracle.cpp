#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_cache_build61.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/mesh_edge_build61.h"
#include "lo_semantics/mesh_edge_flags61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <cstdlib>
#include <fstream>
#include <set>
namespace cache_build_oracle {
using Registers = mesh_edge_build61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Owner = 0x30000, Descriptor = 0x31000, Source = 0x32000, Input = 0x35000,
                       Positions = 0x36000, AllocatorTable = 0x37000, Allocate = 0x2000,
                       Free = 0x2004, Constants = 0x83214e88;
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
    mesh_cache_build61::Dependencies Deps() { return {EdgeDeps(), {guest, accepted.fp}}; }
};
Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    auto d = env.Deps();
    switch (e) {
    case 0x82bd0798u:
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
        break;
    case 0x82bbce58u:
    case 0x82bbd1e0u:
    case 0x82bbd4c0u:
        (void)mesh_edge_build61::Apply(e, m, d.edge, s);
        break;
    case 0x82bbd4e8u:
        (void)mesh_edge_flags61::Apply(e, m, d.edge, s);
        break;
    case 0x82bbd4e0u:
        (void)mesh_cook_storage61::Apply(e, m, d.lifetime, s);
        break;
    case 0x82b7bc40u:
        crt_reader_chain61::ApplySupport_B7BC40(m, d.edge.engine.sort.accepted, s);
        break;
    default:
        (void)mesh_cache_lifetime61::Apply(e, m, d.lifetime, s);
        break;
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
        m.WriteU32(Owner + 12, mode == 0 ? Source : 0);
        m.WriteU32(Source + 12, 4);
        m.WriteU32(Source + 4, 2);
        m.WriteU32(Source + 8, Input);
        if (mode == 1) {
            m.WriteU32(Descriptor, 4);
            m.WriteU32(Descriptor + 4, 2);
            m.WriteU32(Descriptor + 12, Input);
            m.WriteU8(Descriptor + 16, 1);
        }
        if (mode == 2) {
            m.WriteU32(Descriptor, 2);
            m.WriteU32(Descriptor + 4, Input);
            m.WriteU32(Descriptor + 16, Positions);
            m.WriteU32(Descriptor + 20, std::bit_cast<std::uint32_t>(0.1f));
        }
        unsigned indices[]{0, 1, 2, 2, 1, 3};
        for (unsigned i = 0; i < 6; ++i)
            if (mode == 1)
                m.WriteU16(Input + 2 * i, indices[i]);
            else
                m.WriteU32(Input + 4 * i, indices[i]);
        float points[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, -1};
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU32(Positions + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x82000dac, std::bit_cast<std::uint32_t>(0.1f));
        for (unsigned i = 0; i < constants.size(); ++i)
            m.WriteU8(Constants + i, constants[i]);
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
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
        __imp__sub_82BB3130(c, before.Bytes());
    else if (mode == 1)
        __imp__sub_82BBC9F0(c, before.Bytes());
    else
        __imp__sub_82BBDDF0(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    auto entry = mode == 0 ? 0x82bb3130u : mode == 1 ? 0x82bbc9f0u : 0x82bbddf0u;
    if (!mesh_cache_build61::Apply(entry, m, actual.Deps(), s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("cache build Full72/RAM/host/callback mismatch mode " +
                                 std::to_string(mode));
    if (s.r[3] != 1)
        throw std::runtime_error("cache build return");
    if (mode < 2) {
        auto cache = mode == 0 ? m.ReadU32(Owner + 16) : Owner;
        auto records = m.ReadU32(cache + 12), bytes = m.ReadU32(cache + 16);
        if (m.ReadU32(cache + 4) != 4 || m.ReadU32(cache + 8) != 10 ||
            actual.guest.live.size() != (mode == 0 ? 3u : 2u) ||
            !actual.guest.live.contains(records) || !actual.guest.live.contains(bytes))
            throw std::runtime_error("valence counts/ownership");
        unsigned degrees[]{2, 3, 3, 2}, offsets[]{0, 2, 5, 8},
            neighbors[]{1, 2, 0, 2, 3, 0, 1, 3, 1, 2};
        for (unsigned i = 0; i < 4; ++i)
            if (m.ReadU16(records + 4 * i) != degrees[i] ||
                m.ReadU16(records + 4 * i + 2) != offsets[i])
                throw std::runtime_error("valence degree/offset");
        for (unsigned i = 0; i < 10; ++i)
            if (m.ReadU8(bytes + i) != neighbors[i])
                throw std::runtime_error("valence neighbors");
        if (mode == 0 &&
            (m.ReadU32(Source + 84) != cache + 4 || !actual.guest.live.contains(cache)))
            throw std::runtime_error("lazy borrowed cache publication");
    } else {
        if (m.ReadU32(Owner) != 5 || m.ReadU32(Owner + 8) != 2 || m.ReadU32(Owner + 12) ||
            m.ReadU32(Owner + 16) || m.ReadU32(Owner + 20) || actual.guest.live.size() != 1 ||
            !actual.guest.live.contains(m.ReadU32(Owner + 4)))
            throw std::runtime_error("topology release-unretained ownership");
    }
}
} // namespace cache_build_oracle
void CacheBuildIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cache_build_oracle::original->guest.CallIndirect(e, *cache_build_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void CacheBuildLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cache_build_oracle::Lower(e, *cache_build_oracle::memory, *cache_build_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void CacheBuildSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cache_build_oracle::memory;
    for (unsigned i = 29; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void CacheBuildRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cache_build_oracle::memory;
    for (unsigned i = 29; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        const char *path = std::getenv("LO_MESH_MATH_CONSTANTS");
        if (!path)
            throw std::runtime_error("LO_MESH_MATH_CONSTANTS private block required");
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(cache_build_oracle::constants.data()), 184);
        if (f.gcount() != 184)
            throw std::runtime_error("private math constant block size");
        for (unsigned i = 0; i < 3; ++i)
            cache_build_oracle::Check(i);
        std::puts("PASS mesh-cache-build61 3 original-chain/shared-concrete-topology cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
