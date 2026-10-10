#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_progression61.h"
#include <iostream>
struct ProgressionGuest final : manager_release_context61::GuestServices {
  unsigned unlocks = 0, achievement = 0, platformCalls = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x8229dfd8) {
      s.r[3] = 0x100000;
      return;
    }
    if (e == 0x828208f8) {
      if (s.r[3] != achievement)
        throw std::runtime_error("achievement ID forwarding");
      ++unlocks;
      return;
    }
    throw std::runtime_error("progression direct");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e == 0x123400) {
      if (s.r[3] != 0x70000)
        throw std::runtime_error("platform receiver");
      ++platformCalls;
      s.r[3] = 0x100000;
      return;
    }
    if (e == 0x123404) {
      if (s.r[3] != 0x100000)
        throw std::runtime_error("achievement receiver");
      achievement = unsigned(s.r[4]);
      return;
    }
    throw std::runtime_error("progression indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83315000, 0x1000});
    regions.push_back({0x83291000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ProgressionGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("progression state");
    };
    m.WriteU32(0x83315fb4, 0x70000);
    m.WriteU32(0x70000, 0x71000);
    m.WriteU32(0x71000 + 352, 0x123400);
    m.WriteU32(0x100000, 0x110000);
    m.WriteU32(0x110000 + 404, 0x123404);
    m.WriteU32(0x83291dc0, 0x73000);
    auto run = [&](unsigned e, unsigned argument, unsigned value = 0) {
      s.r[3] = 0x73000;
      s.r[4] = argument;
      s.r[5] = value;
      check(battle_progression61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[29] == initial.r[29] &&
            s.r[31] == initial.r[31]);
    };
    run(0x82ac34f8, 0, 2);
    run(0x82ac34f8, 0, 2);
    check(g.unlocks == 0);
    run(0x82ac34f8, 0, 2);
    check(g.unlocks == 1 && g.achievement == 27);
    run(0x82ac34f8, 0, 2);
    check(g.unlocks == 1);
    run(0x82ac34f8, 0, 1);
    check(!m.ReadU32(0x100000 + 170824));
    m.WriteU32(0x100000 + 170828, 499);
    run(0x82ac34f8, 1, 2);
    check(g.achievement == 28 && m.ReadU32(0x100000 + 170828) == 500);
    m.WriteU32(0x100000 + 170832, 999999);
    run(0x82ac34f8, 2, 4);
    check(g.achievement == 29 && m.ReadU32(0x100000 + 170832) == 1000003);
    run(0x82ac34f8, 5, 7);
    check(g.achievement == 14);
    run(0x82ac34f8, 5, 9);
    check(g.achievement == 13);
    run(0x82ac34f8, 6);
    check(g.achievement == 15);
    run(0x82ac34f8, 7);
    check(g.achievement == 5);
    auto calls = g.platformCalls;
    m.WriteU32(0x80000 + 124, 0x10000000);
    run(0x82ac6348, 0x80000);
    check(g.platformCalls == calls);
    m.WriteU32(0x80000 + 124, 0x02000000);
    m.WriteU32(0x80000 + 64, 24);
    m.WriteU32(0x80000 + 68, 7);
    m.WriteU32(0x100000 + 170836, 999);
    run(0x82ac6348, 0x80000);
    check(g.achievement == 30 && m.ReadU32(0x73000 + 100) == 7 &&
          m.ReadU32(0x73000 + 104) == 24 && m.ReadU8(0x73000 + 108) == 1);
    run(0x82ac6348, 0x80000);
    check(m.ReadU32(0x73000 + 112) == 0 &&
          m.ReadU32(0x100000 + 170836) == 1000);
    m.WriteU32(0x80000 + 64, 25);
    m.WriteU32(0x80000 + 124, 0x04000000);
    run(0x82ac6348, 0x80000);
    check(!m.ReadU32(0x73000 + 112));
    std::cout << "battle progression logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
