#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/mesh_partition61.h"
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
    std::array<unsigned char, 184> constants{};
    std::ifstream f(std::getenv("LO_MESH_MATH_CONSTANTS"), std::ios::binary);
    f.read(reinterpret_cast<char *>(constants.data()), constants.size());
    if (f.gcount() != 184)
      throw std::runtime_error("private atan2 constants");
    for (unsigned i = 0; i < 184; ++i)
      m.WriteU8(0x83214e88 + i, constants[i]);
    m.WriteU32(0x82000d7c, std::bit_cast<unsigned>(.01f));
    m.WriteU32(0x820d6670, std::bit_cast<unsigned>(.001f));
    m.WriteU32(0x82000dac, std::bit_cast<unsigned>(.1f));
    m.WriteU32(0x821baa74, std::bit_cast<unsigned>(2.f));
    storage(0x82bc5c38, mesh, 4);
    float xyz[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    unsigned indices[]{0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3};
    for (unsigned i = 0; i < 12; ++i) {
      m.WriteU32(m.ReadU32(mesh + 8) + 4 * i, std::bit_cast<unsigned>(xyz[i]));
      m.WriteU32(m.ReadU32(mesh + 12) + 4 * i, indices[i]);
    }
    auto partition = 0x60000;
    s.r[3] = partition;
    (void)mesh_partition_support61::Apply(0x82bc10f0, m, deps, s);
    s.r[3] = partition;
    s.r[4] = 4;
    s.r[5] = m.ReadU32(mesh + 12);
    s.r[6] = 4;
    s.r[7] = m.ReadU32(mesh + 8);
    (void)mesh_partition61::Apply(0x82bc1f00, m, deps, s);
    if (s.r[3] != 1 || m.ReadU32(partition + 36) != 1 ||
        m.ReadU32(partition + 44) != 4)
      throw std::runtime_error("tetrahedron convex group/categories");
    for (unsigned i = 0; i < 4; ++i)
      if (m.ReadU32(m.ReadU32(partition + 40) + 4 * i) != 0)
        throw std::runtime_error("convex partition label");
    s.r[3] = partition;
    (void)mesh_partition_support61::Apply(0x82bc1108, m, deps, s);
    m.WriteU32(adapter, mesh);
    s.r[3] = adapter;
    (void)mesh_partition61::Apply(0x82bb4cf0, m, deps, s);
    if (s.r[3] != 1 || m.ReadU32(mesh + 24) != 1 || m.ReadU32(mesh + 28) != 4)
      throw std::runtime_error("partition owner export");
    storage(0x82b9e220, Owner);
    if (!env.guest.live.empty())
      throw std::runtime_error("edge flags ownership");
    if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr))
      throw std::runtime_error("edge flags ABI");
    std::puts("PASS tetrahedron convex partition/categories and "
              "complete cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
