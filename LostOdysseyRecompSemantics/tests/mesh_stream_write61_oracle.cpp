#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_stream_write61.h"
#include "lo_semantics/recovery_abi.h"
namespace mesh_stream_oracle {
using Registers = mesh_stream_write61::Registers;
constexpr GuestAddress Writer = 0x30000, Table = 0x31000, Buffer = 0x32000, Input = 0x33000;
constexpr std::array<test::Region, 1> Regions{{{0, 0x120000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
} native;
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    void CallIndirect(GuestAddress e, GuestMemory &m, Registers &s) override {
        std::array<std::uint64_t, 73> ev{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), ev.begin());
        ev.back() = e;
        events.push_back(ev);
        if (!growable_output61::Apply(e, m, {*this, native}, s))
            throw std::runtime_error("unexpected mesh stream callback");
    }
};
Guest *guest = nullptr;
GuestMemory *memory = nullptr;
void Check(unsigned requested) {
    const unsigned mode = requested == 11   ? 10u
                          : requested == 12 ? 1u
                          : requested == 13 ? 3u
                                            : requested;
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Writer, Table);
        m.WriteU32(Writer + 8, 256);
        m.WriteU32(Writer + 12, Buffer);
        m.WriteU32(Table + 28, 0x82bde331);
        m.WriteU32(Table + 32, 0x82bde379);
        m.WriteU32(Table + 36, 0x82bde3c1);
        m.WriteU32(Table + 40, 0x82bde409);
        m.WriteU32(Table + 48, 0x82bde49b);
        m.WriteU32(Input, 0x3fc00000);
        m.WriteU32(Input + 4, 0xc0200000);
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
    auto entry = mode < 2   ? 0x82bada70u
                 : mode < 5 ? 0x82badcd0u
                 : mode < 7 ? 0x82badd60u
                 : mode < 9 ? 0x82bd8078u
                            : 0x82bd7db0u;
    if (mode < 2) {
        s.r[4] = mode;
        s.r[5] = Writer;
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(1.5);
    } else if (mode < 5) {
        s.r[3] = Input;
        s.r[4] = mode == 4 ? 0 : 2;
        s.r[5] = mode == 3 ? 1 : 0;
        s.r[6] = Writer;
    } else if (mode >= 9) {
        s.r[3] = 0x12345678;
        s.r[4] = mode == 10;
        s.r[5] = Writer;
    } else {
        s.r[3] = 'M';
        s.r[4] = 'E';
        s.r[5] = 'S';
        s.r[6] = 'H';
        s.r[7] = 0x12345678;
        s.r[8] = (mode == 6 || mode == 8) ? 1 : 0;
        s.r[9] = Writer;
    }
    if (requested == 11)
        entry = 0x82bd7d00u;
    else if (requested == 12)
        entry = 0x82bd7e70u;
    else if (requested == 13)
        entry = 0x82bd7ff0u;
    Guest expected, actual;
    auto om = before.Memory();
    memory = &om;
    guest = &expected;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (requested == 11)
        __imp__sub_82BD7D00(c, before.Bytes());
    else if (requested == 12)
        __imp__sub_82BD7E70(c, before.Bytes());
    else if (requested == 13)
        __imp__sub_82BD7FF0(c, before.Bytes());
    else if (mode < 2)
        __imp__sub_82BADA70(c, before.Bytes());
    else if (mode < 5)
        __imp__sub_82BADCD0(c, before.Bytes());
    else if (mode < 7)
        __imp__sub_82BADD60(c, before.Bytes());
    else if (mode < 9)
        __imp__sub_82BD8078(c, before.Bytes());
    else
        __imp__sub_82BD7DB0(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    memory = nullptr;
    guest = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!mesh_stream_write61::Apply(entry, m, {actual, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events ||
        csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("mesh stream Full72/RAM/CSR/callback mismatch mode " +
                                 std::to_string(mode));
    if (requested == 11) {
        if (m.ReadU32(Writer + 4) != 2 || m.ReadU16(Buffer) != 0x7856)
            throw std::runtime_error("virtual halfword endian bytes");
        return;
    }
    const auto size = (mode < 2 || mode >= 9) ? 4u : mode == 4 ? 0u : mode < 5 ? 8u : 12u;
    if (m.ReadU32(Writer + 4) != size)
        throw std::runtime_error("mesh stream output size");
    if (mode < 4) {
        bool swap = mode == 1 || mode == 3;
        if (m.ReadU32(Buffer) != (swap ? 0x0000c03fu : 0x3fc00000u))
            throw std::runtime_error("mesh stream float bytes");
        if (mode >= 2 && m.ReadU32(Buffer + 4) != (swap ? 0x000020c0u : 0xc0200000u))
            throw std::runtime_error("mesh stream span bytes");
    }
    if (mode >= 5 && mode < 9 &&
        (m.ReadU32(Buffer) != (mode >= 7 ? (mode == 8 ? 0x49434501u : 0x49434500u)
                                         : (mode == 6 ? 0x4e585301u : 0x4e585300u)) ||
         m.ReadU32(Buffer + 4) != 0x4d455348u ||
         m.ReadU32(Buffer + 8) != ((mode == 6 || mode == 8) ? 0x78563412u : 0x12345678u) ||
         s.r[3] != 1))
        throw std::runtime_error("mesh section header bytes");
    if (mode >= 9 && m.ReadU32(Buffer) != (mode == 10 ? 0x78563412u : 0x12345678u))
        throw std::runtime_error("mesh scalar word bytes");
}
} // namespace mesh_stream_oracle
void MeshStreamIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    mesh_stream_oracle::guest->CallIndirect(e, *mesh_stream_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void MeshStreamSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mesh_stream_oracle::memory;
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
}
void MeshStreamRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mesh_stream_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 14; ++i)
            mesh_stream_oracle::Check(i);
        std::puts("PASS mesh-stream-write61 14 original-upper/shared-concrete-writer cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
