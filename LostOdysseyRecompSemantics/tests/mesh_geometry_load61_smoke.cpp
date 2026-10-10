#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_geometry_load61.h"
int main() {
    try {
        using namespace cook_main_smoke;
        auto regions = cook_main_smoke::Regions;
        for (auto &r : regions)
            if (r.base == 0x832dc000)
                r.size = 0x3000;
        test::GuestWindow w(regions);
        w.Fill(0);
        auto m = w.Memory();
        cook_main_smoke::Environment env(w);
        env.guest.acceptExitRegistration = true;
        auto f = [&](unsigned p, float v) { m.WriteU32(p, std::bit_cast<unsigned>(v)); };
        f(0x82000e50, 0);
        f(0x82007784, 1);
        auto path = std::getenv("LO_HULL_INCREMENTAL_CONSTANTS");
        if (!path)
            throw std::runtime_error("hull coefficients missing");
        std::ifstream input(path, std::ios::binary);
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
        path = std::getenv("LO_NORMAL_DECODE_STEP");
        if (!path)
            throw std::runtime_error("normal decode step missing");
        std::ifstream step(path, std::ios::binary);
        std::array<unsigned char, 4> angle{};
        step.read(reinterpret_cast<char *>(angle.data()), 4);
        if (step.gcount() != 4)
            throw std::runtime_error("normal step size");
        for (unsigned i = 0; i < 4; ++i)
            m.WriteU8(0x820d6954 + i, angle[i]);
        m.WriteU32(Input, Table);
        m.WriteU32(Table + 24, 0x82bde5d8);
        constexpr unsigned packed[]{1, 2, 4, 9, 18, 36};
        for (unsigned swap : {0u, 1u}) {
            for (unsigned i = 0; i < 6; ++i)
                m.WriteU16(Positions + 2 * i,
                           swap ? __builtin_bswap16(std::uint16_t(packed[i])) : packed[i]);
            m.WriteU32(Input + 4, Positions);
            auto s = sort_engine61_oracle::Initial(0), initial = s;
            s.r[3] = 6;
            s.r[4] = Triangles;
            s.r[5] = swap;
            s.r[6] = Input;
            if (!mesh_geometry_load61::Apply(0x82bc69e0, m, env.Deps(), s))
                throw std::runtime_error("normal reader dispatch");
            for (unsigned i = 0; i < 6; ++i)
                for (unsigned axis = 0; axis < 3; ++axis) {
                    auto actual = std::bit_cast<float>(m.ReadU32(Triangles + 12 * i + 4 * axis));
                    auto expected = axis == i % 3 ? (i < 3 ? 1.f : -1.f) : 0.f;
                    if (actual != expected)
                        throw std::runtime_error("axis/sign normal decode");
                }
            if (m.ReadU32(Input + 4) != Positions + 12 || s.r[1] != initial.r[1] ||
                s.lr != Address(initial.lr))
                throw std::runtime_error("normal cursor/ABI");
        }
        if (env.guest.exitCallbacks != std::vector<GuestAddress>{0x830d9a60u} ||
            !m.ReadU8(0x832df538))
            throw std::runtime_error("lookup once-only registration/cache");
        std::puts("PASS packed axis/sign normals in both byte orders, lazy lookup reuse and "
                  "callback identity");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
