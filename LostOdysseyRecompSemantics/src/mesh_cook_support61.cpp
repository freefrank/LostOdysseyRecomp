#include "lo_semantics/mesh_cook_support61.h"
#include "lo_semantics/cube_projection_table61.h"
#include "lo_semantics/mesh_support_stream61.h"
#include "lo_semantics/pointer_fields.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::mesh_cook_support61 {
namespace {
using recovery_abi::Address;
struct Support {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t v, std::uint32_t other = 0) {
        auto x = Address(v);
        s.cr6 = {std::uint8_t(x < other), std::uint8_t(x > other), std::uint8_t(x == other),
                 s.xer_so};
    }
    void Enter(unsigned first) {
        auto &r = s.r;
        r[12] = s.lr;
        Store(r[1] - 8, r[12]);
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        auto old = r[1];
        r[1] -= 112;
        Store(r[1], old);
    }
    void Leave(unsigned first) {
        auto &r = s.r;
        r[1] += 112;
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
    }
    void Call(GuestAddress continuation) {
        s.ctr = s.r[11];
        s.lr = continuation;
        d.tree.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Allocator(GuestAddress continuation) {
        s.lr = continuation;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m, d.tree.guest, s);
    }
    void Free(unsigned offset, GuestAddress allocator, GuestAddress invoke) {
        Allocator(allocator);
        s.r[11] = Word(s.r[3]);
        s.r[4] = Word(s.r[31] + offset);
        s.r[11] = Word(s.r[11] + 12);
        Call(invoke);
    }
    void Base(bool initialize) {
        // Bridge the existing pointer-fields family; no new entry credit.
        PointerFieldRegisters fields{};
        fields.r3 = s.r[3];
        const ConstantFieldAssignment assignments[] = {
            {PointerFieldRegister::R11, 0xffffffff820d6964ull}, {PointerFieldRegister::R10, 0}};
        const ConstantFieldWrite writes[] = {{4, PointerFieldWidth::Word, 0},
                                             {0, PointerFieldWidth::Word, 0x820d6964u},
                                             {8, PointerFieldWidth::Word, 0}};
        if (initialize) {
            InitializeConstantFields(m, fields, PointerFieldRegister::R3, assignments, writes);
            s.r[10] = fields.r10;
            s.r[11] = fields.r11;
        } else {
            InitializeConstantFields(m, fields, PointerFieldRegister::R3, std::span(assignments, 1),
                                     std::span(writes + 1, 1));
            s.r[11] = fields.r11;
        }
    }
    void Construct() {
        Enter(30);
        auto &r = s.r;
        r[31] = r[3];
        r[30] = r[4];
        s.lr = 0x82bc61d0u;
        Base(true);
        r[11] = 0xffffffff820d0000ull;
        Store(r[31] + 32, r[30]);
        r[3] = r[31];
        r[10] = r[11] + 26944;
        r[11] = 0;
        Store(r[31], r[10]);
        for (unsigned o : {12u, 24u, 28u})
            Store(r[31] + o, 0);
        Leave(30);
    }
    void Cleanup() {
        Enter(30);
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        r[31] = r[3];
        r[11] += 26944;
        Store(r[31], r[11]);
        r[11] = Word(r[31] + 12);
        Compare(r[11]);
        if (!s.cr6.eq) {
            Free(12, 0x82bc6244u, 0x82bc6258u);
            r[11] = 0;
            Store(r[31] + 12, 0);
        } else {
            r[11] = Word(r[31] + 28);
            r[30] = 0;
            Compare(r[11]);
            if (!s.cr6.eq) {
                Free(28, 0x82bc6278u, 0x82bc628cu);
                Store(r[31] + 28, r[30]);
            }
            r[11] = Word(r[31] + 24);
            Compare(r[11]);
            if (!s.cr6.eq) {
                Free(24, 0x82bc62a0u, 0x82bc62b4u);
                Store(r[31] + 24, r[30]);
            }
        }
        r[3] = r[31];
        s.lr = 0x82bc62c0u;
        Base(false);
        Leave(30);
    }
    void Destroy() {
        Enter(30);
        auto &r = s.r;
        r[31] = r[3];
        r[30] = r[4];
        s.lr = 0x82bc63e8u;
        Cleanup();
        r[11] = r[30] & 1u;
        Compare(r[11]);
        if (!s.cr6.eq) {
            Allocator(0x82bc63f8u);
            r[11] = Word(r[3]);
            r[4] = r[31];
            r[11] = Word(r[11] + 12);
            Call(0x82bc640cu);
        }
        r[3] = r[31];
        Leave(30);
    }
    void Reject(unsigned text, unsigned line, GuestAddress continuation) {
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        r[6] = 0;
        r[7] = r[11] + text;
        r[11] = 0xffffffff820d0000ull;
        r[5] = line;
        r[4] = r[11] + 23768;
        r[3] = 4;
        s.lr = continuation;
        (void)diagnostic_format_routes61::Apply(0x82b9c298u, m, d.diagnostics, s);
        r[3] = 0;
    }
    void Build() {
        Enter(31);
        auto &r = s.r;
        r[31] = r[3];
        r[11] = Word(r[31] + 192);
        Compare(r[11], 255);
        if (s.cr6.gt) {
            Reject(24176, 544, 0x82b9eb98u);
            Leave(31);
            return;
        }
        r[11] = Word(r[31] + 168);
        Compare(r[11], 255);
        if (s.cr6.gt) {
            Reject(24120, 550, 0x82b9ebdcu);
            Leave(31);
            return;
        }
        r[3] = Word(r[31] + 288);
        Compare(r[3]);
        if (!s.cr6.eq) {
            r[11] = Word(r[3]);
            r[4] = 1;
            r[11] = Word(r[11]);
            Call(0x82b9ec14u);
            r[11] = 0;
            Store(r[31] + 288, 0);
        }
        r[11] = Word(r[31] + 168);
        Compare(r[11], 32);
        if (s.cr6.gt) {
            Allocator(0x82b9ec2cu);
            r[11] = Word(r[3]);
            r[5] = 10;
            r[4] = 36;
            r[11] = Word(r[11]);
            Call(0x82b9ec44u);
            Compare(r[3]);
            if (!s.cr6.eq) {
                r[4] = r[31] + 156;
                s.lr = 0x82b9ec54u;
                Construct();
                r[4] = r[3];
            } else
                r[4] = 0;
            r[3] = r[1] + 80;
            Store(r[31] + 288, r[4]);
            s.lr = 0x82b9ec6cu;
            (void)mesh_support_stream61::Apply(0x82bb3408u, m, {d.tree.guest, d.tree.fp}, s);
            r[4] = 16;
            r[3] = r[1] + 80;
            s.lr = 0x82b9ec78u;
            (void)cube_projection_table61::Apply(0x82bb38a0u, m, {d.tree.guest, d.tree.fp}, s);
            r[3] = r[1] + 80;
            s.lr = 0x82b9ec80u;
            (void)mesh_support_stream61::Apply(0x82bb3420u, m, {d.tree.guest, d.tree.fp}, s);
        }
        r[3] = 1;
        Leave(31);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Support x{m, d, s};
    switch (e) {
    case 0x82bc61b0u:
        x.Construct();
        break;
    case 0x82bc6210u:
        x.Cleanup();
        break;
    case 0x82bc63c8u:
        x.Destroy();
        break;
    case 0x82b9eb58u:
        x.Build();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_cook_support61
