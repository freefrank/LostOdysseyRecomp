#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cloth_storage61.h"
#include "lo_semantics/cloth_stream61.h"
#include "lo_semantics/cloth_load61.h"
#include "lo_semantics/cloth_cook61.h"
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

    auto path = std::getenv("LO_CLOTH_ANGLE_CONSTANTS");
    if (!path)
      throw std::runtime_error("LO_CLOTH_ANGLE_CONSTANTS");
    std::ifstream f(path, std::ios::binary);
    std::array<unsigned char, 20> constants{};
    f.read(reinterpret_cast<char *>(constants.data()), 20);
    if (f.gcount() != 20)
      throw std::runtime_error("private cloth angle constants");
    for (unsigned i = 0; i < 20; ++i)
      m.WriteU8(0x820d6098 + i, constants[i]);
    ClothReadGuest guest(env.guest);
    m.WriteU32(0x51010, 0x2010);
    constexpr unsigned desc = 0x60000, points = 0x61000, indices = 0x62000,
                       weights = 0x63000, attributes = 0x64000,
                       loaded = 0x69000, reader = 0x6a000,
                       readerTable = 0x6b000;
    m.WriteU32(reader, readerTable);
    constexpr unsigned readers[]{0x82bde550, 0x82bde568, 0x82bde580,
                                 0x82bde598, 0x82bde5b8, 0x82bde5d8};
    for (unsigned i = 0; i < 6; ++i)
      m.WriteU32(readerTable + 4 + 4 * i, readers[i]);
    for (unsigned kind = 1; kind <= 2; ++kind)
      for (unsigned swap = 0; swap <= 1; ++swap) {
        for (unsigned i = 0; i < 60; ++i)
          m.WriteU8(desc + i, 0);
        m.WriteU32(desc, kind);
        m.WriteU32(desc + 4, 5);
        m.WriteU32(desc + 8, 16);
        m.WriteU32(desc + 28, points);
        m.WriteU32(desc + 40, 0x100);
        m.WriteU32(desc + (kind == 1 ? 12 : 20), 2);
        m.WriteU32(desc + (kind == 1 ? 16 : 24), kind == 1 ? 12 : 16);
        m.WriteU32(desc + (kind == 1 ? 32 : 36), indices);
        m.WriteU32(desc + 44, 8);
        m.WriteU32(desc + 48, 8);
        m.WriteU32(desc + 52, weights);
        m.WriteU32(desc + 56, attributes);
        float triPoints[]{9, 9, 9, 8, 8, 8, 0, 0, 0, 1, 0, 0, 0, 1, 0},
            tetraPoints[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, -1};
        unsigned tri[]{2, 3, 4, 4, 3, 2}, tetra[]{0, 1, 2, 3, 0, 2, 1, 4};
        auto xyz = kind == 1 ? triPoints : tetraPoints;
        auto ids = kind == 1 ? tri : tetra;
        for (unsigned i = 0; i < 5; ++i) {
          for (unsigned j = 0; j < 3; ++j)
            m.WriteU32(points + 16 * i + 4 * j,
                       std::bit_cast<unsigned>(xyz[3 * i + j]));
          m.WriteU32(weights + 8 * i, std::bit_cast<unsigned>(float(i + 1)));
          m.WriteU32(attributes + 8 * i, 10 + i);
        }
        for (unsigned i = 0; i < (kind == 1 ? 6u : 8u); ++i)
          m.WriteU32(indices + 4 * i, ids[i]);
        s.r[3] = Owner;
        (void)cloth_storage61::Apply(0x82b9cdd8, m, {guest, native}, s);
        s.r[3] = Owner;
        s.r[4] = desc;
        (void)cloth_cook61::Apply(0x82bac6d0, m, {guest, native}, s);
        if (s.r[3] != 1 || m.ReadU32(Owner + 88) != m.ReadU32(Owner + 84))
          throw std::runtime_error("cloth complete cook");
        auto permutation = m.ReadU32(Owner + 124),
             inverse = m.ReadU32(Owner + 144);
        for (unsigned i = 0; i < 5; ++i) {
          auto p = m.ReadU32(permutation + 4 * i);
          if (p >= 5 || m.ReadU32(inverse + 4 * p) != i)
            throw std::runtime_error("cloth inverse permutation");
          for (unsigned j = 0; j < 3; ++j)
            if (m.ReadU32(m.ReadU32(Owner + 4) + 12 * p + 4 * j) !=
                std::bit_cast<unsigned>(xyz[3 * i + j]))
              throw std::runtime_error("cloth remapped positions");
          if (m.ReadU32(m.ReadU32(Owner + 44) + 4 * p) !=
                  std::bit_cast<unsigned>(float(i + 1)) ||
              m.ReadU32(m.ReadU32(Owner + 64) + 4 * p) != 10 + i)
            throw std::runtime_error("cloth remapped channels");
        }
        for (unsigned i = 0; i < (kind == 1 ? 6u : 8u); ++i)
          if (m.ReadU32(m.ReadU32(Owner + 24) + 4 * i) !=
              m.ReadU32(permutation + 4 * ids[i]))
            throw std::runtime_error("cloth remapped indices");
        if (kind == 1 && (m.ReadU32(m.ReadU32(Owner + 104)) ||
                          m.ReadU32(m.ReadU32(Owner + 104) + 4)))
          throw std::runtime_error("cloth duplicate map");
        m.WriteU32(Writer + 4, 0);
        s.r[3] = Owner;
        s.r[4] = Writer;
        s.r[5] = swap;
        (void)cloth_stream61::Apply(0x82ba7760, m, {guest, native}, s);
        auto length = m.ReadU32(Writer + 4);
        std::vector<unsigned char> bytes(length);
        for (unsigned i = 0; i < length; ++i)
          bytes[i] = m.ReadU8(cook_main_smoke::Buffer + i);
        s.r[3] = loaded;
        (void)cloth_storage61::Apply(0x82b9cdd8, m, {guest, native}, s);
        m.WriteU32(reader + 4, cook_main_smoke::Buffer);
        s.r[3] = loaded;
        s.r[4] = reader;
        (void)cloth_load61::Apply(0x82baa130, m, {guest, native}, s);
        if (s.r[3] != 1 ||
            m.ReadU32(reader + 4) != cook_main_smoke::Buffer + length)
          throw std::runtime_error("cloth cooked readback");
        m.WriteU32(Writer + 4, 0);
        s.r[3] = loaded;
        s.r[4] = Writer;
        s.r[5] = swap;
        (void)cloth_stream61::Apply(0x82ba7760, m, {guest, native}, s);
        if (m.ReadU32(Writer + 4) != length)
          throw std::runtime_error("cloth reserialized length");
        for (unsigned i = 0; i < length; ++i)
          if (m.ReadU8(cook_main_smoke::Buffer + i) != bytes[i])
            throw std::runtime_error("cloth cooked roundtrip bytes");
        constexpr unsigned publicDesc = 0x65000;
        unsigned publicWords[]{5,
                               2,
                               16,
                               kind == 1 ? 12u : 16u,
                               points,
                               indices,
                               kind == 1 ? 0x100u : 4u,
                               8,
                               8,
                               weights,
                               attributes};
        for (unsigned i = 0; i < 11; ++i)
          m.WriteU32(publicDesc + 4 * i, publicWords[i]);
        m.WriteU32(0x832dc180, swap ? 1 : 2);
        m.WriteU32(Writer + 4, 0);
        s.r[3] = publicDesc;
        s.r[4] = Writer;
        (void)cloth_cook61::Apply(kind == 1 ? 0x82b9ce98 : 0x82b9d0a0, m,
                                  {guest, native}, s);
        if (s.r[3] != 1 || m.ReadU32(Writer + 4) != length)
          throw std::runtime_error("cloth public cook/write entry");
        m.WriteU32(reader + 4, cook_main_smoke::Buffer);
        s.r[3] = loaded;
        s.r[4] = reader;
        (void)cloth_load61::Apply(0x82baa130, m, {guest, native}, s);
        if (s.r[3] != 1 ||
            m.ReadU32(reader + 4) != cook_main_smoke::Buffer + length ||
            m.ReadU32(loaded + 212) != kind)
          throw std::runtime_error("cloth public entry readback");
        for (unsigned object : {loaded, Owner}) {
          s.r[3] = object;
          (void)cloth_storage61::Apply(0x82ba8858, m, {guest, native}, s);
        }
        if (!env.guest.live.empty())
          throw std::runtime_error("cloth full chain ownership");
      }
    std::puts("PASS complete triangle/tetra cloth cook, channel remap, "
              "both-endian roundtrip and cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
