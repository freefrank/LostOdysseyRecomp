#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_bounds_select61.h"
#include "lo_semantics/mesh_mass_cache61.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <set>
namespace select_oracle {
using Registers = mesh_bounds_select61::Registers;
constexpr GuestAddress Output = 0x30000, Points = 0x31000, Context = 0x32000, Table = 0x33000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr std::array<test::Region, 7> Regions{{{0, 0x120000},
                                               {0x82000000, 0x10000},
                                               {0x8201f000, 0x1000},
                                               {0x820d6000, 0x1000},
                                               {0x821ba000, 0x1000},
                                               {0x82bc9000, 0x1000},
                                               {0x832df000, 0x1000}}};
std::array<unsigned char, 8> constants{};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
} native;
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::set<GuestAddress> live;
    std::vector<std::array<std::uint64_t, 73>> events;
    void CallIndirect(GuestAddress t, GuestMemory &, Registers &s) override {
        auto snap = crt_full_oracle::Snapshot(s);
        std::array<std::uint64_t, 73> e{};
        std::copy(snap.begin(), snap.end(), e.begin());
        e[72] = t;
        events.push_back(e);
        if (t == Allocate) {
            s.r[3] = 0x50000;
            live.insert(0x50000);
        } else if (t == Free) {
            if (!live.erase(Address(s.r[4])))
                throw std::runtime_error("unknown sphere free");
            s.r[3] = 0;
        } else
            throw std::runtime_error("unknown sphere callback");
        s.r[8] ^= 0x1234;
        s.cr7.lt ^= 1;
    }
};
Guest *original = nullptr;
GuestMemory *memory = nullptr;
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        constexpr float points[]{-2, 1, 0, 6, 1, 0, 2, 5, 0, 2, -3, 0, 2, 1, 8, 1, 1, 0, 2, 1, -4};
        for (unsigned i = 0; i < 21; ++i)
            m.WriteU32(Points + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        if (mode == 2) {
            constexpr float tetra[]{0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 0, 1, 0, 0, 0, 1};
            for (unsigned i = 0; i < 21; ++i)
                m.WriteU32(Points + 4 * i, std::bit_cast<std::uint32_t>(tetra[i]));
        }
        for (unsigned i = 0; i < 4; ++i) {
            m.WriteU8(0x820d6a18 + i, constants[i]);
            m.WriteU8(0x82000d70 + i, constants[i + 4]);
        }
        m.WriteU32(0x82000d64, 0xff7fffff);
        m.WriteU32(0x82000e0c, 0x7f7fffff);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x8201f9f0, std::bit_cast<std::uint32_t>(.5f));
        m.WriteU32(0x821baa74, std::bit_cast<std::uint32_t>(2.f));
        constexpr GuestAddress cases[]{0x82bc9998, 0x82bc99a8, 0x82bc99c4, 0x82bc9aa8, 0x82bc9ac0};
        for (unsigned i = 0; i < 5; ++i)
            m.WriteU32(0x82bc9984 + 4 * i, cases[i]);
        m.WriteU32(0x832df548, Context);
        m.WriteU32(Context, Table);
        m.WriteU32(Table + 8, Allocate | 1);
        m.WriteU32(Table + 20, Free | 3);
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = Output;
    s.r[4] = mode == 3 ? 0 : mode == 1 ? 4 : 7;
    s.r[5] = mode == 4 ? 0 : Points;
    auto count = Address(s.r[4]);
    const GuestAddress entry = mode == 0 ? 0x82bc9af0u : 0x82bc9c68u;
    if (mode == 0) {
        s.r[4] = Points;
        s.r[5] = 7;
    }
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    Guest expected, actual;
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode == 0)
        __imp__sub_82BC9AF0(c, before.Bytes());
    else
        __imp__sub_82BC9C68(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_bounds_select61::Apply(entry, m, {actual, native}, s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || expected.events != actual.events ||
        !expected.live.empty() || !actual.live.empty() || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "sphere select%u Full%d RAM%d events%d host%d\n", mode, a == b,
                     before.EqualCommitted(after), expected.events == actual.events,
                     host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("sphere selection original mismatch");
    }
    if (mode >= 3) {
        if (s.r[3] || !actual.events.empty())
            throw std::runtime_error("invalid sphere input");
        return;
    }
    auto val = [&](GuestAddress p) { return std::bit_cast<float>(m.ReadU32(p)); };
    for (unsigned i = 0; i < count; ++i) {
        double sum = 0;
        for (unsigned axis = 0; axis < 3; ++axis) {
            double delta = val(Points + 12 * i + 4 * axis) - val(Output + 4 * axis);
            sum += delta * delta;
        }
        if (std::sqrt(sum) > val(Output + 12) + 1e-5)
            throw std::runtime_error("selected sphere enclosure");
    }
    if (mode == 1 && (s.r[3] != 1 || val(Output) != 2 || val(Output + 4) != 1 ||
                      val(Output + 8) != 0 || val(Output + 12) != 4))
        throw std::runtime_error("axis sphere fallback");
    if (mode == 2 && s.r[3] != 2)
        throw std::runtime_error("recursive sphere selected");
}
} // namespace select_oracle
void SelectIndirect(std::uint32_t t, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    select_oracle::original->CallIndirect(t, *select_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void SelectClassify(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    mesh_mass_cache61::Classify(*select_oracle::memory, select_oracle::native, s);
    crt_full_oracle::ToPpc(c, s);
}
void SelectGprSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *select_oracle::memory;
    for (unsigned i = 26; i < 32; ++i)
        WriteU64(m, Address(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(Address(s.r[1] - 8), Address(s.r[12]));
}
void SelectGprRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *select_oracle::memory;
    for (unsigned i = 26; i < 32; ++i)
        s.r[i] = ReadU64(m, Address(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(Address(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
void SelectFpSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    for (unsigned i = first; i < 32; ++i)
        WriteU64(*select_oracle::memory, Address(s.r[12] - 8 * (32 - i)), s.fpr_bits[i]);
}
void SelectFpRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    for (unsigned i = first; i < 32; ++i)
        s.fpr_bits[i] = ReadU64(*select_oracle::memory, Address(s.r[12] - 8 * (32 - i)));
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        auto path = std::getenv("LO_BOUNDS_CONSTANTS");
        if (!path)
            throw std::runtime_error("LO_BOUNDS_CONSTANTS required");
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(select_oracle::constants.data()), 8);
        if (f.gcount() != 8)
            throw std::runtime_error("bounds bundle size");
        for (unsigned mode = 0; mode < 5; ++mode)
            select_oracle::Check(mode);
        std::puts("PASS mesh-bounds-select61 5 complete original sphere graph / ownership cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
