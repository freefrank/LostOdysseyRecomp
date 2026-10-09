#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_edge_flags61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <cstdlib>
#include <fstream>
#include <set>
namespace mesh_flags_oracle {
using Registers = mesh_edge_flags61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Owner = 0x30000, Pairs = 0x31000, Mapping = 0x32000, Records = 0x33000,
                       Incidence = 0x34000, Input = 0x35000, Positions = 0x36000,
                       AllocatorTable = 0x37000, Allocate = 0x2000, Free = 0x2004,
                       Constants = 0x83214e88;
constexpr std::array<test::Region, 4> Regions{
    {{0, 0x120000}, {0x82000000, 0x10000}, {0x83214000, 0x3000}, {0x832df000, 0x1000}}};
std::array<unsigned char, 184> constants{};
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    std::set<GuestAddress> live;
    unsigned allocations = 0;
    void CallIndirect(GuestAddress e, GuestMemory &, Registers &s) override {
        std::array<std::uint64_t, 73> ev{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), ev.begin());
        ev.back() = e;
        events.push_back(ev);
        if (e == Allocate) {
            if (s.r[4] > 4096)
                throw std::runtime_error("edge allocation size");
            s.r[3] = 0x90000 + 4096 * allocations++;
            live.insert(std::uint32_t(s.r[3]));
        } else if (e == Free) {
            if (!live.erase(std::uint32_t(s.r[4])))
                throw std::runtime_error("edge unknown free");
            s.r[3] = 0;
        } else
            throw std::runtime_error("edge allocator callback");
        s.r[8] ^= 0x1234u;
        s.cr7.eq ^= 1;
    }
};
struct Sync final : diagnostic_lock61::SynchronizationServices {
    std::uint32_t LoadReservedWord(GuestAddress, GuestMemory &) override {
        throw std::runtime_error("unexpected diagnostic reservation");
    }
    bool CompareExchangeWord(GuestAddress, std::uint32_t, std::uint32_t, GuestMemory &) override {
        throw std::runtime_error("unexpected diagnostic CAS");
    }
    void EnterCriticalSection(GuestMemory &, Registers &, Machine &) override {
        throw std::runtime_error("unexpected diagnostic lock");
    }
    void LeaveCriticalSection(GuestMemory &, Registers &, Machine &) override {
        throw std::runtime_error("unexpected diagnostic unlock");
    }
};
struct DiagnosticGuest final : crt_narrow_formatter61::GuestServices {
    void CallIndirect(GuestAddress, GuestMemory &, Registers &) override {
        throw std::runtime_error("unexpected diagnostic output");
    }
    void CallOutput(GuestMemory &, Registers &) override {
        throw std::runtime_error("unexpected formatter output");
    }
};

struct Environment {
    sort_engine61_oracle::Environment accepted;
    Guest guest;
    Sync sync;
    DiagnosticGuest diagnostic;
    Machine machine{0x020a8020u, 0xcafebabe11223344ull};
    explicit Environment(test::GuestWindow &w) : accepted(w) {}
    mesh_edge_flags61::Dependencies Deps() {
        return {{{guest, accepted.Deps().sort.accepted}, accepted.fp},
                {{accepted.Deps().sort.accepted, diagnostic}, sync, machine}};
    }
};
Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    auto d = env.Deps();
    switch (e) {
    case 0x82bd0798u:
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
        break;
    case 0x82bd92c0u:
    case 0x822da388u:
        (void)mesh_geometry_math61::Apply(e, m, env.accepted.fp, s);
        break;
    case 0x82b7bc40u:
        crt_reader_chain61::ApplySupport_B7BC40(m, d.engine.sort.accepted, s);
        break;
    case 0x82b7a0b0u:
        (void)crt_copy_full_context::Apply(e, m, s);
        break;
    case 0x82b9d328u:
        (void)diagnostic_format_routes61::Apply(e, m, d.diagnostics, s);
        break;
    default:
        throw std::runtime_error("edge lower");
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
        m.WriteU32(0x83216624, AllocatorTable);
        m.WriteU32(AllocatorTable, Allocate | 1);
        m.WriteU32(AllocatorTable + 12, Free | 3);
        m.WriteU32(Owner, 5);
        m.WriteU32(Owner + 4, Pairs);
        m.WriteU32(Owner + 8, 2);
        m.WriteU32(Owner + 12, Mapping);
        m.WriteU32(Owner + 16, Records);
        m.WriteU32(Owner + 20, Incidence);
        unsigned pairs[]{0, 1, 0, 2, 1, 2, 1, 3, 2, 3}, map[]{0, 2, 1, 2, 3, 4},
            counts[]{1, 1, 2, 1, 1}, offsets[]{0, 1, 2, 4, 5}, triangles[]{0, 0, 0, 1, 1, 1},
            input[]{0, 1, 2, 2, 1, 3};
        for (unsigned i = 0; i < 10; ++i)
            m.WriteU32(Pairs + 4 * i, pairs[i]);
        for (unsigned i = 0; i < 6; ++i) {
            m.WriteU32(Mapping + 4 * i, map[i]);
            m.WriteU32(Incidence + 4 * i, triangles[i]);
            if (mode == 2)
                m.WriteU16(Input + 2 * i, input[i]);
            else
                m.WriteU32(Input + 4 * i, input[i]);
        }
        for (unsigned i = 0; i < 5; ++i) {
            m.WriteU16(Records + 8 * i + 2, counts[i]);
            m.WriteU32(Records + 8 * i + 4, offsets[i]);
        }
        float points[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, mode == 0 ? 0.f : -1.f};
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU32(Positions + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        for (unsigned i = 0; i < constants.size(); ++i)
            m.WriteU8(Constants + i, constants[i]);
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = 2;
    s.r[5] = mode == 2 ? 0 : Input;
    s.r[6] = mode == 2 ? Input : 0;
    s.r[7] = Positions;
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(mode == 2 ? 2.0 : 0.1);
    auto om = before.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BBD4E8(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    original = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!mesh_edge_flags61::Apply(0x82bbd4e8u, m, actual.Deps(), s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("mesh flags Full72/RAM/host/callback mismatch mode " +
                                 std::to_string(mode));
    if (s.r[3] != 1 || actual.guest.allocations != 2 || !actual.guest.live.empty())
        throw std::runtime_error("mesh flags temporary lifetime");
    unsigned map[]{0, 2, 1, 2, 3, 4};
    for (unsigned i = 0; i < 6; ++i) {
        unsigned flags = (map[i] == 2 && mode != 1) ? 0x40000000u : 0xc0000000u;
        if (m.ReadU32(Mapping + 4 * i) != (map[i] | flags))
            throw std::runtime_error("mesh side/vertex flags");
    }
    for (unsigned i = 0; i < 5; ++i)
        if (m.ReadU16(Records + 8 * i) != (i == 2 && mode != 1 ? 0 : 1))
            throw std::runtime_error("mesh edge flags");
}
} // namespace mesh_flags_oracle
void MeshFlagsIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    mesh_flags_oracle::original->guest.CallIndirect(e, *mesh_flags_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void MeshFlagsLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    mesh_flags_oracle::Lower(e, *mesh_flags_oracle::memory, *mesh_flags_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void MeshFlagsSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mesh_flags_oracle::memory;
    for (unsigned i = 14; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void MeshFlagsRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mesh_flags_oracle::memory;
    for (unsigned i = 14; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
void MeshFlagsSaveFp(PPCContext &c, std::uint8_t *) {
    c.fpscr.disableFlushMode();
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mesh_flags_oracle::memory;
    for (unsigned i = 25; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[12] - 8 * (32 - i)), s.fpr_bits[i]);
}
void MeshFlagsRestoreFp(PPCContext &c, std::uint8_t *) {
    c.fpscr.disableFlushMode();
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mesh_flags_oracle::memory;
    for (unsigned i = 25; i < 32; ++i)
        s.fpr_bits[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[12] - 8 * (32 - i)));
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        const char *path = std::getenv("LO_MESH_MATH_CONSTANTS");
        if (!path)
            throw std::runtime_error("LO_MESH_MATH_CONSTANTS private block required");
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(mesh_flags_oracle::constants.data()), 184);
        if (f.gcount() != 184)
            throw std::runtime_error("private math constant block size");
        for (unsigned i = 0; i < 3; ++i)
            mesh_flags_oracle::Check(i);
        std::puts("PASS mesh-edge-flags61 3 original-upper/shared-concrete-geometry cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
