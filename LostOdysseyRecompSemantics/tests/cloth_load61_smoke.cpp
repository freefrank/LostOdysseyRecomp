#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cloth_storage61.h"
#include "lo_semantics/cloth_stream61.h"
#include "lo_semantics/cloth_load61.h"
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

    for (unsigned type : {1u, 2u})
      for (unsigned swap : {0u, 1u}) {
        s.r[3] = Owner;
        (void)cloth_storage61::Apply(0x82b9cdd8, m, deps.lifetime, s);
        unsigned cursor = 0x60000;
        auto vector = [&](unsigned object, unsigned offset, unsigned words) {
          auto p = cursor;
          cursor += 256;
          m.WriteU32(object + offset, p);
          m.WriteU32(object + offset + 4, p + 4 * words);
          for (unsigned i = 0; i < words; ++i)
            m.WriteU32(p + 4 * i, 1000 + offset + i);
          return p;
        };
        m.WriteU32(Owner + 212, type);
        m.WriteU32(Owner + 216, 0x55);
        vector(Owner, 4, 9);
        vector(Owner, 24, type == 1 ? 3 : 4);
        vector(Owner, 44, 2);
        vector(Owner, 64, 1);
        vector(Owner, 104, 1);
        m.WriteU32(vector(Owner, 124, 1), 0);
        vector(Owner, 164, 1);
        auto children = vector(Owner, 184, 1);
        unsigned child = 0x68000;
        m.WriteU32(children, child);
        m.WriteU32(child, 17);
        vector(child, 4, 2);
        auto record = vector(child, 24, 8);
        for (unsigned i = 0; i < 16; ++i)
          m.WriteU16(record + 2 * i, 300 + i);
        m.WriteU32(Writer + 4, 0);
        s.r[3] = Owner;
        s.r[4] = Writer;
        s.r[5] = swap;
        (void)cloth_stream61::Apply(0x82ba7760, m, {env.guest, native}, s);
        unsigned length = m.ReadU32(Writer + 4),
                 expected = type == 1 ? 162 : 166;
        if (s.r[3] != 1 || length != expected)
          throw std::runtime_error("CLTH output size");
        auto read32 = [&](unsigned off) {
          auto value = m.ReadU32(cook_main_smoke::Buffer + off);
          return swap ? __builtin_bswap32(value) : value;
        };
        auto read16 = [&](unsigned off) {
          auto value = m.ReadU16(cook_main_smoke::Buffer + off);
          return swap ? __builtin_bswap16(value) : value;
        };
        if (m.ReadU32(cook_main_smoke::Buffer + 4) != 0x434c5448 ||
            read32(8) != 3 || read32(12) != type || read32(20) != 3 ||
            read16(length - 2) != 314)
          throw std::runtime_error("CLTH header/record fields");
        unsigned loaded = 0x69000, reader = 0x6a000, readerTable = 0x6b000;
        constexpr unsigned readers[]{0x82bde550, 0x82bde568, 0x82bde580,
                                     0x82bde598, 0x82bde5b8, 0x82bde5d8};
        m.WriteU32(reader, readerTable);
        for (unsigned i = 0; i < 6; ++i)
          m.WriteU32(readerTable + 4 + 4 * i, readers[i]);
        m.WriteU32(0x51010, 0x2010);
        ClothReadGuest guest(env.guest);
        s.r[3] = loaded;
        (void)cloth_storage61::Apply(0x82b9cdd8, m, {guest, native}, s);
        for (unsigned repeat = 0; repeat < 2; ++repeat) {
          m.WriteU32(reader + 4, cook_main_smoke::Buffer);
          s.r[3] = loaded;
          s.r[4] = reader;
          (void)cloth_load61::Apply(0x82baa130, m, {guest, native}, s);
          if (s.r[3] != 1 ||
              m.ReadU32(reader + 4) != cook_main_smoke::Buffer + length ||
              m.ReadU32(loaded + 212) != type)
            throw std::runtime_error("CLTH load/cursor");
          for (unsigned off : {4u, 24u, 44u, 64u, 124u, 164u}) {
            unsigned size = m.ReadU32(Owner + off + 4) - m.ReadU32(Owner + off);
            if (m.ReadU32(loaded + off + 4) - m.ReadU32(loaded + off) != size)
              throw std::runtime_error("CLTH vector size");
            for (unsigned i = 0; i < size; ++i)
              if (m.ReadU8(m.ReadU32(loaded + off) + i) !=
                  m.ReadU8(m.ReadU32(Owner + off) + i))
                throw std::runtime_error("CLTH vector content");
          }
          if (m.ReadU32(m.ReadU32(loaded + 144)) != 0)
            throw std::runtime_error("CLTH inverse permutation");
          auto child = m.ReadU32(m.ReadU32(loaded + 184)),
               p = m.ReadU32(child + 24);
          if (m.ReadU32(child) != 17 || m.ReadU16(p + 28) != 314 ||
              (type == 1 && m.ReadU32(p + 8) != 0xffffffff))
            throw std::runtime_error("CLTH child record");
        }
        s.r[3] = loaded;
        (void)cloth_storage61::Apply(0x82ba8858, m, {guest, native}, s);
        if (!env.guest.live.empty())
          throw std::runtime_error("CLTH readback ownership");
        if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr))
          throw std::runtime_error("CLTH writer ABI");
      }
    std::puts("PASS CLTH both types/endians, repeated load, inverse map, "
              "nested records and cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
