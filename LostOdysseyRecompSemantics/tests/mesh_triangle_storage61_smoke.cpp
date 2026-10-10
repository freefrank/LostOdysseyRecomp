#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
int main() {
  try {
    using namespace cook_main_smoke;
    for (unsigned deleting : {0u, 1u}) {
      test::GuestWindow w(cook_main_smoke::Regions);
      w.Fill(0);
      auto m = w.Memory();
      cook_main_smoke::Environment env(w);
      auto s = sort_engine61_oracle::Initial(0), initial = s;
      m.WriteU32(0x83216624, AllocatorTable);
      m.WriteU32(AllocatorTable, Allocate);
      m.WriteU32(AllocatorTable + 12, Free);
      m.WriteU32(0x832df548, 0x50000);
      m.WriteU32(0x50000, 0x51000);
      m.WriteU32(0x51008, Allocate);
      m.WriteU32(0x51014, Free);
      for (auto [p, v] :
           std::array<std::pair<unsigned, float>, 5>{{{0x82000d6c, .001f},
                                                      {0x82000d64, -100.f},
                                                      {0x82000e0c, 100.f},
                                                      {0x82000e50, 0.f},
                                                      {0x82000e40, -1.f}}})
        m.WriteU32(p, std::bit_cast<unsigned>(v));
      auto allocate = [&](unsigned size) {
        s.r[4] = size;
        env.guest.CallIndirect(Allocate, m, s);
        return Address(s.r[3]);
      };
      unsigned owner = deleting ? allocate(264) : Owner;
      auto call = [&](unsigned entry, unsigned self, unsigned arg = 0) {
        s.r[3] = self;
        s.r[4] = arg;
        (void)mesh_triangle_storage61::Apply(entry, m, env.Deps(), s);
      };
      call(0x82b9e2d8, owner);
      auto mesh = owner + 4;
      if (m.ReadU32(owner) != 0x820d5c58 ||
          m.ReadU32(mesh + 96) != std::bit_cast<unsigned>(.001f) ||
          m.ReadU32(owner + 172) != 255 || m.ReadU32(owner + 176) != 255)
        throw std::runtime_error("triangle constructor defaults");
      for (unsigned off = 0; off < 96; off += 4)
        if (m.ReadU32(mesh + off))
          throw std::runtime_error("triangle initialized pointer fields");
      call(0x82bc5c90, mesh);
      if (s.r[3] != 0)
        throw std::runtime_error("empty material array");
      call(0x82bc5be0, mesh, 4);
      call(0x82bc5c38, mesh, 2);
      call(0x82bc5c90, mesh);
      call(0x82bc5d00, mesh);
      if (m.ReadU32(mesh) != 4 || m.ReadU32(mesh + 4) != 2)
        throw std::runtime_error("triangle counts");
      for (unsigned off : {8, 12, 76, 80})
        if (!env.guest.live.contains(m.ReadU32(mesh + off)))
          throw std::runtime_error("triangle owned arrays");
      std::ifstream f(std::getenv("LO_MESH_MATH_CONSTANTS"), std::ios::binary);
      std::array<unsigned char, 184> constants;
      f.read(reinterpret_cast<char *>(constants.data()), constants.size());
      if (f.gcount() != 184)
        throw std::runtime_error("private atan2 constants");
      for (unsigned i = 0; i < 184; ++i)
        m.WriteU8(0x83214e88 + i, constants[i]);
      m.WriteU32(0x82007784, std::bit_cast<unsigned>(1.f));
      float points[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
      for (unsigned i = 0; i < 12; ++i)
        m.WriteU32(m.ReadU32(mesh + 8) + 4 * i,
                   std::bit_cast<unsigned>(points[i]));
      for (unsigned i = 0; i < 6; ++i)
        m.WriteU32(m.ReadU32(mesh + 12) + 4 * i, i % 3);
      s.r[3] = owner;
      s.r[4] = 0;
      s.r[5] = 2;
      (void)mesh_triangle_storage61::Apply(0x82b9e0d8, m, env.Deps(), s);
      auto normals = Address(s.r[3]);
      for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 3; ++j) {
          float expected = j == (i == 3 ? 1u : 2u) ? 1.f : 0.f;
          if (std::abs(
                  std::bit_cast<float>(m.ReadU32(normals + 12 * i + 4 * j)) -
                  expected) > 1e-5f)
            throw std::runtime_error(
                "triangle angle weighted normals/fallback");
        }
      auto allocations = env.guest.allocations;
      s.r[3] = owner;
      s.r[4] = 0;
      s.r[5] = 2;
      (void)mesh_triangle_storage61::Apply(0x82b9e0d8, m, env.Deps(), s);
      if (s.r[3] != normals || env.guest.allocations != allocations)
        throw std::runtime_error("cached normal view");
      for (unsigned off : {32, 36, 40})
        m.WriteU32(mesh + off, allocate(16));
      for (unsigned off : {192, 196, 204})
        m.WriteU32(owner + off, allocate(16));
      if (deleting) {
        auto links = allocate(8), payload = allocate(16);
        m.WriteU32(links + 4, payload + 4);
        m.WriteU32(mesh + 84, links);
      } else
        m.WriteU32(mesh + 84, 1);
      auto edge = allocate(24);
      for (unsigned off : {0, 4, 8, 12, 16, 20})
        m.WriteU32(edge + off, 0);
      m.WriteU32(mesh + 88, edge);
      call(deleting ? 0x82b9e388 : 0x82b9e220, owner);
      if (!env.guest.live.empty() || m.ReadU32(owner) != 0x820d5a18)
        throw std::runtime_error("triangle owner cleanup");
      for (unsigned off : {8, 12, 16, 32, 36, 40, 76, 80, 84, 88})
        if (m.ReadU32(mesh + off))
          throw std::runtime_error("triangle released field");
      if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr))
        throw std::runtime_error("triangle storage ABI");
    }
    std::puts(
        "PASS triangle mesh angle-weighted normals, cached view, array "
        "ownership, nested prefixed links, sentinel and deleting cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
