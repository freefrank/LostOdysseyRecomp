#include "lo_semantics/mesh_edge_build61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::mesh_edge_build61 {
namespace {
using recovery_abi::Address;
struct Edges {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    static std::uint32_t Shift(std::uint64_t v, unsigned bits) { return Address(v) << bits; }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.engine.sort.guest, s);
            break;
        case 0x82bd2c50u:
            (void)object_sort_support61::Apply(e, m, {d.engine.sort.guest, d.engine.fp}, s);
            break;
        case 0x82bd2c78u:
            (void)crt_reader_follow61::Apply(e, m, d.engine.sort.guest, s);
            break;
        case 0x82bd2df0u:
            (void)crt_reader_bucket_sort61::Apply(e, m, d.engine.sort, s);
            break;
        case 0x82b7a0b0u:
            (void)crt_copy_full_context::Apply(e, m, s);
            break;
        case 0x82b9d328u:
            (void)diagnostic_format_routes61::Apply(e, m, d.diagnostics, s);
            break;
        }
    }
    void Call(unsigned reg, GuestAddress cont) {
        s.ctr = s.r[reg];
        s.lr = cont;
        d.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Initialize() {
        s.r[11] = 0;
        for (unsigned off : {0u, 12u, 4u, 16u, 20u})
            Store(s.r[3] + off, s.r[11]);
    }
    void Normalize() {
        auto &r = s.r;
        r[4] = r[30];
        r[8] = r[27] + 8;
        r[10] = r[26] + 4;
        r[6] = r[25] + 4;
        r[31] = r[30] - r[27];
        r[3] = r[27] - r[26];
        r[5] = r[28];
        do {
            Compare(r[30]);
            if (!s.cr6.eq) {
                r[9] = r[31] + r[8];
                r[7] = Word(r[4]);
                r[11] = Word(r[9] - 4);
                r[9] = Word(r[9]);
            } else {
                Compare(r[25]);
                if (!s.cr6.eq) {
                    r[7] = m.ReadU16(Address(r[6] - 4));
                    r[11] = m.ReadU16(Address(r[6] - 2));
                    r[9] = m.ReadU16(Address(r[6]));
                } else {
                    r[7] = 0;
                    r[11] = 1;
                    r[9] = 2;
                }
            }
            Compare(r[7], r[11]);
            if (s.cr6.lt) {
                Store(r[8] - 8, r[7]);
                Store(r[10] - 4, r[11]);
            } else {
                Store(r[8] - 8, r[11]);
                Store(r[10] - 4, r[7]);
            }
            Compare(r[11], r[9]);
            if (s.cr6.lt) {
                Store(r[3] + r[10], r[11]);
                Store(r[10], r[9]);
            } else {
                Store(r[3] + r[10], r[9]);
                Store(r[10], r[11]);
            }
            Compare(r[9], r[7]);
            if (s.cr6.lt) {
                Store(r[8], r[9]);
                Store(r[10] + 4, r[7]);
            } else {
                Store(r[8], r[7]);
                Store(r[10] + 4, r[9]);
            }
            --r[5];
            r[6] += 6;
            r[4] += 12;
            r[8] += 12;
            r[10] += 12;
            Compare(r[5]);
        } while (!s.cr6.eq);
    }
    void Unique() {
        auto &r = s.r;
        r[11] = 0;
        r[7] = ~std::uint64_t{0};
        r[6] = Word(r[3] + 4);
        Compare(r[31]);
        Store(r[29] + 8, r[28]);
        r[30] = r[7];
        Store(r[29], r[11]);
        if (s.cr6.eq)
            return;
        r[11] = 0xffffffffaaaa0000ull;
        r[4] = r[11] | 43691u;
        do {
            r[11] = Word(r[6]);
            r[10] = (std::uint64_t(Address(r[11])) * Address(r[4])) >> 32;
            r[8] = Shift(r[11], 2);
            r[9] = Address(r[10]) >> 1;
            r[5] = Shift(r[9], 1);
            r[10] = Word(r[8] + r[27]);
            r[9] += r[5];
            r[8] = Word(r[8] + r[26]);
            Compare(r[10], r[7]);
            r[7] = r[11] - r[9];
            if (s.cr6.eq)
                Compare(r[8], r[30]);
            if (!s.cr6.eq) {
                r[11] = Word(r[29]);
                r[11] = Shift(r[11], 3);
                Store(r[11] + r[24], r[10]);
                r[11] = Word(r[29]);
                r[11] = Shift(r[11], 3);
                r[11] += r[24];
                Store(r[11] + 4, r[8]);
                r[11] = Word(r[29]);
                ++r[11];
                Store(r[29], r[11]);
            }
            r[11] = Word(r[29]);
            r[9] += r[7];
            r[5] = Word(r[29] + 12);
            --r[31];
            --r[11];
            r[9] = Shift(r[9], 2);
            r[7] = r[10];
            r[30] = r[8];
            r[6] += 4;
            Compare(r[31]);
            Store(r[9] + r[5], r[11]);
        } while (!s.cr6.eq);
    }
    void Build() {
        auto &r = s.r;
        r[28] = r[4];
        r[29] = r[3];
        r[30] = r[5];
        r[25] = r[6];
        Compare(r[28]);
        bool invalid = s.cr6.eq;
        if (!invalid) {
            Compare(r[30]);
            if (s.cr6.eq) {
                Compare(r[25]);
                invalid = s.cr6.eq;
            }
        }
        if (invalid) {
            r[11] = 0xffffffff820d0000ull;
            r[5] = 149;
            r[4] = r[11] + 25852;
            r[11] = 0xffffffff820d0000ull;
            r[3] = r[11] + 25804;
            Lower(0x82b9d328u, 0x82bbd1d4u);
            return;
        }
        r[11] = Word(r[29] + 12);
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[3] = 1;
            return;
        }
        Lower(0x82bd0798u, 0x82bbce9cu);
        r[11] = Shift(r[28], 1);
        r[10] = Word(r[3]);
        r[5] = 14;
        r[11] += r[28];
        r[31] = Shift(r[11], 2);
        r[11] = Word(r[10]);
        r[4] = r[31];
        Call(11, 0x82bbcec0u);
        Compare(r[3]);
        Store(r[29] + 12, r[3]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        for (unsigned i = 0; i < 2; ++i) {
            Lower(0x82bd0798u, i ? 0x82bbcef8u : 0x82bbced0u);
            r[11] = Word(r[3]);
            r[5] = 1;
            r[4] = r[31];
            r[11] = Word(r[11]);
            Call(11, i ? 0x82bbcf10u : 0x82bbcee8u);
            r[i ? 26 : 27] = r[3];
            Compare(r[i ? 26 : 27]);
            if (s.cr6.eq) {
                r[3] = 0;
                return;
            }
        }
        Lower(0x82bd0798u, 0x82bbcf20u);
        r[10] = Word(r[3]);
        r[11] = Shift(r[28], 1);
        r[5] = 1;
        r[11] += r[28];
        r[10] = Word(r[10]);
        r[4] = Shift(r[11], 3);
        Call(10, 0x82bbcf40u);
        r[24] = r[3];
        Compare(r[24]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        Compare(r[28]);
        if (!s.cr6.eq)
            Normalize();
        r[3] = r[1] + 80;
        Lower(0x82bd2c50u, 0x82bbd030u);
        r[11] = Shift(r[28], 1);
        r[6] = 0;
        r[31] = r[28] + r[11];
        r[4] = r[26];
        r[3] = r[1] + 80;
        r[5] = r[31];
        Lower(0x82bd2df0u, 0x82bbd04cu);
        r[4] = r[27];
        r[5] = r[31];
        r[6] = 0;
        Lower(0x82bd2df0u, 0x82bbd05cu);
        Unique();
        Lower(0x82bd0798u, 0x82bbd114u);
        r[11] = Word(r[29]);
        r[5] = 7;
        r[4] = Shift(r[11], 3);
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(11, 0x82bbd130u);
        Compare(r[3]);
        Store(r[29] + 4, r[3]);
        if (s.cr6.eq) {
            r[3] = r[1] + 80;
            Lower(0x82bd2c78u, 0x82bbd144u);
            r[3] = 0;
            return;
        }
        r[11] = Word(r[29]);
        r[4] = r[24];
        r[5] = Shift(r[11], 3);
        Lower(0x82b7a0b0u, 0x82bbd160u);
        constexpr unsigned regs[]{24, 26, 27};
        constexpr GuestAddress sites[]{0x82bbd164u, 0x82bbd17cu, 0x82bbd194u};
        for (unsigned i = 0; i < 3; ++i) {
            Lower(0x82bd0798u, sites[i]);
            r[11] = Word(r[3]);
            r[4] = r[regs[i]];
            r[11] = Word(r[11] + 12);
            Call(11, sites[i] + 20);
        }
        r[3] = r[1] + 80;
        Lower(0x82bd2c78u, 0x82bbd1b0u);
        r[3] = 1;
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Edges v{m, d, s};
    if (e == 0x82bbd4c0u) {
        v.Initialize();
        return true;
    }
    if (e != 0x82bbce58u)
        return false;
    auto &r = s.r;
    r[12] = s.lr;
    s.lr = 0x82bbce60u;
    for (unsigned i = 24; i < 32; ++i)
        recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
    m.WriteU32(Address(r[1] - 8), Address(r[12]));
    auto old = r[1];
    r[1] -= 176;
    m.WriteU32(Address(r[1]), Address(old));
    v.Build();
    r[1] += 176;
    for (unsigned i = 24; i < 32; ++i)
        r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
    r[12] = m.ReadU32(Address(r[1] - 8));
    s.lr = r[12];
    return true;
}
} // namespace lo::semantic::gpu::mesh_edge_build61
