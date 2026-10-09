#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_support_stream61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/serialization_control61.h"
namespace support_stream_oracle {
using Registers = mesh_support_stream61::Registers;
constexpr GuestAddress Writer = 0x30000, Table = 0x31000, Buffer = 0x32000, Input = 0x33000,
                       Adapter = 0x34000, Source = 0x35000, Second = 0x36000;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x832dc000, 0x2000}}};
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
void Check(unsigned mode) {
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Writer, Table);
        m.WriteU32(Writer + 8, 256);
        m.WriteU32(Writer + 12, Buffer);
        m.WriteU32(Table + 28, 0x82bde331);
        m.WriteU32(Table + 36, 0x82bde3c1);
        m.WriteU32(Table + 48, 0x82bde49b);
        m.WriteU32(Adapter, 0xabcdef00);
        m.WriteU32(Adapter + 4, Source);
        m.WriteU32(Adapter + 8, Source);
        m.WriteU32(Source + 4, 7);
        m.WriteU32(Source + 8, 3);
        m.WriteU32(Source + 24, Input);
        m.WriteU32(Source + 28, Second);
        for (unsigned i = 0; i < 3; ++i) {
            m.WriteU8(Input + i, 10 + i);
            m.WriteU8(Second + i, 20 + i);
        }
        m.WriteU32(0x832dc180, mode == 3 ? 0 : 1);
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
    s.r[4] = mode < 2 ? Source : Writer;
    Guest expected, actual;
    auto om = before.Memory();
    memory = &om;
    guest = &expected;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    if (mode == 0)
        __imp__sub_82BB3408(c, before.Bytes());
    else if (mode == 1)
        __imp__sub_82BB3420(c, before.Bytes());
    else
        __imp__sub_82BB36F0(c, before.Bytes());
    memory = nullptr;
    guest = nullptr;
    auto m = after.Memory();
    auto entry = mode == 0 ? 0x82bb3408u : mode == 1 ? 0x82bb3420u : 0x82bb36f0u;
    if (!mesh_support_stream61::Apply(entry, m, {actual, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events)
        throw std::runtime_error("support stream Full72/RAM/callback mismatch mode " +
                                 std::to_string(mode));
    if (mode < 2) {
        if (m.ReadU32(Adapter) != (mode == 0 ? 0x820d6280u : 0x820d6258u) ||
            m.ReadU32(Adapter + 4) != Source || m.ReadU32(Adapter + 8) != Source ||
            !actual.events.empty())
            throw std::runtime_error("borrowed support adapter layout");
    } else {
        if (m.ReadU32(Writer + 4) != 38 || s.r[3] != 1 ||
            m.ReadU32(Buffer) != (mode == 3 ? 0x49434501u : 0x49434500u) ||
            m.ReadU32(Buffer + 4) != 0x5355504du || m.ReadU32(Buffer + 12) != m.ReadU32(Buffer) ||
            m.ReadU32(Buffer + 16) != 0x47415553u ||
            m.ReadU32(Buffer + 24) != (mode == 3 ? 0x07000000u : 7u) ||
            m.ReadU32(Buffer + 28) != (mode == 3 ? 0x03000000u : 3u))
            throw std::runtime_error("support nested header/count bytes");
        for (unsigned i = 0; i < 3; ++i)
            if (m.ReadU8(Buffer + 32 + i) != 10 + i || m.ReadU8(Buffer + 35 + i) != 20 + i)
                throw std::runtime_error("support dual byte arrays");
    }
}
} // namespace support_stream_oracle
void SupportStreamIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    support_stream_oracle::guest->CallIndirect(e, *support_stream_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void SupportStreamLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *support_stream_oracle::memory;
    auto d = mesh_stream_write61::Dependencies{*support_stream_oracle::guest,
                                               support_stream_oracle::native};
    if (e == 0x82b9cb70u)
        (void)serialization_control61::Apply(e, m, d, s);
    else
        (void)mesh_stream_write61::Apply(e, m, d, s);
    crt_full_oracle::ToPpc(c, s);
}
void SupportStreamSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *support_stream_oracle::memory;
    for (unsigned i = 29; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void SupportStreamRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *support_stream_oracle::memory;
    for (unsigned i = 29; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 4; ++i)
            support_stream_oracle::Check(i);
        std::puts("PASS mesh-support-stream61 4 original-upper/shared-concrete-output cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
