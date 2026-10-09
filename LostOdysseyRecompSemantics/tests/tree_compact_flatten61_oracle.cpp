#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_compact_flatten61.h"
namespace compact_flatten_oracle {
using Registers = tree_compact_flatten61::Registers;
constexpr GuestAddress Nodes = 0x30000, Ids = 0x31000, Output = 0x32000, Counter = 0x33000;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x8201f000, 0x1000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
GuestMemory *memory = nullptr;
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    const auto seed = [&](test::GuestWindow &w) {
        w.Fill(0xa5);
        auto m = w.Memory();
        constexpr float bounds[]{-4, 2, -8, 6, 10, 4};
        for (unsigned n = 0; n < 7; ++n) {
            auto node = Nodes + 40 * n;
            for (unsigned i = 0; i < 6; ++i)
                m.WriteU32(node + 4 * i, std::bit_cast<std::uint32_t>(bounds[i]));
            m.WriteU32(node + 24, 1);
            m.WriteU32(node + 32, Ids + 4 * n);
            m.WriteU32(Ids + 4 * n, 10 + n);
        }
        m.WriteU32(Nodes + 24, (Nodes + 40) | 1);
        if (mode)
            m.WriteU32(Nodes + 40 + 24, (Nodes + 120) | 1);
        if (mode == 2)
            m.WriteU32(Nodes + 80 + 24, (Nodes + 200) | 1);
        m.WriteU32(Counter, 1);
        m.WriteU32(0x8201f9f0, std::bit_cast<std::uint32_t>(0.5f));
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = Output;
    s.r[4] = 0;
    s.r[5] = Counter;
    s.r[6] = Nodes;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    auto om = before.Memory();
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BDCE38(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    memory = nullptr;
    auto m = after.Memory();
    Native native;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!tree_compact_flatten61::Apply(0x82bdce38u, m, native, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("compact flatten Full72/RAM/host mismatch");
    constexpr std::uint32_t flags[]{0xc000000bu, 0x8000000cu, 0xdeadu};
    if (m.ReadU32(Output + 24) != flags[mode] || m.ReadU32(Output + 28) != mode ||
        m.ReadU32(Counter) != mode + 1u)
        throw std::runtime_error("compact leaf folding/order/count");
    if (mode && m.ReadU32(Output + 32 + 24) != 0xc000000du)
        throw std::runtime_error("compact first recursive node");
    if (mode == 2 && m.ReadU32(Output + 64 + 24) != 0xc000000fu)
        throw std::runtime_error("compact second recursive node");
}
} // namespace compact_flatten_oracle
void CompactSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *compact_flatten_oracle::memory;
    for (unsigned i = 26; i < 32; ++i)
        WriteU64(m, Address(s.r[1] - 16u - 8u * (31u - i)), s.r[i]);
    m.WriteU32(Address(s.r[1] - 8u), Address(s.r[12]));
}
void CompactRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *compact_flatten_oracle::memory;
    for (unsigned i = 26; i < 32; ++i)
        s.r[i] = ReadU64(m, Address(s.r[1] - 16u - 8u * (31u - i)));
    s.r[12] = m.ReadU32(Address(s.r[1] - 8u));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 3; ++mode)
            compact_flatten_oracle::Check(mode);
        std::puts("PASS tree-compact-flatten61 3 original recursive-body cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
