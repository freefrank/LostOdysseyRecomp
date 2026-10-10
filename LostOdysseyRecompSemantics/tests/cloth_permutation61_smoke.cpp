#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cloth_storage61.h"
#include "lo_semantics/cloth_permutation61.h"
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
    constexpr unsigned workspace = 0x60000;
    s.r[3] = workspace;
    s.r[4] = 0;
    s.r[5] = 0;
    s.r[6] = Owner + 4;
    s.r[7] = Owner + 84;
    s.r[8] = Owner + 124;
    s.r[9] = Owner + 164;
    s.r[10] = Owner + 184;
    (void)cloth_storage61::Apply(0x82ba7f90, m, {guest, native}, s);
    (void)vector(4, 60);
    m.WriteU32(workspace + 712, 3);
    unsigned assignments[3][5] = {{0xffffffff, 0, 0xffffffff, 0xffffffff, 0},
                                  {1, 1, 0xffffffff, 0xffffffff, 1},
                                  {2, 2, 0xffffffff, 2, 2}};
    for (unsigned k = 0; k < 3; ++k) {
      s.r[4] = 20;
      env.guest.CallIndirect(Allocate, m, s);
      auto p = Address(s.r[3]);
      m.WriteU32(workspace + 68 + 20 * k, p);
      m.WriteU32(workspace + 72 + 20 * k, p + 20);
      m.WriteU32(workspace + 76 + 20 * k, p + 20);
      for (unsigned j = 0; j < 5; ++j)
        m.WriteU32(p + 4 * j, assignments[k][j]);
    }
    auto children = vector(184, 12);
    for (unsigned k = 0; k < 3; ++k) {
      s.r[4] = 44;
      env.guest.CallIndirect(Allocate, m, s);
      auto child = Address(s.r[3]);
      for (unsigned i = 0; i < 44; ++i)
        m.WriteU8(child + i, 0);
      m.WriteU32(children + 4 * k, child);
      auto count = k == 0 ? 2u : 1u;
      s.r[4] = 4 * count;
      env.guest.CallIndirect(Allocate, m, s);
      auto p = Address(s.r[3]);
      m.WriteU32(child + 4, p);
      m.WriteU32(child + 8, p + 4 * count);
      m.WriteU32(child + 12, p + 4 * count);
      m.WriteU32(p, k == 0 ? 4 : k == 1 ? 0 : 3);
      if (k == 0)
        m.WriteU32(p + 4, 1);
    }
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
      s.r[3] = workspace;
      (void)cloth_permutation61::Apply(0x82bb7b48, m, {guest, native}, s);
      unsigned expected[]{3, 2, 0, 4, 1};
      auto p = m.ReadU32(Owner + 124);
      if (m.ReadU32(Owner + 128) != p + 20 || m.ReadU32(Owner + 132) != p + 20)
        throw std::runtime_error("packed permutation shape");
      for (unsigned i = 0; i < 5; ++i)
        if (m.ReadU32(p + 4 * i) != expected[i])
          throw std::runtime_error(
              "signed bucket/local order and first bucket priority");
    }
    s.r[3] = workspace;
    (void)cloth_storage61::Apply(0x82ba8018, m, {guest, native}, s);
    s.r[3] = Owner;
    (void)cloth_storage61::Apply(0x82ba8858, m, {guest, native}, s);
    if (!env.guest.live.empty())
      throw std::runtime_error("permutation ownership");
    std::puts("PASS cloth signed bucket ordering, local child order, "
              "permutation reuse and cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
