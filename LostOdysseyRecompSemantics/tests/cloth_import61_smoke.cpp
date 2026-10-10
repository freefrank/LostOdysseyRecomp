#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cloth_storage61.h"
#include "lo_semantics/cloth_import61.h"
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

    for (unsigned tetra : {0u, 1u})
      for (unsigned half : {0u, 1u}) {
        unsigned desc = 0x60000, points = 0x61000, indices = 0x62000,
                 weights = 0x63000, attributes = 0x64000;
        for (unsigned i = 0; i < 60; ++i)
          m.WriteU8(desc + i, 0);
        m.WriteU32(desc + 4, 4);
        m.WriteU32(desc + 8, 16);
        m.WriteU32(desc + (tetra ? 20 : 12), 1);
        m.WriteU32(desc + (tetra ? 24 : 16), 16);
        m.WriteU32(desc + 28, points);
        m.WriteU32(desc + (tetra ? 36 : 32), indices);
        m.WriteU32(desc + 40, 1 + 2 * half);
        m.WriteU32(desc + 44, 8);
        m.WriteU32(desc + 48, 8);
        m.WriteU32(desc + 52, weights);
        m.WriteU32(desc + 56, attributes);
        for (unsigned i = 0; i < 4; ++i) {
          for (unsigned j = 0; j < 3; ++j)
            m.WriteU32(points + 16 * i + 4 * j, 100 + 3 * i + j);
          m.WriteU32(weights + 8 * i, 200 + i);
          m.WriteU32(attributes + 8 * i, 300 + i);
          if (half)
            m.WriteU16(indices + 2 * i, i);
          else
            m.WriteU32(indices + 4 * i, i);
        }
        s.r[3] = Owner;
        (void)cloth_storage61::Apply(0x82b9cdd8, m, deps.lifetime, s);
        s.r[3] = Owner;
        s.r[4] = desc;
        (void)cloth_import61::Apply(tetra ? 0x82ba9530 : 0x82ba8ae8, m,
                                    deps.lifetime, s);
        if (s.r[3] != 1 || m.ReadU32(Owner + 8) - m.ReadU32(Owner + 4) != 48)
          throw std::runtime_error("cloth strided positions");
        for (unsigned i = 0; i < 4; ++i) {
          for (unsigned j = 0; j < 3; ++j)
            if (m.ReadU32(m.ReadU32(Owner + 4) + 12 * i + 4 * j) !=
                100 + 3 * i + j)
              throw std::runtime_error("cloth imported xyz");
          if (m.ReadU32(m.ReadU32(Owner + 44) + 4 * i) != 200 + i ||
              m.ReadU32(m.ReadU32(Owner + 64) + 4 * i) != 300 + i)
            throw std::runtime_error("cloth optional channels");
        }
        for (unsigned i = 0; i < (tetra ? 4u : 3u); ++i) {
          unsigned expected = tetra || !i ? i : 3 - i;
          if (m.ReadU32(m.ReadU32(Owner + 24) + 4 * i) != expected)
            throw std::runtime_error("cloth half/word winding");
        }
        s.r[3] = Owner;
        (void)cloth_storage61::Apply(0x82ba8858, m, deps.lifetime, s);
        if (!env.guest.live.empty())
          throw std::runtime_error("cloth import ownership");
      }
    std::puts("PASS triangle/tetrahedron strided inputs, half/word indices, "
              "channels and cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
