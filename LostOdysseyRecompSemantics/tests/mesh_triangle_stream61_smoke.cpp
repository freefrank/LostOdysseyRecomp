#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_stream61.h"
#include "lo_semantics/mesh_triangle_flags61.h"
#include "lo_semantics/mesh_triangle_storage61.h"
int main() {
  try {
    using namespace cook_main_smoke;
    for (unsigned endian : {0u, 1u}) {
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
      m.WriteU32(0x832dc414, endian ? 0 : 1);
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

      const unsigned mesh = Owner + 4, adapter = 0x60000, reader = 0x61000,
                     readerTable = 0x62000;
      auto storage = [&](unsigned e, unsigned self, unsigned count = 0) {
        s.r[3] = self;
        s.r[4] = count;
        (void)mesh_triangle_storage61::Apply(e, m, deps, s);
      };
      storage(0x82b9e2d8, Owner);
      storage(0x82bc5be0, mesh, 27);
      storage(0x82bc5c38, mesh, 9);
      storage(0x82bc5c90, mesh);
      storage(0x82bc5d00, mesh);
      for (unsigned face = 0; face < 9; ++face) {
        float xyz[]{float(3 * face), 0, 0, float(3 * face + 1), 0, 0,
                    float(3 * face), 1, 0};
        for (unsigned j = 0; j < 9; ++j)
          m.WriteU32(m.ReadU32(mesh + 8) + 36 * face + 4 * j,
                     std::bit_cast<unsigned>(xyz[j]));
        for (unsigned j = 0; j < 3; ++j)
          m.WriteU32(m.ReadU32(mesh + 12) + 12 * face + 4 * j, 3 * face + j);
        m.WriteU16(m.ReadU32(mesh + 76) + 2 * face, 100 + face);
        m.WriteU32(m.ReadU32(mesh + 80) + 4 * face, 200 + face);
      }
      m.WriteU32(adapter, mesh);
      s.r[3] = adapter;
      s.r[4] = 255;
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(0.0);
      (void)mesh_triangle_tree61::Apply(0x82bb4160, m, deps, s);
      if (s.r[3] != 1 || !m.ReadU32(mesh + 44))
        throw std::runtime_error("triangle tree build");
      if (std::none_of(
              env.guest.events.begin(), env.guest.events.end(),
              [](const auto &event) { return event.back() == 0x82bb4138; }))
        throw std::runtime_error("triangle reorder callback not exercised");
      for (unsigned i = 0; i < 9; ++i) {
        auto source = m.ReadU32(m.ReadU32(mesh + 80) + 4 * i) - 200;
        if (source >= 9 ||
            m.ReadU16(m.ReadU32(mesh + 76) + 2 * i) != 100 + source ||
            m.ReadU32(m.ReadU32(mesh + 12) + 12 * i) != 3 * source)
          throw std::runtime_error("coupled triangle remap");
      }
      m.WriteU32(0x821baa74, std::bit_cast<unsigned>(2.f));
      m.WriteU32(0x82000dac, std::bit_cast<unsigned>(.1f));
      s.r[3] = adapter;
      (void)mesh_triangle_flags61::Apply(0x82bb5230, m, deps, s);
      m.WriteU32(Owner + 28, 2);
      m.WriteU32(Owner + 32, 3);
      for (unsigned off : {36u, 40u}) {
        s.r[4] = 18;
        env.guest.CallIndirect(Allocate, m, s);
        m.WriteU32(Owner + off, Address(s.r[3]));
      }
      for (unsigned i = 0; i < 9; ++i) {
        m.WriteU16(m.ReadU32(Owner + 36) + 2 * i, i % 2);
        m.WriteU8(m.ReadU32(Owner + 40) + i, i % 3);
      }
      m.WriteU32(0x83216174, 17);
      m.WriteU32(0x8321614c, 17);
      // A pre-existing zero mass cache keeps this open-surface fixture focused
      // on I/O.
      m.WriteU32(Owner + 208, 0);
      s.r[3] = Owner;
      s.r[4] = Writer;
      (void)mesh_triangle_stream61::Apply(0x82ba6b18, m, deps, s);
      if (s.r[3] != 1)
        throw std::runtime_error("triangle owner write");
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
      if (s.r[3] != 1 || m.ReadU32(mesh) != 27 || m.ReadU32(mesh + 4) != 9 ||
          m.ReadU32(reader + 4) !=
              cook_main_smoke::Buffer + m.ReadU32(Writer + 4))
        throw std::runtime_error("triangle owner load/cursor");
      for (unsigned i = 0; i < 9; ++i) {
        auto source = m.ReadU32(m.ReadU32(mesh + 80) + 4 * i) - 200;
        if (source >= 9 ||
            m.ReadU16(m.ReadU32(mesh + 76) + 2 * i) != 100 + source ||
            m.ReadU32(m.ReadU32(mesh + 12) + 12 * i) != 3 * source)
          throw std::runtime_error("triangle owner coupled arrays");
      }
      for (unsigned i = 0; i < 9; ++i)
        if (m.ReadU16(m.ReadU32(Owner + 36) + 2 * i) != i % 2 ||
            m.ReadU8(m.ReadU32(Owner + 40) + i) != i % 3 ||
            !m.ReadU32(Owner + 44))
          throw std::runtime_error("triangle category/flags reload");
      storage(0x82b9e220, Owner);
      if (!env.guest.live.empty())
        throw std::runtime_error("triangle tree ownership");
      if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr))
        throw std::runtime_error("triangle tree ABI");
      std::puts(
          "PASS nine-triangle full owner write, coupled material/remap order, "
          "stream reload and complete cleanup");
    }
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
