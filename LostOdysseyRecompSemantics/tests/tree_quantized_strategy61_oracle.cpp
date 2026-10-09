#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_quantized_strategy61.h"
#include <bit>
#include <limits>
#include <set>
namespace quantized_oracle {
using Registers = tree_quantized_strategy61::Registers;
constexpr GuestAddress Owner = 0x30000, Tree = 0x31000, Nodes = 0x32000, Ids = 0x33000,
                       Table = 0x34000, Old = 0x90000, New = 0x91000, Final = 0x92000;
constexpr GuestAddress Allocate = 0x2000, Free = 0x2004;
constexpr std::array<test::Region, 5> Regions{{{0, 0x120000},
                                               {0x82000000, 0x10000},
                                               {0x82218000, 0x1000},
                                               {0x8201f000, 0x1000},
                                               {0x83216000, 0xca000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    unsigned allocations = 0;
    bool compact = false;
    std::set<GuestAddress> live{Old};
    std::vector<std::array<std::uint64_t, 73>> events;
    void CallIndirect(GuestAddress target, GuestMemory &, Registers &s) override {
        std::array<std::uint64_t, 73> e{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), e.begin());
        e[72] = target;
        events.push_back(e);
        if (target == Allocate) {
            if (s.r[4] != (compact ? (allocations ? 24u : 36u) : (allocations ? 76u : 112u)) ||
                s.r[5] != (compact ? (allocations ? 38u : 31u) : (allocations ? 32u : 30u)))
                throw std::runtime_error("quantized allocation contract");
            s.r[3] = allocations++ ? Final : New;
            live.insert(Address(s.r[3]));
        } else if (target == Free) {
            if (!live.erase(Address(s.r[4])))
                throw std::runtime_error("strategy ownership");
            s.r[3] = 0;
        } else
            throw std::runtime_error("unexpected strategy target");
        s.r[8] ^= 0x123456789abcdef0ull;
        s.fpr_bits[7] ^= 0x100u;
        s.cr7.lt ^= 1u;
    }
};
GuestMemory *originalMemory = nullptr;
Guest *originalGuest = nullptr;
void Check(unsigned mode, bool compact) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    const auto seed = [&](test::GuestWindow &w) {
        w.Fill(0xa5u);
        auto m = w.Memory();
        m.WriteU32(Owner + 4, 1);
        m.WriteU32(Owner + 8, Old + 4);
        m.WriteU32(Old, 1);
        m.WriteU32(Tree + 4, Nodes);
        m.WriteU32(Tree + 16, 3);
        m.WriteU32(Nodes + 36, 2);
        for (unsigned n = 0; n < 3; ++n) {
            auto node = Nodes + 40 * n;
            constexpr float bounds[3][6]{
                {-4, 2, -8, 6, 10, 4}, {-3, 3, -6, 1, 6, 0}, {1, 4, -2, 5, 9, 3}};
            const auto &b = bounds[n];
            for (unsigned i = 0; i < 6; ++i)
                m.WriteU32(node + 4 * i, std::bit_cast<std::uint32_t>(b[i]));
            m.WriteU32(node + 24, 1);
            m.WriteU32(node + 32, Ids + 4 * n);
            m.WriteU32(Ids + 4 * n, 10 + n);
        }
        m.WriteU32(Nodes + 24, (Nodes + 40) | 1u);
        m.WriteU32(0x82000d64, std::bit_cast<std::uint32_t>(-std::numeric_limits<float>::max()));
        m.WriteU32(0x82000e50, std::bit_cast<std::uint32_t>(0.f));
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x8221864c, std::bit_cast<std::uint32_t>(32767.f));
        m.WriteU8(0x83216670, mode == 2 ? 1 : 0);
        m.WriteU32(0x8201f9f0, std::bit_cast<std::uint32_t>(0.5f));
        m.WriteU32(0x832df554, 0);
        m.WriteU32(0x83216624, Table);
        m.WriteU32(Table, Allocate | 1);
        m.WriteU32(Table + 12, Free | 3);
    };
    seed(before);
    seed(after);
    Guest expected, actual;
    expected.compact = actual.compact = compact;
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = Owner;
    s.r[4] = mode ? Tree : 0;
    s.lr = 0x9988776681234567ull;
    s.cached_fp_control = 0x9fc0;
    s.xer_so = 1;
    const auto initial = s;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    auto om = before.Memory();
    originalMemory = &om;
    originalGuest = &expected;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (compact)
        __imp__sub_82BDD1E8(c, before.Bytes());
    else
        __imp__sub_82BDC208(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    originalMemory = nullptr;
    originalGuest = nullptr;
    auto m = after.Memory();
    Native native;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!tree_quantized_strategy61::Apply(compact   ? 0x82bdd1e8u
                                          : compact ? 0x82bdd1e8u
                                                    : 0x82bdc208u,
                                          m, {actual, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events ||
        expected.live != actual.live || host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("strategy Full72/RAM/callback/host mismatch");
    if (s.r[3] != (mode ? 1u : 0u))
        throw std::runtime_error("strategy result");
    if (mode) {
        const auto output = m.ReadU32(Owner + 8);
        if (output != Final + 4 || m.ReadU32(Owner + 4) != (compact ? 1u : 3u) ||
            m.ReadU32(output + 12) != (compact ? 0xc000000bu : 1u) ||
            m.ReadU32(output + 16) != (compact ? 0u : 2u) ||
            (!compact && m.ReadU32(output + 20) != 2u))
            throw std::runtime_error("quantized topology/owner");
        if (actual.live != std::set<GuestAddress>{Final} || actual.events.size() != 4u)
            throw std::runtime_error("quantized temporary/final allocation lifetime");
        for (unsigned axis = 0; axis < 3; ++axis) {
            const auto center = std::bit_cast<std::int16_t>(m.ReadU16(output + 2 * axis));
            const auto extent = m.ReadU16(output + 6 + 2 * axis);
            const float cs = std::bit_cast<float>(m.ReadU32(Owner + 12 + 4 * axis)),
                        es = std::bit_cast<float>(m.ReadU32(Owner + 24 + 4 * axis));
            constexpr float lo[]{-4, 2, -8}, hi[]{6, 10, 4};
            const float decodedCenter = center * cs, decodedExtent = extent * es;
            if (mode == 2 && (decodedCenter - decodedExtent > lo[axis] ||
                              decodedCenter + decodedExtent < hi[axis]))
                throw std::runtime_error("conservative bounds lost coverage");
        }
    } else if (!actual.events.empty())
        throw std::runtime_error("null tree allocation");
}
} // namespace quantized_oracle
void QuantizedSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *quantized_oracle::originalMemory;
    for (unsigned i = first; i < 32; ++i)
        WriteU64(m, Address(s.r[1] - 16u - 8u * (31u - i)), s.r[i]);
    m.WriteU32(Address(s.r[1] - 8u), Address(s.r[12]));
}
void QuantizedRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *quantized_oracle::originalMemory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = ReadU64(m, Address(s.r[1] - 16u - 8u * (31u - i)));
    s.r[12] = m.ReadU32(Address(s.r[1] - 8u));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
void QuantizedAllocator(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, *quantized_oracle::originalMemory,
                                                    *quantized_oracle::originalGuest, s);
    crt_full_oracle::ToPpc(c, s);
}
void QuantizedIndirect(std::uint32_t target, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    quantized_oracle::originalGuest->CallIndirect(target, *quantized_oracle::originalMemory, s);
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 3; ++mode)
            for (bool compact : {false, true})
                quantized_oracle::Check(mode, compact);
        std::puts("PASS tree-quantized-strategy61 6 original binder/recursive-lower cases "
                  "(20/24-byte formats)");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
