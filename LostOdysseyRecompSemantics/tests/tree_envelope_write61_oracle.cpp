#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_close_next61.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/mesh_valence_stream61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/tree_envelope_write61.h"
#include "lo_semantics/tree_flat_write61.h"
namespace envelope_oracle {
using Registers = tree_envelope_write61::Registers;
constexpr GuestAddress Writer = 0x30000, Node = 0x31000, Data = 0x32000, Owner = 0x33000,
                       Strategy = 0x34000, Table = 0x35000, Record = 0x36000, First = 0x37000,
                       Second = 0x38000;
constexpr std::array<test::Region, 1> Regions{{{0, 0x120000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
} native;
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    void CallIndirect(GuestAddress e, GuestMemory &m, Registers &s) override {
        std::array<std::uint64_t, 73> ev{};
        auto a = crt_full_oracle::Snapshot(s);
        std::copy(a.begin(), a.end(), ev.begin());
        ev.back() = e;
        events.push_back(ev);
        if (!tree_flat_write61::Apply(e, m, {*this, native}, s))
            throw std::runtime_error("envelope unexpected callback");
    }
};
Guest *guest = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Guest &g, Registers &s) {
    switch (e) {
    case 0x82bd0938u:
    case 0x82bd0fc8u:
        (void)crt_close_next61::Apply(e, m, g, s);
        break;
    case 0x82bd1050u:
        (void)crt_reader_units61::Apply(e, m, g, s);
        break;
    case 0x82badfa0u:
        (void)mesh_valence_stream61::Apply(e, m, {g, native}, s);
        break;
    default:
        (void)tree_scalar_write61::Apply(e, m, {g, native}, s);
        break;
    }
}
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Writer, Node);
        m.WriteU32(Node, Data);
        m.WriteU32(Node + 8, 2048);
        m.WriteU32(Owner + 8, 0);
        m.WriteU32(Owner + 16, mode == 2 ? 0 : Strategy);
        m.WriteU32(Owner + 20, 2);
        m.WriteU32(Owner + 24, First);
        m.WriteU32(Owner + 28, 2);
        m.WriteU32(Owner + 32, Second);
        m.WriteU32(Strategy, Table);
        m.WriteU32(Table + 20, 0x82bdd869);
        m.WriteU32(Strategy + 4, 1);
        m.WriteU32(Strategy + 8, Record);
        for (unsigned i = 0; i < 6; ++i)
            m.WriteU32(Record + 4 * i, std::bit_cast<std::uint32_t>(float(i)));
        m.WriteU32(Record + 24, 1);
        m.WriteU32(First, 1);
        m.WriteU32(First + 4, mode == 3 ? 2 : 300);
        m.WriteU32(Second, 1);
        m.WriteU32(Second + 4, 70000);
    };
    seed(before);
    seed(after);
    Guest expected, actual;
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.lr = 0x9988776681234567ull;
    s.cached_fp_control = 0x9fc0;
    s.xer_so = 1;
    s.r[3] = Owner;
    s.r[4] = mode == 1;
    s.r[5] = Writer;
    if (mode == 3) {
        s.r[3] = 2;
        s.r[4] = 2;
        s.r[5] = First;
        s.r[6] = Writer;
        s.r[7] = 0;
    }
    auto om = before.Memory();
    memory = &om;
    guest = &expected;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode == 3)
        __imp__sub_82BD8550(c, before.Bytes());
    else
        __imp__sub_82BD1BF8(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    memory = nullptr;
    guest = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)tree_envelope_write61::Apply(mode == 3 ? 0x82bd8550u : 0x82bd1bf8u, m, {actual, native},
                                       s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events ||
        csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("tree envelope Full72/RAM/CSR/events " + std::to_string(mode));
    if (mode == 3) {
        if (m.ReadU32(Node + 4) != 2 || m.ReadU8(Data) != 1 || m.ReadU8(Data + 1) != 2)
            throw std::runtime_error("adaptive byte payload");
        return;
    }
    unsigned pos = 0;
    auto read = [&](unsigned width) {
        unsigned v = 0;
        for (unsigned i = 0; i < width; ++i) {
            unsigned b = m.ReadU8(Data + pos++);
            if (mode == 1)
                v |= b << (8 * i);
            else
                v = (v << 8) | b;
        }
        return v;
    };
    for (unsigned b : {'O', 'P', 'C'})
        if (read(1) != b)
            throw std::runtime_error("OPC tag");
    if (read(1) != (mode == 1) || read(4) != 1 || read(4) != 0)
        throw std::runtime_error("OPC metadata");
    if (mode == 2) {
        if (s.r[3] || m.ReadU32(Node + 4) != 12)
            throw std::runtime_error("missing-strategy failure");
        return;
    }
    if (read(4) != 1)
        throw std::runtime_error("strategy count");
    for (unsigned i = 0; i < 9; ++i)
        if (read(4) != m.ReadU32(Record + 4 * i))
            throw std::runtime_error("concrete strategy bytes");
    for (unsigned b : {'H', 'B', 'M'})
        if (read(1) != b)
            throw std::runtime_error("HBM tag");
    if (read(1) != (mode == 1) || read(4) != 0 || read(4) != 2 || read(4) != 300)
        throw std::runtime_error("HBM metadata");
    if (read(2) != 1 || read(2) != 300 || read(4) != 2 || read(4) != 70000 || read(4) != 1 ||
        read(4) != 70000 || s.r[3] != 1 || pos != m.ReadU32(Node + 4))
        throw std::runtime_error("adaptive map payload");
}
} // namespace envelope_oracle
void EnvelopeLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    envelope_oracle::Lower(e, *envelope_oracle::memory, *envelope_oracle::guest, s);
    crt_full_oracle::ToPpc(c, s);
}
void EnvelopeIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    envelope_oracle::guest->CallIndirect(e, *envelope_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void EnvelopeSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *envelope_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void EnvelopeRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *envelope_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 4; ++i)
            envelope_oracle::Check(i);
        std::puts("PASS tree-envelope-write61 4 original-local-chain/concrete-strategy cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
