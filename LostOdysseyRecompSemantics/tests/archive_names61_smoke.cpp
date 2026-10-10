#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/archive_names61.h"
#include <map>
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x82046000, 0x1000});
    regions.push_back({0x831e9000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    cook_main_smoke::Environment env(w);
    auto deps = env.Deps();
    env.guest.geometryDeps = &deps;
    auto s = sort_engine61_oracle::Initial(0);
    auto initial = s;
    constexpr unsigned header = 0x60000, archives = 0x61000, output = 0x62000,
                       pool = 0x63000, extensions = 0x64000, path = 0x65000,
                       exact = 0x66000, candidates = 0x67000;
    for (unsigned i = 0; i < 26; ++i)
      m.WriteU8(0x820468dc + 1 + i, 'A' + i);
    m.WriteU8(0x820468dc + 27, '\\');
    auto code = [](char c) -> unsigned {
      if (c == '\\')
        return 27;
      if (c >= 'a' && c <= 'z')
        c -= 32;
      return c ? unsigned(c - 'A' + 1) : 0;
    };
    auto encode = [&](unsigned index, std::string text) {
      auto at = [&](unsigned i) {
        return i < text.size() ? code(text[i]) : 0u;
      };
      auto rest = text.size() > 2 ? text.size() - 2 : 0u;
      unsigned words = (rest + 3) / 3;
      m.WriteU16(pool + 2 * index, words + 40 * at(0) + 1600 * at(1));
      for (unsigned j = 0; j < words; ++j)
        m.WriteU16(pool + 2 * index + 2 + 2 * j,
                   at(2 + 3 * j) + 40 * at(3 + 3 * j) + 1600 * at(4 + 3 * j));
    };
    auto string = [&](unsigned p, std::string text) {
      for (unsigned i = 0; i <= text.size(); ++i)
        m.WriteU8(p + i, i == text.size() ? 0 : text[i]);
    };
    auto read = [&](unsigned p) {
      std::string text;
      while (m.ReadU8(p))
        text += char(m.ReadU8(p++));
      return text;
    };
    for (unsigned i = 0; i < 3; ++i) {
      m.WriteU32(archives + 48 * i + 8, pool);
      m.WriteU32(archives + 48 * i + 12, extensions);
    }
    encode(1, "HELLO");
    encode(32, "TXT");
    m.WriteU16(extensions, 32);
    m.WriteU32(0x831e9170, 0x68000);
    string(0x68000, "-X");
    m.WriteU32(archives + 20, 1 | (1 << 18) | (1 << 23));
    s.r[3] = archives;
    s.r[4] = archives + 20;
    s.r[5] = output;
    (void)archive_names61::Apply(0x82853030, m, s);
    if (read(output) != "hello-X.txt" || s.r[3] != 11)
      throw std::runtime_error("packed archive name/suffix/extension");
    encode(64, "LOC\\ENG");
    encode(96, "LOC");
    m.WriteU32(archives + 20, 64);
    m.WriteU32(archives + 48 + 20, 96);
    m.WriteU32(archives + 96 + 20, 0);
    m.WriteU16(header + 26, 3);
    m.WriteU32(header + 32, archives);
    string(path, "LoC\\ENG\\file");
    for (unsigned i = 0; i < 3; ++i) {
      m.WriteU32(candidates + 16 * i + 4, 0x11110000 + i);
      m.WriteU32(candidates + 16 * i + 12, 0x22220000 + i);
    }
    s.r[3] = header;
    s.r[4] = candidates;
    s.r[6] = path;
    s.r[7] = 0;
    (void)archive_names61::Apply(0x82853da8, m, s);
    if (s.r[3] != 3)
      throw std::runtime_error("archive prefix candidates");
    unsigned expected[]{archives + 96, archives + 48, archives},
        lengths[]{0, 3, 7};
    for (unsigned i = 0; i < 3; ++i)
      if (m.ReadU32(candidates + 16 * i) != expected[i] ||
          m.ReadU32(candidates + 16 * i + 8) != lengths[i] ||
          m.ReadU32(candidates + 16 * i + 4) != 0x11110000 + i ||
          m.ReadU32(candidates + 16 * i + 12) != 0x22220000 + i)
        throw std::runtime_error("archive prefix order/untouched words");
    string(path, "loc\\eng");
    s.r[3] = header;
    s.r[4] = candidates;
    s.r[6] = path;
    s.r[7] = exact;
    (void)archive_names61::Apply(0x82853da8, m, s);
    if (s.r[3] != 0xffffffffffffffffull || m.ReadU32(exact) != archives)
      throw std::runtime_error("archive exact prefix exit");
    string(path, "part\\next");
    s.r[3] = output;
    s.r[4] = path;
    (void)archive_names61::Apply(0x82852d68, m, s);
    if (s.r[3] != 4 || read(output) != "part")
      throw std::runtime_error("archive path segment");
    std::puts("PASS packed base40 names, suffix/extension, prefix candidates "
              "and exact lookup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
