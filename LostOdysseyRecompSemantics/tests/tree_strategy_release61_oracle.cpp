#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_strategy_release61.h"
namespace strategy_release_oracle {
using Registers = tree_strategy_release61::Registers;
constexpr GuestAddress Owner = 0x30000, Payload = 0x31004, Table = 0x32000, Free = 0x2000;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x83216000, 0xca000}}};
struct Guest final : tree_strategy_release61::GuestServices {
    std::vector<GuestAddress> freed;
    std::vector<std::array<std::uint64_t, 73>> events;
    void CallIndirect(GuestAddress e, GuestMemory &, Registers &s) override {
        if (e != Free)
            throw std::runtime_error("unexpected strategy release target");
        std::array<std::uint64_t, 73> event{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), event.begin());
        event.back() = e;
        events.push_back(event);
        freed.push_back(Address(s.r[4]));
        s.r[3] = 0x1234567800000000ull;
        s.r[8] ^= 0xabcdefu;
        s.cr7.eq ^= 1;
        s.xer_ca ^= 1;
    }
};
Guest *guest = nullptr;
GuestMemory *memory = nullptr;
void Check(unsigned mode) {
    test::GuestWindow before(Regions), after(Regions);
    const auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Owner + 4, 7);
        m.WriteU32(Owner + 8, mode == 4 ? 0 : Payload);
        m.WriteU32(0x83216624, Table);
        m.WriteU32(Table + 12, Free | 3);
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = 0xaabbccdd00000000ull | Owner;
    s.r[4] = mode == 5 ? 2u : 1u;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    const auto initial = s;
    Guest expected, actual;
    auto om = before.Memory();
    guest = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    constexpr GuestAddress entries[]{0x82bddac0u, 0x82bddcd8u, 0x82bddd38u,
                                     0x82bddd98u, 0x82bdcfe0u, 0x82bddac0u};
    switch (mode) {
    case 0:
    case 5:
        __imp__sub_82BDDAC0(c, before.Bytes());
        break;
    case 1:
        __imp__sub_82BDDCD8(c, before.Bytes());
        break;
    case 2:
        __imp__sub_82BDDD38(c, before.Bytes());
        break;
    case 3:
        __imp__sub_82BDDD98(c, before.Bytes());
        break;
    default:
        __imp__sub_82BDCFE0(c, before.Bytes());
        break;
    }
    guest = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    if (!tree_strategy_release61::Apply(entries[mode], m, actual, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events)
        throw std::runtime_error("strategy release Full72/RAM/events mismatch");
    const std::vector<GuestAddress> wanted = mode < 4
                                                 ? std::vector<GuestAddress>{Payload - 4u, Owner}
                                             : mode == 4 ? std::vector<GuestAddress>{}
                                                         : std::vector<GuestAddress>{Payload - 4u};
    if (actual.freed != wanted || m.ReadU32(Owner) != 0x820d6e5cu || m.ReadU32(Owner + 8) != 0 ||
        m.ReadU32(Owner + 4) != 7 || s.r[3] != initial.r[3])
        throw std::runtime_error("strategy release ordering/base/return contract");
}
} // namespace strategy_release_oracle
void StrategyReleaseAllocator(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, *strategy_release_oracle::memory,
                                                    *strategy_release_oracle::guest, s);
    crt_full_oracle::ToPpc(c, s);
}
void StrategyReleaseIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    strategy_release_oracle::guest->CallIndirect(e, *strategy_release_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 6; ++mode)
            strategy_release_oracle::Check(mode);
        std::puts("PASS tree-strategy-release61 6 composed original wrapper/destructor cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
