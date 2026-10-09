#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_sort_float61.h"
#include "lo_semantics/mesh_indexed_remap61.h"
#include "lo_semantics/mesh_indexed_workspace61.h"
#include "lo_semantics/mesh_polygon_collect61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <set>
namespace remap_oracle {
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
    case 0x82b7bc40u:
        crt_reader_chain61::ApplySupport_B7BC40(m, env.Deps().sort.accepted, s);
        break;
    case 0x82bd2870u:
        (void)reader_buffer_growth61::Apply(e, m, {env.guest, env.accepted.fp}, s);
        break;
    default:
        throw std::runtime_error("indexed remap lower");
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
        m.WriteU32(Owner + 228, 4);
        m.WriteU32(Owner + 232, 10);
        m.WriteU32(Owner + 248, 0x61000);
        m.WriteU32(Owner + 252, 0x62000);
        m.WriteU8(Owner + 288, mode != 0);
        m.WriteU32(Owner + 140, std::bit_cast<std::uint32_t>(2.f));
        m.WriteU32(Owner + 188, std::bit_cast<std::uint32_t>(2.f));
        constexpr unsigned ids[]{0, 1, 2, 0, 2, 3};
        for (unsigned i = 0; i < 6; ++i)
            m.WriteU32(0x61000 + 48 * (i / 3) + 12 + 4 * (i % 3), ids[i]);
        m.WriteU32(0x61000 + 28, 7);
        m.WriteU32(0x61000 + 48 + 28, 9);
        for (unsigned i = 0; i < 4; ++i)
            for (unsigned channel = 0; channel < 3; ++channel)
                m.WriteU32(0x62000 + 12 * i + 4 * channel, i + 10 * channel);
        m.WriteU32(Input, 0);
        m.WriteU32(Input + 4, 1);
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    expected.guest.live = {0x61000, 0x62000};
    actual.guest.live = expected.guest.live;
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = Input;
    s.r[5] = mode == 2 ? 0 : 2;
    s.r[6] = Owner + 128;
    auto om = before.Memory(), m = after.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BBFEA8(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_indexed_remap61::Apply(0x82bbfea8u, m, actual.Deps(), s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "remap%u Full%d RAM%d events%d host%d\n", mode, a == b,
                     before.EqualCommitted(after), expected.guest.events == actual.guest.events,
                     host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("indexed remap original mismatch");
    }
    unsigned added = mode == 2 ? 0 : 4, base = mode == 0 ? 10 : 0;
    if (s.r[3] != added || m.ReadU32(Owner + 232) != base + added ||
        m.ReadU32(Owner + 132) != 4 * added || m.ReadU32(Owner + 180) != 2)
        throw std::runtime_error("remap counters");
    auto batches = m.ReadU32(Owner + 184);
    if (m.ReadU32(batches) != (mode == 2 ? 0u : 2u) || m.ReadU32(batches + 4) != added)
        throw std::runtime_error("batch metadata");
    if (mode != 2) {
        constexpr unsigned ids[]{0, 1, 2, 0, 2, 3};
        for (unsigned i = 0; i < 6; ++i)
            if (m.ReadU32(0x61000 + 48 * (i / 3) + 4 * (i % 3)) != base + ids[i])
                throw std::runtime_error("final face indices");
        auto output = m.ReadU32(Owner + 136);
        for (unsigned i = 0; i < 4; ++i) {
            for (unsigned channel = 0; channel < 3; ++channel)
                if (m.ReadU32(output + 16 * i + 4 * channel) != i + 10 * channel)
                    throw std::runtime_error("packed remap record");
            if (m.ReadU32(output + 16 * i + 12) != (i == 3 ? 9u : 7u))
                throw std::runtime_error("record smoothing");
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
        throw std::runtime_error("remap complete teardown");
    original = nullptr;
    memory = nullptr;
}
} // namespace remap_oracle
void RemapIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    remap_oracle::original->guest.CallIndirect(e, *remap_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void RemapLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    remap_oracle::Lower(e, *remap_oracle::memory, *remap_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void RemapSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *remap_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void RemapRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *remap_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 3; ++i)
            remap_oracle::Check(i);
        std::puts("PASS mesh-indexed-remap61 3 original-local-chain/shared-concrete-buffer cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
