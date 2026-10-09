#include "lo_semantics/mesh_indexed_workspace61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_sort_float61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_indexed_workspace61 {
namespace {
using recovery_abi::Address;
struct Workspace {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
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
        case 0x82bd2a28u:
            (void)crt_reader_sort_float61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82b7a0b0u:
            (void)crt_copy_full_context::Apply(e, m, s);
            break;
        case 0x82b7bc40u:
            crt_reader_chain61::ApplySupport_B7BC40(m, d.sort.accepted, s);
            break;
        case 0x82bbe070u:
            Reset();
            break;
        case 0x82bbe278u:
            CopyChannel();
            break;
        }
    }
    void Call(GuestAddress c) {
        s.ctr = s.r[11];
        s.lr = c;
        d.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Initialize() {
        Enter(31, 96);
        auto &r = s.r;
        r[31] = r[3];
        for (unsigned i = 0; i < 13; ++i) {
            if (i)
                r[3] = r[31] + 16 * i;
            Lower(0x82bd2a08u, 0x82bbdf78u + 8 * i);
        }
        r[11] = 0;
        r[10] = 1;
        r[3] = r[31];
        for (unsigned o : {208u, 212u, 216u, 220u, 224u, 228u, 232u, 236u, 240u, 244u, 248u, 252u,
                           264u, 268u, 272u, 256u, 276u, 260u})
            Word(r[31] + o, r[11]);
        for (unsigned o = 280; o <= 290; ++o)
            m.WriteU8(Address(r[31] + o), std::uint8_t((o == 280 || o == 285) ? r[10] : r[11]));
        Leave(31, 96);
    }
    void Reset() {
        Enter(30, 112);
        auto &r = s.r;
        r[31] = r[3];
        for (unsigned i = 0; i < 13; ++i) {
            if (i)
                r[3] = r[31] + 16 * i;
            Lower(0x82bd2a28u, 0x82bbe08cu + 8 * i);
        }
        constexpr unsigned offsets[]{236, 240, 244, 248, 252, 264, 268, 272, 256};
        for (unsigned i = 0; i < 9; ++i) {
            r[11] = Word(r[31] + offsets[i]);
            if (!i)
                r[30] = 0;
            Compare(r[11]);
            if (!s.cr6.eq) {
                Lower(0x82bd0798u, 0x82bbe100u + 40 * i);
                r[11] = Word(r[3]);
                r[4] = Word(r[31] + offsets[i]);
                r[11] = Word(r[11] + 12);
                Call(0x82bbe114u + 40 * i);
                Word(r[31] + offsets[i], r[30]);
            }
        }
        r[3] = r[31];
        Leave(30, 112);
    }
    void Destroy() {
        Enter(31, 96);
        auto &r = s.r;
        r[31] = r[3];
        Lower(0x82bbe070u, 0x82bbf5a8u);
        for (unsigned i = 0; i < 13; ++i) {
            r[3] = r[31] + 16 * (12 - i);
            Lower(0x82bd2c08u, 0x82bbf5b0u + 8 * i);
        }
        Leave(31, 96);
    }
    void CopyChannel() {
        Enter(29, 112, 0x82bbe280u);
        auto &r = s.r;
        r[31] = r[3];
        r[29] = r[4];
        r[30] = r[5];
        Compare(r[31]);
        Word(r[6], r[31]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bbe2a0u);
            r[11] = Address(r[31]) << 1;
            r[10] = Word(r[3]);
            r[5] = 0;
            r[11] += r[31];
            r[31] = Address(r[11]) << 2;
            r[11] = Word(r[10]);
            r[4] = r[31];
            Call(0x82bbe2c4u);
            Compare(r[3]);
            Word(r[30], r[3]);
            if (s.cr6.eq) {
                r[3] = 0;
                Leave(29, 112);
                return;
            }
            Compare(r[29]);
            r[5] = r[31];
            if (!s.cr6.eq) {
                r[4] = r[29];
                Lower(0x82b7a0b0u, 0x82bbe2f0u);
            } else {
                r[4] = 0;
                Lower(0x82b7bc40u, 0x82bbe304u);
            }
        }
        r[3] = 1;
        Leave(29, 112);
    }
    bool Success() {
        s.r[11] = Address(s.r[3]) & 255u;
        Compare(s.r[11]);
        return !s.cr6.eq;
    }
    bool ConfigureBody() {
        auto &r = s.r;
        r[31] = r[3];
        r[30] = r[4];
        Lower(0x82bbe070u, 0x82bbf640u);
        r[11] = m.ReadU8(Address(r[30] + 28));
        r[6] = r[31] + 212;
        r[5] = r[31] + 236;
        m.WriteU8(Address(r[31] + 280), std::uint8_t(r[11]));
        for (unsigned i = 1; i < 12; ++i) {
            r[11] = m.ReadU8(Address(r[30] + 28 + i));
            m.WriteU8(Address(r[31] + 280 + i), std::uint8_t(r[11]));
        }
        r[4] = Word(r[30] + 16);
        r[3] = Word(r[30]);
        Lower(0x82bbe278u, 0x82bbf6b4u);
        if (!Success())
            return false;
        r[29] = r[31] + 216;
        r[4] = Word(r[30] + 20);
        r[28] = r[31] + 240;
        r[3] = Word(r[30] + 8);
        r[6] = r[29];
        r[5] = r[28];
        Lower(0x82bbe278u, 0x82bbf6e8u);
        if (!Success())
            return false;
        r[6] = r[31] + 220;
        r[4] = Word(r[30] + 24);
        r[5] = r[31] + 244;
        r[3] = Word(r[30] + 12);
        Lower(0x82bbe278u, 0x82bbf708u);
        if (!Success())
            return false;
        r[11] = m.ReadU8(Address(r[31] + 281));
        Compare(r[11]);
        if (s.cr6.eq) {
            r[11] = Word(r[28]);
            Compare(r[11]);
            if (!s.cr6.eq) {
                r[11] = Word(r[29]);
                r[10] = 0;
                Compare(r[11]);
                if (s.cr6.gt) {
                    r[9] = 0xffffffff82000000ull;
                    r[11] = 0;
                    if (s.cached_fp_control & 0x8040u) {
                        s.cached_fp_control &= ~0x8040u;
                        d.fp.SetHostFpControl(s.cached_fp_control);
                    }
                    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(
                        double(std::bit_cast<float>(Word(r[9] + 3664))));
                    do {
                        r[9] = Word(r[28]);
                        ++r[10];
                        r[9] += r[11];
                        r[11] += 12;
                        Word(r[9] + 8, std::bit_cast<std::uint32_t>(
                                           float(std::bit_cast<double>(s.fpr_bits[0]))));
                        r[9] = Word(r[29]);
                        Compare(r[10], r[9]);
                    } while (s.cr6.lt);
                }
            }
        }
        r[11] = Word(r[30] + 4);
        Compare(r[11]);
        Word(r[31] + 208, r[11]);
        if (s.cr6.eq)
            return false;
        Lower(0x82bd0798u, 0x82bbf77cu);
        r[11] = Word(r[31] + 208);
        r[9] = Word(r[3]);
        r[5] = 0;
        r[10] = Address(r[11]) << 1;
        r[11] += r[10];
        r[4] = Address(r[11]) << 4;
        r[11] = Word(r[9]);
        Call(0x82bbf7a0u);
        Compare(r[3]);
        Word(r[31] + 248, r[3]);
        if (s.cr6.eq)
            return false;
        Lower(0x82bd0798u, 0x82bbf7b0u);
        r[11] = Word(r[31] + 208);
        r[9] = Word(r[3]);
        r[5] = 0;
        r[10] = Address(r[11]) << 3;
        r[11] += r[10];
        r[4] = Address(r[11]) << 2;
        r[11] = Word(r[9]);
        Call(0x82bbf7d4u);
        r[11] = std::countl_zero(Address(r[3]));
        Word(r[31] + 252, r[3]);
        r[11] = (r[11] >> 5) & 1u;
        r[3] = r[11] ^ 1u;
        return r[3] != 0;
    }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(v)));
    }
    void Load(unsigned i, std::uint64_t p) {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    static std::uint32_t Shift(std::uint64_t v, unsigned n) { return Address(v) << n; }
    // Returns true for faces the original deliberately skips (duplicate indices
    // or exactly zero area), without consuming a face/corner slot.
    bool Degenerate() {
        auto &r = s.r;
        r[10] = Address(r[10]);
        r[9] = Word(r[10]);
        r[10] = Word(r[10] + 4);
        Compare(r[9], r[10]);
        if (s.cr6.eq)
            return true;
        r[10] = Word(r[4] + 12);
        r[9] = Word(r[10]);
        r[10] = Word(r[10] + 8);
        Compare(r[9], r[10]);
        if (s.cr6.eq)
            return true;
        r[10] = Word(r[4] + 12);
        r[9] = Word(r[10] + 4);
        r[10] = Word(r[10] + 8);
        Compare(r[9], r[10]);
        if (s.cr6.eq)
            return true;
        r[10] = Word(r[4] + 12);
        r[9] = Word(r[11] + 236);
        r[8] = Word(r[10]);
        r[7] = Word(r[10] + 4);
        r[6] = Word(r[10] + 8);
        r[10] = Shift(r[8], 1);
        r[5] = Shift(r[7], 1);
        r[10] += r[8];
        r[8] = Shift(r[6], 1);
        r[7] += r[5];
        r[6] += r[8];
        r[10] = Shift(r[10], 2);
        r[8] = Shift(r[7], 2);
        r[7] = Shift(r[6], 2);
        r[10] += r[9];
        r[8] += r[9];
        r[9] += r[7];
        Load(0, r[10]);
        Load(12, r[10] + 8);
        Load(11, r[9]);
        Load(8, r[8]);
        Single(11, F(0) - F(11));
        Load(9, r[9] + 8);
        Single(0, F(0) - F(8));
        Single(9, F(12) - F(9));
        Load(13, r[10] + 4);
        Load(6, r[8] + 8);
        r[10] = 0xffffffff82000000ull;
        Load(10, r[9] + 4);
        Single(12, F(12) - F(6));
        Load(7, r[8] + 4);
        Single(10, F(13) - F(10));
        Single(13, F(13) - F(7));
        Single(8, F(0) * F(9));
        Single(6, F(12) * F(10));
        Single(7, F(11) * F(13));
        Single(12, F(11) * F(12) - F(8));
        Single(13, F(13) * F(9) - F(6));
        Single(0, F(0) * F(10) - F(7));
        Single(12, F(12) * F(12));
        Single(0, F(0) * F(0) + F(12));
        Single(13, F(13) * F(13) + F(0));
        Load(0, r[10] + 3664);
        auto a = F(13), b = F(0);
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
        return s.cr6.eq;
    }
    void CornerInput(unsigned channel, unsigned corner) {
        auto &r = s.r;
        r[10] = Word(r[4] + 12 + 4 * channel);
        Compare(r[10]);
        unsigned value = channel ? 7 : 8;
        if (!s.cr6.eq) {
            if (corner == 0)
                r[value] = Word(r[10]);
            else {
                r[9] = Address(r[6]) & 255u;
                if (corner == 1)
                    ++r[9];
                else {
                    s.xer_ca = Address(r[9]) <= 2;
                    r[9] = 2 - r[9];
                }
                r[9] = Shift(r[9], 2);
                r[value] = Word(r[9] + r[10]);
            }
        } else
            r[value] = r[5];
        r[10] = Word(r[11] + 228);
        if (channel == 0) {
            r[7] = Word(r[11] + 252);
            if (corner)
                r[10] += corner;
            r[9] = Shift(r[10], 1);
            r[10] += r[9];
            r[10] = Shift(r[10], 2);
            Word(r[10] + r[7], r[8]);
        } else {
            r[9] = Word(r[11] + 252);
            r[8] = Shift(r[10], 1);
            r[10] += r[8];
            r[10] = Shift(r[10], 2);
            r[10] += r[9];
            Word(r[10] + 12 * corner + 4 * channel, r[7]);
        }
    }
    void ClampChannel(unsigned channel) {
        auto &r = s.r;
        r[10] = Word(r[4] + 12 + 4 * channel);
        Compare(r[10]);
        if (s.cr6.eq)
            return;
        for (unsigned corner = 0; corner < 3; ++corner) {
            r[10] = Word(r[11] + 228);
            r[9] = Word(r[11] + 252);
            if (channel == 0) {
                if (corner)
                    r[10] += corner;
                if (corner == 0) {
                    r[8] = Shift(r[10], 1);
                    r[6] = Word(r[11] + 212);
                } else {
                    r[6] = Word(r[11] + 212);
                    r[8] = Shift(r[10], 1);
                }
                r[10] += r[8];
                r[10] = Shift(r[10], 2);
                r[8] = Word(r[10] + r[9]);
                Compare(r[8], r[6]);
                if (!s.cr6.lt)
                    Word(r[10] + r[9], r[7]);
            } else {
                r[8] = Shift(r[10], 1);
                r[6] = Word(r[11] + 212 + 4 * channel);
                r[10] += r[8];
                r[10] = Shift(r[10], 2);
                r[10] += r[9];
                r[9] = Word(r[10] + 12 * corner + 4 * channel);
                Compare(r[9], r[6]);
                if (!s.cr6.lt)
                    Word(r[10] + 12 * corner + 4 * channel, r[7]);
            }
        }
    }
    bool InsertBody() {
        auto &r = s.r;
        r[11] = r[3];
        r[31] = Word(r[11] + 248);
        Compare(r[31]);
        if (s.cr6.eq)
            return false;
        r[10] = Word(r[11] + 252);
        Compare(r[10]);
        if (s.cr6.eq)
            return false;
        r[10] = Word(r[11] + 208);
        r[3] = Word(r[11] + 224);
        Compare(r[3], r[10]);
        if (s.cr6.eq)
            return false;
        r[9] = Word(r[4]);
        Compare(r[9], r[10]);
        if (s.cr6.gt)
            return false;
        r[10] = m.ReadU8(Address(r[11] + 280));
        Compare(r[10]);
        if (!s.cr6.eq) {
            r[10] = Word(r[4] + 12);
            Compare(r[10]);
            if (!s.cr6.eq && Degenerate())
                return true;
        }
        r[10] = Shift(r[3], 1);
        r[9] = Word(r[4] + 4);
        r[10] += r[3];
        r[10] = Shift(r[10], 4);
        r[10] += r[31];
        Word(r[10] + 24, r[9]);
        r[10] = m.ReadU8(Address(r[11] + 282));
        Compare(r[10]);
        r[7] = s.cr6.eq ? 1 : Word(r[4] + 8);
        r[10] = Word(r[11] + 224);
        r[5] = ~std::uint64_t(0);
        r[9] = Word(r[11] + 248);
        r[8] = Shift(r[10], 1);
        r[10] += r[8];
        r[10] = Shift(r[10], 4);
        r[10] += r[9];
        Word(r[10] + 28, r[7]);
        r[10] = Word(r[11] + 224);
        r[9] = Word(r[11] + 248);
        r[8] = Shift(r[10], 1);
        r[7] = Word(r[4]);
        r[10] += r[8];
        r[10] = Shift(r[10], 4);
        r[10] += r[9];
        Word(r[10] + 44, r[7]);
        r[9] = m.ReadU8(Address(r[4] + 24));
        r[10] = Word(r[4] + 12);
        r[9] = std::countl_zero(Address(r[9]));
        Compare(r[10]);
        r[9] = (r[9] >> 5) & 1u;
        r[6] = r[9] ^ 1u;
        // First position was already loaded/tested by the guest, unlike later channels.
        if (!s.cr6.eq)
            r[8] = Word(r[10]);
        else
            r[8] = r[5];
        r[10] = Word(r[11] + 228);
        r[7] = Word(r[11] + 252);
        r[9] = Shift(r[10], 1);
        r[10] += r[9];
        r[10] = Shift(r[10], 2);
        Word(r[10] + r[7], r[8]);
        CornerInput(0, 1);
        CornerInput(0, 2);
        for (unsigned channel = 1; channel < 3; ++channel)
            for (unsigned corner = 0; corner < 3; ++corner)
                CornerInput(channel, corner);
        r[7] = 0;
        for (unsigned channel = 0; channel < 3; ++channel)
            ClampChannel(channel);
        r[10] = Word(r[11] + 224);
        r[3] = 1;
        r[9] = Word(r[11] + 248);
        r[8] = Shift(r[10], 1);
        r[7] = Word(r[11] + 228);
        r[10] += r[8];
        r[10] = Shift(r[10], 4);
        r[10] += r[9];
        Word(r[10] + 12, r[7]);
        for (unsigned corner = 1; corner < 3; ++corner) {
            r[10] = Word(r[11] + 224);
            r[9] = Word(r[11] + 228);
            r[7] = Shift(r[10], 1);
            r[8] = Word(r[11] + 248);
            ++r[9];
            r[10] += r[7];
            r[10] = Shift(r[10], 4);
            r[10] += r[8];
            Word(r[11] + 228, r[9]);
            Word(r[10] + 12 + 4 * corner, r[9]);
        }
        r[9] = Word(r[11] + 228);
        r[10] = Word(r[11] + 224);
        ++r[9];
        ++r[10];
        Word(r[11] + 228, r[9]);
        Word(r[11] + 224, r[10]);
        return true;
    }
    void Insert() {
        recovery_abi::WriteU64(m, Address(s.r[1] - 8), s.r[31]);
        s.r[3] = InsertBody() ? 1 : 0;
        s.r[31] = recovery_abi::ReadU64(m, Address(s.r[1] - 8));
    }
    void Configure() {
        Enter(28, 128, 0x82bbf630u);
        if (!ConfigureBody())
            s.r[3] = 0;
        Leave(28, 128);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Workspace x{m, d, s};
    switch (e) {
    case 0x82bbe310u:
        x.Insert();
        break;
    case 0x82bbdf60u:
        x.Initialize();
        break;
    case 0x82bbe070u:
        x.Reset();
        break;
    case 0x82bbe278u:
        x.CopyChannel();
        break;
    case 0x82bbf590u:
        x.Destroy();
        break;
    case 0x82bbf628u:
        x.Configure();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_indexed_workspace61
