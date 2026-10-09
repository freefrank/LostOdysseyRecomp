#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include <set>
namespace boundary_oracle {
using Registers = mesh_boundary_walk61::Registers;
constexpr GuestAddress Owner = 0x30000, Input = 0x31000, Output = 0x32000, Source = 0x33000,
                       Visited = 0x34000, Links = 0x35000, AllocatorTable = 0x37000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr std::array<test::Region, 5> Regions{{{0, 0x120000},
                                               {0x82000000, 0x10000},
                                               {0x821ba000, 0x1000},
                                               {0x83216000, 0x1000},
                                               {0x832df000, 0x1000}}};
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

struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
} native;
Guest *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Guest &g, Registers &s) {
    mesh_boundary_walk61::Dependencies d{g, native};
    switch (e) {
    case 0x82bd0798u:
        (void)crt_close_recursive_buffer_context::Apply(e, m, g, s);
        break;
    case 0x82bd2a28u:
        (void)crt_reader_sort_float61::Apply(e, m, d, s);
        break;
    case 0x82bd2c08u:
        (void)object_sort_support61::Apply(e, m, d, s);
        break;
    case 0x82bd2870u:
        (void)reader_buffer_growth61::Apply(e, m, d, s);
        break;
    default:
        (void)crt_copy_full_context::Apply(e, m, s);
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
        m.WriteU32(Owner, mode == 0 ? 1 : 16);
        m.WriteU32(Owner + 8, Output);
        m.WriteU32(Owner + 12, std::bit_cast<std::uint32_t>(2.f));
        m.WriteU32(Source, 12);
        m.WriteU32(Source + 4, mode == 3 ? 4 : 12);
        m.WriteU32(Source + 8, Input);
        constexpr unsigned edges[]{0, 1, 1, 2, 2, 3, 3, 0, 1, 3, 3, 1};
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU32(Input + 4 * i, edges[i]);
        if (mode == 3) {
            m.WriteU32(Input + 8, 7);
            m.WriteU32(Input + 12, 8);
        }
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU32(Links + 4 * i, 0xffffffffu);
        m.WriteU32(Links, 1);
        m.WriteU32(Links + 8, 2);
        m.WriteU32(Links + 12, 0);
        m.WriteU32(Links + 24 + 4, 3);
        m.WriteU32(Links + 36 + 8, 2);
        if (mode == 1)
            m.WriteU8(Visited, 1);
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x821baa74, std::bit_cast<std::uint32_t>(2.f));
    };
    seed(before);
    seed(after);
    Guest expected, actual;
    expected.live.insert(Output);
    actual.live.insert(Output);
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
    s.r[4] = mode < 2 ? Links : Source;
    s.r[5] = 0;
    s.r[6] = Visited;
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode < 2)
        __imp__sub_82BB8498(c, before.Bytes());
    else
        __imp__sub_82BC2A18(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_boundary_walk61::Apply(mode < 2 ? 0x82bb8498u : 0x82bc2a18u, m, {actual, native}, s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events ||
        expected.live != actual.live || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("boundary Full72/RAM/CSR/events mode " + std::to_string(mode));
    auto out = m.ReadU32(Owner + 8);
    if (actual.live.size() != 1 || !actual.live.contains(out))
        throw std::runtime_error("boundary allocation ownership");
    unsigned used = mode == 0 ? 4 : mode == 1 ? 0 : mode == 2 ? 5 : 2;
    if (m.ReadU32(Owner + 4) != used)
        throw std::runtime_error("boundary output count");
    if (mode == 0 || mode == 2)
        for (unsigned i = 0; i < used; ++i)
            if (m.ReadU32(out + 4 * i) != (i % 4))
                throw std::runtime_error("component or boundary ordering");
    if (mode == 0)
        for (unsigned i = 0; i < 4; ++i)
            if (m.ReadU8(Visited + i) != 1)
                throw std::runtime_error("component visit flags");
    if (mode >= 2 && s.r[3] != (mode == 2 ? 1u : 0u))
        throw std::runtime_error("boundary connected result");
}
} // namespace boundary_oracle
void BoundaryIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    boundary_oracle::original->CallIndirect(e, *boundary_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void BoundaryLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    boundary_oracle::Lower(e, *boundary_oracle::memory, *boundary_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void BoundarySave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *boundary_oracle::memory;
    for (unsigned i = 27; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void BoundaryRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *boundary_oracle::memory;
    for (unsigned i = 27; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}

int main() {
    try {
        for (unsigned i = 0; i < 4; ++i)
            boundary_oracle::Check(i);
        std::puts("PASS mesh-boundary-walk61 4 composed-original-chain cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
