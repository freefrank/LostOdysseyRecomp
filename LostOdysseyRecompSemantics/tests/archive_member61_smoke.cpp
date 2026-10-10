#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/archive_names61.h"
#include "lo_semantics/archive_member61.h"
#include <map>
struct MemberGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e != 0x830da31c)
      throw std::runtime_error("unexpected member boundary");
    recovery_abi::WriteU64(m, Address(s.r[4]), 0x1122334455667788ull);
    s.r[3] = 1;
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected indirect boundary");
  }
};
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
    MemberGuest guest;
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
    unsigned months[]{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (unsigned i = 0; i < 12; ++i)
      m.WriteU8(0x82046150 + i, months[i]);
    constexpr unsigned entries = 0x69000, children = 0x6a000,
                       original = 0x6b000;
    m.WriteU32(archives + 8, pool);
    m.WriteU32(archives + 12, extensions);
    m.WriteU16(header + 26, 1);
    m.WriteU32(header + 32, archives);
    m.WriteU16(archives + 2, 1);
    m.WriteU32(archives + 4, entries);
    m.WriteU8(archives, 7);
    m.WriteU8(archives + 1, 9);
    encode(1, "LOC");
    encode(32, "PACKAGE");
    encode(64, "DIR");
    encode(96, "FILE");
    m.WriteU32(archives + 20, 1);
    m.WriteU32(archives + 24, 32);
    m.WriteU32(entries, 64 | 0x10000000);
    m.WriteU16(entries + 14, 1);
    m.WriteU32(entries + 20, children);
    m.WriteU32(children, 96 | 0x80000000);
    m.WriteU32(children + 8, 3);
    m.WriteU8(children + 14, 0x12);
    m.WriteU8(children + 15, 8);
    m.WriteU32(children + 16, 123);
    m.WriteU32(children + 20, 456);
    auto find = [&](std::string name) {
      for (unsigned i = 0; i < 240; ++i)
        m.WriteU8(output + i, 0);
      string(output + 48, "root/");
      string(path, name);
      s.r[3] = header;
      s.r[4] = original;
      s.r[5] = path;
      s.r[6] = 5;
      s.r[7] = output;
      s.r[8] = exact;
      (void)archive_member61::Apply(0x828548a0, m, {guest, native}, s);
      if (s.r[1] != initial.r[1])
        throw std::runtime_error("member stack restoration");
    };
    find("loc\\dir\\file");
    if (s.r[3] != 1 || read(output + 48) != "root/package" ||
        m.ReadU8(output) != 7 || m.ReadU8(output + 1) != 9 ||
        m.ReadU8(output + 2) != 8 || m.ReadU8(output + 3) != 1 ||
        m.ReadU8(output + 4) != 3 || m.ReadU8(output + 5) != 2 ||
        m.ReadU32(output + 8) != 123 || m.ReadU32(output + 12) != 456 ||
        recovery_abi::ReadU64(m, output + 16) != 6144 ||
        recovery_abi::ReadU64(m, output + 24) != 0x1122334455667788ull ||
        m.ReadU32(output + 36) != archives ||
        m.ReadU32(output + 40) != children || m.ReadU32(output + 44) != 0)
      throw std::runtime_error("recursive file metadata");
    find("loc\\dir");
    if (s.r[3] != 1 || m.ReadU8(output + 3) != 16 ||
        m.ReadU32(output + 40) != entries)
      throw std::runtime_error("directory metadata");
    find("loc");
    if (s.r[3] != 1 || m.ReadU8(output + 3) != 17 ||
        read(output + 48) != "root/package")
      throw std::runtime_error("exact archive metadata");
    m.WriteU32(children + 8, 0xc0000000);
    string(original, "TAIL");
    find("loc\\dir\\file");
    if (s.r[3] != 1 || m.ReadU8(output + 3) != 0 || m.ReadU8(output + 4) != 7 ||
        read(output + 48) != "root/loc\\dir\\fileTAIL")
      throw std::runtime_error("loose path type/flags/append");
    find("loc\\missing");
    if (s.r[3] != 0 || read(output + 48) != "root/")
      throw std::runtime_error("missing member");
    std::puts(
        "PASS recursive file/directory, exact archive, loose path and miss");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
