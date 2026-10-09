#include "lo_semantics/mesh_cache_lifetime61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::mesh_cache_lifetime61 {
namespace {
using recovery_abi::Address;
struct Cache {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Enter(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[12] = s.lr;
        Store(r[1] - 8, r[12]);
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        auto old = r[1];
        r[1] -= frame;
        Store(r[1], old);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Base(bool init) {
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        if (init) {
            Store(r[3] + 4, r[4]);
            r[10] = 0;
        }
        r[11] += 25668;
        if (init)
            m.WriteU16(Address(r[3] + 8), std::uint16_t(r[10]));
        Store(r[3], r[11]);
    }
    void Construct() {
        Enter(30, 112);
        auto &r = s.r;
        r[31] = r[3];
        r[30] = r[4];
        s.lr = 0x82bb3028u;
        Base(true);
        r[11] = 0xffffffff820d0000ull;
        Store(r[31] + 12, r[30]);
        r[10] = 0;
        r[11] += 25156;
        r[3] = r[31];
        Store(r[31] + 16, r[10]);
        Store(r[31], r[11]);
        Leave(30, 112);
    }
    void Descriptor() {
        Enter(31, 96);
        auto &r = s.r;
        r[31] = r[3];
        s.lr = 0x82bbc9d0u;
        (void)mesh_auxiliary_storage61::Apply(0x82d33160u, m, d, s);
        r[3] = r[31];
        Leave(31, 96);
    }
    void ReleaseDescriptor() { (void)mesh_auxiliary_storage61::Apply(0x82bc7e90u, m, d, s); }
    void Release() {
        Enter(30, 112);
        auto &r = s.r;
        r[31] = r[3];
        r[11] = 0xffffffff820d0000ull;
        r[11] += 25156;
        r[30] = Word(r[31] + 16);
        Compare(r[30]);
        Store(r[31], r[11]);
        if (!s.cr6.eq) {
            r[3] = r[30];
            s.lr = 0x82bb3310u;
            ReleaseDescriptor();
            s.lr = 0x82bb3314u;
            (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m, d.guest, s);
            r[11] = Word(r[3]);
            r[4] = r[30];
            r[11] = Word(r[11] + 12);
            s.ctr = r[11];
            s.lr = 0x82bb3328u;
            d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
            r[11] = 0;
            Store(r[31] + 16, r[11]);
        }
        r[3] = r[31];
        s.lr = 0x82bb3338u;
        Base(false);
        Leave(30, 112);
    }
    void Prefix() {
        auto &r = s.r;
        r[9] = Word(r[3] + 12);
        r[11] = 0;
        r[10] = 1;
        m.WriteU16(Address(r[9] + 2), std::uint16_t(r[11]));
        r[11] = Word(r[3] + 4);
        Compare(r[11], 1);
        if (!s.cr6.gt)
            return;
        r[9] = 4;
        do {
            r[11] = Word(r[3] + 12);
            ++r[10];
            r[11] += r[9];
            r[9] += 4;
            r[8] = m.ReadU16(Address(r[11] - 4));
            r[7] = m.ReadU16(Address(r[11] - 2));
            r[8] += r[7];
            m.WriteU16(Address(r[11] + 2), std::uint16_t(r[8]));
            r[11] = Word(r[3] + 4);
            Compare(r[10], r[11]);
        } while (s.cr6.lt);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Cache c{m, d, s};
    switch (e) {
    case 0x82bb8bd8u:
        c.Base(true);
        return true;
    case 0x82bb8bf8u:
        c.Base(false);
        return true;
    case 0x82bb3008u:
        c.Construct();
        return true;
    case 0x82bb32d8u:
        c.Release();
        return true;
    case 0x82bbc9b8u:
        c.Descriptor();
        return true;
    case 0x82bbc9e8u:
        c.ReleaseDescriptor();
        return true;
    case 0x82bc7f48u:
        c.Prefix();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_cache_lifetime61
