#include "lo_semantics/mesh_indexed_workspace61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_sort_float61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
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
