#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cloth_topology_support61.h"
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

    unsigned vector = 0x60000, data = 0x61000, key = 0x62000, out = 0x63000;
    for (unsigned entry : {0x82ba7080u, 0x82ba7250u, 0x82ba7438u}) {
      unsigned words = entry == 0x82ba7250 ? 3 : 4,
               keys = entry == 0x82ba7080 ? 2 : 3;
      m.WriteU32(vector, data);
      m.WriteU32(vector + 4, data + 4 * words * 4);
      unsigned rows[4][4] = {
          {3, 0, 2, 10}, {1, 2, 5, 11}, {1, 2, 4, 12}, {2, 0, 0, 13}};
      for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < words; ++j)
          m.WriteU32(data + 4 * (words * i + j), rows[i][j]);
      s.r[3] = vector;
      s.r[4] = 0;
      s.r[5] = 3;
      (void)cloth_topology_support61::Apply(entry, m, s);
      for (unsigned i = 1; i < 4; ++i) {
        bool ordered = false;
        for (unsigned j = 0; j < keys; ++j) {
          auto x = m.ReadU32(data + 4 * (words * (i - 1) + j)),
               y = m.ReadU32(data + 4 * (words * i + j));
          if (x < y) {
            ordered = true;
            break;
          }
          if (x > y)
            throw std::runtime_error("cloth lexicographic sort");
        }
        (void)ordered;
      }
      if (words == 3) {
        m.WriteU32(key, 2);
        m.WriteU32(key + 4, 0);
        m.WriteU32(key + 8, 0);
        s.r[3] = vector;
        s.r[4] = key;
        (void)cloth_topology_support61::Apply(0x82ba7db8, m, s);
        if (s.r[3] != data + 24)
          throw std::runtime_error("cloth unique record lookup");
        m.WriteU32(key, 1);
        m.WriteU32(key + 4, 2);
        m.WriteU32(key + 8, 5);
        s.r[3] = vector;
        s.r[4] = key;
        (void)cloth_topology_support61::Apply(0x82ba7db8, m, s);
        if (s.r[3] != 0)
          throw std::runtime_error("cloth ambiguous pair lookup");
      }
    }
    m.WriteU32(Owner + 4, data);
    m.WriteU32(Owner + 8, data + 48);
    m.WriteU32(Owner + 24, data + 64);
    m.WriteU32(Owner + 28, data + 112);
    m.WriteU32(Owner + 216, 0x55);
    for (unsigned tetra : {0u, 1u}) {
      m.WriteU32(Owner + 208, tetra);
      s.r[3] = Owner;
      s.r[4] = out;
      (void)cloth_topology_support61::Apply(0x82ba7648, m, s);
      if (s.r[3] != 1 || m.ReadU32(out + 4) != 4 ||
          m.ReadU32(out + 40) != 0x55 ||
          (tetra ? (m.ReadU32(out + 20) != 3 || m.ReadU32(out + 24) != 16 ||
                    m.ReadU32(out + 32) != 0)
                 : (m.ReadU32(out + 12) != 4 || m.ReadU32(out + 16) != 12)))
        throw std::runtime_error("cloth borrowed descriptor");
    }
    std::puts("PASS cloth record sorts, unique/ambiguous lookup and both "
              "descriptor layouts");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
