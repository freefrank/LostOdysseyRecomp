#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/mesh_partition_support61.h"
#include "lo_semantics/object_sort_support61.h"
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

    const unsigned object = Owner, faces = 0x60000, indices = 0x61000,
                   points = 0x62000;
    auto call = [&](unsigned entry, unsigned self, unsigned arg = 0) {
      s.r[3] = self;
      s.r[4] = arg;
      (void)mesh_partition_support61::Apply(entry, m, deps, s);
    };
    call(0x82bc10f0, object);
    auto allocate = [&]() {
      s.r[4] = 64;
      env.guest.CallIndirect(Allocate, m, s);
      return Address(s.r[3]);
    };
    auto groups = allocate();
    m.WriteU32(object + 48, groups);
    m.WriteU32(object + 40, allocate());
    m.WriteU32(object + 44, 4);
    for (unsigned i = 0; i < 3; ++i) {
      m.WriteU32(faces + 4 * i, i);
      m.WriteU32(groups + 4 * i, i);
    }
    float xyz[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    unsigned triangles[]{0, 1, 2, 0, 1, 2, 0, 1, 3};
    for (unsigned i = 0; i < 12; ++i)
      m.WriteU32(points + 4 * i, std::bit_cast<unsigned>(xyz[i]));
    for (unsigned i = 0; i < 9; ++i)
      m.WriteU32(indices + 4 * i, triangles[i]);
    std::array<unsigned char, 184> constants{};
    std::ifstream f(std::getenv("LO_MESH_MATH_CONSTANTS"), std::ios::binary);
    f.read(reinterpret_cast<char *>(constants.data()), constants.size());
    if (f.gcount() != 184)
      throw std::runtime_error("private atan2 constants");
    for (unsigned i = 0; i < 184; ++i)
      m.WriteU8(0x83214e88 + i, constants[i]);
    m.WriteU32(0x82000d7c, std::bit_cast<unsigned>(.01f));
    s.r[3] = object;
    s.r[4] = 3;
    s.r[5] = faces;
    s.r[6] = indices;
    s.r[7] = points;
    (void)mesh_partition_support61::Apply(0x82bc1c68, m, deps, s);
    if (m.ReadU32(groups) != 0 || m.ReadU32(groups + 4) != 0 ||
        m.ReadU32(groups + 8) != 2)
      throw std::runtime_error("coplanar category merge");
    s.r[5] = faces;
    call(0x82bc1190, object, 3);
    if (m.ReadU32(groups + 8) != 1)
      throw std::runtime_error("dense category IDs");
    unsigned queue = 0x63000, out = 0x64000;
    m.WriteU32(queue + 4, 3);
    m.WriteU32(queue + 8, faces);
    for (unsigned i = 0; i < 3; ++i) {
      call(0x82bc4258, queue, out);
      if (s.r[3] != 1 || m.ReadU32(out) != i)
        throw std::runtime_error("partition FIFO");
    }
    call(0x82bc4258, queue, out);
    if (s.r[3] != 0 || m.ReadU32(queue + 16) != 0)
      throw std::runtime_error("empty FIFO/reset");
    const unsigned topology = 0x65000, edgeIds = 0x66000, records = 0x67000,
                   incidences = 0x68000, barriers = 0x69000;
    m.WriteU32(object, indices);
    m.WriteU32(object + 4, points);
    m.WriteU32(object + 8, topology);
    m.WriteU32(object + 20, barriers);
    m.WriteU32(object + 24, groups);
    m.WriteU32(object + 28, 7);
    m.WriteU32(topology + 12, edgeIds);
    m.WriteU32(topology + 16, records);
    m.WriteU32(topology + 20, incidences);
    // Two triangles sharing edge (0,1); only the first edge is traversable.
    for (unsigned i = 0; i < 2; ++i) {
      m.WriteU32(groups + 4 * i, 0xffffffff);
      for (unsigned j = 0; j < 3; ++j)
        m.WriteU32(edgeIds + 12 * i + 4 * j, j);
    }
    m.WriteU16(records + 2, 2);
    m.WriteU32(records + 4, 0);
    m.WriteU32(incidences, 0);
    m.WriteU32(incidences + 4, 1);
    m.WriteU8(barriers + 1, 1);
    m.WriteU8(barriers + 2, 1);
    m.WriteU32(0x821baa74, std::bit_cast<unsigned>(2.f));
    s.r[5] = 2;
    call(0x82bc1c10, object, 0);
    if (s.r[3] != 1 || m.ReadU32(object + 32) != 2 || m.ReadU32(groups) != 7 ||
        m.ReadU32(groups + 4) != 7)
      throw std::runtime_error("connected component flood");
    const unsigned accepted = 0x6a000, vertices = 0x6b000;
    for (unsigned q : {accepted, vertices}) {
      s.r[3] = q;
      (void)object_sort_support61::Apply(0x82bd2a08, m, {env.guest, native}, s);
    }
    m.WriteU32(object + 12, accepted);
    m.WriteU32(object + 16, vertices);
    m.WriteU32(0x820d6670, std::bit_cast<unsigned>(.001f));
    s.r[5] = 0xffffffff;
    call(0x82bc1260, object, 0);
    if (s.r[3] != 1 || m.ReadU32(accepted + 4) != 1 ||
        m.ReadU32(vertices + 4) != 3)
      throw std::runtime_error("partition seed acceptance");
    s.r[5] = 2;
    call(0x82bc1260, object, 1);
    if (s.r[3] != 1 || m.ReadU32(accepted + 4) != 2 ||
        m.ReadU32(vertices + 4) != 4)
      throw std::runtime_error("coplanar face acceptance");
    for (unsigned q : {accepted, vertices}) {
      s.r[3] = q;
      (void)object_sort_support61::Apply(0x82bd2c08, m, {env.guest, native}, s);
    }
    call(0x82bc1108, object);
    if (!env.guest.live.empty())
      throw std::runtime_error("edge flags ownership");
    if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr))
      throw std::runtime_error("edge flags ABI");
    std::puts("PASS coplanar merge, dense IDs, FIFO, component flood, face "
              "acceptance and "
              "complete cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
