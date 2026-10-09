#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/memory_input61.h"
namespace memory_input_oracle {
using Registers = memory_input61::Registers;
constexpr GuestAddress Reader = 0x30000, Data = 0x31000, Output = 0x32000;
constexpr std::array<test::Region, 1> Regions{{{0, 0x120000}}};
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
        m.WriteU32(Reader + 4, Data);
        if (mode == 3)
            m.WriteU32(Data, std::bit_cast<std::uint32_t>(1.5f));
        else if (mode == 4)
            WriteU64(m, Data, std::bit_cast<std::uint64_t>(-2.25));
        else {
            m.WriteU32(Data, 0x12345678);
            m.WriteU32(Data + 4, 0x90abcdef);
        }
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = Reader;
    s.r[4] = Output;
    s.r[5] = 8;
    s.lr = 0x9988776681234567ull;
    s.cached_fp_control = 0x9fc0;
    s.xer_so = 1;
    auto om = before.Memory();
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    switch (mode) {
    case 0:
        __imp__sub_82BDE550(c, before.Bytes());
        break;
    case 1:
        __imp__sub_82BDE568(c, before.Bytes());
        break;
    case 2:
        __imp__sub_82BDE580(c, before.Bytes());
        break;
    case 3:
        __imp__sub_82BDE598(c, before.Bytes());
        break;
    case 4:
        __imp__sub_82BDE5B8(c, before.Bytes());
        break;
    default:
        __imp__sub_82BDE5D8(c, before.Bytes());
    }
    auto host = PPCFPSCRRegister{}.getcsr();
    memory = nullptr;
    auto m = after.Memory();
    Native native;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    constexpr GuestAddress entries[]{0x82bde550u, 0x82bde568u, 0x82bde580u,
                                     0x82bde598u, 0x82bde5b8u, 0x82bde5d8u};
    if (!memory_input61::Apply(entries[mode], m, native, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("memory input Full72/RAM/host mismatch");
    constexpr unsigned lengths[]{1, 2, 4, 4, 8, 8};
    if (m.ReadU32(Reader + 4) != Data + lengths[mode])
        throw std::runtime_error("reader cursor");
    if (mode < 3) {
        constexpr std::uint32_t expected[]{0x12, 0x1234, 0x12345678};
        if (s.r[3] != expected[mode])
            throw std::runtime_error("reader integer");
    } else if (mode < 5) {
        if (std::bit_cast<double>(s.fpr_bits[1]) != (mode == 3 ? 1.5 : -2.25))
            throw std::runtime_error("reader float");
    } else if (m.ReadU32(Output) != 0x12345678 || m.ReadU32(Output + 4) != 0x90abcdef ||
               s.r[3] != Output)
        throw std::runtime_error("reader block");
}
} // namespace memory_input_oracle
void MemoryInputCopy(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    (void)crt_copy_full_context::Apply(0x82b7a0b0u, *memory_input_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 6; ++mode)
            memory_input_oracle::Check(mode);
        std::puts("PASS memory-input61 6 original-body/shared-copy cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
