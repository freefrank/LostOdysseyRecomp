#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cloth_storage61.h"
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

    auto allocate = [&]() {
      s.r[4] = 64;
      env.guest.CallIndirect(Allocate, m, s);
      return Address(s.r[3]);
    };
    auto call = [&](unsigned entry, unsigned self) {
      s.r[3] = self;
      (void)cloth_storage61::Apply(entry, m, deps.lifetime, s);
    };
    call(0x82b9cdd8, Owner);
    for (unsigned off = 4; off <= 184; off += 20) {
      auto p = allocate();
      m.WriteU32(Owner + off, p);
      m.WriteU32(Owner + off + 4, p + 8);
      m.WriteU32(Owner + off + 8, p + 64);
    }
    auto child = allocate();
    m.WriteU32(child + 4, allocate());
    m.WriteU32(child + 24, allocate());
    m.WriteU32(m.ReadU32(Owner + 184), child);
    m.WriteU32(m.ReadU32(Owner + 184) + 4, 0);
    auto vector = m.ReadU32(Owner + 4);
    call(0x82ba8108, Owner);
    if (m.ReadU32(Owner + 4) != vector || m.ReadU32(Owner + 8) != vector ||
        env.guest.live.size() != 10)
      throw std::runtime_error("cloth clear retains capacity/releases child");
    call(0x82ba8858, Owner);
    unsigned workspace = 0x60000;
    s.r[4] = 31;
    s.r[5] = 1;
    for (unsigned i = 6; i < 11; ++i)
      s.r[i] = 100 + i;
    call(0x82ba7f90, workspace);
    if (m.ReadU32(workspace + 60) != 31 || m.ReadU8(workspace + 64) != 1 ||
        m.ReadU32(workspace + 732) != 110)
      throw std::runtime_error("cloth workspace constructor");
    for (unsigned off : {0u, 20u, 40u})
      m.WriteU32(workspace + off, allocate());
    for (unsigned i = 0; i < 32; ++i)
      m.WriteU32(workspace + 68 + 20 * i, allocate());
    call(0x82ba8018, workspace);
    if (!env.guest.live.empty())
      throw std::runtime_error("edge flags ownership");
    if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr))
      throw std::runtime_error("edge flags ABI");
    std::puts("PASS cloth retained capacity, nested mesh, bucket workspace and "
              "complete cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
