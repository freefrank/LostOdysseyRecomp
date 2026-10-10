#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cloth_storage61.h"
#include "lo_semantics/cloth_schedule_support61.h"
#include <map>
struct ClothReadGuest final : manager_release_context61::GuestServices {
  cook_main_smoke::Guest &guest;
  explicit ClothReadGuest(cook_main_smoke::Guest &g) : guest(g) {}
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    guest.CallDirect(e, m, s);
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (e == 0x2010) {
      if (!guest.live.contains(Address(s.r[4])) || s.r[5] > 16384)
        throw std::runtime_error("CLTH reallocate range");
      s.r[3] = s.r[4];
      return;
    }
    guest.CallIndirect(e, m, s);
  }
};
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

    ClothReadGuest guest(env.guest);
    m.WriteU32(0x51010, 0x2010);
    s.r[3] = Owner;
    (void)cloth_storage61::Apply(0x82b9cdd8, m, {guest, native}, s);
    auto vector = [&](unsigned offset, unsigned bytes) {
      s.r[4] = bytes;
      env.guest.CallIndirect(Allocate, m, s);
      auto p = Address(s.r[3]);
      m.WriteU32(Owner + offset, p);
      m.WriteU32(Owner + offset + 4, p + bytes);
      m.WriteU32(Owner + offset + 8, p + bytes);
      return p;
    };
    constexpr unsigned workspace = 0x60000, labels = 0x61000, box = 0x62000,
                       best = 0x63000;
    s.r[3] = workspace;
    s.r[4] = 0;
    s.r[5] = 0;
    s.r[6] = Owner + 4;
    s.r[7] = Owner + 84;
    s.r[8] = Owner + 124;
    s.r[9] = Owner + 164;
    s.r[10] = Owner + 184;
    (void)cloth_storage61::Apply(0x82ba7f90, m, {guest, native}, s);
    auto points = vector(4, 48), constraints = vector(84, 136);
    float xyz[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    for (unsigned i = 0; i < 12; ++i)
      m.WriteU32(points + 4 * i, std::bit_cast<unsigned>(xyz[i]));
    for (unsigned i = 0; i < 4; ++i) {
      m.WriteU32(constraints + 4 * i, i == 3 ? 0xffffffff : i);
      m.WriteU32(constraints + 68 + 4 * i, i);
    }
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
      s.r[3] = workspace;
      (void)cloth_schedule_support61::Apply(0x82bb59a8, m, {guest, native}, s);
      unsigned counts[]{2, 2, 2, 1}, offsets[]{0, 2, 4, 6},
          edges[]{0, 1, 0, 1, 0, 1, 1};
      for (unsigned i = 0; i < 4; ++i)
        if (m.ReadU32(m.ReadU32(workspace) + 4 * i) != counts[i] ||
            m.ReadU32(m.ReadU32(workspace + 20) + 4 * i) != offsets[i])
          throw std::runtime_error("adjacency count/offset");
      for (unsigned i = 0; i < 7; ++i)
        if (m.ReadU32(m.ReadU32(workspace + 40) + 4 * i) != edges[i])
          throw std::runtime_error("adjacency stable constraint ids");
    }
    m.WriteU32(labels, labels + 16);
    for (unsigned i = 0; i < 4; ++i)
      m.WriteU32(labels + 16 + 4 * i, i ? 0xffffffff : 0);
    for (unsigned i = 0; i < 6; ++i)
      m.WriteU32(box + 4 * i, 0);
    auto select = [&](unsigned id) {
      s.r[3] = workspace;
      s.r[4] = id;
      s.r[5] = 0;
      s.r[6] = labels;
      s.r[7] = box;
      s.r[8] = best;
      s.r[9] = best + 4;
      s.r[10] = best + 8;
      (void)cloth_schedule_support61::Apply(0x82bb56d0, m, {guest, native}, s);
    };
    m.WriteU32(best, 0);
    m.WriteU32(best + 4, std::bit_cast<unsigned>(100.f));
    m.WriteU32(best + 8, 0xffffffff);
    select(1);
    if (m.ReadU32(best) != 1 || m.ReadU32(best + 8) != 1 ||
        std::bit_cast<float>(m.ReadU32(best + 4)) != 0.f)
      throw std::runtime_error("tetra compact candidate");
    select(0);
    if (m.ReadU32(best) != 2 || m.ReadU32(best + 8) != 0 ||
        std::bit_cast<float>(m.ReadU32(best + 4)) != 2.f)
      throw std::runtime_error("boundary matched-count priority");
    m.WriteU32(best + 4, std::bit_cast<unsigned>(3.f));
    select(0);
    if (std::bit_cast<float>(m.ReadU32(best + 4)) != 2.f)
      throw std::runtime_error("candidate metric tiebreak");
    m.WriteU32(labels + 16, 1);
    m.WriteU32(best, 0);
    m.WriteU32(best + 8, 99);
    select(0);
    if (m.ReadU32(best + 8) != 99)
      throw std::runtime_error("conflicting bucket candidate rejection");
    s.r[3] = workspace;
    (void)cloth_storage61::Apply(0x82ba8018, m, {guest, native}, s);
    s.r[3] = Owner;
    (void)cloth_storage61::Apply(0x82ba8858, m, {guest, native}, s);
    if (!env.guest.live.empty())
      throw std::runtime_error("schedule support ownership");
    std::puts("PASS cloth adjacency, candidate priority/metric/conflict, reuse "
              "and cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
