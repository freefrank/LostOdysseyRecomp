#include "lo_semantics/mesh_triangle_links61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/mesh_cache_build61.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_triangle_links61 {
namespace {
using recovery_abi::Address;
struct Links {
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
    void Enter(unsigned first, unsigned frame, GuestAddress cont = 0) {
        auto &r = s.r;
        r[12] = s.lr;
        if (cont)
            s.lr = cont;
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
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
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        if (e == 0x82bd0798u)
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.edge.engine.sort.guest, s);
        else if (e == 0x82b9d328u)
            (void)diagnostic_format_routes61::Apply(e, m, d.edge.diagnostics, s);
        else if (e == 0x82bd2c50u)
            (void)object_sort_support61::Apply(e, m, {d.edge.engine.sort.guest, d.edge.engine.fp}, s);
        else if (e == 0x82bd2c78u)
            (void)crt_reader_follow61::Apply(e, m, d.edge.engine.sort.guest, s);
        else if (e == 0x82bd2df0u)
            (void)crt_reader_bucket_sort61::Apply(e, m, d.edge.engine.sort, s);
        else if (e == 0x82bbd4c0u)
            (void)mesh_edge_build61::Apply(e, m, d.edge, s);
        else if (e == 0x82bbddf0u)
            (void)mesh_cache_build61::Apply(e, m, d, s);
        else if (e == 0x82bbd4e0u)
            (void)mesh_cook_storage61::Apply(e, m, d.lifetime, s);
        else
            (void)mesh_triangle_links61::Apply(e, m, d, s);
    }
    void Call(GuestAddress cont) {
        s.ctr = s.r[11];
        s.lr = cont;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void SortRecords() {
        Enter(24, 192, 0x82bc3cb8u);
        SortBody();
        Leave(24, 192);
    }
    void SortFailure() {
        s.r[3] = s.r[1] + 96;
        Lower(0x82bd2c78u, 0x82bc3d04u);
        s.r[3] = 0;
    }
    void SortBody() {
        auto &r = s.r;
        r[25] = r[3];
        r[3] = r[1] + 96;
        r[31] = r[4];
        r[26] = r[5];
        r[24] = r[6];
        Lower(0x82bd2c50u, 0x82bc3cd4u);
        Lower(0x82bd0798u, 0x82bc3cd8u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = Shift(r[31], 2);
        r[11] = Word(r[11]);
        Call(0x82bc3cf0u);
        r[30] = r[3];
        Compare(r[30]);
        if (s.cr6.eq) {
            SortFailure();
            return;
        }
        // Stable endpoint sorts group equal undirected edges while retaining owners.
        for (unsigned pass = 0; pass < 2; ++pass) {
            Compare(r[31]);
            if (!s.cr6.eq) {
                r[9] = r[30];
                r[10] = r[26] + 4 * pass;
                r[11] = r[31];
                do {
                    r[8] = Word(r[10]);
                    --r[11];
                    r[10] += 12;
                    Compare(r[11]);
                    Store(r[9], r[8]);
                    r[9] += 4;
                } while (!s.cr6.eq);
            }
            r[6] = 0;
            r[5] = r[31];
            r[4] = r[30];
            r[3] = r[1] + 96;
            Lower(0x82bd2df0u, pass ? 0x82bc3d98u : 0x82bc3d54u);
        }
        Lower(0x82bd0798u, 0x82bc3d9cu);
        r[11] = Word(r[3]);
        r[4] = r[30];
        r[11] = Word(r[11] + 12);
        Call(0x82bc3db0u);
        r[29] = Word(r[1] + 100);
        r[9] = 0;
        Compare(r[31]);
        r[11] = Word(r[29]);
        Offset();
        r[11] += r[26];
        r[5] = Word(r[11]);
        r[6] = Word(r[11] + 4);
        bool diagnostic = false;
        if (!s.cr6.eq)
            do {
                r[11] = Word(r[29]);
                --r[31];
                r[29] += 4;
                Offset();
                r[11] += r[26];
                r[30] = Word(r[11]);
                r[27] = Word(r[11] + 8);
                r[28] = Word(r[11] + 4);
                Compare(r[30], r[5]);
                bool same = s.cr6.eq;
                if (same) {
                    Compare(r[28], r[6]);
                    same = s.cr6.eq;
                }
                if (same) {
                    r[11] = Shift(r[9], 2);
                    r[10] = r[1] + 80;
                    ++r[9];
                    Compare(r[9], 3);
                    Store(r[11] + r[10], r[27]);
                    if (s.cr6.eq) {
                        r[11] = 0xffffffff820d0000ull;
                        r[5] = 785;
                        r[4] = r[11] + 26236;
                        r[11] = 0xffffffff820d0000ull;
                        r[3] = r[11] + 26424;
                        Lower(0x82b9d328u, 0x82bc3e44u);
                        r[31] = r[3];
                        diagnostic = true;
                        break;
                    }
                } else {
                    Compare(r[9], 2);
                    if (s.cr6.eq) {
                        PairArguments();
                        Lower(0x82bc3b10u, 0x82bc3e64u);
                        r[11] = Address(r[3]) & 255u;
                        Compare(r[11]);
                        if (s.cr6.eq) {
                            SortFailure();
                            return;
                        }
                    }
                    Store(r[1] + 80, r[27]);
                    r[9] = 1;
                    r[5] = r[30];
                    r[6] = r[28];
                }
                Compare(r[31]);
            } while (!s.cr6.eq);
        if (!diagnostic) {
            r[31] = 1;
            Compare(r[9], 2);
            if (s.cr6.eq) {
                PairArguments();
                Lower(0x82bc3b10u, 0x82bc3ea8u);
                r[31] = r[3];
            }
        }
        r[3] = r[1] + 96;
        Lower(0x82bd2c78u, 0x82bc3eb4u);
        r[3] = r[31];
    }
    void PairArguments() {
        auto &r = s.r;
        r[8] = r[24];
        r[4] = Word(r[1] + 84);
        r[7] = r[25];
        r[3] = Word(r[1] + 80);
    }
    void AllocationSize() {
        auto &r = s.r;
        Compare(r[31], r[29]);
        if (!s.cr6.gt) {
            r[11] = Shift(r[31], 1);
            r[11] += r[31];
            r[11] = Shift(r[11], 2);
            Compare(r[11], r[28]);
            r[30] = r[11] + 4;
            if (!s.cr6.gt)
                return;
        }
        r[30] = r[26];
    }
    void Build() {
        Enter(24, 224, 0x82bc3f28u);
        BuildBody();
        Leave(24, 224);
    }
    void BuildBody() {
        auto &r = s.r;
        r[25] = r[4];
        r[27] = r[3];
        r[31] = Word(r[25]);
        Compare(r[31]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = 357892096;
        Store(r[27], r[31]);
        r[26] = ~std::uint64_t(0);
        r[29] = r[11] | 21845;
        r[28] = 0xfffffffffffffffbull;
        AllocationSize();
        Lower(0x82bd0798u, 0x82bc3f88u);
        r[11] = Word(r[3]);
        r[5] = 15;
        r[4] = r[30];
        r[11] = Word(r[11]);
        Call(0x82bc3fa0u);
        r[24] = 0;
        Compare(r[3]);
        if (!s.cr6.eq) {
            r[11] = r[3] + 4;
            Store(r[3], r[31]);
        } else
            r[11] = r[24];
        Compare(r[11]);
        Store(r[27] + 4, r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = Word(r[27]);
        r[10] = Shift(r[11], 1);
        r[31] = r[11] + r[10];
        AllocationSize();
        Lower(0x82bd0798u, 0x82bc3ffcu);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = r[30];
        r[11] = Word(r[11]);
        Call(0x82bc4014u);
        Compare(r[3]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[29] = r[3] + 4;
        Store(r[3], r[31]);
        Compare(r[29]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = Word(r[27]);
        r[6] = r[24];
        Store(r[1] + 80, r[24]);
        Compare(r[11]);
        if (s.cr6.gt) {
            r[30] = r[24];
            r[31] = r[24];
            do {
                BuildIndices();
                r[9] = r[29];
                r[7] = Word(r[27] + 4);
                r[8] = r[1] + 80;
                Lower(0x82bc3970u, 0x82bc40e4u);
                r[11] = Word(r[27]);
                ++r[6];
                r[31] += 12;
                r[30] += 6;
                Compare(r[6], r[11]);
            } while (s.cr6.lt);
        }
        r[6] = r[25];
        r[3] = Word(r[27] + 4);
        r[5] = r[29];
        r[4] = Word(r[1] + 80);
        Lower(0x82bc3cb0u, 0x82bc4110u);
        r[31] = r[3];
        Lower(0x82bd0798u, 0x82bc4118u);
        r[11] = Word(r[3]);
        r[4] = r[29] - 4;
        r[11] = Word(r[11] + 12);
        Call(0x82bc412cu);
        r[11] = Address(r[31]) & 255u;
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[11] = Word(r[25] + 12);
            Compare(r[11]);
            if (!s.cr6.eq)
                Filter();
        }
        r[3] = r[31];
    }
    void BuildIndices() {
        auto &r = s.r;
        r[10] = Word(r[25] + 4);
        Compare(r[10]);
        if (!s.cr6.eq)
            r[3] = Word(r[31] + r[10]);
        else {
            r[11] = Word(r[25] + 8);
            Compare(r[11]);
            r[3] = s.cr6.eq ? r[24] : m.ReadU16(Address(r[30]) + Address(r[11]));
        }
        for (unsigned i = 1; i < 3; ++i) {
            Compare(r[10]);
            if (!s.cr6.eq) {
                r[11] = r[31] + r[10];
                s.r[3 + i] = Word(r[11] + 4 * i);
            } else {
                r[11] = Word(r[25] + 8);
                Compare(r[11]);
                if (!s.cr6.eq) {
                    r[11] += r[30];
                    s.r[3 + i] = m.ReadU16(Address(r[11] + 2 * i));
                } else
                    s.r[3 + i] = i;
            }
        }
    }
    void Filter() {
        auto &r = s.r;
        r[10] = Word(r[25]);
        r[3] = r[1] + 128;
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[0] =
            std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(r[25] + 16))));
        m.WriteU8(Address(r[1] + 109), std::uint8_t(r[24]));
        Store(r[1] + 116,
              std::bit_cast<std::uint32_t>(float(std::bit_cast<double>(s.fpr_bits[0]))));
        Store(r[1] + 112, r[11]);
        Store(r[1] + 96, r[10]);
        r[10] = Word(r[25] + 4);
        Store(r[1] + 100, r[10]);
        r[10] = Word(r[25] + 8);
        Store(r[1] + 104, r[10]);
        r[10] = 1;
        m.WriteU8(Address(r[1] + 108), std::uint8_t(r[10]));
        Lower(0x82bbd4c0u, 0x82bc417cu);
        r[4] = r[1] + 96;
        r[3] = r[1] + 128;
        Lower(0x82bbddf0u, 0x82bc4188u);
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[11] = Word(r[27]);
            r[8] = r[24];
            Compare(r[11]);
            if (s.cr6.gt) {
                r[11] = r[24];
                do {
                    r[10] = Word(r[1] + 140);
                    r[9] = r[11] + r[10];
                    r[10] = Word(r[9]);
                    r[10] = Address(r[10]) >> 31;
                    Compare(r[10]);
                    r[10] = Word(r[27] + 4);
                    r[7] = Word(r[11] + r[10]);
                    Tag(7);
                    Store(r[11] + r[10], r[7]);
                    r[10] = Word(r[9] + 8);
                    r[10] = Address(r[10]) >> 31;
                    Compare(r[10]);
                    r[10] = Word(r[27] + 4);
                    r[10] += r[11];
                    r[7] = Word(r[10] + 4);
                    Tag(7);
                    Store(r[10] + 4, r[7]);
                    r[10] = Word(r[9] + 4);
                    r[10] = Address(r[10]) >> 31;
                    Compare(r[10]);
                    r[10] = Word(r[27] + 4);
                    r[10] += r[11];
                    r[9] = Word(r[10] + 8);
                    Tag(9);
                    Store(r[10] + 8, r[9]);
                    ++r[8];
                    r[10] = Word(r[27]);
                    r[11] += 12;
                    Compare(r[8], r[10]);
                } while (s.cr6.lt);
            }
        }
        r[3] = r[1] + 128;
        Lower(0x82bbd4e0u, 0x82bc424cu);
    }
    void Tag(unsigned reg) {
        if (!s.cr6.eq)
            s.r[reg] |= 0x20000000u;
        else
            s.r[reg] =
                (std::uint64_t(Address(s.r[reg])) | (std::uint64_t(Address(s.r[reg])) << 32)) &
                0xffffffffdfffffffull;
    }
    void Count() {
        auto &r = s.r;
        r[11] = Word(r[3] + 4);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[10] = Word(r[3]);
        r[3] = 0;
        Compare(r[10]);
        if (s.cr6.eq)
            return;
        r[8] = r[10];
        r[10] = 536805376;
        r[9] = r[10] | 65535;
        do {
            r[7] = Word(r[11]);
            r[10] = 0;
            r[7] = Address(r[7]) & 0x1fffffffu;
            Compare(r[7], r[9]);
            if (s.cr6.eq)
                r[10] = 1;
            for (unsigned off : {4u, 8u}) {
                r[7] = Word(r[11] + off);
                r[7] = Address(r[7]) & 0x1fffffffu;
                Compare(r[7], r[9]);
                if (s.cr6.eq)
                    ++r[10];
            }
            --r[8];
            r[3] += r[10];
            r[11] += 12;
            Compare(r[8]);
        } while (!s.cr6.eq);
    }
    void Offset() {
        auto &r = s.r;
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[11] = Shift(r[11], 2);
    }
    void EdgeRecord(unsigned a, unsigned b) {
        auto &r = s.r;
        bool ascending = s.cr6.lt;
        Store(r[11] + r[9], r[ascending ? a : b]);
        r[11] = Word(r[8]);
        Offset();
        r[11] += r[9];
        Store(r[11] + 4, r[ascending ? b : a]);
    }
    void Emit() {
        auto &r = s.r;
        recovery_abi::WriteU64(m, Address(r[1] - 16), r[30]);
        recovery_abi::WriteU64(m, Address(r[1] - 8), r[31]);
        r[11] = ~std::uint64_t(0);
        Compare(r[3], r[4]);
        r[10] = r[11];
        r[31] = r[11];
        r[30] = r[11];
        r[11] = Shift(r[6], 1);
        r[11] += r[6];
        r[11] = Shift(r[11], 2);
        r[11] += r[7];
        Store(r[11], r[10]);
        Store(r[11] + 4, r[31]);
        Store(r[11] + 8, r[30]);
        r[11] = Word(r[8]);
        Offset();
        EdgeRecord(3, 4);
        r[11] = Word(r[8]);
        Compare(r[3], r[5]);
        Offset();
        r[11] += r[9];
        Store(r[11] + 8, r[6]);
        r[11] = Word(r[8]);
        ++r[11];
        r[10] = Shift(r[11], 1);
        Store(r[8], r[11]);
        r[11] += r[10];
        r[11] = Shift(r[11], 2);
        EdgeRecord(3, 5);
        r[11] = Word(r[8]);
        Compare(r[4], r[5]);
        Offset();
        r[11] += r[9];
        Store(r[11] + 8, r[6]);
        r[11] = Word(r[8]);
        ++r[11];
        r[10] = Shift(r[11], 1);
        Store(r[8], r[11]);
        r[11] += r[10];
        r[11] = Shift(r[11], 2);
        EdgeRecord(4, 5);
        r[11] = Word(r[8]);
        Offset();
        r[11] += r[9];
        Store(r[11] + 8, r[6]);
        r[11] = Word(r[8]);
        ++r[11];
        Store(r[8], r[11]);
        r[30] = recovery_abi::ReadU64(m, Address(r[1] - 16));
        r[31] = recovery_abi::ReadU64(m, Address(r[1] - 8));
    }
    void Side() {
        auto &r = s.r;
        r[11] = Word(r[3]);
        Compare(r[11], r[4]);
        if (s.cr6.eq) {
            r[10] = Word(r[3] + 4);
            Compare(r[10], r[5]);
            if (s.cr6.eq) {
                r[3] = 0;
                return;
            }
        }
        Compare(r[11], r[5]);
        if (s.cr6.eq) {
            r[10] = Word(r[3] + 4);
            Compare(r[10], r[4]);
            if (s.cr6.eq) {
                r[3] = 0;
                return;
            }
        }
        Compare(r[11], r[4]);
        if (s.cr6.eq) {
            r[10] = Word(r[3] + 8);
            Compare(r[10], r[5]);
            if (s.cr6.eq) {
                r[3] = 1;
                return;
            }
        }
        Compare(r[11], r[5]);
        if (s.cr6.eq) {
            r[11] = Word(r[3] + 8);
            Compare(r[11], r[4]);
            if (s.cr6.eq) {
                r[3] = 1;
                return;
            }
        }
        r[11] = Word(r[3] + 4);
        Compare(r[11], r[4]);
        if (s.cr6.eq) {
            r[10] = Word(r[3] + 8);
            Compare(r[10], r[5]);
            if (s.cr6.eq) {
                r[3] = 2;
                return;
            }
        }
        Compare(r[11], r[5]);
        if (s.cr6.eq) {
            r[11] = Word(r[3] + 8);
            r[3] = 2;
            Compare(r[11], r[4]);
            if (s.cr6.eq)
                return;
        }
        r[3] = 255;
    }
    void Release() {
        Enter(30, 112);
        auto &r = s.r;
        r[31] = r[3];
        r[30] = Word(r[31] + 4);
        Compare(r[30]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bc3ee8u);
            r[11] = Word(r[3]);
            r[4] = r[30] - 4;
            r[11] = Word(r[11] + 12);
            Call(0x82bc3efcu);
            r[11] = 0;
            Store(r[31] + 4, r[11]);
        }
        Leave(30, 112);
    }
    void ReadTriangles(bool half) {
        auto &r = s.r;
        r[10] = Address(r[11]);
        r[11] = Shift(r[31], 1);
        r[9] = Shift(r[30], 1);
        r[11] += r[31];
        r[9] += r[30];
        r[11] = Shift(r[11], half ? 1 : 2);
        r[9] = Shift(r[9], half ? 1 : 2);
        r[11] += r[10];
        r[10] += r[9];
        auto read = [&](std::uint64_t p) -> std::uint32_t {
            return half ? m.ReadU16(Address(p)) : Word(p);
        };
        unsigned step = half ? 2 : 4;
        r[9] = read(r[11]);
        Store(r[1] + 80, r[9]);
        r[9] = read(r[11] + step);
        Store(r[1] + 84, r[9]);
        r[11] = read(r[11] + 2 * step);
        Store(r[1] + 88, r[11]);
        for (unsigned i = 0; i < 3; ++i) {
            r[11] = read(r[10] + i * step);
            Store(r[1] + 96 + 4 * i, r[11]);
        }
    }
    void Link() {
        Enter(26, 176, 0x82bc3b18u);
        LinkBody();
        Leave(26, 176);
    }
    void LinkBody() {
        auto &r = s.r;
        r[11] = Word(r[8] + 4);
        r[31] = r[3];
        r[30] = r[4];
        r[27] = r[5];
        r[26] = r[6];
        r[28] = r[7];
        Compare(r[11]);
        if (!s.cr6.eq)
            ReadTriangles(false);
        r[11] = Word(r[8] + 8);
        Compare(r[11]);
        if (!s.cr6.eq)
            ReadTriangles(true);
        r[5] = r[26];
        r[4] = r[27];
        r[3] = r[1] + 80;
        Lower(0x82bd9218u, 0x82bc3c00u);
        r[29] = Address(r[3]) & 255u;
        Compare(r[29], 255);
        if (s.cr6.eq) {
            r[11] = 0xffffffff820d0000ull;
            r[5] = 727;
            r[4] = r[11] + 26236;
            r[11] = 0xffffffff820d0000ull;
            r[3] = r[11] + 26352;
            Lower(0x82b9d328u, 0x82bc3c24u);
            return;
        }
        r[5] = r[26];
        r[4] = r[27];
        r[3] = r[1] + 96;
        Lower(0x82bd9218u, 0x82bc3c3cu);
        r[11] = Address(r[3]) & 255u;
        Compare(r[11], 255);
        if (s.cr6.eq) {
            r[11] = 0xffffffff820d0000ull;
            r[5] = 728;
            r[4] = r[11] + 26236;
            r[11] = 0xffffffff820d0000ull;
            r[3] = r[11] + 26280;
            Lower(0x82b9d328u, 0x82bc3c60u);
            return;
        }
        r[9] = Shift(r[29], 30);
        r[10] = Shift(r[11], 30);
        r[7] = r[9] | r[31];
        r[8] = r[10] | r[30];
        r[9] = Shift(r[31], 1);
        r[10] = Shift(r[30], 1);
        r[9] += r[31];
        r[10] += r[30];
        r[9] += r[29];
        r[11] += r[10];
        r[10] = Shift(r[9], 2);
        r[11] = Shift(r[11], 2);
        r[3] = 1;
        Store(r[10] + r[28], r[8]);
        Store(r[11] + r[28], r[7]);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Links l{m, d, s};
    switch (e) {
    case 0x82bc38e8u:
        l.Count();
        return true;
    case 0x82bc3970u:
        l.Emit();
        return true;
    case 0x82bd9218u:
        l.Side();
        return true;
    case 0x82bc3ec0u:
        l.Release();
        return true;
    case 0x82bc3b10u:
        l.Link();
        return true;
    case 0x82bc3cb0u:
        l.SortRecords();
        return true;
    case 0x82bc3f20u:
        l.Build();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_triangle_links61
