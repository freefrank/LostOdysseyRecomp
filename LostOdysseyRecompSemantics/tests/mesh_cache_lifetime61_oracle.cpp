#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_cache_lifetime61.h"
namespace cache_oracle {
using Registers = mesh_cache_lifetime61::Registers;
constexpr GuestAddress Owner = 0x30000, Source = 0x31000, Descriptor = 0x32000, First = 0x33000,
                       Second = 0x34000, Table = 0x35000, Free = 0x2000;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x83216000, 0xca000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
} native;
struct Guest final : manager_release_context61::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    std::vector<GuestAddress> freed;
    void CallDirect(GuestAddress, GuestMemory &, Registers &) override {
        throw std::runtime_error("cache unexpected direct");
    }
    void CallIndirect(GuestAddress e, GuestMemory &, Registers &s) override {
        std::array<std::uint64_t, 73> ev{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), ev.begin());
        ev.back() = e;
        events.push_back(ev);
        if (e != Free)
            throw std::runtime_error("cache unexpected callback");
        freed.push_back(std::uint32_t(s.r[4]));
        s.r[3] = 0;
        s.r[8] ^= 0x1234;
        s.cr7.eq ^= 1;
    }
};
Guest *guest = nullptr;
GuestMemory *memory = nullptr;
void Check(unsigned mode) {
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(0x83216624, Table);
        m.WriteU32(Table + 12, Free | 1);
        m.WriteU32(Owner + 4, Source);
        m.WriteU32(Owner + 12, Source);
        m.WriteU32(Owner + 16, mode == 1 ? Descriptor : 0);
        m.WriteU32(Descriptor + 12, First);
        m.WriteU32(Descriptor + 16, Second);
        if (mode == 3)
            for (unsigned i = 0; i < 5; ++i)
                m.WriteU32(Descriptor + 4 * i, 0xa5a5a5a5);
        if (mode == 4) {
            m.WriteU32(Descriptor + 4, 3);
            m.WriteU16(First, 65530);
            m.WriteU16(First + 4, 10);
            m.WriteU16(First + 8, 3);
            m.WriteU16(First + 2, 111);
            m.WriteU16(First + 6, 222);
            m.WriteU16(First + 10, 333);
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
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    s.r[3] = mode >= 3 ? Descriptor : Owner;
    s.r[4] = Source;
    Guest expected, actual;
    auto om = before.Memory();
    memory = &om;
    guest = &expected;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    if (mode == 0)
        __imp__sub_82BB3008(c, before.Bytes());
    else if (mode < 3)
        __imp__sub_82BB32D8(c, before.Bytes());
    else if (mode == 3)
        __imp__sub_82BBC9B8(c, before.Bytes());
    else
        __imp__sub_82BC7F48(c, before.Bytes());
    memory = nullptr;
    guest = nullptr;
    auto m = after.Memory();
    auto entry = mode == 0   ? 0x82bb3008u
                 : mode < 3  ? 0x82bb32d8u
                 : mode == 3 ? 0x82bbc9b8u
                             : 0x82bc7f48u;
    if (!mesh_cache_lifetime61::Apply(entry, m, {actual, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events)
        throw std::runtime_error("cache Full72/RAM/callback mismatch mode " + std::to_string(mode));
    if (mode < 3) {
        if (m.ReadU32(Owner) != (mode == 0 ? 0x820d6244u : 0x820d6444u) ||
            m.ReadU32(Owner + 4) != Source || m.ReadU32(Owner + 12) != Source ||
            m.ReadU32(Owner + 16))
            throw std::runtime_error("cache owner layout");
        std::vector<GuestAddress> want = mode == 1
                                             ? std::vector<GuestAddress>{Second, First, Descriptor}
                                             : std::vector<GuestAddress>{};
        if (actual.freed != want)
            throw std::runtime_error("cache free order");
    }
    if (mode == 3)
        for (unsigned i = 0; i < 5; ++i)
            if (m.ReadU32(Descriptor + 4 * i))
                throw std::runtime_error("cache descriptor zero");
    if (mode == 4 &&
        (m.ReadU16(First + 2) != 0 || m.ReadU16(First + 6) != 65530 || m.ReadU16(First + 10) != 4))
        throw std::runtime_error("cache wrapping prefix offsets");
}
} // namespace cache_oracle
void CacheIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cache_oracle::guest->CallIndirect(e, *cache_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void CacheLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cache_oracle::memory;
    if (e == 0x82bd0798u)
        (void)crt_close_recursive_buffer_context::Apply(e, m, *cache_oracle::guest, s);
    else
        (void)mesh_auxiliary_storage61::Apply(e, m, {*cache_oracle::guest, cache_oracle::native},
                                              s);
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 5; ++i)
            cache_oracle::Check(i);
        std::puts("PASS mesh-cache-lifetime61 5 original-chain/shared-auxiliary cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
