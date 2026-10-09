#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/serialization_control61.h"
namespace serial_control_oracle {
using Registers = serialization_control61::Registers;
constexpr GuestAddress Writer = 0x30000, Table = 0x31000, Buffer = 0x32000, Context = 0x33000,
                       LogTable = 0x34000;
constexpr GuestAddress Logger = 0x2000, Log = 0x2004;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x832dc000, 0x4000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
Native native;
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    void CallIndirect(GuestAddress e, GuestMemory &m, Registers &s) override {
        std::array<std::uint64_t, 73> event{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), event.begin());
        event.back() = e;
        events.push_back(event);
        if (e == 0x82bde3c0u || e == 0x82bde498u) {
            (void)growable_output61::Apply(e, m, {*this, native}, s);
            return;
        }
        if (e == Logger)
            s.r[3] = Context;
        else if (e == Log)
            s.r[3] = 0x77;
        else
            throw std::runtime_error("serialization control callback");
        s.r[8] ^= 0x1234u;
        s.cr7.eq ^= 1u;
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
        m.WriteU32(Writer + 8, 64);
        m.WriteU32(Writer + 12, Buffer);
        m.WriteU32(Table + 36, 0x82bde3c1u);
        m.WriteU32(Table + 48, 0x82bde49bu);
        m.WriteU32(Context, LogTable);
        m.WriteU32(LogTable + 132, Logger | 1);
        m.WriteU32(LogTable + 8, Logger | 3);
        m.WriteU32(LogTable, Log | 1);
        if (mode == 4)
            m.WriteU32(0x832df588, Context);
        if (mode >= 5)
            m.WriteU32(0x832dc180, mode - 5);
        m.WriteU8(0x80000 - 16, 0x5a);
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = mode < 2 ? 0x12345678u : mode == 2 ? 0u : Context;
    s.r[4] = mode == 1 ? 1u : 0u;
    s.r[5] = Writer;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    Guest expected, actual;
    auto om = before.Memory();
    guest = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    if (mode < 2)
        __imp__sub_82BDE948(c, before.Bytes());
    else if (mode < 5)
        __imp__sub_82BDE9B8(c, before.Bytes());
    else
        __imp__sub_82B9CB70(c, before.Bytes());
    guest = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    auto entry = mode < 2 ? 0x82bde948u : mode < 5 ? 0x82bde9b8u : 0x82b9cb70u;
    if (!serialization_control61::Apply(entry, m, {actual, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events)
        throw std::runtime_error("serialization control Full72/RAM/callback mismatch");
    if (mode < 2) {
        if (m.ReadU32(Buffer) != (mode ? 0x78563412u : 0x12345678u) || m.ReadU32(Writer + 4) != 4u)
            throw std::runtime_error("endian virtual output");
    } else if (mode < 5) {
        if (s.r[3] != (mode == 3 ? 1u : 0u) ||
            m.ReadU32(0x832df588) != (mode == 2 ? 0u : Context) ||
            actual.events.size() != (mode == 4 ? 3u : 0u))
            throw std::runtime_error("global registration");
    } else {
        constexpr unsigned value[]{1, 0, 0, 0x5a};
        if (s.r[3] != value[mode - 5])
            throw std::runtime_error("endian mode result");
    }
}
} // namespace serial_control_oracle
void SerialControlIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    serial_control_oracle::guest->CallIndirect(e, *serial_control_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 9; ++mode)
            serial_control_oracle::Check(mode);
        std::puts("PASS serialization-control61 9 original-upper/shared-writer cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
