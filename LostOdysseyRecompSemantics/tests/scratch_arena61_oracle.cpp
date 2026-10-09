#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/scratch_arena61.h"
namespace arena_oracle {
using Registers = scratch_arena61::Registers;
constexpr GuestAddress State = 0x832df55c, Alignment = 0x832167dc, Allocator = 0x30000,
                       Table = 0x31000, Raw = 0x50003, External = 0x60000, LogObject = 0x32000,
                       LogTable = 0x33000;
constexpr GuestAddress Allocate = 0x2000, Reallocate = 0x2004, Logger = 0x2008, Log = 0x200c;
constexpr std::array<test::Region, 3> Regions{
    {{0, 0x120000}, {0x832df000, 0x1000}, {0x83216000, 0x1000}}};
struct Guest final : scratch_arena61::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    void CallIndirect(GuestAddress e, GuestMemory &, Registers &s) override {
        std::array<std::uint64_t, 73> event{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), event.begin());
        event.back() = e;
        events.push_back(event);
        if (e == Allocate)
            s.r[3] = s.lr == 0x82bde8c0u ? External : Raw;
        else if (e == Reallocate)
            s.r[3] = Raw;
        else if (e == Logger)
            s.r[3] = LogObject;
        else if (e == Log)
            s.r[3] = 0x77;
        else
            throw std::runtime_error("arena callback");
        s.r[8] ^= 0x1234u;
        s.cr7.eq ^= 1u;
    }
};
Guest *guest = nullptr;
GuestMemory *memory = nullptr;
void Check(unsigned mode) {
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(State + 48, Allocator);
        m.WriteU32(State + 44, LogObject);
        m.WriteU32(Allocator, Table);
        m.WriteU32(Table + 12, Allocate | 1);
        m.WriteU32(Table + 16, Reallocate | 3);
        m.WriteU32(LogObject, LogTable);
        m.WriteU32(LogTable + 132, Logger | 1);
        m.WriteU32(LogTable + 8, Logger | 3);
        m.WriteU32(LogTable, Log | 1);
        m.WriteU32(Alignment, 16);
        if (mode == 0) {
            m.WriteU32(State + 20, 0x51000);
            m.WriteU32(State + 24, 0x51000);
            m.WriteU32(State + 28, 0x51100);
        }
        if (mode == 2)
            m.WriteU32(State + 16, 0x48000);
        if (mode == 3) {
            m.WriteU32(State + 20, 0x51000);
            m.WriteU32(State + 24, 0x51004);
        }
        if (mode == 5) {
            m.WriteU8(State, 1);
            m.WriteU32(State + 8, 24);
            m.WriteU32(State + 12, 16);
        }
        if (mode == 7) {
            m.WriteU8(State + 1, 1);
            m.WriteU32(State + 20, 0x51000);
            m.WriteU32(State + 24, 0x51010);
            m.WriteU32(State + 32, External);
            m.WriteU32(State + 36, External);
            m.WriteU32(State + 40, External + 128);
            m.WriteU32(Alignment, 32);
            m.WriteU32(Alignment + 4, 16);
        }
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = mode < 4 ? 64u : (mode == 4 || mode == 6 ? 1u : 0u);
    s.r[4] = 128;
    s.r[5] = 64;
    s.r[6] = 32;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    Guest expected, actual;
    auto om = before.Memory();
    guest = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    if (mode < 4)
        __imp__sub_82BDE628(c, before.Bytes());
    else if (mode < 6)
        __imp__sub_82BDE738(c, before.Bytes());
    else
        __imp__sub_82BDE810(c, before.Bytes());
    guest = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    auto entry = mode < 4 ? 0x82bde628u : mode < 6 ? 0x82bde738u : 0x82bde810u;
    if (!scratch_arena61::Apply(entry, m, actual, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events)
        throw std::runtime_error("scratch arena Full72/RAM/callback mismatch");
    if (mode == 1 || mode == 2 || mode == 6) {
        if (m.ReadU32(State + 16) != Raw || m.ReadU32(State + 20) != 0x50010 ||
            m.ReadU32(State + 24) != 0x50010 || m.ReadU32(State + 28) != 0x5005f)
            throw std::runtime_error("arena alignment/storage");
    }
    if (mode == 3 && (s.r[3] != 0 || actual.events.size() != 3))
        throw std::runtime_error("arena live cursor rejection");
    if (mode == 4 && (m.ReadU8(State) != 1 || m.ReadU32(State + 4) || m.ReadU32(State + 8)))
        throw std::runtime_error("arena counting entry");
    if (mode == 5 && (m.ReadU8(State) || m.ReadU32(State + 12) != 24))
        throw std::runtime_error("arena highwater");
    if (mode == 6 && (m.ReadU8(State + 1) != 1 || m.ReadU32(State + 32) != External ||
                      m.ReadU32(Alignment) != 32 || m.ReadU32(Alignment + 4) != 16))
        throw std::runtime_error("arena scoped entry");
    if (mode == 7 && (m.ReadU8(State + 1) || m.ReadU32(State + 32) ||
                      m.ReadU32(State + 24) != 0x51000 || m.ReadU32(Alignment) != 16))
        throw std::runtime_error("arena scoped restore");
}
} // namespace arena_oracle
void ArenaIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    arena_oracle::guest->CallIndirect(e, *arena_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void ArenaSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *arena_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        WriteU64(m, Address(s.r[1] - 16u - 8u * (31u - i)), s.r[i]);
    m.WriteU32(Address(s.r[1] - 8u), Address(s.r[12]));
}
void ArenaRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *arena_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = ReadU64(m, Address(s.r[1] - 16u - 8u * (31u - i)));
    s.r[12] = m.ReadU32(Address(s.r[1] - 8u));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 8; ++mode)
            arena_oracle::Check(mode);
        std::puts("PASS scratch-arena61 8 original-body cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
