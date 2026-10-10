#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_snapshot61.h"
#include <iostream>
struct SnapshotGuest final : manager_release_context61::GuestServices {
  unsigned allocations = 0;
  void Need(bool b) {
    if (!b)
      throw std::runtime_error("action snapshot ABI");
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82b7bc40) {
      for (unsigned i = 0; i < unsigned(s.r[5]); ++i)
        m.WriteU8(unsigned(s.r[3]) + i, unsigned(s.r[4]));
      return;
    }
    throw std::runtime_error("unexpected snapshot direct");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    Need(e == 0x123400 && s.r[3] == 0x70000 && s.r[6] == 8);
    ++allocations;
    s.r[3] = s.r[5] ? 0x200000 : 0;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x8330b000, 0x1000});
    regions.push_back({0x821a8000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    SnapshotGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    m.WriteU32(0x8330b608, 0x70000);
    m.WriteU32(0x70000, 0x71000);
    m.WriteU32(0x71008, 0x123400);
    m.WriteU32(0x82000e40, 0x3f800000);
    s.r[3] = 0x70000;
    s.r[4] = 7;
    s.r[5] = 8;
    s.r[6] = 9;
    g.Need(battle_action_snapshot61::Apply(0x82acd3b0, m, {g, native}, s));
    g.Need(m.ReadU32(0x70004) == 7 && m.ReadU32(0x70008) == 8 &&
           m.ReadU32(0x7000c) == 9);
    constexpr unsigned resource = 0x80000, src = 0x100000, dst = 0x200000;
    m.WriteU32(resource + 14656, src);
    m.WriteU32(resource + 14660, 1);
    m.WriteU32(resource + 14664, 1);
    m.WriteU32(resource + 64, 24);
    m.WriteU32(resource + 60, 2);
    m.WriteU32(resource + 88, 3);
    m.WriteU32(resource + 92, 4);
    m.WriteU32(resource + 96, 5);
    m.WriteU32(resource + 100, 0xc0000020);
    m.WriteU32(resource + 172, 0x12345678);
    m.WriteU32(src, 7);
    m.WriteU32(src + 4, 9);
    m.WriteU32(src + 20, 2);
    m.WriteU32(src + 248, 0x80000005);
    m.WriteU32(src + 15096, 0x80000007);
    m.WriteU32(src + 36, 24);
    m.WriteU32(src + 14884, 25);
    m.WriteU32(src + 72, 0x40200000);
    m.WriteU32(src + 31 * 464 + 15012, 0x40800000);
    auto prepare = [&](unsigned mode) {
      s.r[3] = 0x70000;
      s.r[4] = resource;
      s.r[5] = mode;
      g.Need(battle_action_snapshot61::Apply(0x82acee70, m, {g, native}, s));
      g.Need(s.r[1] == initial.r[1] && s.r[26] == initial.r[26] &&
             s.r[31] == initial.r[31]);
    };
    prepare(0);
    g.Need(m.ReadU32(resource + 156) == 2 && m.ReadU32(resource + 60) == 2 &&
           m.ReadU32(resource + 160) == 3 && m.ReadU32(resource + 164) == 4 &&
           m.ReadU32(resource + 168) == 5 &&
           m.ReadU32(resource + 172) == 0xd2345678);
    g.Need(m.ReadU32(resource + 14668) == dst &&
           m.ReadU32(resource + 14672) == 1 && m.ReadU32(dst) == 7 &&
           m.ReadU32(dst + 4) == 9 && m.ReadU32(dst + 20) == 2 &&
           m.ReadU32(dst + 36) == 24 && m.ReadU32(dst + 14884) == 25 &&
           m.ReadU32(dst + 248) == 0x80000000 &&
           m.ReadU32(dst + 72) == 0x40200000 &&
           m.ReadU32(dst + 31 * 464 + 15012) == 0x40800000);
    // Destination payload flags survive direct snapshot copy; only bit31
    // follows source.
    m.WriteU32(dst + 248, 11);
    s.r[3] = 0x70000;
    s.r[4] = src;
    s.r[5] = dst;
    g.Need(battle_action_snapshot61::Apply(0x82acd3e0, m, {g, native}, s));
    g.Need(m.ReadU32(dst + 248) == 0x8000000b && s.r[3] == 0);
    prepare(1);
    g.Need(m.ReadU32(resource + 156) == 0xfffffffe &&
           !m.ReadU32(resource + 60) && g.allocations == 3);
    g.Need(!battle_action_snapshot61::Apply(0, m, {g, native}, s));
    std::cout << "battle_action_snapshot61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
