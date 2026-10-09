#include "lo_semantics/tree_compact_strategy61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/tree_compact_flatten61.h"
namespace lo::semantic::gpu::tree_compact_strategy61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Bind {
    GuestMemory &m;
    Dependencies deps;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Allocator(GuestAddress lr) {
        s.lr = lr;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m, deps.guest, s);
    }
    void Indirect(GuestAddress lr) {
        s.ctr = s.r[11];
        s.lr = lr;
        deps.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    bool Replace() {
        auto &r = s.r;
        r[31] = Word(r[30] + 8u);
        Store(r[30] + 4u, r[11]);
        Compare(r[31]);
        if (!s.cr6.eq) {
            Allocator(0x82bdd0c0u);
            r[11] = Word(r[3]);
            r[4] = r[31] - 4u;
            r[11] = Word(r[11] + 12u);
            Indirect(0x82bdd0d4u);
            r[11] = 0;
            Store(r[30] + 8u, r[11]);
        }
        r[11] = 134152192u;
        r[29] = Word(r[30] + 4u);
        r[11] |= 65535u;
        Compare(r[29], r[11]);
        if (!s.cr6.gt) {
            r[11] = (r[29] << 5u) & 0xffffffe0u;
            r[10] = std::uint64_t(-5);
            r[31] = r[11] + 4u;
            Compare(r[11], r[10]);
            if (s.cr6.gt)
                r[31] = ~std::uint64_t(0);
        } else
            r[31] = ~std::uint64_t(0);
        Allocator(0x82bdd10cu);
        r[11] = Word(r[3]);
        r[5] = 31;
        r[4] = r[31];
        r[11] = Word(r[11]);
        Indirect(0x82bdd124u);
        Compare(r[3]);
        if (!s.cr6.eq) {
            r[11] = r[3] + 4u;
            Store(r[3], r[29]);
        } else
            r[11] = 0;
        Compare(r[11]);
        Store(r[30] + 8u, r[11]);
        return !s.cr6.eq;
    }
    bool Run() {
        auto &r = s.r;
        r[28] = r[4];
        r[30] = r[3];
        Compare(r[28]);
        if (s.cr6.eq)
            return false;
        r[11] = Word(r[28] + 4u);
        r[9] = Word(r[28] + 16u);
        r[11] = Word(r[11] + 36u);
        r[10] = (r[11] << 1u) & 0xfffffffeu;
        --r[10];
        Compare(r[9], r[10]);
        if (!s.cr6.eq)
            return false;
        r[10] = Word(r[30] + 4u);
        --r[11];
        Compare(r[10], r[11]);
        if (!s.cr6.eq && !Replace())
            return false;
        r[11] = 1;
        r[6] = Word(r[28] + 4u);
        r[5] = r[1] + 80u;
        r[3] = Word(r[30] + 8u);
        r[4] = 0;
        Store(r[1] + 80u, r[11]);
        s.lr = 0x82bdd164u;
        (void)tree_compact_flatten61::Apply(0x82bdce38u, m, deps.fp, s);
        return true;
    }
    void Execute() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bdd060u;
        for (unsigned i = 28; i < 32; ++i)
            WriteU64(m, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        Store(r[1] - 8u, r[12]);
        const auto stack = r[1];
        r[1] -= 128u;
        Store(r[1], stack);
        r[3] = Run() ? 1u : 0u;
        r[1] += 128u;
        for (unsigned i = 28; i < 32; ++i)
            r[i] = ReadU64(m, Address(r[1] - 16u - 8u * (31u - i)));
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &m, Dependencies deps, Registers &s) {
    if (entry != 0x82bdd058u)
        return false;
    Bind{m, deps, s}.Execute();
    return true;
}
} // namespace lo::semantic::gpu::tree_compact_strategy61
