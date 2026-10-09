#include "lo_semantics/tree_flat_load61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::tree_flat_load61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Load {
    GuestMemory &m;
    GuestServices &guest;
    Registers &s;
    bool compact;
    unsigned Stride() const { return compact ? 32u : 36u; }
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        const auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Indirect(GuestAddress lr) {
        s.ctr = s.r[11];
        s.lr = lr;
        guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Allocator(GuestAddress lr) {
        s.lr = lr;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m, guest, s);
    }
    void SwapCount() {
        auto &r = s.r;
        for (unsigned i = 0; i < 2; ++i) {
            r[11] = m.ReadU8(Address(r[1] + 80u + i));
            r[10] = m.ReadU8(Address(r[1] + 83u - i));
            m.WriteU8(Address(r[1] + 83u - i), std::uint8_t(r[11]));
            m.WriteU8(Address(r[1] + 80u + i), std::uint8_t(r[10]));
        }
        r[3] = Word(r[1] + 80u);
    }
    bool ReplaceStorage() {
        auto &r = s.r;
        if (compact) {
            r[30] = Word(r[31] + 8u);
            r[27] = (r[3] << 5u) & 0xffffffe0u;
            Store(r[31] + 4u, r[3]);
            Compare(r[30]);
        } else {
            r[11] = (r[3] << 3u) & 0xfffffff8u;
            r[30] = Word(r[31] + 8u);
            Store(r[31] + 4u, r[3]);
            r[11] += r[3];
            Compare(r[30]);
            r[27] = (r[11] << 2u) & 0xfffffffcu;
        }
        if (!s.cr6.eq) {
            Allocator((compact ? 0x82bdb3c8u : 0x82bdbf58u));
            r[11] = Word(r[3]);
            r[4] = r[30] - 4u;
            r[11] = Word(r[11] + 12u);
            Indirect((compact ? 0x82bdb3dcu : 0x82bdbf6cu));
            r[11] = 0;
            Store(r[31] + 8u, r[11]);
        }
        if (compact) {
            r[11] = 134152192u;
            r[29] = Word(r[31] + 4u);
            r[11] |= 65535u;
            Compare(r[29], r[11]);
            if (!s.cr6.gt) {
                r[11] = (r[29] << 5u) & 0xffffffe0u;
                r[10] = std::uint64_t(-5);
                r[30] = r[11] + 4u;
                Compare(r[11], r[10]);
                if (s.cr6.gt)
                    r[30] = ~std::uint64_t(0);
            } else
                r[30] = ~std::uint64_t(0);
        } else {
            r[11] = 119275520u;
            r[30] = Word(r[31] + 4u);
            r[11] |= 29127u;
            Compare(r[30], r[11]);
            if (!s.cr6.gt) {
                r[11] = (r[30] << 3u) & 0xfffffff8u;
                r[10] = std::uint64_t(-5);
                r[11] += r[30];
                r[11] = (r[11] << 2u) & 0xfffffffcu;
                Compare(r[11], r[10]);
                r[29] = r[11] + 4u;
                if (s.cr6.gt)
                    r[29] = ~std::uint64_t(0);
            } else
                r[29] = ~std::uint64_t(0);
        }
        Allocator((compact ? 0x82bdb414u : 0x82bdbfacu));
        r[11] = Word(r[3]);
        r[5] = 30;
        r[4] = compact ? r[30] : r[29];
        r[11] = Word(r[11]);
        Indirect((compact ? 0x82bdb42cu : 0x82bdbfc4u));
        Compare(r[3]);
        if (!s.cr6.eq) {
            r[4] = r[3] + 4u;
            Store(r[3], compact ? r[29] : r[30]);
        } else
            r[4] = 0;
        Compare(r[4]);
        Store(r[31] + 8u, r[4]);
        return !s.cr6.eq;
    }
    void SwapNodes() {
        auto &r = s.r;
        r[11] = Word(r[31] + 4u);
        r[9] = 0;
        Compare(r[11]);
        if (!s.cr6.gt)
            return;
        r[11] = 0;
        do {
            for (unsigned offset = 0; offset < Stride(); offset += 4u) {
                r[10] = Word(r[31] + 8u);
                if (offset == 0)
                    ++r[9];
                r[10] += r[11];
                if (offset + 4u == Stride())
                    r[11] += Stride();
                r[10] += offset;
                r[7] = m.ReadU8(Address(r[10]));
                r[8] = m.ReadU8(Address(r[10] + 1u));
                r[6] = m.ReadU8(Address(r[10] + 3u));
                r[5] = m.ReadU8(Address(r[10] + 2u));
                m.WriteU8(Address(r[10] + 3u), std::uint8_t(r[7]));
                m.WriteU8(Address(r[10] + 2u), std::uint8_t(r[8]));
                m.WriteU8(Address(r[10]), std::uint8_t(r[6]));
                m.WriteU8(Address(r[10] + 1u), std::uint8_t(r[5]));
            }
            r[10] = Word(r[31] + 4u);
            Compare(r[9], r[10]);
        } while (s.cr6.lt);
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = (compact ? 0x82bdb358u : 0x82bdbee0u);
        for (unsigned i = 26; i < 32; ++i)
            WriteU64(m, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        Store(r[1] - 8u, r[12]);
        const auto stack = r[1];
        r[1] -= 144u;
        Store(r[1], stack);
        r[28] = r[5];
        r[31] = r[3];
        r[30] = r[4];
        r[3] = r[28];
        r[11] = Word(r[28]);
        r[11] = Word(r[11] + 12u);
        Indirect((compact ? 0x82bdb37cu : 0x82bdbf04u));
        r[26] = Address(r[30]) & 255u;
        Store(r[1] + 80u, r[3]);
        Compare(r[26]);
        if (!s.cr6.eq)
            SwapCount();
        if (ReplaceStorage()) {
            r[11] = Word(r[28]);
            r[5] = r[27];
            r[3] = r[28];
            r[11] = Word(r[11] + 24u);
            Indirect((compact ? 0x82bdb474u : 0x82bdc00cu));
            Compare(r[26]);
            if (!s.cr6.eq)
                SwapNodes();
            r[3] = 1;
        } else
            r[3] = 0;
        r[1] += 144u;
        for (unsigned i = 26; i < 32; ++i)
            r[i] = ReadU64(m, Address(r[1] - 16u - 8u * (31u - i)));
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &memory, GuestServices &guest, Registers &state) {
    if (entry != 0x82bdbed8u && entry != 0x82bdb350u)
        return false;
    Load{memory, guest, state, entry == 0x82bdb350u}.Run();
    return true;
}
} // namespace lo::semantic::gpu::tree_flat_load61
