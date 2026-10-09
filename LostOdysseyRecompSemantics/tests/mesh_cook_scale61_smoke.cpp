#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_cook_scale61.h"
int main() {
    try {
        using namespace cook_main_smoke;
        test::GuestWindow w(cook_main_smoke::Regions);
        w.Fill(0);
        auto m = w.Memory();
        cook_main_smoke::Environment env(w);
        auto f = [&](unsigned p, float v) { m.WriteU32(p, std::bit_cast<std::uint32_t>(v)); };
        auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
        auto file = std::getenv("LO_POWER_CONSTANTS");
        if (!file)
            throw std::runtime_error("private power constants");
        std::ifstream input(file, std::ios::binary);
        std::array<unsigned char, 1448> data{};
        input.read(reinterpret_cast<char *>(data.data()), data.size());
        if (input.gcount() != 1448)
            throw std::runtime_error("power bundle size");
        unsigned offset = 0;
        for (auto region : std::array<test::Region, 6>{{{0x82000e00, 768},
                                                        {0x820d2f68, 512},
                                                        {0x83215500, 40},
                                                        {0x822181a0, 112},
                                                        {0x83214fc0, 8},
                                                        {0x820d5e30, 8}}})
            for (unsigned i = 0; i < region.size; ++i)
                m.WriteU8(region.base + i, data[offset++]);
        m.WriteU32(Owner + 44, 0x50000);
        m.WriteU32(0x50000, 0x51000);
        m.WriteU32(0x51008, 2);
        m.WriteU32(0x5100c, Positions);
        m.WriteU32(0x51020, 1);
        m.WriteU32(0x51024, PolygonData);
        m.WriteU32(Owner + 8, Table);
        m.WriteU32(Table + 16, 0x2008);
        m.WriteU8(0x832dc188, 1);
        m.WriteU32(Owner + 168, 2);
        m.WriteU32(Owner + 160, 1);
        for (unsigned i = 0; i < 3; ++i) {
            f(0x51014 + 4 * i, float(i + 1));
            f(PolygonData + 24 + 4 * i, float(i + 1));
            f(Owner + 332 + 4 * i, float(i + 1));
        }
        for (unsigned i = 0; i < 6; ++i) {
            f(Positions + 4 * i, float(i + 1));
            f(Owner + 112 + 4 * i, float(i + 1));
        }
        for (unsigned i = 0; i < 4; ++i)
            f(Owner + 136 + 4 * i, float(i + 1));
        for (unsigned i = 0; i < 9; ++i)
            f(Owner + 296 + 4 * i, float(i + 1));
        auto s = sort_engine61_oracle::Initial(0), saved = s;
        s.r[3] = Owner;
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(2.);
        auto deps =
            mesh_cook_scale61::Dependencies{env.Deps().lifetime, env.EdgeDeps().diagnostics};
        if (!mesh_cook_scale61::Apply(0x82b9ec98, m, deps, s) || s.r[3] != 1 ||
            env.guest.refreshCalls != 1)
            throw std::runtime_error("scale/tree refresh");
        for (unsigned i = 0; i < 6; ++i)
            if (get(Positions + 4 * i) != 2 * float(i + 1) ||
                get(Owner + 112 + 4 * i) != 2 * float(i + 1))
                throw std::runtime_error("scaled positions/bounds");
        for (unsigned i = 0; i < 3; ++i)
            if (get(0x51014 + 4 * i) != 2 * float(i + 1) ||
                get(PolygonData + 24 + 4 * i) != 2 * float(i + 1) ||
                get(Owner + 332 + 4 * i) != 2 * float(i + 1))
                throw std::runtime_error("scaled geometry/centroid");
        for (unsigned i = 0; i < 9; ++i)
            if (get(Owner + 296 + 4 * i) != 4 * float(i + 1))
                throw std::runtime_error("squared inertia scaling");
        if (get(Owner + 148) != 8 || std::abs(get(Owner + 152) - 12.f * 0x1p-22f) > 1e-12f)
            throw std::runtime_error("radius/relative tolerance");
        if (s.r[1] != saved.r[1] || s.lr != Address(saved.lr) ||
            s.fpr_bits[31] != saved.fpr_bits[31])
            throw std::runtime_error("scale ABI");
        for (unsigned i = 14; i < 32; ++i)
            if (s.r[i] != saved.r[i])
                throw std::runtime_error("scale GPR");
        for (auto e : {0x82b9f188u, 0x82b9f190u}) {
            s.r[3] = Owner;
            (void)mesh_cook_scale61::Apply(e, m, deps, s);
            if (s.r[3] != (e == 0x82b9f188u ? 2 : 1))
                throw std::runtime_error("count getters");
        }
        for (unsigned i = 0; i < 3; ++i)
            m.WriteU32(Input + 4 * i, 10 + i);
        m.WriteU32(0x832dc418, 1);
        s.r[4] = Input;
        (void)mesh_cook_scale61::Apply(0x82b9caf8, m, deps, s);
        for (unsigned i = 0; i < 3; ++i)
            if (m.ReadU32(0x832dc180 + 4 * i) != 10 + i || m.ReadU32(0x832dc190 + 4 * i) != 10 + i)
                throw std::runtime_error("active cook settings");
        std::puts("PASS uniform geometry/bounds/inertia scaling, tree refresh boundary, count "
                  "getters and settings");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
