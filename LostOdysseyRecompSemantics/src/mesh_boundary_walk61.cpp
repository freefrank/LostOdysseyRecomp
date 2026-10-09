#include "lo_semantics/mesh_boundary_walk61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_boundary_walk61 {
namespace {
using recovery_abi::Address;
struct Boundary {
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
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.guest, s);
            break;
        case 0x82bd2870u:
            (void)reader_buffer_growth61::Apply(e, m, d, s);
            break;
        case 0x82bd2a28u:
            (void)crt_reader_sort_float61::Apply(e, m, d, s);
            break;
        case 0x82bd2c08u:
            (void)object_sort_support61::Apply(e, m, d, s);
            break;
        case 0x82b7a0b0u:
            (void)crt_copy_full_context::Apply(e, m, s);
            break;
        default:
            (void)mesh_boundary_walk61::Apply(e, m, d, s);
            break;
        }
    }
    void Capacity() {
        Enter(30, 112);
        auto &r = s.r;
        r[31] = r[3];
        r[30] = r[4];
        Lower(0x82bd2a28u, 0x82bd29a8u);
        Compare(r[30]);
        if (s.cr6.eq)
            r[3] = 0;
        else {
            Store(r[31], r[30]);
            Lower(0x82bd0798u, 0x82bd29c0u);
            r[11] = Word(r[31]);
            r[5] = 64;
            r[4] = Shift(r[11], 2);
            r[11] = Word(r[3]);
            r[11] = Word(r[11]);
            s.ctr = r[11];
            s.lr = 0x82bd29dcu;
            d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
            r[11] = std::countl_zero(Address(r[3]));
            Store(r[31] + 8, r[3]);
            r[11] = (Address(r[11]) >> 5) & 1u;
            r[3] = r[11] ^ 1u;
        }
        Leave(30, 112);
    }
    void Copy() {
        Enter(30, 112);
        auto &r = s.r;
        r[11] = 0xffffffff821c0000ull;
        r[31] = r[3];
        r[30] = r[4];
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[0] =
            std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(r[11] - 21900))));
        r[11] = 0;
        Store(r[31] + 12,
              std::bit_cast<std::uint32_t>(float(std::bit_cast<double>(s.fpr_bits[0]))));
        Store(r[31], r[11]);
        Store(r[31] + 4, r[11]);
        Store(r[31] + 8, r[11]);
        r[4] = Word(r[30] + 4);
        Lower(0x82bd2988u, 0x82bd2bd0u);
        r[11] = Word(r[31]);
        r[4] = Word(r[30] + 8);
        r[5] = Shift(r[11], 2);
        r[3] = Word(r[31] + 8);
        Lower(0x82b7a0b0u, 0x82bd2be4u);
        r[11] = Word(r[31]);
        r[3] = r[31];
        Store(r[31] + 4, r[11]);
        Leave(30, 112);
    }
    void Walk() {
        Enter(27, 128, 0x82bb84a0u);
        auto &r = s.r;
        r[30] = r[5];
        r[28] = r[6];
        r[31] = r[3];
        r[29] = r[4];
        r[11] = m.ReadU8(Address(r[30]) + Address(r[28]));
        Compare(r[11]);
        if (s.cr6.eq) {
            r[27] = 1;
            do {
                m.WriteU8(Address(r[30]) + Address(r[28]), std::uint8_t(r[27]));
                r[11] = Word(r[31] + 4);
                r[10] = Word(r[31]);
                Compare(r[11], r[10]);
                if (s.cr6.eq) {
                    r[4] = 1;
                    r[3] = r[31];
                    Lower(0x82bd2870u, 0x82bb84e4u);
                }
                r[11] = Word(r[31] + 4);
                r[10] = Word(r[31] + 8);
                r[11] = Shift(r[11], 2);
                Store(r[11] + r[10], r[30]);
                r[11] = Word(r[31] + 4);
                ++r[11];
                Store(r[31] + 4, r[11]);
                r[11] = Shift(r[30], 1);
                r[11] += r[30];
                r[11] = Shift(r[11], 2);
                r[30] = r[11] + r[29];
                for (unsigned i = 0; i < 2; ++i) {
                    r[11] = Word(r[30] + 4 * i);
                    r[10] = Address(r[11]) & 0x20000000u;
                    Compare(r[10]);
                    if (s.cr6.eq) {
                        r[6] = r[28];
                        r[5] = Address(r[11]) & 0x1fffffffu;
                        r[4] = r[29];
                        r[3] = r[31];
                        Lower(0x82bb8498u, i ? 0x82bb8558u : 0x82bb8534u);
                    }
                }
                r[11] = Word(r[30] + 8);
                r[10] = Address(r[11]) & 0x20000000u;
                Compare(r[10]);
                if (!s.cr6.eq)
                    break;
                r[30] = Address(r[11]) & 0x1fffffffu;
                r[11] = m.ReadU8(Address(r[30]) + Address(r[28]));
                Compare(r[11]);
            } while (s.cr6.eq);
        }
        Leave(27, 128);
    }
    bool FindDuplicate() {
        auto &r = s.r;
        r[31] = Word(r[1] + 100);
        r[6] = 0;
        r[30] = Word(r[1] + 104);
        r[4] = Address(r[31]) >> 1;
        r[7] = r[30] + 8;
        while (true) {
            Compare(r[6], r[4]);
            if (!s.cr6.lt)
                return false;
            r[3] = r[6] + 1;
            r[8] = Word(r[7] - 8);
            r[5] = Word(r[7] - 4);
            r[10] = r[3];
            Compare(r[10], r[4]);
            if (s.cr6.lt) {
                r[11] = r[7];
                do {
                    r[9] = Word(r[11]);
                    Compare(r[9], r[8]);
                    if (s.cr6.eq) {
                        r[29] = Word(r[11] + 4);
                        Compare(r[29], r[5]);
                        if (s.cr6.eq)
                            return true;
                    }
                    r[29] = Word(r[11] + 4);
                    Compare(r[29], r[8]);
                    if (s.cr6.eq) {
                        Compare(r[9], r[5]);
                        if (s.cr6.eq)
                            return true;
                    }
                    ++r[10];
                    r[11] += 8;
                    Compare(r[10], r[4]);
                } while (s.cr6.lt);
            }
            r[6] = r[3];
            r[7] += 8;
        }
    }
    void CancelPair() {
        auto &r = s.r;
        r[9] = r[31] - 1;
        r[11] = Shift(r[10], 3);
        r[10] = Shift(r[6], 3);
        r[8] = r[11] + r[30];
        Store(r[1] + 100, r[9]);
        r[9] = Shift(r[9], 2);
        r[9] = Word(r[9] + r[30]);
        Store(r[8] + 4, r[9]);
        r[9] = Word(r[1] + 100);
        --r[9];
        r[8] = Shift(r[9], 2);
        Store(r[1] + 100, r[9]);
        r[9] = Word(r[1] + 104);
        r[8] = Word(r[8] + r[9]);
        Store(r[11] + r[9], r[8]);
        r[11] = Word(r[1] + 100);
        --r[11];
        r[9] = Shift(r[11], 2);
        Store(r[1] + 100, r[11]);
        r[11] = Word(r[1] + 104);
        r[8] = r[10] + r[11];
        r[11] = Word(r[9] + r[11]);
        Store(r[8] + 4, r[11]);
        r[11] = Word(r[1] + 100);
        --r[11];
        r[9] = Shift(r[11], 2);
        Store(r[1] + 100, r[11]);
        r[11] = Word(r[1] + 104);
        r[9] = Word(r[9] + r[11]);
        Store(r[10] + r[11], r[9]);
    }
    void Reserve(GuestAddress cont) {
        auto &r = s.r;
        r[4] = 1;
        r[3] = r[27];
        Lower(0x82bd2870u, cont);
    }
    void BeginChain() {
        auto &r = s.r;
        r[11] = Word(r[27]);
        r[10] = Word(r[27] + 4);
        r[31] = Word(r[30]);
        r[29] = Word(r[30] + 4);
        Compare(r[10], r[11]);
        if (s.cr6.eq)
            Reserve(0x82bc2b60u);
        r[11] = Word(r[27] + 4);
        r[10] = Word(r[27] + 8);
        r[11] = Shift(r[11], 2);
        Store(r[11] + r[10], r[31]);
        r[11] = Word(r[27] + 4);
        r[10] = Word(r[27]);
        ++r[11];
        Compare(r[11], r[10]);
        Store(r[27] + 4, r[11]);
        if (s.cr6.eq)
            Reserve(0x82bc2b94u);
        r[11] = Word(r[27] + 4);
        r[10] = Word(r[27] + 8);
        r[11] = Shift(r[11], 2);
        Store(r[11] + r[10], r[29]);
        r[11] = Word(r[1] + 100);
        r[10] = Word(r[27] + 4);
        --r[11];
        r[9] = Shift(r[11], 2);
        Store(r[1] + 100, r[11]);
        r[11] = r[10] + 1;
        Store(r[27] + 4, r[11]);
        r[11] = Word(r[1] + 104);
        r[10] = Word(r[9] + r[11]);
        Store(r[11] + 4, r[10]);
        r[11] = Word(r[1] + 100);
        --r[11];
        r[10] = Shift(r[11], 2);
        Store(r[1] + 100, r[11]);
        r[11] = Word(r[1] + 104);
        r[10] = Word(r[10] + r[11]);
        Store(r[11], r[10]);
        r[30] = Word(r[1] + 104);
        r[31] = Word(r[1] + 100);
    }
    void ConsumeEdge() {
        auto &r = s.r;
        r[10] = Word(r[1] + 100);
        r[11] = Shift(r[28], 3);
        r[9] = Word(r[27] + 4);
        --r[10];
        r[8] = Shift(r[10], 2);
        Store(r[1] + 100, r[10]);
        r[10] = r[9] + 1;
        Store(r[27] + 4, r[10]);
        r[10] = Word(r[1] + 104);
        r[9] = Word(r[8] + r[10]);
        r[10] += r[11];
        Store(r[10] + 4, r[9]);
        r[10] = Word(r[1] + 100);
        --r[10];
        r[9] = Shift(r[10], 2);
        Store(r[1] + 100, r[10]);
        r[10] = Word(r[1] + 104);
        r[9] = Word(r[9] + r[10]);
        Store(r[11] + r[10], r[9]);
        r[11] = Word(r[1] + 100);
        r[10] = Address(r[11]) >> 1;
        Compare(r[10]);
    }
    bool ContinueChain() {
        auto &r = s.r;
        r[10] = Address(r[31]) >> 1;
        Compare(r[10]);
        if (s.cr6.eq)
            return true;
        while (true) {
            r[28] = 0;
            r[11] = r[30];
            while (true) {
                Compare(r[28], r[10]);
                if (!s.cr6.lt)
                    return false;
                r[31] = Word(r[11]);
                r[30] = Word(r[11] + 4);
                Compare(r[31], r[29]);
                if (s.cr6.eq)
                    break;
                Compare(r[30], r[29]);
                if (s.cr6.eq)
                    break;
                ++r[28];
                r[11] += 8;
            }
            bool forward = Address(r[31]) == Address(r[29]);
            r[11] = Word(r[27]);
            r[10] = Word(r[27] + 4);
            Compare(r[10], r[11]);
            if (s.cr6.eq)
                Reserve(forward ? 0x82bc2c5cu : 0x82bc2c90u);
            r[10] = Word(r[27] + 4);
            r[29] = forward ? r[30] : r[31];
            r[9] = Word(r[27] + 8);
            r[10] = Shift(r[10], 2);
            Store(r[10] + r[9], forward ? r[30] : r[31]);
            ConsumeEdge();
            if (s.cr6.eq)
                return true;
            r[30] = Word(r[1] + 104);
        }
    }
    void Chain() {
        Enter(27, 160, 0x82bc2a20u);
        auto &r = s.r;
        r[27] = r[3];
        r[3] = r[1] + 96;
        Lower(0x82bd2b90u, 0x82bc2a30u);
        while (FindDuplicate())
            CancelPair();
        Compare(r[31], 2);
        bool enough = !s.cr6.lt;
        if (enough) {
            Compare(r[30]);
            enough = !s.cr6.eq;
        }
        if (enough)
            BeginChain();
        else
            r[29] = Word(r[1] + 80);
        bool ok = ContinueChain();
        r[3] = r[1] + 96;
        Lower(0x82bd2c08u, ok ? 0x82bc2d08u : 0x82bc2d1cu);
        r[3] = ok ? 1 : 0;
        Leave(27, 160);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Boundary b{m, d, s};
    switch (e) {
    case 0x82bd2988u:
        b.Capacity();
        return true;
    case 0x82bd2b90u:
        b.Copy();
        return true;
    case 0x82bb8498u:
        b.Walk();
        return true;
    case 0x82bc2a18u:
        b.Chain();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_boundary_walk61
