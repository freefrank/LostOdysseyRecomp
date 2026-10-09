#include "lo_semantics/mesh_indexed_compact61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_vertex_dedup61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_indexed_compact61 {
namespace {
using recovery_abi::Address;
struct Compact {
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
    void Signed(std::uint64_t a, std::uint64_t b) {
        auto x = std::int32_t(a), y = std::int32_t(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
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
    void Enter(unsigned first, unsigned frame, GuestAddress cont = 0) {
        auto &r = s.r;
        r[12] = s.lr;
        if (cont)
            s.lr = cont;
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= frame;
        Word(r[1], old);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Lower(GuestAddress e, GuestAddress c) {
        s.lr = c;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.sort.guest, s);
            break;
        case 0x82b7a0b0u:
            (void)crt_copy_full_context::Apply(e, m, s);
            break;
        case 0x82b7bc40u:
            crt_reader_chain61::ApplySupport_B7BC40(m, d.sort.accepted, s);
            break;
        case 0x82bd2a08u:
        case 0x82bd2c08u:
            (void)object_sort_support61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82bd2870u:
            (void)reader_buffer_growth61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82bc2d28u:
            mesh_vertex_dedup61::Initialize(m, s);
            break;
        case 0x82bc2dd0u:
        case 0x82bc38e0u:
            (void)mesh_vertex_dedup61::Apply(e, m, d, s);
            break;
        case 0x82bbf7f0u:
            Run();
            break;
        }
    }
    void Call(unsigned reg, GuestAddress c) {
        s.ctr = s.r[reg];
        s.lr = c;
        d.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Free(unsigned reg, bool indirect, GuestAddress a, GuestAddress c) {
        auto &r = s.r;
        Lower(0x82bd0798u, a);
        r[11] = Word(r[3]);
        r[4] = indirect ? Word(r[reg]) : r[reg];
        r[11] = Word(r[11] + 12);
        Call(11, c);
    }
    bool Gather() {
        auto &r = s.r;
        r[23] = 0;
        r[28] = r[23];
        Lower(0x82bd0798u, 0x82bbf824u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[4] = Word(r[26]);
        r[11] = Word(r[11]);
        Call(11, 0x82bbf83cu);
        r[29] = r[3];
        Compare(r[29]);
        if (s.cr6.eq)
            return false;
        r[4] = 0;
        r[5] = Word(r[26]);
        Lower(0x82b7bc40u, 0x82bbf854u);
        r[11] = Word(r[31] + 228);
        r[9] = r[23];
        Compare(r[11]);
        if (s.cr6.gt) {
            r[10] = Word(r[1] + 80);
            r[11] = r[23];
            r[8] = 1;
            do {
                Signed(r[24], 1);
                if (s.cr6.eq) {
                    r[10] = Word(r[31] + 252);
                    r[10] = Word(r[10] + r[11]);
                } else {
                    Signed(r[24], 2);
                    if (s.cr6.eq) {
                        r[10] = Word(r[31] + 252);
                        r[10] += r[11];
                        r[10] = Word(r[10] + 4);
                    } else {
                        Signed(r[24], 4);
                        if (s.cr6.eq) {
                            r[10] = Word(r[31] + 252);
                            r[10] += r[11];
                            r[10] = Word(r[10] + 8);
                        }
                    }
                }
                m.WriteU8(Address(r[10] + r[29]), std::uint8_t(r[8]));
                ++r[9];
                r[7] = Word(r[31] + 228);
                r[11] += 12;
                Compare(r[9], r[7]);
            } while (s.cr6.lt);
        }
        Lower(0x82bd0798u, 0x82bbf8ccu);
        r[11] = Word(r[26]);
        r[5] = 1;
        r[4] = Shift(r[11], 2);
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(11, 0x82bbf8e8u);
        r[30] = r[3];
        Compare(r[30]);
        if (s.cr6.eq) {
            Free(29, false, 0x82bbf944u, 0x82bbf958u);
            return false;
        }
        Lower(0x82bd0798u, 0x82bbf8f8u);
        r[11] = Word(r[26]);
        r[9] = Word(r[3]);
        r[5] = 1;
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[4] = Shift(r[11], 2);
        r[11] = Word(r[9]);
        Call(11, 0x82bbf91cu);
        r[27] = r[3];
        Compare(r[27]);
        if (s.cr6.eq) {
            Free(30, false, 0x82bbf92cu, 0x82bbf940u);
            Free(29, false, 0x82bbf944u, 0x82bbf958u);
            return false;
        }
        r[11] = Word(r[26]);
        r[6] = r[23];
        Compare(r[11]);
        if (s.cr6.gt) {
            r[7] = r[23];
            r[9] = r[27];
            r[8] = r[30];
            do {
                r[11] = m.ReadU8(Address(r[6] + r[29]));
                Compare(r[11]);
                if (!s.cr6.eq) {
                    r[10] = r[28];
                    r[11] = r[9];
                    ++r[28];
                    r[9] += 12;
                    Word(r[8], r[10]);
                    r[10] = Word(r[25]);
                    r[10] += r[7];
                    for (unsigned axis = 0; axis < 3; ++axis) {
                        Load(r[10] + 4 * axis);
                        Store(r[11] + 4 * axis);
                    }
                }
                r[11] = Word(r[26]);
                ++r[6];
                r[8] += 4;
                r[7] += 12;
                Compare(r[6], r[11]);
            } while (s.cr6.lt);
        }
        Free(29, false, 0x82bbf9dcu, 0x82bbf9f0u);
        return true;
    }
    void Remap(bool dedup) {
        auto &r = s.r;
        r[11] = Word(r[31] + 228);
        r[9] = r[23];
        Compare(r[11]);
        if (!s.cr6.gt)
            return;
        r[11] = r[23];
        do {
            unsigned offset = 0;
            bool selected = true;
            Signed(r[24], 1);
            if (!s.cr6.eq) {
                Signed(r[24], 2);
                if (s.cr6.eq)
                    offset = 4;
                else {
                    Signed(r[24], 4);
                    if (s.cr6.eq)
                        offset = 8;
                    else
                        selected = false;
                }
            }
            if (selected) {
                r[10] = Word(r[31] + 252);
                if (dedup)
                    r[7] = Word(r[1] + 96);
                if (offset)
                    r[10] += r[11];
                r[8] = Word(offset ? r[10] + offset : r[10] + r[11]);
                r[8] = Shift(r[8], 2);
                r[8] = Word(r[8] + (dedup ? r[7] : r[30]));
                Word(offset ? r[10] + offset : r[10] + r[11], r[8]);
            }
            r[10] = Word(r[31] + 228);
            ++r[9];
            r[11] += 12;
            Compare(r[9], r[10]);
        } while (s.cr6.lt);
    }
    void DedupCleanup(GuestAddress cont) {
        s.r[3] = s.r[1] + 128;
        Lower(0x82bc38e0u, cont);
    }
    bool RemoveDegenerateFaces() {
        auto &r = s.r;
        r[3] = r[1] + 112;
        Lower(0x82bd2a08u, 0x82bbfc98u);
        r[11] = Word(r[31] + 224);
        r[29] = r[23];
        Compare(r[11]);
        if (s.cr6.gt) {
            r[5] = Word(r[1] + 116);
            r[30] = r[23];
            do {
                r[11] = Word(r[31] + 248);
                r[10] = Word(r[31] + 252);
                r[11] += r[30];
                r[8] = Word(r[11] + 16);
                r[9] = Word(r[11] + 12);
                r[6] = Word(r[11] + 20);
                r[7] = Shift(r[8], 1);
                r[11] = Shift(r[9], 1);
                r[8] += r[7];
                r[11] += r[9];
                r[9] = Shift(r[8], 2);
                r[11] = Shift(r[11], 2);
                r[9] = Word(r[9] + r[10]);
                r[11] = Word(r[11] + r[10]);
                Compare(r[11], r[9]);
                if (!s.cr6.eq) {
                    r[8] = Shift(r[6], 1);
                    r[8] += r[6];
                    r[8] = Shift(r[8], 2);
                    r[10] = Word(r[8] + r[10]);
                    Compare(r[11], r[10]);
                    if (!s.cr6.eq) {
                        Compare(r[9], r[10]);
                        if (!s.cr6.eq) {
                            r[11] = Word(r[1] + 112);
                            Compare(r[5], r[11]);
                            if (s.cr6.eq) {
                                r[4] = 1;
                                r[3] = r[1] + 112;
                                Lower(0x82bd2870u, 0x82bbfd28u);
                                r[5] = Word(r[1] + 116);
                            }
                            r[10] = Word(r[1] + 120);
                            r[11] = Shift(r[5], 2);
                            Word(r[11] + r[10], r[29]);
                            r[11] = Word(r[1] + 116);
                            r[5] = r[11] + 1;
                            Word(r[1] + 116, r[5]);
                        }
                    }
                }
                r[11] = Word(r[31] + 224);
                ++r[29];
                r[30] += 48;
                Compare(r[29], r[11]);
            } while (s.cr6.lt);
        }
        Lower(0x82bd0798u, 0x82bbfd5cu);
        r[11] = Word(r[31] + 224);
        r[9] = Word(r[3]);
        r[5] = 0;
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[4] = Shift(r[11], 4);
        r[11] = Word(r[9]);
        Call(11, 0x82bbfd80u);
        r[30] = r[3];
        Compare(r[30]);
        if (s.cr6.eq) {
            r[3] = r[1] + 112;
            Lower(0x82bd2c08u, 0x82bbfd94u);
            return false;
        }
        r[11] = Word(r[1] + 116);
        r[8] = r[23];
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[9] = r[23];
            r[11] = r[30] + 40;
            do {
                r[10] = Word(r[1] + 120);
                ++r[8];
                r[7] = Word(r[31] + 248);
                r[10] = Word(r[9] + r[10]);
                r[9] += 4;
                r[6] = Shift(r[10], 1);
                r[10] += r[6];
                r[10] = Shift(r[10], 4);
                r[10] += r[7];
                for (unsigned o = 0; o < 32; o += 4) {
                    r[7] = Word(r[10] + o);
                    Word(r[11] + int(o) - 40, r[7]);
                }
                for (unsigned o = 32; o < 44; o += 4) {
                    Load(r[10] + o);
                    Store(r[11] + int(o) - 40);
                }
                r[10] = Word(r[10] + 44);
                Word(r[11] + 4, r[10]);
                r[11] += 48;
                r[10] = Word(r[1] + 116);
                Compare(r[8], r[10]);
            } while (s.cr6.lt);
        }
        r[11] = Word(r[31] + 248);
        Compare(r[11]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bbfe64u);
            r[11] = Word(r[3]);
            r[4] = Word(r[31] + 248);
            r[11] = Word(r[11] + 12);
            Call(11, 0x82bbfe78u);
            Word(r[31] + 248, r[23]);
        }
        r[11] = Word(r[1] + 116);
        r[3] = r[1] + 112;
        Word(r[31] + 248, r[30]);
        Word(r[31] + 224, r[11]);
        Lower(0x82bd2c08u, 0x82bbfe90u);
        return true;
    }
    bool Body() {
        auto &r = s.r;
        r[26] = r[4];
        r[31] = r[3];
        r[25] = r[5];
        r[24] = r[6];
        r[11] = Word(r[26]);
        Compare(r[11]);
        if (s.cr6.eq)
            return true;
        if (!Gather())
            return false;
        Remap(false);
        Free(30, false, 0x82bbfa80u, 0x82bbfa94u);
        r[5] = r[28];
        r[4] = r[27];
        r[3] = r[1] + 128;
        Lower(0x82bc2d28u, 0x82bbfaa4u);
        r[4] = r[1] + 88;
        r[3] = r[1] + 128;
        Lower(0x82bc2dd0u, 0x82bbfab0u);
        r[11] = Word(r[1] + 92);
        Compare(r[11], r[28]);
        if (s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bbfac0u);
            r[11] = Shift(r[28], 1);
            r[10] = Word(r[3]);
            r[5] = 0;
            r[11] += r[28];
            r[30] = Shift(r[11], 2);
            r[11] = Word(r[10]);
            r[4] = r[30];
            Call(11, 0x82bbfae4u);
            r[31] = r[3];
            Compare(r[31]);
            if (s.cr6.eq) {
                DedupCleanup(0x82bbfd9cu);
                return false;
            }
            r[5] = r[30];
            r[4] = r[27];
            Lower(0x82b7a0b0u, 0x82bbfafcu);
            Word(r[26], r[28]);
            r[11] = Word(r[25]);
            Compare(r[11]);
            if (!s.cr6.eq) {
                Free(25, true, 0x82bbfb10u, 0x82bbfb24u);
                Word(r[25], r[23]);
            }
            Word(r[25], r[31]);
            Free(27, false, 0x82bbfb30u, 0x82bbfb44u);
            DedupCleanup(0x82bbfb4cu);
            return true;
        }
        Free(27, false, 0x82bbfb5cu, 0x82bbfb70u);
        Remap(true);
        Lower(0x82bd0798u, 0x82bbfc0cu);
        r[11] = Word(r[1] + 92);
        r[9] = Word(r[3]);
        r[5] = 0;
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[10] = Word(r[9]);
        r[4] = Shift(r[11], 2);
        Call(10, 0x82bbfc30u);
        r[30] = r[3];
        Compare(r[30]);
        if (s.cr6.eq) {
            DedupCleanup(0x82bbfd9cu);
            return false;
        }
        r[11] = Word(r[1] + 92);
        r[4] = Word(r[1] + 88);
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[5] = Shift(r[11], 2);
        Lower(0x82b7a0b0u, 0x82bbfc54u);
        r[11] = Word(r[1] + 92);
        Word(r[26], r[11]);
        r[11] = Word(r[25]);
        Compare(r[11]);
        if (!s.cr6.eq) {
            Free(25, true, 0x82bbfc6cu, 0x82bbfc80u);
            Word(r[25], r[23]);
        }
        Signed(r[24], 1);
        Word(r[25], r[30]);
        if (s.cr6.eq && !RemoveDegenerateFaces()) {
            DedupCleanup(0x82bbfd9cu);
            return false;
        }
        DedupCleanup(0x82bbfe98u);
        return true;
    }
    void Run() {
        Enter(23, 240, 0x82bbf7f8u);
        s.r[3] = Body() ? 1 : 0;
        Leave(23, 240);
    }
    void All() {
        Enter(31, 96);
        auto &r = s.r;
        r[31] = r[3];
        r[11] = m.ReadU8(Address(r[31] + 289));
        Compare(r[11]);
        bool okay = true;
        if (s.cr6.eq) {
            r[6] = 1;
            r[5] = r[31] + 236;
            r[4] = r[31] + 212;
            Lower(0x82bbf7f0u, 0x82bc0088u);
            r[11] = Address(r[3]) & 255u;
            Compare(r[11]);
            okay = !s.cr6.eq;
        }
        if (okay) {
            r[6] = 2;
            r[5] = r[31] + 240;
            r[4] = r[31] + 216;
            r[3] = r[31];
            Lower(0x82bbf7f0u, 0x82bc00c0u);
            r[11] = Address(r[3]) & 255u;
            Compare(r[11]);
            okay = !s.cr6.eq;
        }
        if (okay) {
            r[6] = 4;
            r[5] = r[31] + 244;
            r[4] = r[31] + 220;
            r[3] = r[31];
            Lower(0x82bbf7f0u, 0x82bc00e0u);
            r[11] = Address(r[3]) & 255u;
            r[11] = std::countl_zero(Address(r[11]));
            r[11] = (r[11] >> 5) & 1u;
            r[3] = r[11] ^ 1u;
        } else
            r[3] = 0;
        Leave(31, 96);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Compact c{m, d, s};
    switch (e) {
    case 0x82bbf7f0u:
        c.Run();
        break;
    case 0x82bc0058u:
        c.All();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_indexed_compact61
