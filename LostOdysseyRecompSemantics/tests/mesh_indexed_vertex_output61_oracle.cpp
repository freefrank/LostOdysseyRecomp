#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_sort_float61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/mesh_indexed_channels61.h"
#include "lo_semantics/mesh_indexed_compact61.h"
#include "lo_semantics/mesh_indexed_normals61.h"
#include "lo_semantics/mesh_indexed_remap61.h"
#include "lo_semantics/mesh_indexed_vertex_output61.h"
#include "lo_semantics/mesh_indexed_workspace61.h"
#include "lo_semantics/mesh_polygon_collect61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include "object_sort_engine61_oracle_fixture.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <set>
namespace vertex_output_oracle {
using Registers = mesh_indexed_workspace61::Registers;

constexpr GuestAddress Owner = 0x30000, Input = 0x31000, AllocatorTable = 0x32000,
                       Allocate = 0x2000, Free = 0x2004;
constexpr auto Regions = [] {
    auto r = sort_engine61_oracle::EngineRegions;
    r[1].size = 0x10000;
    r[3] = {0x83214000, 0x3000};
    return r;
}();
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
struct Environment {
    sort_engine61_oracle::Environment accepted;
    Guest guest;
    explicit Environment(test::GuestWindow &w) : accepted(w) {}
    mesh_indexed_workspace61::Dependencies Deps() {
        return {{guest, accepted.Deps().sort.accepted}, accepted.fp};
    }
};
Environment *original = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Environment &env, Registers &s) {
    switch (e) {
    case 0x82b7bc40u:
        crt_reader_chain61::ApplySupport_B7BC40(m, env.Deps().sort.accepted, s);
        break;
    case 0x82bc0058u:
        (void)mesh_indexed_compact61::Apply(e, m, env.Deps(), s);
        break;
    case 0x82bbed20u:
        (void)mesh_indexed_normals61::Apply(e, m, env.Deps(), s);
        break;
    case 0x82bbe948u:
    case 0x82bbebe0u:
    case 0x82bbf208u:
        (void)mesh_indexed_channels61::Apply(e, m, env.Deps(), s);
        break;
    case 0x82bc0930u:
        (void)mesh_indexed_vertex_output61::Apply(e, m, env.Deps(), s);
        break;
    case 0x82bd0798u:
        (void)crt_close_recursive_buffer_context::Apply(e, m, env.guest, s);
        break;
    case 0x82bd2c50u:
        (void)object_sort_support61::Apply(e, m, {env.guest, env.accepted.fp}, s);
        break;
    case 0x82bd2c78u:
        (void)crt_reader_follow61::Apply(e, m, env.guest, s);
        break;
    case 0x82bd2df0u:
        (void)crt_reader_bucket_sort61::Apply(e, m, env.Deps().sort, s);
        break;
    case 0x82bc0108u:
        (void)mesh_indexed_vertex_output61::Apply(e, m, env.Deps(), s);
        break;
    case 0x82bd2870u:
        (void)reader_buffer_growth61::Apply(e, m, {env.guest, env.accepted.fp}, s);
        break;
    case 0x82bd2a08u:
    case 0x82bd2c08u:
        (void)object_sort_support61::Apply(e, m, {env.guest, env.accepted.fp}, s);
        break;
    case 0x82bb3c00u:
        (void)mesh_indexed_channels61::Apply(e, m, env.Deps(), s);
        break;
    case 0x82bbfea8u:
        (void)mesh_indexed_remap61::Apply(e, m, env.Deps(), s);
        break;
    case 0x822da388u:
        (void)mesh_geometry_math61::Apply(e, m, env.accepted.fp, s);
        break;
    default:
        throw std::runtime_error("indexed vertex lower");
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
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        for (unsigned i = 0; i < 184; ++i)
            m.WriteU8(0x83214e88 + i, constants[i]);
        for (unsigned i = 0; i < 13; ++i)
            m.WriteU32(Owner + 16 * i + 12, std::bit_cast<std::uint32_t>(2.f));
        for (unsigned ch = 0; ch < 3; ++ch) {
            m.WriteU32(Owner + 212 + 4 * ch, 4);
            m.WriteU32(Owner + 236 + 4 * ch, 0x60000 + 0x1000 * ch);
            m.WriteU8(Owner + 285 + ch, mode == 1);
        }
        m.WriteU32(Owner + 224, 2);
        m.WriteU32(Owner + 228, 4);
        m.WriteU32(Owner + 248, 0x64000);
        m.WriteU32(Owner + 252, 0x65000);
        m.WriteU32(Owner + 256, 0x66000);
        m.WriteU32(Owner + 264, 0x67000);
        m.WriteU32(Owner + 268, 0x68000);
        m.WriteU32(Owner + 272, 0x69000);
        m.WriteU8(Owner + 281, mode == 1);
        m.WriteU8(Owner + 282, mode != 2);
        m.WriteU8(Owner + 284, mode != 2);
        m.WriteU8(Owner + 288, 1);
        m.WriteU8(Owner + 290, mode == 1);
        constexpr float xyz[]{0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 1};
        for (unsigned i = 0; i < 12; ++i) {
            m.WriteU32(0x60000 + 4 * i, std::bit_cast<std::uint32_t>(xyz[i]));
            m.WriteU32(0x61000 + 4 * i, std::bit_cast<std::uint32_t>(float(i + 1)));
            m.WriteU32(0x62000 + 4 * i, std::bit_cast<std::uint32_t>(float(i + 21)));
        }
        constexpr unsigned ids[]{0, 1, 2, 0, 2, 3};
        for (unsigned i = 0; i < 6; ++i)
            m.WriteU32(0x64000 + 48 * (i / 3) + 12 + 4 * (i % 3), ids[i]);
        for (unsigned i = 0; i < 4; ++i)
            for (unsigned ch = 0; ch < 3; ++ch)
                m.WriteU32(0x65000 + 12 * i + 4 * ch, i);
        m.WriteU32(0x64000 + 28, 1);
        m.WriteU32(0x64000 + 48 + 28, 1);
        m.WriteU32(0x64000 + 40, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x64000 + 48 + 32, std::bit_cast<std::uint32_t>(1.f));
        constexpr unsigned counts[]{2, 1, 2, 1}, prefix[]{0, 2, 3, 5}, adj[]{0, 1, 0, 0, 1, 1};
        for (unsigned i = 0; i < 4; ++i) {
            m.WriteU32(0x67000 + 4 * i, counts[i]);
            m.WriteU32(0x68000 + 4 * i, prefix[i]);
        }
        for (unsigned i = 0; i < 6; ++i)
            m.WriteU32(0x69000 + 4 * i, adj[i]);
        if (mode >= 4) {
            m.WriteU32(0x64000 + 24, mode == 5 ? 7 : 4);
            m.WriteU32(0x64000 + 48 + 24, 4);
            m.WriteU32(0x64000 + 28, mode == 6 ? 3 : 1);
            m.WriteU32(0x64000 + 48 + 28, 1);
            m.WriteU32(0x64000 + 44, 0);
            m.WriteU32(0x64000 + 48 + 44, 1);
        }
        if (mode >= 7) {
            m.WriteU32(Owner + 208, 2);
            for (unsigned off : {256, 264, 268, 272})
                m.WriteU32(Owner + off, 0);
            if (mode == 8)
                m.WriteU32(0x64000 + 24, 7);
            if (mode == 9) {
                m.WriteU8(Owner + 282, 0);
                m.WriteU8(Owner + 284, 0);
                for (unsigned off : {285, 286, 287})
                    m.WriteU8(Owner + off, 1);
            }
            if (mode == 10)
                m.WriteU32(Owner + 224, 0);
        }
        m.WriteU32(Input, 0);
        m.WriteU32(Input + 4, 1);
    };
    seed(before);
    seed(after);
    Environment expected(before), actual(after);
    expected.guest.live = {0x60000, 0x61000, 0x62000, 0x64000, 0x65000,
                           0x66000, 0x67000, 0x68000, 0x69000};
    if (mode >= 7)
        for (unsigned p : {0x66000, 0x67000, 0x68000, 0x69000})
            expected.guest.live.erase(p);
    actual.guest.live = expected.guest.live;
    auto s = sort_engine61_oracle::Initial(0);
    s.r[3] = Owner;
    s.r[4] = Input;
    s.r[5] = mode == 3 ? 0 : 2;
    s.r[6] = 7;
    s.r[7] = 9;
    auto om = before.Memory(), m = after.Memory();
    original = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode < 4)
        __imp__sub_82BC0108(c, before.Bytes());
    else if (mode < 7)
        __imp__sub_82BC0930(c, before.Bytes());
    else
        __imp__sub_82BC0BA8(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_indexed_vertex_output61::Apply(mode < 4   ? 0x82bc0108u
                                              : mode < 7 ? 0x82bc0930u
                                                         : 0x82bc0ba8u,
                                              m, actual.Deps(), s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || expected.guest.events != actual.guest.events ||
        expected.guest.live != actual.guest.live || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "vertex%u Full%d RAM%d events%d host%d\n", mode, a == b,
                     before.EqualCommitted(after), expected.guest.events == actual.guest.events,
                     host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        for (unsigned i = 0; i < std::min(expected.guest.events.size(), actual.guest.events.size());
             ++i)
            if (expected.guest.events[i] != actual.guest.events[i]) {
                std::fprintf(stderr, "first event %u\n", i);
                for (unsigned k = 0; k < 73; ++k)
                    if (expected.guest.events[i][k] != actual.guest.events[i][k])
                        std::fprintf(stderr, "ev%u %llx/%llx\n", k,
                                     (unsigned long long)expected.guest.events[i][k],
                                     (unsigned long long)actual.guest.events[i][k]);
                break;
            }
        throw std::runtime_error("indexed vertex original mismatch");
    }
    if (mode < 4) {
        unsigned faces = mode == 3 ? 0 : 2, vertices = mode == 3 ? 0 : 4;
        if (s.r[3] != faces || m.ReadU32(Owner + 4) != 3 * faces || m.ReadU32(Owner + 20) != 1 ||
            m.ReadU32(m.ReadU32(Owner + 24)) != faces || m.ReadU32(Owner + 180) != 5)
            throw std::runtime_error("vertex batch counts");
        constexpr unsigned ids[]{0, 1, 2, 0, 2, 3};
        for (unsigned i = 0; i < 3 * faces; ++i)
            if (m.ReadU32(m.ReadU32(Owner + 8) + 4 * i) != ids[i])
                throw std::runtime_error("emitted face indices");
        auto batch = m.ReadU32(Owner + 184);
        unsigned words[]{7, 9, faces, vertices, 0};
        for (unsigned i = 0; i < 5; ++i)
            if (m.ReadU32(batch + 4 * i) != words[i])
                throw std::runtime_error("batch descriptor");
        if (m.ReadU32(Owner + 132) != (mode == 2 ? 0u : 3 * vertices) ||
            m.ReadU32(Owner + 260) != faces)
            throw std::runtime_error("normal count/face map");
        if (mode != 2)
            for (unsigned i = 0; i < vertices; ++i) {
                auto p = m.ReadU32(Owner + 136) + 12 * i;
                float x = std::bit_cast<float>(m.ReadU32(p)),
                      y = std::bit_cast<float>(m.ReadU32(p + 4)),
                      z = std::bit_cast<float>(m.ReadU32(p + 8));
                if (std::abs(x * x + y * y + z * z - 1) > 1e-5f || y != 0 || x < 0 || z < 0)
                    throw std::runtime_error("normalized smooth vector");
            }
        for (unsigned ch = 0; ch < 3; ++ch) {
            unsigned desc = Owner + (mode == 1 ? 32 + 16 * ch : 80 + 16 * ch), dims = mode == 1 ? 1
                                                                                      : ch == 1 ? 2
                                                                                                : 3;
            if (m.ReadU32(desc + 4) != vertices * dims)
                throw std::runtime_error("emitted channel dimensions");
            auto p = m.ReadU32(desc + 8);
            for (unsigned i = 0; i < vertices * dims; ++i) {
                unsigned want =
                    mode == 1 ? i
                              : m.ReadU32(0x60000 + 0x1000 * ch + 12 * (i / dims) + 4 * (i % dims));
                if (m.ReadU32(p + 4 * i) != want)
                    throw std::runtime_error("emitted channel values");
            }
        }

    } else if (mode < 7) {
        unsigned batches = mode == 4 ? 1 : 2;
        if (s.r[3] != 1 || m.ReadU32(Owner + 20) != batches ||
            m.ReadU32(Owner + 180) != 5 * batches || m.ReadU32(Owner + 260) != 2 ||
            m.ReadU32(Owner + 4) != 6)
            throw std::runtime_error("sorted batch counts");
        auto batch = m.ReadU32(Owner + 184), ids = m.ReadU32(Owner + 256);
        if (m.ReadU32(batch) != 4 || m.ReadU32(batch + 4) != 1 ||
            m.ReadU32(batch + 8) != (mode == 4 ? 2 : 1))
            throw std::runtime_error("first sorted batch");
        if (m.ReadU32(ids) != (mode == 4 ? 0 : 1) || m.ReadU32(ids + 4) != (mode == 4 ? 1 : 0))
            throw std::runtime_error("stable sorted face IDs");
        if (batches == 2 && (m.ReadU32(batch + 20) != (mode == 5 ? 7 : 4) ||
                             m.ReadU32(batch + 24) != (mode == 6 ? 3 : 1)))
            throw std::runtime_error("second sorted batch");
    }
    if (mode >= 7) {
        if (mode == 10) {
            if (s.r[3] != 0 || !actual.guest.events.empty())
                throw std::runtime_error("empty pipeline rejected");
        } else {
            unsigned groups = mode == 8 ? 2 : 1;
            if (s.r[3] != 1 || m.ReadU32(Input) != 2 || m.ReadU32(Input + 4) != 2 ||
                m.ReadU32(Input + 8) != groups || m.ReadU32(Input + 88) != groups ||
                m.ReadU32(Input + 44) != (mode == 8 ? 6 : 4))
                throw std::runtime_error("pipeline result counts");
            auto labels = m.ReadU32(Input + 92);
            if (m.ReadU32(labels) != 4 || m.ReadU32(labels + 4) != (mode == 8 ? 1 : 2) ||
                m.ReadU32(labels + 8) != (mode == 8 ? 3 : 4) || m.ReadU32(labels + 12) != 1)
                throw std::runtime_error("label summary");
            auto map = m.ReadU32(Input + 28);
            if (mode == 8) {
                if (!map || m.ReadU32(map) != 1 || m.ReadU32(map + 4) != 0)
                    throw std::runtime_error("original face permutation");
            } else if (map)
                throw std::runtime_error("identity face permutation elided");
            for (unsigned off : {12, 16, 24, 60, 64, 68})
                if (!m.ReadU32(Input + off))
                    throw std::runtime_error("published channel view");
            if (mode != 9 && (!m.ReadU32(Input + 72) || !m.ReadU32(Input + 80)))
                throw std::runtime_error("normal and incidence output");
        }
    }
    auto os = crt_full_oracle::FromPpc(c);
    os.r[3] = Owner;
    s.r[3] = Owner;
    PPCFPSCRRegister{}.setcsr(os.cached_fp_control);
    (void)mesh_indexed_workspace61::Apply(0x82bbf590u, om, expected.Deps(), os);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_indexed_workspace61::Apply(0x82bbf590u, m, actual.Deps(), s);
    if (!actual.guest.live.empty() || !expected.guest.live.empty() ||
        !before.EqualCommitted(after) || expected.guest.events != actual.guest.events)
        throw std::runtime_error("vertex complete teardown");
    original = nullptr;
    memory = nullptr;
}
} // namespace vertex_output_oracle
void VertexIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    vertex_output_oracle::original->guest.CallIndirect(e, *vertex_output_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void VertexLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    vertex_output_oracle::Lower(e, *vertex_output_oracle::memory, *vertex_output_oracle::original,
                                s);
    crt_full_oracle::ToPpc(c, s);
}
void VertexSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *vertex_output_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void VertexRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *vertex_output_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}

void VertexSaveFpr(unsigned first, PPCContext &c, std::uint8_t *) {
    c.fpscr.disableFlushMode();
    auto s = crt_full_oracle::FromPpc(c);
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(*vertex_output_oracle::memory, std::uint32_t(s.r[12] - 8 * (32 - i)),
                               s.fpr_bits[i]);
}
void VertexRestoreFpr(unsigned first, PPCContext &c, std::uint8_t *) {
    c.fpscr.disableFlushMode();
    auto s = crt_full_oracle::FromPpc(c);
    for (unsigned i = first; i < 32; ++i)
        s.fpr_bits[i] = recovery_abi::ReadU64(*vertex_output_oracle::memory,
                                              std::uint32_t(s.r[12] - 8 * (32 - i)));
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        const char *p = std::getenv("LO_MESH_MATH_CONSTANTS");
        if (!p)
            throw std::runtime_error("private atan2 constants required");
        std::ifstream f(p, std::ios::binary);
        f.read(reinterpret_cast<char *>(vertex_output_oracle::constants.data()), 184);
        if (f.gcount() != 184)
            throw std::runtime_error("atan2 constants size");
        for (unsigned i = 0; i < 11; ++i)
            vertex_output_oracle::Check(i);
        std::puts(
            "PASS mesh-indexed-vertex-output61 11 original-upper/shared-concrete-geometry cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
