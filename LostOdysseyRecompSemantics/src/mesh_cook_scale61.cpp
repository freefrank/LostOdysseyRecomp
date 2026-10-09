#include "lo_semantics/mesh_cook_scale61.h"
#include "lo_semantics/power_math61.h"
#include "lo_semantics/integer_leaf.h"
#include "lo_semantics/recovery_abi.h"
#include <algorithm>
#include <bit>
namespace lo::semantic::gpu::mesh_cook_scale61 {
namespace {
using recovery_abi::Address;
struct Scale {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    unsigned Word(unsigned p) { return m.ReadU32(p); }
    float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
    void Float(unsigned p, float v) { m.WriteU32(p, std::bit_cast<unsigned>(v)); }
    void Values(unsigned pointer, unsigned count, double factor) {
        for (unsigned i = 0; i < count; ++i)
            Float(pointer + 4 * i, float(Float(pointer + 4 * i) * factor));
    }
    bool Body(unsigned owner, double factor) {
        auto handle = Word(owner + 44);
        if (!handle)
            return false;
        auto geometry = Word(handle);
        if (!geometry)
            return false;
        Values(geometry + 20, 3, factor);
        Values(Word(geometry + 12), 3 * Word(geometry + 8), factor);
        for (unsigned i = 0; i < Word(geometry + 32); ++i)
            Values(Word(geometry + 36) + 36 * i + 24, 3, factor);
        Values(owner + 112, 6, factor);
        s.fpr_bits[2] = recovery_abi::ReadU64(m, 0x820d5e30u);
        s.fpr_bits[1] = recovery_abi::ReadU64(m, 0x82001010u);
        s.lr = 0x82b9efccu;
        (void)power_math61::Apply(0x82b7e860u, m, d.tree.fp, s);
        float relative = float(std::bit_cast<double>(s.fpr_bits[1]));
        Float(owner + 152,
              float(std::max({Float(owner + 124), Float(owner + 128), Float(owner + 132)}) *
                    relative));
        Values(owner + 136, 4, factor);
        Values(owner + 332, 3, factor);
        Values(owner + 296, 9, float(factor * factor));
        lo::semantic::integer_leaf::Registers leaf{};
        (void)lo::semantic::integer_leaf::Apply(0x82b9cb60u, leaf);
        bool fast = m.ReadU8(Address(leaf.r3) + 8) != 0;
        if (fast) {
            s.r[3] = owner + 8;
            s.ctr = Word(Word(owner + 8) + 16);
            s.lr = 0x82b9f0f8u;
            d.tree.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        } else {
            s.r[3] = owner;
            s.lr = 0x82b9f13cu;
            (void)mesh_cook_tree61::Apply(0x82b9e6a8u, m, d, s);
        }
        if ((s.r[3] & 255u) != 0)
            return true;
        s.r[3] = 4;
        s.r[4] = 0xffffffff820d5cd8ull;
        s.r[5] = fast ? 726 : 734;
        s.r[6] = 0;
        s.r[7] = fast ? 0xffffffff820d5ec8ull : 0xffffffff820d5ea8ull;
        s.lr = fast ? 0x82b9f124u : 0x82b9f168u;
        (void)diagnostic_format_routes61::Apply(0x82b9c298u, m, d.diagnostics, s);
        return false;
    }
    void Run() {
        auto owner = Address(s.r[3]);
        auto factor = std::bit_cast<double>(s.fpr_bits[1]);
        auto old = s.r[1];
        recovery_abi::WriteU64(m, Address(old - 16), s.r[31]);
        recovery_abi::WriteU64(m, Address(old - 24), s.r[30]);
        recovery_abi::WriteU64(m, Address(old - 32), s.r[29]);
        recovery_abi::WriteU64(m, Address(old - 40), s.r[28]);
        recovery_abi::WriteU64(m, Address(old - 48), s.fpr_bits[31]);
        m.WriteU32(Address(old - 8), Address(s.lr));
        s.r[1] -= 128;
        m.WriteU32(Address(s.r[1]), Address(old));
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.tree.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.r[3] = Body(owner, factor) ? 1 : 0;
        s.r[1] += 128;
        s.r[31] = recovery_abi::ReadU64(m, Address(s.r[1] - 16));
        s.r[30] = recovery_abi::ReadU64(m, Address(s.r[1] - 24));
        s.r[29] = recovery_abi::ReadU64(m, Address(s.r[1] - 32));
        s.r[28] = recovery_abi::ReadU64(m, Address(s.r[1] - 40));
        s.fpr_bits[31] = recovery_abi::ReadU64(m, Address(s.r[1] - 48));
        s.lr = m.ReadU32(Address(s.r[1] - 8));
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e == 0x82b9ec98u) {
        Scale{m, d, s}.Run();
        return true;
    }
    if (e == 0x82b9f188u || e == 0x82b9f190u) {
        s.r[3] = m.ReadU32(Address(s.r[3]) + (e == 0x82b9f188u ? 168 : 160));
        return true;
    }
    if (e == 0x82b9caf8u) {
        constexpr unsigned settings = 0x832dc180u;
        auto input = Address(s.r[4]), depth = m.ReadU32(settings + 664);
        for (unsigned off : {0, 4, 8})
            m.WriteU32(settings + off, m.ReadU32(input + off));
        if (depth)
            for (unsigned off : {0, 4, 8})
                m.WriteU32(settings + 16 + 20 * (depth - 1) + off, m.ReadU32(settings + off));
        s.r[3] = 1;
        return true;
    }
    return false;
}
} // namespace lo::semantic::gpu::mesh_cook_scale61
