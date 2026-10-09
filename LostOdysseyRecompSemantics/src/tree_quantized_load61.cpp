#include "lo_semantics/tree_quantized_load61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::tree_quantized_load61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Load {
    GuestMemory &m;
    Dependencies deps;
    Registers &s;
    bool compact;
    unsigned Stride() const { return compact ? 20u : 24u; }
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        const auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Indirect(GuestAddress lr) {
        s.ctr = s.r[11];
        s.lr = lr;
        deps.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Allocator(GuestAddress lr) {
        s.lr = lr;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m, deps.guest, s);
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
        r[11] = compact ? ((r[3] << 2u) & 0xfffffffcu) : ((r[3] << 1u) & 0xfffffffeu);
        r[30] = Word(r[31] + 8u);
        Store(r[31] + 4u, r[3]);
        r[11] += r[3];
        Compare(r[30]);
        r[28] = compact ? ((r[11] << 2u) & 0xfffffffcu) : ((r[11] << 3u) & 0xfffffff8u);
        if (!s.cr6.eq) {
            Allocator((compact ? 0x82bdb878u : 0x82bdca70u));
            r[11] = Word(r[3]);
            r[4] = r[30] - 4u;
            r[11] = Word(r[11] + 12u);
            Indirect((compact ? 0x82bdb88cu : 0x82bdca84u));
            r[11] = 0;
            Store(r[31] + 8u, r[11]);
        }
        r[11] = compact ? 214695936u : 178913280u;
        r[30] = Word(r[31] + 4u);
        r[11] |= compact ? 52428u : 43690u;
        Compare(r[30], r[11]);
        if (!s.cr6.gt) {
            r[11] = compact ? ((r[30] << 2u) & 0xfffffffcu) : ((r[30] << 1u) & 0xfffffffeu);
            r[10] = std::uint64_t(-5);
            r[11] += r[30];
            r[11] = compact ? ((r[11] << 2u) & 0xfffffffcu) : ((r[11] << 3u) & 0xfffffff8u);
            Compare(r[11], r[10]);
            r[29] = r[11] + 4u;
            if (s.cr6.gt)
                r[29] = ~std::uint64_t(0);
        } else
            r[29] = ~std::uint64_t(0);
        Allocator((compact ? 0x82bdb8ccu : 0x82bdcac4u));
        r[11] = Word(r[3]);
        r[5] = 30;
        r[4] = r[29];
        r[11] = Word(r[11]);
        Indirect((compact ? 0x82bdb8e4u : 0x82bdcadcu));
        Compare(r[3]);
        if (!s.cr6.eq) {
            r[4] = r[3] + 4u;
            Store(r[3], r[30]);
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
            for (unsigned offset = 0; offset < 12u; offset += 2u) {
                r[10] = Word(r[31] + 8u);
                if (offset == 0)
                    ++r[9];
                r[10] += r[11];
                r[10] += offset;
                r[8] = m.ReadU8(Address(r[10]));
                r[7] = m.ReadU8(Address(r[10] + 1u));
                m.WriteU8(Address(r[10] + 1u), std::uint8_t(r[8]));
                m.WriteU8(Address(r[10]), std::uint8_t(r[7]));
            }
            for (unsigned offset = 12; offset < Stride(); offset += 4u) {
                r[10] = Word(r[31] + 8u);
                r[10] += r[11];
                if (offset + 4u == Stride())
                    r[11] += Stride();
                r[10] += offset;
                if (offset == 16u) {
                    r[7] = m.ReadU8(Address(r[10] + 3u));
                    r[6] = m.ReadU8(Address(r[10]));
                    r[8] = m.ReadU8(Address(r[10] + 1u));
                    m.WriteU8(Address(r[10]), std::uint8_t(r[7]));
                    r[7] = m.ReadU8(Address(r[10] + 2u));
                    m.WriteU8(Address(r[10] + 3u), std::uint8_t(r[6]));
                    m.WriteU8(Address(r[10] + 2u), std::uint8_t(r[8]));
                    m.WriteU8(Address(r[10] + 1u), std::uint8_t(r[7]));
                } else {
                    r[7] = m.ReadU8(Address(r[10]));
                    r[8] = m.ReadU8(Address(r[10] + 1u));
                    r[6] = m.ReadU8(Address(r[10] + 3u));
                    r[5] = m.ReadU8(Address(r[10] + 2u));
                    m.WriteU8(Address(r[10] + 3u), std::uint8_t(r[7]));
                    m.WriteU8(Address(r[10] + 2u), std::uint8_t(r[8]));
                    m.WriteU8(Address(r[10]), std::uint8_t(r[6]));
                    m.WriteU8(Address(r[10] + 1u), std::uint8_t(r[5]));
                }
            }
            r[10] = Word(r[31] + 4u);
            Compare(r[9], r[10]);
        } while (s.cr6.lt);
    }
    void Scales() {
        auto &r = s.r;
        constexpr GuestAddress returns[]{0x82bdcc90u, 0x82bdccd8u, 0x82bdcd20u,
                                         0x82bdcd68u, 0x82bdcdb0u, 0x82bdcdf8u};
        constexpr GuestAddress compactReturns[]{0x82bdba6cu, 0x82bdbab4u, 0x82bdbafcu,
                                                0x82bdbb44u, 0x82bdbb8cu, 0x82bdbbd4u};
        r[11] = Word(r[27]);
        r[3] = r[27];
        for (unsigned axis = 0; axis < 6; ++axis) {
            if (axis)
                r[11] = Word(r[27]);
            r[11] = Word(r[11] + 12u);
            Indirect(compact ? compactReturns[axis] : returns[axis]);
            Store(r[1] + 80u, r[3]);
            Compare(r[26]);
            if (!s.cr6.eq) {
                for (unsigned i = 0; i < 2; ++i) {
                    r[11] = m.ReadU8(Address(r[1] + 80u + i));
                    r[10] = m.ReadU8(Address(r[1] + 83u - i));
                    m.WriteU8(Address(r[1] + 83u - i), std::uint8_t(r[11]));
                    m.WriteU8(Address(r[1] + 80u + i), std::uint8_t(r[10]));
                }
            }
            if (s.cached_fp_control & 0x8040u) {
                s.cached_fp_control &= ~0x8040u;
                deps.fp.SetHostFpControl(s.cached_fp_control);
            }
            s.fpr_bits[0] =
                std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(r[1] + 80u))));
            r[3] = axis == 5 ? 1u : r[27];
            Store(r[31] + 12u + 4u * axis, std::bit_cast<std::uint32_t>(static_cast<float>(
                                               std::bit_cast<double>(s.fpr_bits[0]))));
        }
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = (compact ? 0x82bdb800u : 0x82bdc9f8u);
        for (unsigned i = 26; i < 32; ++i)
            WriteU64(m, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        Store(r[1] - 8u, r[12]);
        const auto stack = r[1];
        r[1] -= 144u;
        Store(r[1], stack);
        r[27] = r[5];
        r[31] = r[3];
        r[30] = r[4];
        r[3] = r[27];
        r[11] = Word(r[27]);
        r[11] = Word(r[11] + 12u);
        Indirect((compact ? 0x82bdb824u : 0x82bdca1cu));
        r[26] = Address(r[30]) & 255u;
        Store(r[1] + 80u, r[3]);
        Compare(r[26]);
        if (!s.cr6.eq)
            SwapCount();
        if (ReplaceStorage()) {
            r[11] = Word(r[27]);
            r[5] = r[28];
            r[3] = r[27];
            r[11] = Word(r[11] + 24u);
            Indirect((compact ? 0x82bdb92cu : 0x82bdcb24u));
            Compare(r[26]);
            if (!s.cr6.eq)
                SwapNodes();
            Scales();
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
bool Apply(GuestAddress entry, GuestMemory &memory, Dependencies deps, Registers &state) {
    if (entry != 0x82bdc9f0u && entry != 0x82bdb7f8u)
        return false;
    Load{memory, deps, state, entry == 0x82bdb7f8u}.Run();
    return true;
}
} // namespace lo::semantic::gpu::tree_quantized_load61
