#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_valence_stream61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/serialization_control61.h"
namespace valence_oracle {
using Registers = mesh_valence_stream61::Registers;
constexpr GuestAddress Writer = 0x30000, Table = 0x31000, Buffer = 0x32000, Input = 0x33000,
                       Adapter = 0x34000, Source = 0x35000, Second = 0x36000, Temporary = 0x37000,
                       AllocTable = 0x38000, Allocate = 0x2000, Free = 0x2004;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x83216000, 0xca000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
} native;
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    bool live = false;
    unsigned allocations = 0, frees = 0;
    void CallIndirect(GuestAddress e, GuestMemory &m, Registers &s) override {
        std::array<std::uint64_t, 73> ev{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), ev.begin());
        ev.back() = e;
        events.push_back(ev);
        if (e == Allocate) {
            if (live || s.r[4] > 16)
                throw std::runtime_error("valence temporary allocation");
            live = true;
            ++allocations;
            s.r[3] = Temporary;
            return;
        }
        if (e == Free) {
            if (!live || s.r[4] != Temporary)
                throw std::runtime_error("valence temporary release");
            live = false;
            ++frees;
            s.r[3] = 0;
            return;
        }
        if (e == 0x82bde498u && s.r[4] == Second && live)
            throw std::runtime_error("valence release order");
        if (!growable_output61::Apply(e, m, {*this, native}, s))
            throw std::runtime_error("unexpected mesh stream callback");
    }
};
Guest *guest = nullptr;
GuestMemory *memory = nullptr;
void Check(unsigned mode) {
    const bool wide = mode == 2 || mode == 3, swapped = mode == 1 || mode == 3, empty = mode == 4;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Writer, Table);
        m.WriteU32(Writer + 8, 256);
        m.WriteU32(Writer + 12, Buffer);
        m.WriteU32(Table + 28, 0x82bde331);
        m.WriteU32(Table + 32, 0x82bde379);
        m.WriteU32(Table + 36, 0x82bde3c1);
        m.WriteU32(Table + 48, 0x82bde49b);
        m.WriteU32(0x83216624, AllocTable);
        m.WriteU32(AllocTable, Allocate | 1);
        m.WriteU32(AllocTable + 12, Free | 3);
        m.WriteU32(0x832dc180, swapped ? 0 : 1);
        m.WriteU32(Adapter + 4, empty ? 0 : 3);
        m.WriteU32(Adapter + 8, 3);
        m.WriteU32(Adapter + 12, Input);
        m.WriteU32(Adapter + 16, Second);
        m.WriteU16(Input, 2);
        m.WriteU16(Input + 4, wide ? 300 : 7);
        m.WriteU16(Input + 8, 1);
        for (unsigned i = 0; i < 3; ++i)
            m.WriteU8(Second + i, 20 + i);
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    s.r[3] = Adapter;
    s.r[4] = Writer;
    Guest expected, actual;
    auto om = before.Memory();
    memory = &om;
    guest = &expected;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    __imp__sub_82BBCC28(c, before.Bytes());
    memory = nullptr;
    guest = nullptr;
    auto m = after.Memory();
    if (!mesh_valence_stream61::Apply(0x82bbcc28u, m, {actual, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events)
        throw std::runtime_error("valence Full72/RAM/callback mismatch mode " +
                                 std::to_string(mode));
    auto word = [&](unsigned p, unsigned v) {
        if (swapped)
            v = __builtin_bswap32(v);
        if (m.ReadU32(Buffer + p) != v)
            throw std::runtime_error("valence header/count word");
    };
    if (m.ReadU32(Buffer) != (swapped ? 0x49434501u : 0x49434500u) ||
        m.ReadU32(Buffer + 4) != 0x56414c45u)
        throw std::runtime_error("valence ICE/VALE header");
    word(8, 2);
    word(12, empty ? 0 : 3);
    word(16, 3);
    word(20, empty ? 0 : wide ? 300 : 7);
    auto size = empty ? 0u : wide ? 6u : 3u;
    if (m.ReadU32(Writer + 4) != 27 + size || s.r[3] != 1 || actual.live ||
        actual.allocations != 1 || actual.frees != 1)
        throw std::runtime_error("valence bytes/lifetime");
    if (!empty) {
        unsigned values[]{2, wide ? 300u : 7u, 1};
        for (unsigned i = 0; i < 3; ++i) {
            auto v = values[i];
            if (wide) {
                if (swapped)
                    v = ((v & 255) << 8) | (v >> 8);
                if (m.ReadU16(Buffer + 24 + 2 * i) != v)
                    throw std::runtime_error("valence halfword output");
            } else if (m.ReadU8(Buffer + 24 + i) != v)
                throw std::runtime_error("valence byte output");
        }
    }
    for (unsigned i = 0; i < 3; ++i)
        if (m.ReadU8(Buffer + 24 + size + i) != 20 + i)
            throw std::runtime_error("valence raw adjacency output");
}
void CheckWords(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    unsigned values[]{mode == 2 ? 0x3f800000u : 2u,
                      mode == 2   ? 0x40000000u
                      : mode == 1 ? 300u
                                  : 7u,
                      mode == 2 ? 0xc0000000u : 1u};
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Writer, Table);
        m.WriteU32(Writer + 8, 256);
        m.WriteU32(Writer + 12, Buffer);
        m.WriteU32(Table + 28, 0x82bde331);
        m.WriteU32(Table + 32, 0x82bde379);
        m.WriteU32(Table + 40, 0x82bde409);
        m.WriteU32(Table + 48, 0x82bde49b);
        for (unsigned i = 0; i < 3; ++i)
            m.WriteU32(Input + 4 * i, values[i]);
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    s.r[3] = Input;
    s.r[4] = 3;
    Guest expected, actual;
    auto om = before.Memory();
    memory = &om;
    guest = &expected;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BADFA0(c, before.Bytes());
    auto max = c.r3.u32;
    c.r4.u64 = 3;
    c.r5.u64 = Input;
    c.r6.u64 = Writer;
    c.r7.u64 = mode == 1 ? 1 : 0;
    __imp__sub_82BD8668(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    memory = nullptr;
    guest = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_valence_stream61::Apply(0x82badfa0u, m, {actual, native}, s);
    if (s.r[3] != max || max != *std::max_element(values, values + 3))
        throw std::runtime_error("word maximum");
    s.r[4] = 3;
    s.r[5] = Input;
    s.r[6] = Writer;
    s.r[7] = mode == 1 ? 1 : 0;
    (void)mesh_valence_stream61::Apply(0x82bd8668u, m, {actual, native}, s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events ||
        csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("word packing Full72/RAM/CSR/callback");
    unsigned width = mode == 0 ? 1 : mode == 1 ? 2 : 4;
    if (m.ReadU32(Writer + 4) != 3 * width)
        throw std::runtime_error("word packing size");
    for (unsigned i = 0; i < 3; ++i) {
        auto v = values[i];
        if (mode == 0) {
            if (m.ReadU8(Buffer + i) != v)
                throw std::runtime_error("packed byte");
        } else if (mode == 1) {
            if (m.ReadU16(Buffer + 2 * i) != (((v & 255) << 8) | (v >> 8)))
                throw std::runtime_error("packed halfword");
        } else if (m.ReadU32(Buffer + 4 * i) != v)
            throw std::runtime_error("word float-path bit staging");
    }
}
} // namespace valence_oracle
void ValenceIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    valence_oracle::guest->CallIndirect(e, *valence_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void ValenceLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *valence_oracle::memory;
    auto d = mesh_stream_write61::Dependencies{*valence_oracle::guest, valence_oracle::native};
    if (e == 0x82b9cb70u)
        (void)serialization_control61::Apply(e, m, d, s);
    else if (e == 0x82bd0798u)
        (void)crt_close_recursive_buffer_context::Apply(e, m, *valence_oracle::guest, s);
    else
        (void)mesh_stream_write61::Apply(e, m, d, s);
    crt_full_oracle::ToPpc(c, s);
}
void ValenceSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *valence_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void ValenceRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *valence_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 5; ++i)
            valence_oracle::Check(i);
        for (unsigned i = 0; i < 3; ++i)
            valence_oracle::CheckWords(i);
        std::puts("PASS mesh-valence-stream61 8 original-chain/shared-concrete-output cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
