#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cloth_storage61.h"
#include "lo_semantics/cloth_triangle_edges61.h"
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

    std::array<unsigned char, 20> constants{};
    auto path = std::getenv("LO_CLOTH_ANGLE_CONSTANTS");
    if (!path)
      throw std::runtime_error("LO_CLOTH_ANGLE_CONSTANTS");
    std::ifstream f(path, std::ios::binary);
    f.read(reinterpret_cast<char *>(constants.data()), 20);
    if (f.gcount() != 20)
      throw std::runtime_error("private cloth angle constants");
    for (unsigned i = 0; i < 20; ++i)
      m.WriteU8(0x820d6098 + i, constants[i]);
    auto allocate = [&](unsigned size) {
      s.r[4] = size;
      env.guest.CallIndirect(Allocate, m, s);
      return Address(s.r[3]);
    };
    s.r[3] = Owner;
    (void)cloth_storage61::Apply(0x82b9cdd8, m, deps.lifetime, s);
    auto vector = [&](unsigned off, unsigned bytes) {
      auto p = allocate(bytes);
      m.WriteU32(Owner + off, p);
      m.WriteU32(Owner + off + 4, p + bytes);
      m.WriteU32(Owner + off + 8, p + bytes);
      return p;
    };
    auto points = vector(4, 48), indices = vector(24, 36),
         mapping = vector(104, 12);
    float xyz[]{0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
    unsigned tri[]{0, 1, 2, 0, 2, 3, 2, 1, 0};
    for (unsigned i = 0; i < 12; ++i)
      m.WriteU32(points + 4 * i, std::bit_cast<unsigned>(xyz[i]));
    for (unsigned i = 0; i < 9; ++i)
      m.WriteU32(indices + 4 * i, tri[i]);
    m.WriteU32(mapping, 0);
    m.WriteU32(mapping + 4, 1);
    m.WriteU32(mapping + 8, 0);
    s.r[3] = Owner;
    (void)cloth_triangle_edges61::Apply(0x82bab468, m, deps.lifetime, s);
    auto output = m.ReadU32(Owner + 84);
    if (m.ReadU32(Owner + 88) - output != 5 * 68)
      throw std::runtime_error("unique cloth edges/duplicate suppression");
    unsigned interior = 0;
    for (unsigned i = 0; i < 5; ++i) {
      auto p = output + 68 * i;
      bool shared = m.ReadU32(p + 12) != 0xffffffff;
      auto value = [&](unsigned off) {
        return std::bit_cast<float>(m.ReadU32(p + off));
      };
      if (shared) {
        ++interior;
        if (m.ReadU32(p) != 0 || m.ReadU32(p + 4) != 2 ||
            std::abs(value(28) - std::sqrt(2.f)) > 1e-5f ||
            std::abs(value(32) - std::sqrt(2.f)) > 1e-5f ||
            !(value(36) > 2.f && value(36) < 4.f))
          throw std::runtime_error("cloth shared-edge geometry");
      } else if (value(28) != 1.f || value(32) != 0.f || value(36) != 0.f)
        throw std::runtime_error("cloth boundary-edge geometry");
    }
    if (interior != 1)
      throw std::runtime_error("cloth shared diagonal");
    s.r[3] = Owner;
    (void)cloth_storage61::Apply(0x82ba8858, m, deps.lifetime, s);
    if (!env.guest.live.empty())
      throw std::runtime_error("cloth edge ownership");
    std::puts("PASS cloth shared/boundary edges, duplicate suppression, "
              "geometry and cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
