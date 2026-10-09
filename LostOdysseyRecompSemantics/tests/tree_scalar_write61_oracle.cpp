#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/tree_scalar_write61.h"
namespace scalar_write_oracle {
using Registers = tree_scalar_write61::Registers;
constexpr GuestAddress Writer = 0x30000, Node = 0x31000, Data = 0x32000;
constexpr std::array<test::Region, 1> Regions{{{0, 0x120000}}};
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    void CallIndirect(GuestAddress, GuestMemory &, Registers &) override {
        throw std::runtime_error("unexpected scalar writer allocation");
    }
};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
Guest guest;
Native native;
GuestMemory *memory = nullptr;
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Writer, Node);
        m.WriteU32(Node, Data);
        m.WriteU32(Node + 8, 256);
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = 0xaabbccdd12345678ull;
    s.r[4] = mode & 1u;
    s.r[5] = Writer;
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(1.5);
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    auto om = before.Memory();
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode < 2)
        __imp__sub_82BD7D58(c, before.Bytes());
    else
        __imp__sub_82BD7E18(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!tree_scalar_write61::Apply(mode < 2 ? 0x82bd7d58u : 0x82bd7e18u, m, {guest, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("scalar writer Full72/RAM/host mismatch");
    constexpr std::uint32_t expected[]{0x12345678, 0x78563412, 0x3fc00000, 0x0000c03f};
    if (m.ReadU32(Data) != expected[mode] || m.ReadU32(Node + 4) != 4 ||
        m.ReadU32(Writer + 16) != Data)
        throw std::runtime_error("scalar endian output");
}
} // namespace scalar_write_oracle
void ScalarWriteLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *scalar_write_oracle::memory;
    if (e == 0x82bd1050u)
        (void)crt_reader_units61::Apply(e, m, scalar_write_oracle::guest, s);
    else
        (void)crt_reader_float61::Apply(
            e, m, {scalar_write_oracle::guest, scalar_write_oracle::native}, s);
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 4; ++mode)
            scalar_write_oracle::Check(mode);
        std::puts("PASS tree-scalar-write61 4 original-upper/shared-writer cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
