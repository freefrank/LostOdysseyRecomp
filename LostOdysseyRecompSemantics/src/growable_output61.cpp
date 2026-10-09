#include "lo_semantics/growable_output61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::growable_output61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Output {
    GuestMemory &m;
    Dependencies deps;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Enter(unsigned first, unsigned frame, GuestAddress continuation = 0) {
        auto &r = s.r;
        r[12] = s.lr;
        if (continuation)
            s.lr = continuation;
        Store(r[1] - 8u, r[12]);
        for (unsigned i = first; i < 32; ++i)
            WriteU64(m, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        const auto stack = r[1];
        r[1] -= frame;
        Store(r[1], stack);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
        for (unsigned i = first; i < 32; ++i)
            r[i] = ReadU64(m, Address(r[1] - 16u - 8u * (31u - i)));
    }
    void Indirect(GuestAddress continuation) {
        s.ctr = s.r[11];
        s.lr = continuation;
        deps.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Copy(GuestAddress continuation) {
        s.lr = continuation;
        (void)crt_copy_full_context::Apply(0x82b7a0b0u, m, s);
    }
    void Scalar(unsigned width, bool floating, GuestAddress continuation) {
        Enter(31, 96);
        auto &r = s.r;
        r[31] = r[3];
        const auto slot = Address(r[1] + 128u - width);
        if (floating) {
            if (s.cached_fp_control & 0x8040u) {
                s.cached_fp_control &= ~0x8040u;
                deps.fp.SetHostFpControl(s.cached_fp_control);
            }
            if (width == 8)
                WriteU64(m, slot, s.fpr_bits[1]);
            else
                m.WriteU32(slot, std::bit_cast<std::uint32_t>(
                                     static_cast<float>(std::bit_cast<double>(s.fpr_bits[1]))));
        } else if (width == 1)
            m.WriteU8(slot, std::uint8_t(r[4]));
        else if (width == 2)
            m.WriteU16(slot, std::uint16_t(r[4]));
        else
            m.WriteU32(slot, Address(r[4]));
        r[5] = width;
        r[4] = r[1] + 128u - width;
        r[11] = Word(r[31]);
        r[11] = Word(r[11] + 48u);
        Indirect(continuation);
        r[3] = r[31];
        Leave(31, 96);
    }
    void Append() {
        Enter(27, 128, 0x82bde4a0u);
        auto &r = s.r;
        r[31] = r[3];
        r[28] = r[5];
        r[27] = r[4];
        r[11] = Word(r[31] + 4u);
        r[10] = Word(r[31] + 8u);
        r[11] += r[28];
        Compare(r[11], r[10]);
        if (s.cr6.gt) {
            r[30] = 0xffffffff832e0000ull;
            r[4] = r[11] + 4096u;
            r[5] = 0;
            r[3] = Word(r[30] - 2676u);
            Store(r[31] + 8u, r[4]);
            r[11] = Word(r[3]);
            r[11] = Word(r[11] + 8u);
            Indirect(0x82bde4e8u);
            r[4] = Word(r[31] + 12u);
            r[29] = r[3];
            Compare(r[4]);
            if (!s.cr6.eq) {
                r[5] = Word(r[31] + 4u);
                Copy(0x82bde500u);
                r[3] = Word(r[30] - 2676u);
                r[4] = Word(r[31] + 12u);
                r[11] = Word(r[3]);
                r[11] = Word(r[11] + 20u);
                Indirect(0x82bde518u);
            }
            Store(r[31] + 12u, r[29]);
        }
        r[11] = Word(r[31] + 12u);
        r[5] = r[28];
        r[10] = Word(r[31] + 4u);
        r[4] = r[27];
        r[3] = r[11] + r[10];
        Copy(0x82bde534u);
        r[11] = Word(r[31] + 4u);
        r[3] = r[31];
        r[11] += r[28];
        Store(r[31] + 4u, r[11]);
        Leave(27, 128);
    }
    void Destroy() {
        Enter(31, 96);
        auto &r = s.r;
        r[31] = r[3];
        r[11] = 0xffffffff820d0000ull;
        r[11] += 28996u;
        r[4] = Word(r[31] + 12u);
        Compare(r[4]);
        Store(r[31], r[11]);
        if (!s.cr6.eq) {
            r[11] = 0xffffffff832e0000ull;
            r[3] = Word(r[11] - 2676u);
            r[11] = Word(r[3]);
            r[11] = Word(r[11] + 20u);
            Indirect(0x82bde30cu);
        }
        r[11] = 0xffffffff821b0000ull;
        r[11] -= 26580u;
        Store(r[31], r[11]);
        Leave(31, 96);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies deps, Registers &s) {
    Output o{m, deps, s};
    switch (e) {
    case 0x82bde330u:
        o.Scalar(1, false, 0x82bde360u);
        return true;
    case 0x82bde378u:
        o.Scalar(2, false, 0x82bde3a8u);
        return true;
    case 0x82bde3c0u:
        o.Scalar(4, false, 0x82bde3f0u);
        return true;
    case 0x82bde408u:
        o.Scalar(4, true, 0x82bde438u);
        return true;
    case 0x82bde450u:
        o.Scalar(8, true, 0x82bde480u);
        return true;
    case 0x82bde498u:
        o.Append();
        return true;
    case 0x82bde2c8u:
        o.Destroy();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::growable_output61
