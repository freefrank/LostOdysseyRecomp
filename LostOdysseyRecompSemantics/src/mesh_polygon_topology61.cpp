#include "lo_semantics/mesh_polygon_topology61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_polygon_topology61 {
namespace {
using recovery_abi::Address;
struct Topology {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    static std::uint32_t Shift(std::uint64_t v, unsigned n) { return Address(v) << n; }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(v)));
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void FloatStore(unsigned i, std::uint64_t p) {
        Store(p, std::bit_cast<std::uint32_t>(float(F(i))));
    }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.edge.engine.sort.guest, s);
            break;
        case 0x82bb9aa8u:
            (void)mesh_polygon_build61::Apply(e, m, d, s);
            break;
        case 0x82bd2c50u:
            (void)object_sort_support61::Apply(e, m, {d.edge.engine.sort.guest, d.edge.engine.fp},
                                               s);
            break;
        case 0x82bd2df0u:
            (void)crt_reader_bucket_sort61::Apply(e, m, d.edge.engine.sort, s);
            break;
        case 0x82bd2c78u:
            (void)crt_reader_follow61::Apply(e, m, d.edge.engine.sort.guest, s);
            break;
        case 0x82b7a0b0u:
            (void)crt_copy_full_context::Apply(e, m, s);
            break;
        case 0x82b7bc40u:
            crt_reader_chain61::ApplySupport_B7BC40(m, d.edge.engine.sort.accepted, s);
            break;
        case 0x82bbb728u:
            (void)mesh_polygon_topology61::Apply(e, m, d, s);
            break;
        }
    }
    void Call(unsigned reg, GuestAddress cont) {
        s.ctr = s.r[reg];
        s.lr = cont;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bbb730u;
        for (unsigned i = 14; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= 256;
        Store(r[1], old);
        Body();
        r[1] += 256;
        for (unsigned i = 14; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Failed(bool sorted) {
        if (sorted) {
            s.r[3] = s.r[1] + 80;
            Lower(0x82bd2c78u, 0x82bbb994u);
        }
        s.r[3] = 0;
    }
    void Allocate(unsigned size, unsigned tag, unsigned result, GuestAddress lookup,
                  GuestAddress call) {
        auto &r = s.r;
        Lower(0x82bd0798u, lookup);
        r[11] = Word(r[3]);
        r[5] = tag;
        r[4] = r[size];
        r[11] = Word(r[11]);
        Call(11, call);
        r[result] = r[3];
        Compare(r[result]);
    }
    void Free(unsigned reg, GuestAddress lookup) {
        auto &r = s.r;
        Lower(0x82bd0798u, lookup);
        r[11] = Word(r[3]);
        r[4] = r[reg];
        r[11] = Word(r[11] + 12);
        Call(11, lookup + 20);
    }
    void ReleaseField(unsigned offset, GuestAddress lookup) {
        auto &r = s.r;
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + offset);
        Compare(r[11]);
        if (!s.cr6.eq)
            ReleaseKnown(offset, lookup);
    }
    void ReleaseKnown(unsigned offset, GuestAddress lookup) {
        auto &r = s.r;
        Lower(0x82bd0798u, lookup);
        r[11] = Word(r[31] + 4);
        r[4] = Word(r[11] + offset);
        r[11] = Word(r[3]);
        r[11] = Word(r[11] + 12);
        Call(11, lookup + 24);
        r[11] = Word(r[31] + 4);
        Store(r[11] + offset, r[16]);
    }
    void CollectEdges() {
        auto &r = s.r;
        r[29] = r[21];
        r[28] = r[22];
        r[27] = r[19];
        r[26] = r[20];
        r[25] = r[16];
        Compare(r[17]);
        if (s.cr6.eq)
            return;
        r[24] = r[16];
        do {
            r[11] = Word(r[31] + 4);
            r[11] = Word(r[11] + 40);
            Compare(r[11]);
            if (s.cr6.eq) {
                r[3] = r[31];
                Lower(0x82bb9aa8u, 0x82bbb87cu);
            }
            r[11] = Word(r[31] + 4);
            r[11] = Word(r[11] + 40);
            Compare(r[11]);
            r[30] = m.ReadU16(Address(r[11]) + Address(r[24]));
            if (s.cr6.eq) {
                r[3] = r[31];
                Lower(0x82bb9aa8u, 0x82bbb898u);
            }
            r[10] = Word(r[31] + 4);
            r[11] = r[16];
            Compare(r[30]);
            r[10] = Word(r[10] + 40);
            r[10] += r[24];
            r[7] = Word(r[10] + 4);
            if (!s.cr6.eq) {
                r[6] = r[7] - 1;
                do {
                    r[9] = r[11] + 1;
                    r[10] = (r[10] & 0xffffffff00000000ull) | (Address(r[9]) / Address(r[30]));
                    r[10] = std::uint64_t(std::int64_t(std::int32_t(r[10])) *
                                          std::int64_t(std::int32_t(r[30])));
                    r[8] = m.ReadU8(Address(r[9]) + Address(r[6]));
                    r[10] = r[9] - r[10];
                    r[10] = m.ReadU8(Address(r[10]) + Address(r[7]));
                    Compare(r[8], r[10]);
                    if (s.cr6.gt) {
                        r[8] ^= r[10];
                        r[10] ^= r[8];
                        r[8] ^= r[10];
                    }
                    r[5] = r[11];
                    Store(r[29], r[8]);
                    Store(r[28], r[10]);
                    r[11] = r[9];
                    Store(r[27], r[25]);
                    r[29] += 4;
                    r[28] += 4;
                    r[27] += 4;
                    Store(r[26], r[5]);
                    Compare(r[11], r[30]);
                    r[26] += 4;
                } while (s.cr6.lt);
            }
            ++r[25];
            r[24] += 36;
            Compare(r[25], r[17]);
        } while (s.cr6.lt);
    }
    void UniqueEdges() {
        auto &r = s.r;
        r[6] = ~std::uint64_t(0);
        Compare(r[18]);
        r[28] = r[6];
        if (s.cr6.eq)
            return;
        r[4] = r[30] - r[26];
        r[10] = r[26];
        r[3] = r[24] - r[26];
        r[30] = r[25] - r[26];
        r[5] = r[18];
        do {
            r[11] = Word(r[4] + r[10]);
            r[11] = Shift(r[11], 2);
            r[9] = Word(r[11] + r[21]);
            r[8] = Word(r[11] + r[19]);
            r[7] = Word(r[11] + r[20]);
            Compare(r[9], r[6]);
            r[11] = Word(r[11] + r[22]);
            bool same = s.cr6.eq;
            if (same) {
                Compare(r[11], r[28]);
                same = s.cr6.eq;
            }
            if (!same) {
                r[23] = Word(r[31] + 4);
                r[14] = r[11];
                r[28] = r[11];
                r[6] = r[9];
                r[11] = Word(r[23] + 52);
                r[11] = Shift(r[11], 1);
                m.WriteU8(Address(r[11]) + Address(r[29]), std::uint8_t(r[9]));
                r[11] = Word(r[31] + 4);
                r[11] = Word(r[11] + 52);
                r[11] = Shift(r[11], 1);
                r[11] += r[29];
                m.WriteU8(Address(r[11] + 1), std::uint8_t(r[14]));
                r[11] = Word(r[31] + 4);
                r[9] = Word(r[11] + 52);
                ++r[9];
                Store(r[11] + 52, r[9]);
            }
            Store(r[10], r[8]);
            --r[5];
            Store(r[3] + r[10], r[7]);
            r[11] = Word(r[31] + 4);
            Compare(r[5]);
            r[11] = Word(r[11] + 52);
            --r[11];
            Store(r[30] + r[10], r[11]);
            r[10] += 4;
        } while (!s.cr6.eq);
    }
    void PolygonEdgeIds() {
        auto &r = s.r;
        Lower(0x82bd0798u, 0x82bbbbccu);
        r[11] = Word(r[3]);
        r[5] = 50;
        r[4] = r[27];
        r[11] = Word(r[11]);
        Call(11, 0x82bbbbe4u);
        r[11] = Word(r[31] + 4);
        Compare(r[18]);
        Store(r[11] + 48, r[3]);
        if (!s.cr6.eq) {
            r[9] = r[16];
            r[10] = r[29];
            r[11] = r[18];
            do {
                r[8] = Word(r[10]);
                --r[11];
                r[7] = Word(r[31] + 4);
                r[10] += 4;
                r[8] = Shift(r[8], 2);
                Compare(r[11]);
                r[7] = Word(r[7] + 48);
                r[8] = Word(r[8] + r[25]);
                m.WriteU16(Address(r[7]) + Address(r[9]), std::uint16_t(r[8]));
                r[9] += 2;
            } while (!s.cr6.eq);
        }
        r[11] = Word(r[31] + 4);
        Compare(r[17]);
        r[8] = Word(r[11] + 48);
        if (!s.cr6.eq) {
            r[9] = Address(r[11]);
            r[11] = r[16];
            r[10] = r[17];
            r[9] = Word(r[9] + 40);
            do {
                r[9] += r[11];
                --r[10];
                Compare(r[10]);
                Store(r[9] + 8, r[8]);
                r[9] = Word(r[31] + 4);
                r[9] = Word(r[9] + 40);
                r[7] = m.ReadU16(Address(r[9]) + Address(r[11]));
                r[11] += 36;
                r[7] = std::rotl(Address(r[7]), 1);
                r[8] += r[7];
            } while (!s.cr6.eq);
        }
    }
    void Prefix() {
        auto &r = s.r;
        r[11] = Word(r[31] + 4);
        r[10] = 1;
        r[11] = Word(r[11] + 64);
        Store(r[11] + 4, r[16]);
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 52);
        Compare(r[11], 1);
        if (s.cr6.gt) {
            r[9] = 8;
            do {
                r[11] = Word(r[31] + 4);
                ++r[10];
                r[11] = Word(r[11] + 64);
                r[11] += r[9];
                r[9] += 8;
                r[8] = m.ReadU16(Address(r[11] - 6));
                r[7] = Word(r[11] - 4);
                r[8] += r[7];
                Store(r[11] + 4, r[8]);
                r[11] = Word(r[31] + 4);
                r[11] = Word(r[11] + 52);
                Compare(r[10], r[11]);
            } while (s.cr6.lt);
        }
    }
    void IncidenceCounts() {
        auto &r = s.r;
        r[11] = Word(r[31] + 4);
        r[4] = 0;
        r[10] = Word(r[11] + 52);
        r[3] = Word(r[11] + 64);
        r[5] = Shift(r[10], 3);
        Lower(0x82b7bc40u, 0x82bbbd60u);
        r[11] = Word(r[31] + 4);
        Compare(r[18]);
        r[10] = Word(r[11] + 48);
        if (!s.cr6.eq) {
            r[11] = r[18];
            do {
                r[8] = Word(r[31] + 4);
                --r[11];
                r[9] = m.ReadU16(Address(r[10]));
                r[10] += 2;
                Compare(r[11]);
                r[9] = std::rotl(Address(r[9]), 3);
                r[8] = Word(r[8] + 64);
                r[9] += r[8];
                r[8] = m.ReadU16(Address(r[9] + 2));
                ++r[8];
                m.WriteU16(Address(r[9] + 2), std::uint16_t(r[8]));
            } while (!s.cr6.eq);
        }
        Prefix();
    }
    void IncidenceBytes() {
        auto &r = s.r;
        r[11] = Word(r[11] + 48);
        Compare(r[18]);
        if (!s.cr6.eq) {
            r[9] = r[29];
            r[10] = r[18];
            do {
                r[8] = Word(r[31] + 4);
                --r[10];
                r[7] = Word(r[9]);
                r[9] += 4;
                r[6] = m.ReadU16(Address(r[11]));
                Compare(r[10]);
                r[5] = Shift(r[7], 2);
                r[7] = std::rotl(Address(r[6]), 3);
                r[6] = Word(r[8] + 64);
                r[8] = Word(r[8] + 68);
                r[7] += r[6];
                r[5] = Word(r[5] + r[26]);
                r[7] = Word(r[7] + 4);
                m.WriteU8(Address(r[7]) + Address(r[8]), std::uint8_t(r[5]));
                r[7] = Word(r[31] + 4);
                r[8] = m.ReadU16(Address(r[11]));
                r[11] += 2;
                r[8] = std::rotl(Address(r[8]), 3);
                r[7] = Word(r[7] + 64);
                r[8] += r[7];
                r[7] = Word(r[8] + 4);
                ++r[7];
                Store(r[8] + 4, r[7]);
            } while (!s.cr6.eq);
        }
        Prefix();
    }
    void Bisectors() {
        auto &r = s.r;
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 56);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = r[31];
            Lower(0x82bbb728u, 0x82bbbfacu);
        }
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 64);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = r[31];
            Lower(0x82bbb728u, 0x82bbbfc4u);
        }
        r[11] = Word(r[31] + 4);
        r[10] = Word(r[11] + 68);
        r[28] = Word(r[11] + 64);
        Compare(r[10]);
        if (s.cr6.eq) {
            r[3] = r[31];
            Lower(0x82bbb728u, 0x82bbbfe0u);
        }
        r[11] = Word(r[31] + 4);
        r[30] = Word(r[11] + 68);
        Lower(0x82bd0798u, 0x82bbbfecu);
        r[11] = Word(r[31] + 4);
        r[5] = 51;
        r[10] = Word(r[3]);
        r[11] = Word(r[11] + 52);
        r[9] = Word(r[10]);
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[4] = Shift(r[11], 2);
        Call(9, 0x82bbc014u);
        r[11] = Word(r[31] + 4);
        r[29] = r[16];
        Store(r[11] + 60, r[3]);
        r[3] = Word(r[31] + 4);
        r[11] = Word(r[3] + 52);
        Compare(r[11]);
        if (!s.cr6.gt)
            return;
        r[10] = 0xffffffff82000000ull;
        r[11] = 0xffffffff82000000ull;
        r[5] = Address(r[3]);
        r[4] = r[16];
        r[8] = r[28] + 4;
        Load(10, r[10] + 30596);
        Load(9, r[11] + 3664);
        do {
            r[11] = Word(r[8]);
            r[10] = Word(r[5] + 40);
            r[11] += r[30];
            r[9] = m.ReadU8(Address(r[11] + 1));
            r[11] = m.ReadU8(Address(r[11]));
            r[6] = std::rotl(Address(r[9]), 3);
            r[7] = std::rotl(Address(r[11]), 3);
            r[9] += r[6];
            r[7] += r[11];
            r[11] = Shift(r[9], 2);
            r[9] = Shift(r[7], 2);
            r[11] += r[10];
            r[10] += r[9];
            Load(0, r[11] + 16);
            Load(13, r[10] + 16);
            Single(13, F(0) + F(13));
            Load(0, r[10] + 20);
            Load(12, r[11] + 20);
            Single(12, F(12) + F(0));
            Load(11, r[11] + 12);
            Load(0, r[10] + 12);
            Single(0, F(0) + F(11));
            Single(11, F(13) * F(13));
            Single(11, F(12) * F(12) + F(11));
            Single(11, F(0) * F(0) + F(11));
            bool u = std::isnan(F(11)) || std::isnan(F(9));
            s.cr6 = {std::uint8_t(!u && F(11) < F(9)), std::uint8_t(!u && F(11) > F(9)),
                     std::uint8_t(!u && F(11) == F(9)), std::uint8_t(u)};
            if (!s.cr6.eq) {
                Single(11, std::sqrt(F(11)));
                Single(11, F(10) / F(11));
                Single(0, F(0) * F(11));
                Single(13, F(13) * F(11));
                Single(12, F(12) * F(11));
            }
            r[11] = Word(r[3] + 60);
            ++r[29];
            r[8] += 8;
            r[11] += r[4];
            r[4] += 12;
            Gradual();
            FloatStore(0, r[11]);
            FloatStore(13, r[11] + 4);
            FloatStore(12, r[11] + 8);
            r[11] = Word(r[3] + 52);
            Compare(r[29], r[11]);
        } while (s.cr6.lt);
    }
    void Body() {
        auto &r = s.r;
        r[31] = r[3];
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 36);
        Compare(r[11]);
        if (s.cr6.eq)
            Lower(0x82bb9aa8u, 0x82bbb74cu);
        r[11] = Word(r[31] + 4);
        r[16] = 0;
        r[18] = r[16];
        r[17] = Word(r[11] + 36);
        Compare(r[17]);
        if (!s.cr6.eq) {
            r[29] = r[16];
            r[30] = r[17];
            do {
                r[11] = Word(r[11] + 40);
                Compare(r[11]);
                if (s.cr6.eq) {
                    r[3] = r[31];
                    Lower(0x82bb9aa8u, 0x82bbb780u);
                }
                r[11] = Word(r[31] + 4);
                --r[30];
                Compare(r[30]);
                r[10] = Word(r[11] + 40);
                r[10] = m.ReadU16(Address(r[10]) + Address(r[29]));
                r[29] += 36;
                r[18] += r[10];
            } while (!s.cr6.eq);
        }
        Lower(0x82bd0798u, 0x82bbb7a4u);
        r[11] = Word(r[3]);
        r[23] = Shift(r[18], 2);
        r[5] = 1;
        r[4] = r[23];
        r[11] = Word(r[11]);
        Call(11, 0x82bbb7c0u);
        r[21] = r[3];
        Compare(r[21]);
        if (s.cr6.eq) {
            Failed(false);
            return;
        }
        Allocate(23, 1, 22, 0x82bbb7d0u, 0x82bbb7e8u);
        if (s.cr6.eq) {
            Failed(false);
            return;
        }
        Allocate(23, 1, 19, 0x82bbb7f8u, 0x82bbb810u);
        if (s.cr6.eq) {
            Failed(false);
            return;
        }
        Allocate(23, 1, 20, 0x82bbb820u, 0x82bbb838u);
        if (s.cr6.eq) {
            Failed(false);
            return;
        }
        CollectEdges();
        r[3] = r[1] + 80;
        Lower(0x82bd2c50u, 0x82bbb930u);
        r[6] = 0;
        r[5] = r[18];
        r[4] = r[22];
        r[3] = r[1] + 80;
        Lower(0x82bd2df0u, 0x82bbb944u);
        r[4] = r[21];
        r[5] = r[18];
        r[6] = 0;
        Lower(0x82bd2df0u, 0x82bbb954u);
        r[11] = Word(r[31] + 4);
        r[30] = Word(r[3] + 4);
        Store(r[11] + 52, r[16]);
        Lower(0x82bd0798u, 0x82bbb964u);
        r[11] = Word(r[3]);
        r[27] = Shift(r[18], 1);
        r[5] = 7;
        r[4] = r[27];
        r[11] = Word(r[11]);
        Call(11, 0x82bbb980u);
        r[29] = r[3];
        Compare(r[29]);
        if (s.cr6.eq) {
            Failed(true);
            return;
        }
        Allocate(23, 1, 26, 0x82bbb9a4u, 0x82bbb9bcu);
        if (s.cr6.eq) {
            Failed(true);
            return;
        }
        Allocate(23, 1, 24, 0x82bbb9ccu, 0x82bbb9e4u);
        if (s.cr6.eq) {
            Failed(true);
            return;
        }
        Allocate(23, 1, 25, 0x82bbb9f4u, 0x82bbba0cu);
        if (s.cr6.eq) {
            Failed(true);
            return;
        }
        UniqueEdges();
        ReleaseField(56, 0x82bbbae0u);
        Lower(0x82bd0798u, 0x82bbbb04u);
        r[11] = Word(r[31] + 4);
        r[5] = 7;
        r[11] = Word(r[11] + 52);
        r[4] = Shift(r[11], 1);
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(11, 0x82bbbb24u);
        r[11] = Word(r[31] + 4);
        Store(r[11] + 56, r[3]);
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 56);
        Compare(r[11]);
        if (s.cr6.eq) {
            Failed(true);
            return;
        }
        r[11] = Word(r[31] + 4);
        r[4] = r[29];
        r[10] = Word(r[11] + 52);
        r[3] = Word(r[11] + 56);
        r[5] = Shift(r[10], 1);
        Lower(0x82b7a0b0u, 0x82bbbb54u);
        Free(29, 0x82bbbb58u);
        r[6] = 0;
        r[5] = r[18];
        r[4] = r[24];
        r[3] = r[1] + 80;
        Lower(0x82bd2df0u, 0x82bbbb80u);
        r[4] = r[26];
        r[5] = r[18];
        r[6] = 0;
        Lower(0x82bd2df0u, 0x82bbbb90u);
        r[11] = Word(r[31] + 4);
        r[29] = Word(r[3] + 4);
        r[11] = Word(r[11] + 48);
        Compare(r[11]);
        if (!s.cr6.eq)
            ReleaseKnown(48, 0x82bbbba8u);
        PolygonEdgeIds();
        Free(20, 0x82bbbc7cu);
        Free(19, 0x82bbbc94u);
        Free(22, 0x82bbbcacu);
        Free(21, 0x82bbbcc4u);
        ReleaseField(64, 0x82bbbcecu);
        Lower(0x82bd0798u, 0x82bbbd10u);
        r[11] = Word(r[31] + 4);
        r[5] = 8;
        r[11] = Word(r[11] + 52);
        r[4] = Shift(r[11], 3);
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(11, 0x82bbbd30u);
        r[11] = Word(r[31] + 4);
        Store(r[11] + 64, r[3]);
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 64);
        Compare(r[11]);
        if (s.cr6.eq) {
            Failed(true);
            return;
        }
        IncidenceCounts();
        r[11] = Word(r[31] + 4);
        r[10] = Word(r[11] + 52);
        r[9] = Word(r[11] + 64);
        r[10] = Shift(r[10], 3);
        r[8] = Word(r[11] + 68);
        r[11] = r[10] + r[9];
        Compare(r[8]);
        r[10] = m.ReadU16(Address(r[11] - 6));
        r[11] = Word(r[11] - 4);
        r[30] = r[10] + r[11];
        if (!s.cr6.eq)
            ReleaseKnown(68, 0x82bbbe2cu);
        Lower(0x82bd0798u, 0x82bbbe50u);
        r[11] = Word(r[3]);
        r[5] = 52;
        r[4] = r[30];
        r[11] = Word(r[11]);
        Call(11, 0x82bbbe68u);
        r[11] = Word(r[31] + 4);
        Store(r[11] + 68, r[3]);
        r[11] = Word(r[31] + 4);
        r[10] = Word(r[11] + 68);
        Compare(r[10]);
        if (s.cr6.eq) {
            Failed(true);
            return;
        }
        IncidenceBytes();
        Free(25, 0x82bbbf50u);
        Free(24, 0x82bbbf68u);
        Free(26, 0x82bbbf80u);
        Bisectors();
        r[3] = r[1] + 80;
        Lower(0x82bd2c78u, 0x82bbc100u);
        r[3] = 1;
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82bbb728u)
        return false;
    Topology{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_polygon_topology61
