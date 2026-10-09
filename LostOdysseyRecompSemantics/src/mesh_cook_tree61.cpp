#include "lo_semantics/mesh_cook_tree61.h"
#include "lo_semantics/grid_transform_support61.h"
#include "lo_semantics/integer_leaf.h"
#include "lo_semantics/owned_tree_reorder_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_cook_tree61 {
namespace {
using recovery_abi::Address;
struct Rebuild {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Virtual(std::uint64_t target, GuestAddress continuation) {
        s.ctr = target;
        s.lr = continuation;
        d.tree.guest.CallIndirect(Address(target) & ~3u, m, s);
    }
    void Body() {
        auto &r = s.r;
        r[31] = r[3];
        r[30] = r[31] + 8;
        r[3] = r[30];
        s.lr = 0x82b9e6c4u;
        (void)owned_tree_reorder_support61::Apply(0x82bd20f0u, m, {d.tree.guest, d.tree.fp}, s);
        r[11] = Word(r[31]);
        r[3] = r[31];
        r[11] = Word(r[11] + 48);
        Virtual(r[11], 0x82b9e6d8u);
        r[11] = Word(r[31]);
        r[10] = r[3];
        r[3] = r[31];
        r[11] = Word(r[11] + 52);
        Store(r[31] + 96, r[10]);
        Virtual(r[11], 0x82b9e6f4u);
        r[29] = r[31] + 84;
        Store(r[31] + 92, r[3]);
        r[3] = r[29];
        r[5] = Word(r[31] + 172);
        r[4] = Word(r[31] + 164);
        s.lr = 0x82b9e70cu;
        (void)diagnostic_format_routes61::Apply(0x82bd18c0u, m, d.diagnostics, s);
        r[3] = r[1] + 80;
        s.lr = 0x82b9e714u;
        (void)grid_transform_support61::Apply(0x82bd1278u, m, d.tree.fp, s);
        r[31] = 1;
        Store(r[1] + 80, r[29]);
        m.WriteU8(Address(r[1] + 104), 1);
        s.lr = 0x82b9e724u;
        // Accepted integer leaf; bridge only its two written registers.
        integer_leaf::Registers leaf{};
        (void)integer_leaf::Apply(0x82b9cb60u, leaf);
        r[3] = leaf.r3;
        r[11] = leaf.r11;
        r[11] = r[3];
        r[10] = Word(r[30]);
        r[9] = 34;
        r[4] = r[1] + 80;
        r[3] = r[30];
        r[11] = m.ReadU8(Address(r[11] + 8));
        Store(r[1] + 88, r[9]);
        r[9] = std::countl_zero(Address(r[11]));
        r[10] = Word(r[10] + 8);
        r[11] = 0;
        Store(r[1] + 84, r[31]);
        r[9] = (r[9] >> 5) & 1u;
        m.WriteU8(Address(r[1] + 106), 0);
        m.WriteU8(Address(r[1] + 105), std::uint8_t(r[9]));
        m.WriteU8(Address(r[1] + 107), 0);
        Virtual(r[10], 0x82b9e768u);
        r[11] = r[3] & 255u;
        s.cr6 = {0, std::uint8_t(r[11] != 0), std::uint8_t(r[11] == 0), s.xer_so};
        if (s.cr6.eq) {
            r[11] = 0xffffffff820d0000ull;
            r[6] = 0;
            r[7] = r[11] + 23972;
            r[11] = 0xffffffff820d0000ull;
            r[5] = 373;
            r[4] = r[11] + 23768;
            r[3] = 4;
            s.lr = 0x82b9e794u;
            (void)diagnostic_format_routes61::Apply(0x82b9c298u, m, d.diagnostics, s);
            r[3] = 0;
        } else
            r[3] = 1;
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82b9e6b0u;
        for (unsigned i = 29; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= 144;
        Store(r[1], old);
        Body();
        r[1] += 144;
        for (unsigned i = 29; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82b9e6a8u)
        return false;
    Rebuild{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_cook_tree61
