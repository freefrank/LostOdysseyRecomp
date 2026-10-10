#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_stream61.h"
#include "lo_semantics/mesh_triangle_cook61.h"
#include "lo_semantics/mesh_triangle_views61.h"
#include "lo_semantics/mesh_triangle_flags61.h"
#include "lo_semantics/mesh_triangle_storage61.h"
int main() {
  try {
    auto load = [](const char *env, auto &out) {
      const char *p = std::getenv(env);
      if (!p)
        throw std::runtime_error(env);
      std::ifstream f(p, std::ios::binary);
      f.read(reinterpret_cast<char *>(out.data()), out.size());
      if (f.gcount() != std::streamsize(out.size()))
        throw std::runtime_error("private bundle size");
    };
    using namespace cook_main_smoke;
    load("LO_MESH_MATH_CONSTANTS", constants);
    load("LO_NORMAL_ENCODING_CONSTANTS", normal_constants);
    load("LO_MASS_CONSTANTS", mass_constants);
    load("LO_POWER_CONSTANTS", power_constants);
    load("LO_BOUNDS_CONSTANTS", bounds_constants);

    for (unsigned mode : {0u, 2u, 4u}) {
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

      for (unsigned i = 0; i < 128; ++i)
        m.WriteU8(0x83214e08 + i, normal_constants[i]);
      constexpr GuestAddress extra[]{0x820d60a8, 0x820d6454, 0x82000e40,
                                     0x822181c4};
      for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
          m.WriteU8(extra[i] + j, normal_constants[128 + 4 * i + j]);
      m.WriteU32(0x8201f9f0, std::bit_cast<std::uint32_t>(0.5f));
      m.WriteU32(0x82000f20, std::bit_cast<std::uint32_t>(1.f / 3.f));
      m.WriteU32(0x82000d64, std::bit_cast<std::uint32_t>(
                                 -std::numeric_limits<float>::max()));
      m.WriteU32(0x82000e0c, std::bit_cast<std::uint32_t>(
                                 std::numeric_limits<float>::max()));
      m.WriteU32(0x82000e50, 0);
      m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
      m.WriteU32(0x82000dac, std::bit_cast<std::uint32_t>(0.1f));
      m.WriteU32(0x820038fc, std::bit_cast<std::uint32_t>(0.1f));
      m.WriteU32(0x821baa74, std::bit_cast<std::uint32_t>(2.f));
      for (unsigned i = 0; i < 184; ++i)
        m.WriteU8(0x83214e88 + i, constants[i]);
      for (unsigned i = 0; i < 72; ++i)
        m.WriteU8(0x820d6a78 + i, mass_constants[i]);
      constexpr GuestAddress massExtra[]{0x82000fe8, 0x82001010, 0x82051430,
                                         0x82000f70, 0x82048090, 0x82000f28,
                                         0x820d6ac0};
      for (unsigned i = 0; i < 7; ++i)
        for (unsigned j = 0; j < 8; ++j)
          m.WriteU8(massExtra[i] + j, mass_constants[72 + 8 * i + j]);

      unsigned offset = 0;
      for (auto region : std::array<test::Region, 6>{{{0x82000e00, 768},
                                                      {0x820d2f68, 512},
                                                      {0x83215500, 40},
                                                      {0x822181a0, 112},
                                                      {0x83214fc0, 8},
                                                      {0x820d5e30, 8}}})
        for (unsigned i = 0; i < region.size; ++i)
          m.WriteU8(region.base + i, power_constants[offset++]);
      for (unsigned i = 0; i < 4; ++i) {
        m.WriteU8(0x820d6a18 + i, bounds_constants[i]);
        m.WriteU8(0x82000d70 + i, bounds_constants[i + 4]);
      }
      m.WriteU32(0x82000d7c, std::bit_cast<unsigned>(.01f));
      m.WriteU32(0x820d6670, std::bit_cast<unsigned>(.001f));
      m.WriteU32(0x82003660, std::bit_cast<unsigned>(2.5f));
      const unsigned mesh = Owner + 4, adapter = 0x60000, reader = 0x61000,
                     readerTable = 0x62000;
      auto storage = [&](unsigned e, unsigned self, unsigned count = 0) {
        s.r[3] = self;
        s.r[4] = count;
        (void)mesh_triangle_storage61::Apply(e, m, deps, s);
      };
      unsigned desc = 0x65000, points = 0x66000, indices = 0x67000;
      float xyz[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
      unsigned tris[]{0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3};
      for (unsigned i = 0; i < 12; ++i) {
        m.WriteU32(points + 4 * i, std::bit_cast<unsigned>(xyz[i]));
        m.WriteU32(indices + 4 * i, tris[i]);
      }
      m.WriteU32(desc, 4);
      m.WriteU32(desc + 4, 4);
      m.WriteU32(desc + 8, 12);
      m.WriteU32(desc + 12, 12);
      m.WriteU32(desc + 16, points);
      m.WriteU32(desc + 20, indices);
      m.WriteU32(desc + 36, 255);
      m.WriteU32(desc + 48, std::bit_cast<unsigned>(.001f));
      if (mode == 2) {
        m.WriteU32(desc + 24, 2);
        m.WriteU32(desc + 12, 6);
        for (unsigned i = 0; i < 12; ++i)
          m.WriteU16(indices + 2 * i, tris[i]);
      }
      if (mode == 4) {
        m.WriteU32(desc, 12);
        m.WriteU32(desc + 20, 0);
        for (unsigned i = 0; i < 12; ++i)
          for (unsigned j = 0; j < 3; ++j)
            m.WriteU32(points + 12 * i + 4 * j,
                       std::bit_cast<unsigned>(xyz[3 * tris[i] + j]));
      }
      m.WriteU32(0x83216174, 17);
      m.WriteU32(0x8321614c, 17);
      m.WriteU32(0x832dc414, 1);
      s.r[3] = desc;
      s.r[4] = Writer;
      (void)mesh_triangle_cook61::Apply(0x82b9cc00, m, deps, s);
      if (s.r[3] != 1 || !env.guest.live.empty())
        throw std::runtime_error("whole triangle cook/cleanup");
      storage(0x82b9e2d8, Owner);
      constexpr unsigned readers[]{0x82bde550, 0x82bde568, 0x82bde580,
                                   0x82bde598, 0x82bde5b8, 0x82bde5d8};
      m.WriteU32(reader, readerTable);
      m.WriteU32(reader + 4, cook_main_smoke::Buffer);
      for (unsigned i = 0; i < 6; ++i)
        m.WriteU32(readerTable + 4 + 4 * i, readers[i]);
      constexpr unsigned adapters[]{0x82b9d4c8, 0x824b9f18, 0x824b9f30,
                                    0x82b9c788, 0x82b9c7a0, 0x82b9d4e0};
      for (unsigned i = 0; i < 6; ++i)
        m.WriteU32(0x820d5b24 + 4 + 4 * i, adapters[i]);

      s.r[3] = Owner;
      s.r[4] = reader;
      (void)mesh_triangle_stream61::Apply(0x82b9d4f8, m, deps, s);
      if (s.r[3] != 1 || m.ReadU32(mesh) != 4 || m.ReadU32(mesh + 4) != 4 ||
          m.ReadU32(reader + 4) !=
              cook_main_smoke::Buffer + m.ReadU32(Writer + 4) ||
          m.ReadU32(mesh + 24) != 1 || m.ReadU32(mesh + 28) != 4)
        throw std::runtime_error("whole triangle reload");
      if (std::abs(std::bit_cast<float>(m.ReadU32(Owner + 208)) - 1.f / 6) >
          1e-5f)
        throw std::runtime_error("whole triangle mass");
      for (unsigned channel = 0; channel < 3; ++channel) {
        s.r[3] = Owner;
        s.r[4] = 0;
        s.r[5] = channel;
        (void)mesh_triangle_views61::Apply(0x82b9e068, m, s);
        if (s.r[3] != 4)
          throw std::runtime_error("triangle channel count");
        s.r[3] = Owner;
        (void)mesh_triangle_views61::Apply(0x82b9e180, m, s);
        if (s.r[3] != 12)
          throw std::runtime_error("triangle channel stride");
        s.r[3] = Owner;
        (void)mesh_triangle_views61::Apply(0x82b9e0a0, m, s);
        if (s.r[3] != (channel ? 1u : 4u))
          throw std::runtime_error("triangle channel format");
      }
      s.r[3] = 0x68000;
      s.r[4] = Owner;
      (void)mesh_triangle_views61::Apply(0x82b9e270, m, s);
      for (unsigned i = 0; i < 13; ++i)
        if (m.ReadU32(0x68000 + 4 * i) != m.ReadU32(Owner + 208 + 4 * i))
          throw std::runtime_error("borrowed mass export");
      storage(0x82b9e220, Owner);
      if (!env.guest.live.empty())
        throw std::runtime_error("triangle tree ownership");
      if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr))
        throw std::runtime_error("triangle tree ABI");
      std::puts("PASS whole tetrahedron triangle cook, partition, mass, "
                "stream reload and complete cleanup");
    }
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
