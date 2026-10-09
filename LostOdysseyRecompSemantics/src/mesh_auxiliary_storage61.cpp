#include "lo_semantics/mesh_auxiliary_storage61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_auxiliary_storage61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Auxiliary {
    GuestMemory &m;
    Dependencies deps;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t v) {
        auto x = Address(v);
        s.cr6 = {0u, std::uint8_t(x != 0u), std::uint8_t(x == 0u), s.xer_so};
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
        for (unsigned i = first; i < 32; ++i)
            r[i] = ReadU64(m, Address(r[1] - 16u - 8u * (31u - i)));
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
    }
    void Allocator(GuestAddress continuation) {
        s.lr = continuation;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m, deps.guest, s);
    }
    void Call(GuestAddress continuation) {
        s.ctr = s.r[11];
        s.lr = continuation;
        deps.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Free(unsigned offset, GuestAddress allocator, GuestAddress invoke) {
        auto &r = s.r;
        Allocator(allocator);
        r[11] = Word(r[3]);
        r[4] = Word(r[31] + offset);
        r[11] = Word(r[11] + 12u);
        Call(invoke);
    }
    void ClearFive() {
        s.r[11] = 0;
        for (unsigned off = 0; off < 20; off += 4)
            Store(s.r[3] + off, s.r[11]);
    }
    void BaseConstruct() {
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        r[10] = r[11] + 26956u;
        r[11] = 0xffffffff82000000ull;
        Store(r[3], r[10]);
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            deps.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[0] =
            std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(r[11] + 3664u))));
        r[11] = 0;
        for (unsigned off : {32u, 28u, 24u})
            Store(r[3] + off, std::bit_cast<std::uint32_t>(
                                  static_cast<float>(std::bit_cast<double>(s.fpr_bits[0]))));
        for (unsigned off :
             {72u, 76u, 4u, 8u, 12u, 16u, 20u, 36u, 40u, 44u, 48u, 52u, 56u, 60u, 64u, 68u})
            Store(r[3] + off, r[11]);
    }
    void Construct() {
        Enter(31, 96);
        auto &r = s.r;
        r[31] = r[3];
        s.lr = 0x82bc85a0u;
        BaseConstruct();
        r[11] = 0xffffffff820d0000ull;
        r[3] = r[31] + 88u;
        r[11] += 26992u;
        Store(r[31], r[11]);
        s.lr = 0x82bc85b4u;
        ClearFive();
        r[11] = r[31] + 108u;
        r[9] = 0;
        r[10] = 6;
        s.ctr = r[10];
        do {
            Store(r[11], r[9]);
            r[11] += 4u;
            --s.ctr;
        } while (Address(s.ctr));
        r[11] = r[31] + 4u;
        r[3] = r[31];
        Store(r[31] + 80u, r[11]);
        Leave(31, 96);
    }
    void ArrayRelease() {
        Enter(30, 112);
        auto &r = s.r;
        r[31] = r[3];
        r[11] = Word(r[31]);
        Compare(r[11]);
        if (!s.cr6.eq) {
            Free(0, 0x82bc7eb8u, 0x82bc7eccu);
            r[11] = 0;
            Store(r[31], r[11]);
        } else {
            r[11] = Word(r[31] + 16u);
            r[30] = 0;
            Compare(r[11]);
            if (!s.cr6.eq) {
                Free(16, 0x82bc7eecu, 0x82bc7f00u);
                Store(r[31] + 16u, r[30]);
            }
            r[11] = Word(r[31] + 12u);
            Compare(r[11]);
            if (!s.cr6.eq) {
                Free(12, 0x82bc7f14u, 0x82bc7f28u);
                Store(r[31] + 12u, r[30]);
            }
        }
        Leave(30, 112);
    }
    void BaseRelease() {
        Enter(29, 112, 0x82bc67d0u);
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        r[31] = r[3];
        r[11] += 26956u;
        Store(r[31], r[11]);
        r[11] = Word(r[31] + 72u);
        Compare(r[11]);
        if (!s.cr6.eq) {
            Free(72, 0x82bc67f4u, 0x82bc6808u);
            r[30] = 0;
            Store(r[31] + 72u, r[30]);
        } else {
            constexpr unsigned offsets[]{64, 68, 60, 56, 48, 44, 40, 8, 20, 16};
            constexpr GuestAddress allocators[]{0x82bc6828u, 0x82bc6850u, 0x82bc6878u, 0x82bc68a0u,
                                                0x82bc68c8u, 0x82bc68f0u, 0x82bc6918u, 0x82bc6940u,
                                                0x82bc6968u, 0x82bc6990u};
            for (unsigned i = 0; i < 10; ++i) {
                r[11] = Word(r[31] + offsets[i]);
                if (i == 0)
                    r[30] = 0;
                Compare(r[11]);
                if (!s.cr6.eq) {
                    Free(offsets[i], allocators[i], allocators[i] + 20u);
                    Store(r[31] + offsets[i], r[30]);
                }
            }
        }
        r[29] = Word(r[31] + 76u);
        Compare(r[29]);
        if (!s.cr6.eq) {
            r[3] = r[29];
            s.lr = 0x82bc69bcu;
            (void)object_sort_support61::Apply(0x82bd2c08u, m, {deps.guest, deps.fp}, s);
            Allocator(0x82bc69c0u);
            r[11] = Word(r[3]);
            r[4] = r[29];
            r[11] = Word(r[11] + 12u);
            Call(0x82bc69d4u);
            Store(r[31] + 76u, r[30]);
        }
        Leave(29, 112);
    }
    void Destroy() {
        Enter(31, 96);
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        r[31] = r[3];
        r[11] += 26992u;
        r[3] = r[31] + 88u;
        Store(r[31], r[11]);
        s.lr = 0x82bc8618u;
        ArrayRelease();
        r[3] = r[31];
        s.lr = 0x82bc8620u;
        BaseRelease();
        Leave(31, 96);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies deps, Registers &s) {
    Auxiliary a{m, deps, s};
    switch (e) {
    case 0x82bc6428u:
        a.BaseConstruct();
        return true;
    case 0x82d33160u:
        a.ClearFive();
        return true;
    case 0x82bc8588u:
        a.Construct();
        return true;
    case 0x82bc7e90u:
        a.ArrayRelease();
        return true;
    case 0x82bc67c8u:
        a.BaseRelease();
        return true;
    case 0x82bc85f0u:
        a.Destroy();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_auxiliary_storage61
