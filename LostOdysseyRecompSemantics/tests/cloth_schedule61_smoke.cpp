#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cloth_storage61.h"
#include "lo_semantics/cloth_schedule61.h"
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
    for (unsigned mode = 1; mode <= 3; ++mode) {
      unsigned kind = mode == 2 ? 2 : 1;
      if (mode > 1) {
        s.r[3] = Owner;
        (void)cloth_storage61::Apply(0x82b9cdd8, m, {guest, native}, s);
      }
      auto count = mode == 3   ? 26u
                   : kind == 1 ? 32u
                               : 5u,
           items = mode == 3   ? 25u
                   : kind == 1 ? 16u
                               : 2u;
      auto points = vector(4, count * 12), constraints = vector(84, items * 68);
      for (unsigned v = 0; v < count; ++v)
        for (unsigned k = 0; k < 3; ++k)
          m.WriteU32(points + 12 * v + 4 * k,
                     std::bit_cast<unsigned>(float(v + k)));
      for (unsigned i = 0; i < items; ++i) {
        auto p = constraints + 68 * i;
        for (unsigned j = 0; j < 68; ++j)
          m.WriteU8(p + j, 0);
        for (unsigned j = 0; j < 4; ++j)
          m.WriteU32(p + 4 * j, kind == 1 ? (j < 2 ? (mode == 3 ? i : 2 * i) + j
                                                   : 0xffffffff)
                                          : (j < 3 ? j : 3 + i));
        if (kind == 1) {
          m.WriteU32(p + 28, std::bit_cast<unsigned>(2.f));
          m.WriteU16(p + 24, 7);
        } else {
          m.WriteU32(p + 40, std::bit_cast<unsigned>(1.f));
          for (unsigned j = 0; j < 6; ++j)
            m.WriteU32(p + 44 + 4 * j, std::bit_cast<unsigned>(float(j + 1)));
        }
      }
      s.r[3] = workspace;
      s.r[4] = kind;
      s.r[5] = 1;
      s.r[6] = Owner + 4;
      s.r[7] = Owner + 84;
      s.r[8] = Owner + 124;
      s.r[9] = Owner + 164;
      s.r[10] = Owner + 184;
      (void)cloth_storage61::Apply(0x82ba7f90, m, {guest, native}, s);
      s.r[3] = workspace;
      (void)cloth_schedule61::Apply(0x82bb6290, m, {guest, native}, s);
      auto children = m.ReadU32(Owner + 184),
           n = (m.ReadU32(Owner + 188) - children) / 4;
      if (n != (mode == 3   ? 3u
                : kind == 1 ? 2u
                            : 1u) ||
          m.ReadU32(workspace + 708) != n ||
          m.ReadU32(workspace + 712) != (mode == 3 ? 2u : 1u)) {
        std::fprintf(stderr, "mode=%u n=%u count=%u tier=%u\n", mode, n,
                     m.ReadU32(workspace + 708), m.ReadU32(workspace + 712));
        throw std::runtime_error("cloth batch count/tier");
      }
      unsigned records = 0, vertices = 0;
      for (unsigned i = 0; i < n; ++i) {
        auto child = m.ReadU32(children + 4 * i),
             nr = (m.ReadU32(child + 28) - m.ReadU32(child + 24)) / 32,
             nv = (m.ReadU32(child + 8) - m.ReadU32(child + 4)) / 4;
        records += nr;
        vertices += nv;
        if (16 * nv + 8 * nr > (kind == 1 ? 560u : 240u))
          throw std::runtime_error("cloth batch budget");
        for (unsigned j = 0; j < nr; ++j) {
          auto p = m.ReadU32(child + 24) + 32 * j;
          for (unsigned k = 0; k < (kind == 1 ? 2u : 4u); ++k)
            if (m.ReadU16(p + 2 * k) >= nv)
              throw std::runtime_error("packed local vertex index");
          if (kind == 1 &&
              (m.ReadU16(p + 4) != 0xffff || m.ReadU16(p + 6) != 0xffff ||
               m.ReadU16(p + 28) != 7 ||
               std::bit_cast<float>(m.ReadU32(p + 12)) != 2.f))
            throw std::runtime_error("triangle packed constraint");
          if (kind == 2) {
            if (std::bit_cast<float>(m.ReadU32(p + 8)) != 1.f)
              throw std::runtime_error("tetra packed volume");
            for (unsigned k = 0; k < 6; ++k)
              if (m.ReadU16(p + 12 + 2 * k) !=
                  (std::bit_cast<unsigned>(float(k + 1)) >> 16))
                throw std::runtime_error("tetra high-word lengths");
          }
        }
      }
      if (records != items || vertices != count + (mode == 3 ? 2u : 0u))
        throw std::runtime_error("cloth batching completeness");
      s.r[3] = workspace;
      (void)cloth_storage61::Apply(0x82ba8018, m, {guest, native}, s);
      s.r[3] = Owner;
      (void)cloth_storage61::Apply(0x82ba8858, m, {guest, native}, s);
      if (!env.guest.live.empty())
        throw std::runtime_error("schedule ownership");
    }
    std::puts(
        "PASS cloth triangle/tetra packing, bounded split batches and cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
