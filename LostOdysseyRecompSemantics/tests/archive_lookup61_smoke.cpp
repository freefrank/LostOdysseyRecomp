#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/archive_names61.h"
#include "lo_semantics/archive_member61.h"
#include "lo_semantics/archive_lookup61.h"
#include <map>
struct LookupGuest final : manager_release_context61::GuestServices {
  unsigned locks = 0, unlocks = 0, paths = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x830d9c6c) {
      ++locks;
      return;
    }
    if (e == 0x830d9c7c) {
      ++unlocks;
      return;
    }
    if (e == 0x82dd1b58) {
      ++paths;
      std::string root = "overlay/";
      for (unsigned i = 0; i <= root.size(); ++i)
        m.WriteU8(Address(s.r[4]) + i, i == root.size() ? 0 : root[i]);
      return;
    }
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
    regions.push_back({0x83264000, 0x2000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    cook_main_smoke::Environment env(w);
    auto deps = env.Deps();
    env.guest.geometryDeps = &deps;
    auto s = sort_engine61_oracle::Initial(0);
    auto initial = s;
    LookupGuest guest;
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
    constexpr unsigned globals = 0x83264d90, table = 0x6c000, node = 0x6d000,
                       owner = 0x6e000, emptyHeader = 0x6f000;
    m.WriteU32(children + 8, 3);
    string(original, "base/");
    m.WriteU32(0x831e9168, original);
    m.WriteU32(globals + 216, header);
    auto lookup = [&](std::string name) {
      string(path, name);
      s.r[3] = path;
      s.r[4] = output;
      s.r[5] = exact;
      (void)archive_lookup61::Apply(0x828551e8, m, {guest, native}, s);
      if (s.r[1] != initial.r[1])
        throw std::runtime_error("lookup stack");
    };
    lookup("/LOC/DIR/FILE");
    if (s.r[3] != 1 || read(output + 48) != "base/package" || guest.locks)
      throw std::runtime_error("normalized base lookup");
    m.WriteU32(globals + 220, table);
    m.WriteU32(table, node);
    m.WriteU32(node + 8, 4);
    m.WriteU32(node + 12, owner);
    m.WriteU32(globals + 212, 1);
    m.WriteU8(globals + 224, 0);
    m.WriteU32(owner + 352, header);
    lookup("LOC/DIR/FILE");
    if (s.r[3] != 1 || read(output + 48) != "overlay/package" ||
        m.ReadU32(output + 32) != owner || guest.locks != 1 ||
        guest.unlocks != 1)
      throw std::runtime_error("overlay first lookup");
    m.WriteU32(owner + 352, emptyHeader);
    lookup("LOC/DIR/FILE");
    if (s.r[3] != 1 || read(output + 48) != "base/package" ||
        m.ReadU32(output + 32) || guest.locks != 2 || guest.unlocks != 2)
      throw std::runtime_error("overlay miss fallback");
    string(path, "/ABC-DEF/File");
    m.WriteU32(exact, path);
    s.r[3] = output;
    s.r[4] = exact;
    (void)archive_lookup61::Apply(0x82852e08, m, {guest, native}, s);
    if (read(output) != "abc_def\\file" || m.ReadU32(exact) != path + 1 ||
        s.r[3] != 12)
      throw std::runtime_error("normalization pointer/length");
    m.WriteU32(node, node + 64);
    m.WriteU32(node + 64 + 8, 8);
    s.r[3] = node;
    s.r[4] = 2;
    s.r[5] = 4;
    (void)archive_lookup61::Apply(0x822a2600, m, {guest, native}, s);
    if (s.r[3] != node + 80)
      throw std::runtime_error("segmented index");
    std::puts("PASS normalization, segmented table, overlay hit/base fallback");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
