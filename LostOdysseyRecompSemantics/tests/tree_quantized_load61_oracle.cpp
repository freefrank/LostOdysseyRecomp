#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_quantized_load61.h"
namespace quant_load_oracle {
using Registers = tree_quantized_load61::Registers;
constexpr GuestAddress Owner = 0x30000, Reader = 0x31000, Table = 0x32000, Old = 0x33004,
                       New = 0x34000, ReaderTable = 0x35000;
constexpr GuestAddress Allocate = 0x2a00, Free = 0x2a04, Count = 0x2a08, Read = 0x2a0c;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x83216000, 0xca000}}};
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    unsigned mode = 0, words = 0;
    std::vector<std::array<std::uint64_t, 73>> events;
    void CallIndirect(GuestAddress e, GuestMemory &m, Registers &s) override {
        std::array<std::uint64_t, 73> event{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), event.begin());
        event.back() = e;
        events.push_back(event);
        if (e == Count) {
            auto value = words++ == 0 ? 2u : std::bit_cast<std::uint32_t>(float(words));
            if (mode == 1)
                value = ((value & 255u) << 24u) | ((value & 0xff00u) << 8u) |
                        ((value & 0xff0000u) >> 8u) | (value >> 24u);
            s.r[3] = value;
        } else if (e == Free) {
            if (s.r[4] != Old - 4u)
                throw std::runtime_error("flat load free base");
            s.r[3] = 0;
        } else if (e == Allocate) {
            if (s.r[4] != 52u || s.r[5] != 30u)
                throw std::runtime_error("flat load allocation shape");
            s.r[3] = mode == 2 ? 0 : New;
        } else if (e == Read) {
            if (s.r[4] != New + 4u || s.r[5] != 48u)
                throw std::runtime_error("flat payload request");
            for (unsigned i = 0; i < 48; ++i)
                m.WriteU8(New + 4u + i, std::uint8_t(i + 1u));
            s.r[3] = 0;
        } else
            throw std::runtime_error("unexpected flat load callback");
        s.r[8] ^= 0xabcdefu;
        s.cr7.eq ^= 1;
    }
};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
Guest *guest = nullptr;
GuestMemory *memory = nullptr;
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    const auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Owner + 4, 1);
        m.WriteU32(Owner + 8, mode ? Old : 0);
        m.WriteU32(Reader, ReaderTable);
        m.WriteU32(ReaderTable + 12, Count | 1);
        m.WriteU32(ReaderTable + 24, Read | 3);
        m.WriteU32(0x83216624, Table);
        m.WriteU32(Table, Allocate | 1);
        m.WriteU32(Table + 12, Free | 3);
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
    s.r[4] = mode == 1 ? 1 : 0;
    s.r[5] = Reader;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    Guest expected, actual;
    expected.mode = actual.mode = mode;
    auto om = before.Memory();
    guest = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BDC9F0(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    guest = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    Native native;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!tree_quantized_load61::Apply(0x82bdc9f0u, m, {actual, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events ||
        host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("flat load Full72/RAM/callback mismatch");
    if (s.r[3] != (mode == 2 ? 0u : 1u) || m.ReadU32(Owner + 4) != 2u ||
        m.ReadU32(Owner + 8) != (mode == 2 ? 0u : New + 4u))
        throw std::runtime_error("flat load owner/result");
    if (mode != 2) {
        for (unsigned axis = 0; axis < 6; ++axis)
            if (m.ReadU32(Owner + 12u + 4u * axis) !=
                std::bit_cast<std::uint32_t>(float(axis + 2u)))
                throw std::runtime_error("quantized decoded scales");
        if (m.ReadU32(New) != 2u)
            throw std::runtime_error("flat load count prefix");
        for (unsigned i = 0; i < 48; ++i) {
            const auto width = i % 24u < 12u ? 2u : 4u;
            auto expectedByte =
                mode == 1 ? ((i / width) * width + (width - 1u - i % width) + 1u) : i + 1u;
            if (m.ReadU8(New + 4u + i) != expectedByte)
                throw std::runtime_error("flat load payload endian order");
        }
    }
}
} // namespace quant_load_oracle
void QuantLoadIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    quant_load_oracle::guest->CallIndirect(e, *quant_load_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void QuantLoadAllocator(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, *quant_load_oracle::memory,
                                                    *quant_load_oracle::guest, s);
    crt_full_oracle::ToPpc(c, s);
}
void QuantLoadSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *quant_load_oracle::memory;
    for (unsigned i = 26; i < 32; ++i)
        WriteU64(m, Address(s.r[1] - 16u - 8u * (31u - i)), s.r[i]);
    m.WriteU32(Address(s.r[1] - 8u), Address(s.r[12]));
}
void QuantLoadRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *quant_load_oracle::memory;
    for (unsigned i = 26; i < 32; ++i)
        s.r[i] = ReadU64(m, Address(s.r[1] - 16u - 8u * (31u - i)));
    s.r[12] = m.ReadU32(Address(s.r[1] - 8u));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0, words = 0; mode < 3; ++mode)
            quant_load_oracle::Check(mode);
        std::puts("PASS tree-quantized-load61 3 original-upper/shared-allocator cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
