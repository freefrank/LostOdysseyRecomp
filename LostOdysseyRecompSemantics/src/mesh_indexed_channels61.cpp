#include "lo_semantics/mesh_indexed_channels61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/mesh_vertex_dedup61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_indexed_channels61 {
namespace {
using recovery_abi::Address;
struct Channels {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
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
        Word(r[1] - 8, r[12]);
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        auto old = r[1];
        r[1] -= frame;
        Word(r[1], old);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
    }
    void Load(std::uint64_t p) {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Store(std::uint64_t p) {
        Word(p, std::bit_cast<std::uint32_t>(float(std::bit_cast<double>(s.fpr_bits[0]))));
    }
    void Lower(GuestAddress e, GuestAddress c) {
        s.lr = c;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.sort.guest, s);
            break;
        case 0x82bd2a08u:
        case 0x82bd2c08u:
            (void)object_sort_support61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82b7a0b0u:
            (void)crt_copy_full_context::Apply(e, m, s);
            break;
        case 0x82bd2870u:
            (void)reader_buffer_growth61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82bb3c00u:
            Append();
            break;
        case 0x82bc2d28u:
            mesh_vertex_dedup61::Initialize(m, s);
            break;
        case 0x82bc2dd0u:
        case 0x82bc38e0u:
            (void)mesh_vertex_dedup61::Apply(e, m, d, s);
            break;
        }
    }
    void Call(unsigned reg, GuestAddress c) {
        s.ctr = s.r[reg];
        s.lr = c;
        d.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Append() {
        Enter(30, 112);
        auto &r = s.r;
        r[31] = r[3];
        r[30] = r[4];
        r[11] = Word(r[31]);
        r[10] = Word(r[31] + 4);
        Load(r[30]);
        Store(r[1] + 80);
        Compare(r[10], r[11]);
        for (unsigned axis = 0; axis < 3; ++axis) {
            if (s.cr6.eq) {
                r[4] = 1;
                if (axis)
                    r[3] = r[31];
                Lower(0x82bd2870u, 0x82bb3c3cu + 64 * axis);
            }
            r[11] = Word(r[31] + 4);
            if (axis == 2)
                r[3] = r[31];
            r[10] = Word(r[1] + 80);
            r[9] = Word(r[31] + 8);
            r[11] = Shift(r[11], 2);
            Word(r[11] + r[9], r[10]);
            r[11] = Word(r[31] + 4);
            if (axis < 2)
                r[10] = Word(r[31]);
            ++r[11];
            if (axis < 2)
                Compare(r[11], r[10]);
            Word(r[31] + 4, r[11]);
            if (axis < 2) {
                Load(r[30] + 4 * (axis + 1));
                Store(r[1] + 80);
            }
        }
        Leave(30, 112);
    }
    void AppendFaceCorner(unsigned corner) {
        auto &r = s.r;
        r[11] = Word(r[31] + 248);
        if (corner == 0) {
            r[11] += r[30];
            r[11] = Word(r[11] + 12);
            r[3] = r[1] + 80;
            r[8] = Word(r[31] + 252);
            r[9] = Shift(r[11], 1);
            r[10] = Word(r[31] + 236);
        } else {
            r[8] = Word(r[31] + 252);
            r[3] = r[1] + 80;
            r[11] += r[30];
            r[10] = Word(r[31] + 236);
            r[11] = Word(r[11] + 12 + 4 * corner);
            r[9] = Shift(r[11], 1);
        }
        r[11] += r[9];
        r[11] = Shift(r[11], 2);
        r[11] = Word(r[11] + r[8]);
        r[9] = Shift(r[11], 1);
        r[11] += r[9];
        r[11] = Shift(r[11], 2);
        r[4] = r[11] + r[10];
        Lower(0x82bb3c00u, 0x82bbe9dcu + 60 * corner);
    }
    bool SplitBody() {
        auto &r = s.r;
        r[31] = r[3];
        r[11] = m.ReadU8(Address(r[31] + 282));
        Compare(r[11]);
        if (s.cr6.eq)
            return true;
        r[3] = r[1] + 80;
        r[29] = Word(r[31] + 212);
        Lower(0x82bd2a08u, 0x82bbe970u);
        r[11] = Word(r[31] + 224);
        r[28] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[30] = 0;
            r[27] = ~std::uint64_t(0);
            do {
                r[11] = Word(r[31] + 248);
                r[11] += r[30];
                r[10] = Word(r[11] + 28);
                Compare(r[10]);
                if (s.cr6.eq) {
                    r[10] = m.ReadU8(Address(r[31] + 289));
                    Compare(r[10]);
                    if (s.cr6.eq) {
                        for (unsigned corner = 0; corner < 3; ++corner)
                            AppendFaceCorner(corner);
                        r[11] = Word(r[31] + 248);
                        r[8] = Word(r[31] + 252);
                        r[10] = r[30] + r[11];
                        r[11] = r[29] + 1;
                        r[7] = r[11];
                        ++r[11];
                        r[10] = Word(r[10] + 12);
                        r[9] = Shift(r[10], 1);
                        r[10] += r[9];
                        r[10] = Shift(r[10], 2);
                        Word(r[10] + r[8], r[29]);
                        r[29] = r[11] + 1;
                        r[10] = Word(r[31] + 248);
                        r[8] = Word(r[31] + 252);
                        r[10] += r[30];
                        r[10] = Word(r[10] + 16);
                        r[9] = Shift(r[10], 1);
                        r[10] += r[9];
                        r[10] = Shift(r[10], 2);
                        Word(r[10] + r[8], r[7]);
                        r[10] = Word(r[31] + 248);
                        r[10] += r[30];
                        r[10] = Word(r[10] + 20);
                        r[9] = Shift(r[10], 1);
                        r[10] += r[9];
                        r[9] = Word(r[31] + 252);
                        r[10] = Shift(r[10], 2);
                        Word(r[10] + r[9], r[11]);
                    }
                    r[11] = Word(r[31] + 248);
                    r[11] += r[30];
                    Word(r[11] + 28, r[27]);
                }
                r[11] = Word(r[31] + 224);
                ++r[28];
                r[30] += 48;
                Compare(r[28], r[11]);
            } while (s.cr6.lt);
        }
        r[11] = 0xffffffffaaaa0000ull;
        r[10] = Word(r[1] + 84);
        r[11] |= 43691;
        r[11] = (std::uint64_t(Address(r[10])) * Address(r[11])) >> 32;
        r[29] = (Address(r[11]) >> 1) & 0x7fffffffu;
        Compare(r[29]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bbeb04u);
            r[11] = Word(r[31] + 212);
            r[9] = Word(r[3]);
            r[5] = 0;
            r[11] += r[29];
            r[10] = Shift(r[11], 1);
            r[11] += r[10];
            r[10] = Word(r[9]);
            r[4] = Shift(r[11], 2);
            Call(10, 0x82bbeb2cu);
            r[30] = r[3];
            Compare(r[30]);
            if (s.cr6.eq) {
                r[3] = r[1] + 80;
                Lower(0x82bd2c08u, 0x82bbeb40u);
                return false;
            }
            r[11] = Word(r[31] + 212);
            r[3] = r[30];
            r[4] = Word(r[31] + 236);
            r[10] = Shift(r[11], 1);
            r[11] += r[10];
            r[5] = Shift(r[11], 2);
            Lower(0x82b7a0b0u, 0x82bbeb68u);
            r[11] = Word(r[31] + 212);
            r[10] = Shift(r[29], 1);
            r[4] = Word(r[1] + 88);
            r[9] = Shift(r[11], 1);
            r[10] += r[29];
            r[11] += r[9];
            r[5] = Shift(r[10], 2);
            r[11] = Shift(r[11], 2);
            r[3] = r[11] + r[30];
            Lower(0x82b7a0b0u, 0x82bbeb90u);
            r[11] = Word(r[31] + 236);
            Compare(r[11]);
            if (!s.cr6.eq) {
                Lower(0x82bd0798u, 0x82bbeba0u);
                r[11] = Word(r[3]);
                r[4] = Word(r[31] + 236);
                r[11] = Word(r[11] + 12);
                Call(11, 0x82bbebb4u);
                r[11] = 0;
                Word(r[31] + 236, r[11]);
            }
            r[11] = Word(r[31] + 212);
            Word(r[31] + 236, r[30]);
            r[11] += r[29];
            Word(r[31] + 212, r[11]);
        }
        r[3] = r[1] + 80;
        Lower(0x82bd2c08u, 0x82bbebd4u);
        return true;
    }
    void Split() {
        Enter(27, 144, 0x82bbe950u);
        s.r[3] = SplitBody() ? 1 : 0;
        Leave(27, 144);
    }
    void Deduplicate() {
        Enter(28, 160, 0x82bbebe8u);
        auto &r = s.r;
        r[31] = r[3];
        r[3] = r[1] + 96;
        r[5] = Word(r[31] + 228);
        r[4] = Word(r[31] + 252);
        Lower(0x82bc2d28u, 0x82bbec00u);
        r[4] = r[1] + 80;
        r[3] = r[1] + 96;
        Lower(0x82bc2dd0u, 0x82bbec0cu);
        r[11] = Word(r[31] + 224);
        r[9] = Word(r[1] + 88);
        r[10] = 0;
        r[29] = Word(r[1] + 84);
        Compare(r[11]);
        r[28] = Word(r[1] + 80);
        if (s.cr6.gt) {
            r[11] = 0;
            do {
                r[8] = Word(r[31] + 248);
                ++r[10];
                r[8] += r[11];
                r[7] = Word(r[8] + 12);
                r[7] = Shift(r[7], 2);
                r[7] = Word(r[7] + r[9]);
                Word(r[8] + 12, r[7]);
                r[8] = Word(r[31] + 248);
                r[8] += r[11];
                r[7] = Word(r[8] + 16);
                r[7] = Shift(r[7], 2);
                r[7] = Word(r[7] + r[9]);
                Word(r[8] + 16, r[7]);
                r[8] = Word(r[31] + 248);
                r[8] += r[11];
                r[11] += 48;
                r[7] = Word(r[8] + 20);
                r[7] = Shift(r[7], 2);
                r[7] = Word(r[7] + r[9]);
                Word(r[8] + 20, r[7]);
                r[8] = Word(r[31] + 224);
                Compare(r[10], r[8]);
            } while (s.cr6.lt);
        }
        r[11] = Word(r[31] + 252);
        Compare(r[11]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bbec98u);
            r[11] = Word(r[3]);
            r[4] = Word(r[31] + 252);
            r[11] = Word(r[11] + 12);
            Call(11, 0x82bbecacu);
            r[11] = 0;
            Word(r[31] + 252, r[11]);
        }
        Lower(0x82bd0798u, 0x82bbecb8u);
        r[11] = Shift(r[29], 1);
        r[10] = Word(r[3]);
        r[5] = 0;
        r[11] += r[29];
        r[30] = Shift(r[11], 2);
        r[11] = Word(r[10]);
        r[4] = r[30];
        Call(11, 0x82bbecdcu);
        Compare(r[3]);
        Word(r[31] + 252, r[3]);
        if (s.cr6.eq) {
            r[3] = r[1] + 96;
            Lower(0x82bc38e0u, 0x82bbecf0u);
            r[3] = 0;
        } else {
            r[5] = r[30];
            r[4] = r[28];
            Lower(0x82b7a0b0u, 0x82bbed08u);
            r[3] = r[1] + 96;
            Word(r[31] + 228, r[29]);
            Lower(0x82bc38e0u, 0x82bbed14u);
            r[3] = 1;
        }
        Leave(28, 160);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Channels c{m, d, s};
    switch (e) {
    case 0x82bb3c00u:
        c.Append();
        break;
    case 0x82bbe948u:
        c.Split();
        break;
    case 0x82bbebe0u:
        c.Deduplicate();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_indexed_channels61
