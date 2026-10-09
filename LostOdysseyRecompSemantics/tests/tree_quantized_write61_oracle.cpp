#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/tree_quantized_write61.h"
namespace quant_write_oracle {
using Registers = tree_quantized_write61::Registers;
constexpr GuestAddress Writer = 0x30000, Node = 0x31000, Data = 0x32000, Owner = 0x33000,
                       Records = 0x34000;
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
void Check(unsigned mode, bool compact) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [compact](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Writer, Node);
        m.WriteU32(Node, Data);
        m.WriteU32(Node + 8, 256);
        m.WriteU32(Owner + 4, 2);
        m.WriteU32(Owner + 8, Records);
        for (unsigned i = 0; i < (compact ? 40u : 48u); ++i)
            m.WriteU8(Records + i, std::uint8_t(i + 1));
        for (unsigned i = 0; i < 6; ++i)
            m.WriteU32(Owner + 12 + 4 * i, std::bit_cast<std::uint32_t>(float(i + 1)));
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = Owner;
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
    if (compact)
        __imp__sub_82BDB660(c, before.Bytes());
    else
        __imp__sub_82BDC838(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!tree_quantized_write61::Apply(compact ? 0x82bdb660u : 0x82bdc838u, m, {guest, native},
                                       s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("scalar writer Full72/RAM/host mismatch");
    const auto expectedCount = mode ? 0x02000000u : 2u;
    if (m.ReadU32(Data) != expectedCount || m.ReadU32(Node + 4) != (compact ? 68u : 76u) ||
        s.r[3] != 1u)
        throw std::runtime_error("quantized writer count/length");
    for (unsigned i = 0; i < (compact ? 40u : 48u); ++i) {
        const auto width = i % (compact ? 20u : 24u) < 12 ? 2u : 4u;
        const auto wanted = mode ? (i / width) * width + width - 1u - i % width + 1u : i + 1u;
        if (m.ReadU8(Data + 4 + i) != wanted || m.ReadU8(Records + i) != i + 1u)
            throw std::runtime_error("quantized writer record endian/source");
    }
    for (unsigned i = 0; i < 6; ++i) {
        auto bits = std::bit_cast<std::uint32_t>(float(i + 1));
        if (mode)
            bits = ((bits & 255u) << 24u) | ((bits & 0xff00u) << 8u) | ((bits & 0xff0000u) >> 8u) |
                   (bits >> 24u);
        if (m.ReadU32(Data + (compact ? 44u : 52u) + 4 * i) != bits)
            throw std::runtime_error("quantized writer scales");
    }
}
} // namespace quant_write_oracle
void QuantWriteLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *quant_write_oracle::memory;
    if (e == 0x82bd1050u || e == 0x82bd1160u)
        (void)crt_reader_units61::Apply(e, m, quant_write_oracle::guest, s);
    else
        (void)crt_reader_float61::Apply(e, m,
                                        {quant_write_oracle::guest, quant_write_oracle::native}, s);
    crt_full_oracle::ToPpc(c, s);
}
void QuantWriteSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *quant_write_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        WriteU64(m, Address(s.r[1] - 16u - 8u * (31u - i)), s.r[i]);
    m.WriteU32(Address(s.r[1] - 8u), Address(s.r[12]));
}
void QuantWriteRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *quant_write_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = ReadU64(m, Address(s.r[1] - 16u - 8u * (31u - i)));
    s.r[12] = m.ReadU32(Address(s.r[1] - 8u));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 2; ++mode)
            for (bool compact : {false, true})
                quant_write_oracle::Check(mode, compact);
        std::puts("PASS tree-quantized-write61 4 original upper/scalar wrappers, shared append "
                  "cases (20/24-byte formats)");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
