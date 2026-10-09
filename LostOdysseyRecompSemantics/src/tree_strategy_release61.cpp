#include "lo_semantics/tree_strategy_release61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
namespace lo::semantic::gpu::tree_strategy_release61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Variant {
    GuestAddress cleanup, deleting;
    std::uint32_t tableOffset;
};
constexpr std::array<Variant, 4> Variants{{{0x82bdcfe0u, 0x82bddac0u, 28284u},
                                           {0x82bdd170u, 0x82bddcd8u, 28316u},
                                           {0x82bdd7f0u, 0x82bddd38u, 28348u},
                                           {0x82bdda48u, 0x82bddd98u, 28380u}}};
struct Release {
    GuestMemory &m;
    GuestServices &guest;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void CompareZero(std::uint64_t v) {
        auto x = Address(v);
        s.cr6 = {0u, std::uint8_t(x != 0u), std::uint8_t(x == 0u), s.xer_so};
    }
    void Enter() {
        auto &r = s.r;
        r[12] = s.lr;
        Store(r[1] - 8u, r[12]);
        WriteU64(m, Address(r[1] - 24u), r[30]);
        WriteU64(m, Address(r[1] - 16u), r[31]);
        const auto stack = r[1];
        r[1] -= 112u;
        Store(r[1], stack);
        r[31] = r[3];
    }
    void Leave() {
        auto &r = s.r;
        r[1] += 112u;
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
        r[30] = ReadU64(m, Address(r[1] - 24u));
        r[31] = ReadU64(m, Address(r[1] - 16u));
    }
    void Allocator(GuestAddress lr) {
        s.lr = lr;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m, guest, s);
    }
    void Free(GuestAddress lr) {
        s.r[11] = Word(s.r[11] + 12u);
        s.ctr = s.r[11];
        s.lr = lr;
        guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Cleanup(const Variant &v) {
        Enter();
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        r[11] += v.tableOffset;
        r[30] = Word(r[31] + 8u);
        CompareZero(r[30]);
        Store(r[31], r[11]);
        if (!s.cr6.eq) {
            Allocator(v.cleanup + 0x34u);
            r[11] = Word(r[3]);
            r[4] = r[30] - 4u;
            Free(v.cleanup + 0x48u);
            r[11] = 0;
            Store(r[31] + 8u, r[11]);
        }
        r[11] = 0xffffffff820d0000ull;
        r[11] += 28252u;
        Store(r[31], r[11]);
        Leave();
    }
    void Delete(const Variant &v) {
        Enter();
        auto &r = s.r;
        r[30] = r[4];
        s.lr = v.deleting + 0x20u;
        Cleanup(v);
        r[11] = Address(r[30]) & 1u;
        CompareZero(r[11]);
        if (!s.cr6.eq) {
            Allocator(v.deleting + 0x30u);
            r[11] = Word(r[3]);
            r[4] = r[31];
            Free(v.deleting + 0x44u);
        }
        r[3] = r[31];
        Leave();
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &m, GuestServices &guest, Registers &s) {
    for (const auto &v : Variants) {
        if (entry == v.cleanup) {
            Release{m, guest, s}.Cleanup(v);
            return true;
        }
        if (entry == v.deleting) {
            Release{m, guest, s}.Delete(v);
            return true;
        }
    }
    return false;
}
} // namespace lo::semantic::gpu::tree_strategy_release61
