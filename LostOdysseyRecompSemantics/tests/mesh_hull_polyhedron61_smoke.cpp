#define main incremental_fixture_main
#include "mesh_hull_incremental61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_hull_polyhedron61.h"
int main() {
    try {
        using namespace incremental_smoke;
        auto regions = [] {
            std::array<test::Region, 11> r{};
            std::copy(incremental_smoke::Regions.begin(), incremental_smoke::Regions.end(),
                      r.begin());
            r[10] = {0x8204f000, 0x1000};
            return r;
        }();
        test::GuestWindow w(regions);
        w.Fill(0);
        auto m = w.Memory();
        incremental_smoke::Environment env(w);
        auto f = [&](unsigned p, float v) { m.WriteU32(p, std::bit_cast<std::uint32_t>(v)); };
        f(0x82007784, 1);
        f(0x82000e40, -1);
        f(0x82000e50, 0);
        f(0x83216160, .001f);
        std::ifstream input(std::getenv("LO_HULL_INCREMENTAL_CONSTANTS"), std::ios::binary);
        std::array<unsigned char, 192> data{};
        input.read(reinterpret_cast<char *>(data.data()), data.size());
        if (input.gcount() != 192)
            throw std::runtime_error("private hull constants size");
        unsigned offset = 0;
        for (auto region : std::array<test::Region, 17>{{{0x83214d80, 120},
                                                         {0x83215508, 8},
                                                         {0x82000f28, 8},
                                                         {0x82000de0, 4},
                                                         {0x82000b7c, 4},
                                                         {0x82000e44, 4},
                                                         {0x82000dc0, 4},
                                                         {0x820d57f0, 4},
                                                         {0x820009c8, 4},
                                                         {0x82000d7c, 4},
                                                         {0x82000b58, 4},
                                                         {0x822183e8, 4},
                                                         {0x82218644, 4},
                                                         {0x82000e40, 4},
                                                         {0x82000d6c, 4},
                                                         {0x82000da4, 4},
                                                         {0x83216164, 4}}})
            for (unsigned i = 0; i < region.size; ++i)
                m.WriteU8(region.base + i, data[offset++]);
        m.WriteU32(0x832df548, 0x50000);
        m.WriteU32(0x50000, 0x51000);
        m.WriteU32(0x51008, Allocate | 1);
        m.WriteU32(0x51014, Free | 3);
        auto s = sort_engine61_oracle::Initial(0), saved = s;
        auto call = [&](unsigned e, std::initializer_list<unsigned> args) {
            unsigned j = 3;
            for (auto a : args)
                s.r[j++] = a;
            if (!mesh_hull_polyhedron61::Apply(e, m, env.Deps(), s))
                throw std::runtime_error("polyhedron dispatch");
        };
        call(0x82ba2330, {});
        auto box = Address(s.r[3]);
        if (m.ReadU32(box + 4) != 8 || m.ReadU32(box + 16) != 24 || m.ReadU32(box + 28) != 6)
            throw std::runtime_error("unit box counts");
        call(0x82ba1528, {box});
        if (s.r[3] != 1)
            throw std::runtime_error("box topology/planes/winding");
        f(Input, 1);
        f(Input + 4, 0);
        f(Input + 8, 0);
        f(Input + 12, -.5f);
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(.01);
        call(0x82ba17e8, {Input, 1, box});
        if (std::int32_t(s.r[3]) != 0)
            throw std::runtime_error("select cutting plane");
        env.guest.acceptExitRegistration = true;
        call(0x82ba2e00, {box, Input});
        auto clipped = Address(s.r[3]);
        if (!clipped || m.ReadU32(clipped + 4) != 8 || m.ReadU32(clipped + 28) != 6)
            throw std::runtime_error("half box clip");
        call(0x82ba1528, {clipped});
        if (s.r[3] != 1)
            throw std::runtime_error("clipped topology/planes");
        for (unsigned i = 0; i < m.ReadU32(clipped + 4); ++i)
            if (std::bit_cast<float>(m.ReadU32(m.ReadU32(clipped) + 12 * i)) > .50001f)
                throw std::runtime_error("clipped extent");
        call(0x82ba1e30, {clipped});
        s.r[4] = clipped;
        env.guest.CallIndirect(Free, m, s);
        f(Input + 12, -1);
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(.01);
        call(0x82ba17e8, {Input, 1, box});
        if (std::int32_t(s.r[3]) != -1)
            throw std::runtime_error("skip existing plane");
        f(0x82003660, 2.5f);
        f(0x8201f9f0, .5f);
        f(0x8204fc20, 120.f);
        f(0x820a6b8c, 1e-6f);
        f(0x82000e10, .01f);
        f(0x82000d64, -std::numeric_limits<float>::max());
        f(0x82000e0c, std::numeric_limits<float>::max());
        f(0x82000dac, .1f);
        f(0x820d5fb8, -.001f);
        f(0x820d5fbc, .001f);
        m.WriteU32(Input, 5);
        m.WriteU32(Input + 4, 8);
        m.WriteU32(Input + 8, m.ReadU32(box));
        m.WriteU32(Input + 12, 12);
        f(Input + 16, .001f);
        f(Input + 20, .1f);
        m.WriteU32(Input + 24, 0);
        s.r[3] = Owner;
        s.r[4] = Input;
        s.r[5] = Polygons;
        if (!mesh_hull_preprocess61::Apply(0x82ba5cf8, m, env.Deps(), s) || s.r[3] != 0 ||
            m.ReadU32(Polygons + 4) != 8 || m.ReadU32(Polygons + 12) != 12)
            throw std::runtime_error("inflated cube output counts");
        for (unsigned axis = 0; axis < 3; ++axis) {
            float lo = 100, hi = -100;
            for (unsigned i = 0; i < 8; ++i) {
                auto v =
                    std::bit_cast<float>(m.ReadU32(m.ReadU32(Polygons + 8) + 12 * i + 4 * axis));
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
            if (std::abs(lo + .1f) > 1e-5f || std::abs(hi - 1.1f) > 1e-5f)
                throw std::runtime_error("inflated cube extent");
        }
        for (auto off : {8u, 20u}) {
            s.r[4] = m.ReadU32(Polygons + off);
            env.guest.CallIndirect(Free, m, s);
        }
        if (env.guest.exitCallbacks != std::vector<GuestAddress>{0x830d9990u, 0x830d9930u})
            throw std::runtime_error("first-use exit callbacks");
        for (auto pair : std::array<std::pair<unsigned, unsigned>, 2>{
                 {{0x830d9990u, 0x832dc438u}, {0x830d9930u, 0x832dc42cu}}}) {
            s.r[3] = pair.second;
            s.r[4] = 4;
            (void)mesh_hull_preprocess61::Apply(0x82ba1138u, m, env.Deps(), s);
            m.WriteU32(pair.second + 4, 1);
            call(pair.first, {});
            if (m.ReadU32(pair.second) || m.ReadU32(pair.second + 4) || m.ReadU32(pair.second + 8))
                throw std::runtime_error("scratch destructor state");
        }
        call(0x82ba1e30, {box});
        s.r[4] = box;
        env.guest.CallIndirect(Free, m, s);
        if (!env.guest.live.empty())
            throw std::runtime_error("box cleanup");
        for (unsigned i = 14; i < 32; ++i)
            if (s.r[i] != saved.r[i])
                throw std::runtime_error("preserved registers");
        if (s.r[1] != saved.r[1] || s.lr != std::uint32_t(saved.lr))
            throw std::runtime_error("preserved stack/link");
        std::puts("PASS box topology, cutting-plane selection, half-box clipping and full inflated "
                  "cube preparation/cleanup");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
