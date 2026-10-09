#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_sort_float61.h"
#include "lo_semantics/mesh_indexed_channels61.h"
#include "lo_semantics/mesh_indexed_workspace61.h"
#include "lo_semantics/mesh_polygon_collect61.h"
#include "lo_semantics/mesh_vertex_dedup61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <set>
namespace channels_oracle {
using Registers = mesh_indexed_channels61::Registers;

constexpr GuestAddress Owner = 0x30000, Input = 0x31000, AllocatorTable = 0x32000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr auto Regions = sort_engine61_oracle::EngineRegions;
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
struct Environment {
    sort_engine61_oracle::Environment accepted;
    Guest guest;
    explicit Environment(test::GuestWindow &w) : accepted(w) {}
    mesh_indexed_channels61::Dependencies Deps() {
        return {{guest, accepted.Deps().sort.accepted}, accepted.fp};
    }
};
Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    switch (e) {
    case 0x82bd0798u:
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
        break;
    case 0x82bd2a08u:
    case 0x82bd2c08u:
        (void)object_sort_support61::Apply(e, m, {env.guest, env.accepted.fp}, s);
        break;
    case 0x82b7a0b0u:
        (void)crt_copy_full_context::Apply(e, m, s);
        break;
    case 0x82bd2870u:
        (void)reader_buffer_growth61::Apply(e, m, {env.guest, env.accepted.fp}, s);
        break;
    case 0x82bc2d28u:
        mesh_vertex_dedup61::Initialize(m, s);
        break;
    case 0x82bc2dd0u:
    case 0x82bc38e0u:
        (void)mesh_vertex_dedup61::Apply(e, m, env.Deps(), s);
        break;
    default:
        throw std::runtime_error("indexed channels lower");
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
        m.WriteU32(0x821baa74, std::bit_cast<std::uint32_t>(2.f));
        if (mode >= 4) {
            m.WriteU32(Owner, mode == 5 ? 8 : 0);
            m.WriteU32(Owner + 8, mode == 5 ? 0x60000 : 0);
            m.WriteU32(Owner + 12, std::bit_cast<std::uint32_t>(2.f));
            for (unsigned i = 0; i < 3; ++i)
                m.WriteU32(Input + 4 * i, std::bit_cast<std::uint32_t>(float(i + 1)));
            return;
        }
        m.WriteU32(Owner + 212, 4);
        m.WriteU32(Owner + 224, 2);
        m.WriteU32(Owner + 228, 6);
        m.WriteU32(Owner + 236, 0x60000);
        m.WriteU32(Owner + 248, 0x61000);
        m.WriteU32(Owner + 252, 0x62000);
        m.WriteU8(Owner + 282, mode != 2);
        m.WriteU8(Owner + 289, mode == 1);
        constexpr float points[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU32(0x60000 + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        constexpr unsigned ids[]{0, 1, 2, 0, 2, 3};
        for (unsigned i = 0; i < 6; ++i) {
            m.WriteU32(0x61000 + 48 * (i / 3) + 12 + 4 * (i % 3), i);
            for (unsigned channel = 0; channel < 3; ++channel)
                m.WriteU32(0x62000 + 12 * i + 4 * channel, ids[i]);
        }
        m.WriteU32(0x61000 + 48 + 28, 1);
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    if (mode < 4) {
        expected.guest.live = {0x60000, 0x61000, 0x62000};
        actual.guest.live = expected.guest.live;
    } else if (mode == 5) {
        expected.guest.live = {0x60000};
        actual.guest.live = expected.guest.live;
    }
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = Input;
    auto om = before.Memory(), m = after.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode >= 4)
        __imp__sub_82BB3C00(c, before.Bytes());
    else if (mode == 3)
        __imp__sub_82BBEBE0(c, before.Bytes());
    else
        __imp__sub_82BBE948(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_indexed_channels61::Apply(mode >= 4   ? 0x82bb3c00u
                                         : mode == 3 ? 0x82bbebe0u
                                                     : 0x82bbe948u,
                                         m, actual.Deps(), s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "channels%u Full%d RAM%d events%d host%d\n", mode, a == b,
                     before.EqualCommitted(after), expected.guest.events == actual.guest.events,
                     host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("indexed channels original mismatch");
    }
    if (mode >= 4) {
        auto p = m.ReadU32(Owner + 8);
        if (m.ReadU32(Owner + 4) != 3)
            throw std::runtime_error("appended xyz count");
        for (unsigned i = 0; i < 3; ++i)
            if (m.ReadU32(p + 4 * i) != m.ReadU32(Input + 4 * i))
                throw std::runtime_error("appended xyz");
    } else if (mode == 3) {
        if (s.r[3] != 1 || m.ReadU32(Owner + 228) != 4)
            throw std::runtime_error("unique corner tuples");
        constexpr unsigned ids[]{0, 1, 2, 0, 2, 3};
        auto p = m.ReadU32(Owner + 252);
        for (unsigned i = 0; i < 6; ++i) {
            auto id = m.ReadU32(0x61000 + 48 * (i / 3) + 12 + 4 * (i % 3));
            for (unsigned j = 0; j < 3; ++j)
                if (m.ReadU32(p + 12 * id + 4 * j) != ids[i])
                    throw std::runtime_error("corner remap");
        }
    } else {
        if (s.r[3] != 1 || m.ReadU32(Owner + 212) != (mode == 0 ? 7u : 4u) ||
            m.ReadU32(0x61000 + 28) != (mode == 2 ? 0u : 0xffffffffu))
            throw std::runtime_error("split counts/marker");
        if (mode == 0) {
            auto p = m.ReadU32(Owner + 236);
            for (unsigned i = 0; i < 3; ++i) {
                if (m.ReadU32(0x62000 + 12 * i) != 4 + i)
                    throw std::runtime_error("split private position IDs");
                for (unsigned j = 0; j < 3; ++j)
                    if (m.ReadU32(p + 12 * (4 + i) + 4 * j) != m.ReadU32(p + 12 * i + 4 * j))
                        throw std::runtime_error("split copied positions");
            }
        }
    }
    auto os = crt_full_oracle::FromPpc(c);
    os.r[3] = Owner;
    s.r[3] = Owner;
    PPCFPSCRRegister{}.setcsr(os.cached_fp_control);
    if (mode >= 4)
        (void)object_sort_support61::Apply(0x82bd2c08u, om, {expected.guest, expected.accepted.fp},
                                           os);
    else
        (void)mesh_indexed_workspace61::Apply(0x82bbf590u, om, expected.Deps(), os);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode >= 4)
        (void)object_sort_support61::Apply(0x82bd2c08u, m, {actual.guest, actual.accepted.fp}, s);
    else
        (void)mesh_indexed_workspace61::Apply(0x82bbf590u, m, actual.Deps(), s);
    if (!actual.guest.live.empty() || !expected.guest.live.empty() ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events)
        throw std::runtime_error("channels complete teardown");
    original = nullptr;
    memory = nullptr;
}

void Export(unsigned mode) {
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
        m.WriteU32(0x821baa74, std::bit_cast<std::uint32_t>(2.f));
        m.WriteU8(Owner + 281, mode == 1);
        for (unsigned channel = 0; channel < 3; ++channel) {
            m.WriteU32(Owner + 236 + 4 * channel, 0x60000 + 256 * channel);
            m.WriteU32(Owner + 212 + 4 * channel, 2);
            m.WriteU8(Owner + 285 + channel, mode != 2);
            m.WriteU32(Owner + 80 + 16 * channel + 12, std::bit_cast<std::uint32_t>(2.f));
            for (unsigned i = 0; i < 6; ++i)
                m.WriteU32(0x60000 + 256 * channel + 4 * i,
                           std::bit_cast<std::uint32_t>(float(10 * channel + i + 1)));
        }
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    auto om = before.Memory(), m = after.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BBF208(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_indexed_channels61::Apply(0x82bbf208u, m, actual.Deps(), s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "export%u Full%d RAM%d events%d host%d\n", mode, a == b,
                     before.EqualCommitted(after), expected.guest.events == actual.guest.events,
                     host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("channel export mismatch");
    }
    if (s.r[3] != 1)
        throw std::runtime_error("channel export result");
    for (unsigned channel = 0; channel < 3; ++channel) {
        unsigned dims = channel == 1 && mode == 0 ? 2 : 3;
        auto desc = Owner + 80 + 16 * channel, p = m.ReadU32(desc + 8);
        if (m.ReadU32(desc + 4) != (mode == 2 ? 0 : 2 * dims))
            throw std::runtime_error("export packed dimensions");
        if (mode != 2)
            for (unsigned i = 0; i < 2 * dims; ++i)
                if (m.ReadU32(p + 4 * i) !=
                    m.ReadU32(0x60000 + 256 * channel + 12 * (i / dims) + 4 * (i % dims)))
                    throw std::runtime_error("export packed values");
    }
    auto os = crt_full_oracle::FromPpc(c);
    for (unsigned channel = 0; channel < 3; ++channel) {
        os.r[3] = Owner + 80 + 16 * channel;
        s.r[3] = os.r[3];
        PPCFPSCRRegister{}.setcsr(os.cached_fp_control);
        (void)object_sort_support61::Apply(0x82bd2c08u, om, {expected.guest, expected.accepted.fp},
                                           os);
        PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
        (void)object_sort_support61::Apply(0x82bd2c08u, m, {actual.guest, actual.accepted.fp}, s);
    }
    if (!actual.guest.live.empty() || !expected.guest.live.empty() ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events)
        throw std::runtime_error("export teardown");
    original = nullptr;
    memory = nullptr;
}
} // namespace channels_oracle
void ChannelsIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    channels_oracle::original->guest.CallIndirect(e, *channels_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void ChannelsLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    channels_oracle::Lower(e, *channels_oracle::memory, *channels_oracle::original, s);
    crt_full_oracle::ToPpc(c, s);
}
void ChannelsSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *channels_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void ChannelsRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *channels_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 6; ++i)
            channels_oracle::Check(i);
        for (unsigned i = 0; i < 3; ++i)
            channels_oracle::Export(i);
        std::puts("PASS mesh-indexed-channels61 9 original-local-chain/shared-concrete-buffer and "
                  "dedup cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
