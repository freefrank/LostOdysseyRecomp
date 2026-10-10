#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/archive_index_fields61.h"
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
    constexpr unsigned archive = 0x60000, root = 0x61000, table = 0x62000;
    for (unsigned i = 0; i < 48; ++i)
      m.WriteU8(archive + i, 0xa5);
    m.WriteU16(archive + 2, 0x3412);
    for (unsigned off : {4, 8, 12, 16, 20, 24, 28, 32, 36})
      m.WriteU32(archive + off, __builtin_bswap32(0x12340000 + off));
    s.r[3] = archive;
    (void)archive_index_fields61::Apply(0x82853208, m, s);
    if (m.ReadU16(archive + 2) != 0x1234 || s.r[3] != 0x12340004)
      throw std::runtime_error("archive descriptor endian fields");
    for (unsigned off : {4, 8, 12, 16, 20, 24, 28, 32, 36})
      if (m.ReadU32(archive + off) != 0x12340000 + off)
        throw std::runtime_error("archive swapped word");
    if (m.ReadU16(archive) != 0xa5a5 || m.ReadU32(archive + 40) != 0xa5a5a5a5 ||
        m.ReadU32(archive + 44) != 0xa5a5a5a5)
      throw std::runtime_error("archive untouched fields");
    for (unsigned native = 0; native < 2; ++native) {
      m.WriteU32(archive + 4, table);
      auto word = [&](unsigned p, unsigned v) {
        m.WriteU32(p, native ? v : __builtin_bswap32(v));
      };
      auto half = [&](unsigned p, unsigned short v) {
        m.WriteU16(p, native ? v : __builtin_bswap16(v));
      };
      auto entry = [&](unsigned p, bool directory, unsigned children,
                       unsigned offset) {
        for (unsigned i = 0; i < 24; ++i)
          m.WriteU8(p + i, 0xa5);
        word(p, directory ? 0x10000001 : 0x20000002);
        word(p + 4, 0x12345678);
        word(p + 8, 0x01020304);
        half(p + 12, 0x1234);
        if (directory) {
          half(p + 14, children);
          half(p + 16, 0x5678);
        } else
          word(p + 16, 0x11223344);
        word(p + 20, offset);
      };
      entry(root, true, 2, 2);
      entry(table + 48, false, 0, 0x55667788);
      entry(table + 72, true, 1, 4);
      entry(table + 96, false, 0, 0x99aabbcc);
      s.r[3] = root;
      s.r[4] = 1;
      s.r[5] = archive;
      s.r[6] = native;
      (void)archive_index_fields61::Apply(0x82853a10, m, s);
      if (m.ReadU32(root + 20) != table + 48 ||
          m.ReadU32(table + 72 + 20) != table + 96 ||
          m.ReadU32(table + 48 + 16) != 0x11223344 ||
          m.ReadU32(table + 96 + 20) != 0x99aabbcc ||
          m.ReadU16(root + 16) != 0x5678 || m.ReadU16(root + 18) != 0xa5a5)
        throw std::runtime_error("recursive archive entry relocation");
    }
    m.WriteU8(root + 3, 7);
    s.r[3] = root;
    (void)archive_index_fields61::Apply(0x828571b8, m, s);
    if (s.r[3] != 7 * 2048)
      throw std::runtime_error("CPX reserve units");
    std::puts("PASS archive descriptor fields, recursive entry swap/relocation "
              "and CPX reserve");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
