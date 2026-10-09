#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/growable_output61.h"
namespace grow_output_oracle {
using Registers = growable_output61::Registers;
constexpr GuestAddress Owner = 0x30000, Table = 0x31000, Old = 0x32000, New = 0x35000,
                       Source = 0x34000, Allocator = 0x36000, AllocatorTable = 0x37000;
constexpr GuestAddress Allocate = 0x2000, Free = 0x2004, Append = 0x82bde498u;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x832df000, 0x1000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
Native native;
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    void Record(GuestAddress e, Registers &s) {
        std::array<std::uint64_t, 73> event{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), event.begin());
        event.back() = e;
        events.push_back(event);
    }
    void CallIndirect(GuestAddress e, GuestMemory &m, Registers &s) override {
        Record(e, s);
        if (e == Append) {
            (void)growable_output61::Apply(e, m, {*this, native}, s);
            return;
        }
        if (e == Allocate) {
            if (s.r[4] != 4104u || s.r[5] != 0)
                throw std::runtime_error("output growth shape");
            s.r[3] = New;
        } else if (e == Free) {
            if (s.r[4] != Old)
                throw std::runtime_error("output free pointer");
            s.r[3] = 0;
        } else
            throw std::runtime_error("unexpected output callback");
        s.r[8] ^= 0x1234u;
        s.cr7.eq ^= 1u;
    }
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
        m.WriteU32(Owner, Table);
        m.WriteU32(Table + 48, Append | 1);
        m.WriteU32(Owner + 4, mode == 6 ? 4u : 0u);
        m.WriteU32(Owner + 8, mode < 5 ? 128u : 0u);
        m.WriteU32(Owner + 12, (mode == 5 || mode == 7) ? 0u : Old);
        m.WriteU32(Old, 0xaabbccddu);
        m.WriteU32(Source, 0x10203040);
        m.WriteU32(Source + 4, 0x50607080);
        m.WriteU32(0x832df58c, Allocator);
        m.WriteU32(Allocator, AllocatorTable);
        m.WriteU32(AllocatorTable + 8, Allocate | 1);
        m.WriteU32(AllocatorTable + 20, Free | 3);
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
    s.r[4] = mode < 5 ? 0x12345678u : Source;
    s.r[5] = mode == 6 ? 4u : 8u;
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(1.5);
    s.lr = 0x9988776681234567ull;
    s.cached_fp_control = 0x9fc0;
    s.xer_so = 1;
    constexpr GuestAddress entries[]{0x82bde330u, 0x82bde378u, 0x82bde3c0u,
                                     0x82bde408u, 0x82bde450u, Append,
                                     Append,      0x82bde2c8u, 0x82bde2c8u};
    Guest expected, actual;
    auto om = before.Memory();
    guest = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    switch (mode) {
    case 0:
        __imp__sub_82BDE330(c, before.Bytes());
        break;
    case 1:
        __imp__sub_82BDE378(c, before.Bytes());
        break;
    case 2:
        __imp__sub_82BDE3C0(c, before.Bytes());
        break;
    case 3:
        __imp__sub_82BDE408(c, before.Bytes());
        break;
    case 4:
        __imp__sub_82BDE450(c, before.Bytes());
        break;
    case 5:
    case 6:
        __imp__sub_82BDE498(c, before.Bytes());
        break;
    default:
        __imp__sub_82BDE2C8(c, before.Bytes());
    }
    auto host = PPCFPSCRRegister{}.getcsr();
    guest = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!growable_output61::Apply(entries[mode], m, {actual, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events ||
        host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("grow output Full72/RAM/host/callback mismatch");
    if (mode < 5) {
        constexpr unsigned width[]{1, 2, 4, 4, 8};
        constexpr std::uint64_t bits[]{0x78, 0x5678, 0x12345678, 0x3fc00000, 0x3ff8000000000000ull};
        for (unsigned i = 0; i < width[mode]; ++i)
            if (m.ReadU8(Old + i) != ((bits[mode] >> (8u * (width[mode] - 1u - i))) & 255u))
                throw std::runtime_error("scalar payload");
        if (m.ReadU32(Owner + 4) != width[mode] || s.r[3] != Owner)
            throw std::runtime_error("scalar used/owner");
    } else if (mode < 7) {
        if (m.ReadU32(Owner + 4) != 8u || m.ReadU32(Owner + 8) != 4104u ||
            m.ReadU32(Owner + 12) != New ||
            m.ReadU32(New) != (mode == 6 ? 0xaabbccddu : 0x10203040u) ||
            m.ReadU32(New + 4) != (mode == 6 ? 0x10203040u : 0x50607080u))
            throw std::runtime_error("output grow data");
    } else if (m.ReadU32(Owner) != 0x821a982cu || m.ReadU32(Owner + 12) != (mode == 7 ? 0u : Old) ||
               actual.events.size() != (mode == 7 ? 0u : 1u))
        throw std::runtime_error("output destructor behavior");
}
} // namespace grow_output_oracle
void GrowOutputIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *base) {
    auto s = crt_full_oracle::FromPpc(c);
    if (e == grow_output_oracle::Append) {
        grow_output_oracle::guest->Record(e, s);
        __imp__sub_82BDE498(c, base);
    } else {
        grow_output_oracle::guest->CallIndirect(e, *grow_output_oracle::memory, s);
        crt_full_oracle::ToPpc(c, s);
    }
}
void GrowOutputCopy(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    (void)crt_copy_full_context::Apply(0x82b7a0b0u, *grow_output_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void GrowOutputSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *grow_output_oracle::memory;
    for (unsigned i = 27; i < 32; ++i)
        WriteU64(m, Address(s.r[1] - 16u - 8u * (31u - i)), s.r[i]);
    m.WriteU32(Address(s.r[1] - 8u), Address(s.r[12]));
}
void GrowOutputRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *grow_output_oracle::memory;
    for (unsigned i = 27; i < 32; ++i)
        s.r[i] = ReadU64(m, Address(s.r[1] - 16u - 8u * (31u - i)));
    s.r[12] = m.ReadU32(Address(s.r[1] - 8u));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 9; ++mode)
            grow_output_oracle::Check(mode);
        std::puts("PASS growable-output61 9 original scalar/append/cleanup cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
