#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/mesh_triangle_cache61.h"
#include <map>
int main() {
  try {
    using namespace cook_main_smoke;
    test::GuestWindow w(cook_main_smoke::Regions);
    w.Fill(0);
    auto m = w.Memory();
    cook_main_smoke::Environment env(w);
    auto deps = env.Deps();
    env.guest.geometryDeps = &deps;
    auto s = sort_engine61_oracle::Initial(0);
    auto initial = s;
    for (auto [p, v] : std::array<std::pair<unsigned, float>, 6>{
             {{0x82000d6c, .001f},
              {0x82000d64, -std::numeric_limits<float>::max()},
              {0x82000e0c, std::numeric_limits<float>::max()},
              {0x82000e50, 0.f},
              {0x82000e40, -1.f},
              {0x82007784, 1.f}}})
      m.WriteU32(p, std::bit_cast<unsigned>(v));
    m.WriteU32(0x83216624, AllocatorTable);
    m.WriteU32(AllocatorTable, Allocate | 1);
    m.WriteU32(AllocatorTable + 12, Free | 3);
    m.WriteU32(0x832df548, 0x50000);
    m.WriteU32(0x50000, 0x51000);
    m.WriteU32(0x51008, Allocate | 1);
    m.WriteU32(0x51014, Free | 3);
    m.WriteU32(0x832dc414, 1);
    m.WriteU32(0x83216158, 17);
    m.WriteU8(0x83216670, 1);
    m.WriteU32(0x820d5d30 + 48, 0x82b9f188);
    m.WriteU32(0x820d5d30 + 52, 0x82b9f190);
    m.WriteU32(0x820d6970 + 4, 0x82bc8638);
    m.WriteU32(0x820d6284, 0x82bb3430);
    m.WriteU32(0x820d6288, 0x82bb34f8);
    m.WriteU32(0x820d628c, 0x822d3068);
    for (unsigned i = 0; i < 6; ++i)
      m.WriteU32(0x82bb3988 + 4 * i, i < 2   ? 0x82bb39a0
                                     : i < 4 ? 0x82bb39f8
                                             : 0x82bb3a50);
    m.WriteU32(0x820d6940, 0x82bc63c8);
    m.WriteU32(0x820d6940 + 4, 0x82bc62d8);
    m.WriteU32(0x820d6c34, 0x82bd27f8);
    m.WriteU32(0x820d6c34 + 4, 0x82bd2200);
    m.WriteU32(0x820d6c34 + 28, 0x82bd1d08);
    m.WriteU32(0x820d6c34 + 8, 0x82bd22a8);
    m.WriteU32(0x820d6c34 + 24, 0x82bd1bf8);
    constexpr std::array<GuestAddress, 5> triangle{
        0x82bd88e8, 0x82bd8bf8, 0x82bd8ac0, 0x82bd8b40, 0x82bb3b88},
        boxes{0x82bd8ee0, 0x82bb3b60, 0x82bd8848, 0x82bd8888, 0x82bb3b88};
    for (unsigned i = 0; i < 5; ++i) {
      m.WriteU32(0x820d6c58 + 4 * i, triangle[i]);
      m.WriteU32(0x820d6304 + 4 * i, boxes[i]);
    }
    constexpr GuestAddress cleanup[]{0x82bddac0, 0x82bddcd8, 0x82bddd38,
                                     0x82bddd98},
        bind[]{0x82bdd058, 0x82bdd1e8, 0x82bdbd90, 0x82bdc208},
        write[]{0x82bddb20, 0x82bdb660, 0x82bdd868, 0x82bdc838},
        read[]{0x82bdb350, 0x82bdb7f8, 0x82bdbed8, 0x82bdc9f0};
    for (unsigned i = 0; i < 4; ++i) {
      m.WriteU32(0x820d6e7c + 32 * i, cleanup[i]);
      m.WriteU32(0x820d6e80 + 32 * i, bind[i]);
      m.WriteU32(0x820d6e90 + 32 * i, write[i]);
      m.WriteU32(0x820d6e7c + 24 + 32 * i, read[i]);
    }
    m.WriteU32(Writer, Table);
    m.WriteU32(Writer + 8, 16384);
    m.WriteU32(Writer + 12, cook_main_smoke::Buffer);
    constexpr unsigned writers[]{0x82bde330, 0x82bde378, 0x82bde3c0,
                                 0x82bde408, 0x82bde450, 0x82bde498};
    for (unsigned i = 0; i < 6; ++i) {
      m.WriteU32(Table + 28 + 4 * i, writers[i]);
      m.WriteU32(0x820d5d70 + 28 + 4 * i, 0x82b9e528 + 64 * i);
    }

    const unsigned mesh = Owner + 4, adapter = 0x60000;
    auto storage = [&](unsigned e, unsigned self, unsigned count = 0) {
      s.r[3] = self;
      s.r[4] = count;
      (void)mesh_triangle_storage61::Apply(e, m, deps, s);
    };
    storage(0x82b9e2d8, Owner);
    storage(0x82bc5be0, mesh, 4);
    std::array<unsigned char, 128> constants{};
    std::ifstream f(std::getenv("LO_MASS_CONSTANTS"), std::ios::binary);
    f.read(reinterpret_cast<char *>(constants.data()), constants.size());
    if (f.gcount() != 128)
      throw std::runtime_error("private mass constants");
    unsigned offset = 0;
    for (auto region : std::array<test::Region, 8>{{{0x820d6a78, 72},
                                                    {0x82000fe8, 8},
                                                    {0x82001010, 8},
                                                    {0x82051430, 8},
                                                    {0x82000f70, 8},
                                                    {0x82048090, 8},
                                                    {0x82000f28, 8},
                                                    {0x820d6ac0, 8}}})
      for (unsigned i = 0; i < region.size; ++i)
        m.WriteU8(region.base + i, constants[offset++]);
    m.WriteU32(0x82000dac, std::bit_cast<unsigned>(.1f));
    m.WriteU32(0x821baa74, std::bit_cast<unsigned>(2.f));
    storage(0x82bc5c38, mesh, 4);
    float xyz[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    unsigned indices[]{0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3};
    for (unsigned i = 0; i < 12; ++i) {
      m.WriteU32(m.ReadU32(mesh + 8) + 4 * i, std::bit_cast<unsigned>(xyz[i]));
      m.WriteU32(m.ReadU32(mesh + 12) + 4 * i, indices[i]);
    }
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
      s.r[3] = Owner;
      (void)mesh_triangle_cache61::Apply(0x82ba6868, m, deps, s);
      if (s.r[3] != Owner + 208 ||
          std::abs(std::bit_cast<float>(m.ReadU32(Owner + 208)) - 1.f / 6) >
              1e-5f)
        throw std::runtime_error("tetrahedron cached mass");
      for (unsigned i = 0; i < 3; ++i)
        if (std::abs(std::bit_cast<float>(m.ReadU32(Owner + 248 + 4 * i)) -
                     .25f) > 1e-5f)
          throw std::runtime_error("tetrahedron centroid");
    }
    s.r[3] = mesh;
    (void)mesh_triangle_cache61::Apply(0x82bc5ed8, m, deps, s);
    auto cache = m.ReadU32(mesh + 88);
    if (!cache || m.ReadU32(cache + 8) != 4)
      throw std::runtime_error("triangle edge cache");
    storage(0x82b9e220, Owner);
    if (!env.guest.live.empty())
      throw std::runtime_error("edge flags ownership");
    if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr))
      throw std::runtime_error("edge flags ABI");
    std::puts("PASS tetrahedron mass, cache reuse, edge topology and "
              "complete cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
