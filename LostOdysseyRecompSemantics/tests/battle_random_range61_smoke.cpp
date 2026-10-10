#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_random_range61.h"
#include <iostream>
struct RandomGuest final : manager_release_context61::GuestServices {
  unsigned lookups = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e308) {
      if (s.r[4] != 7)
        throw std::runtime_error("random resource lookup");
      ++lookups;
      s.r[3] = 0x80000;
      return;
    }
    throw std::runtime_error("random boundary");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("random indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    RandomGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("random state");
    };
    m.WriteU32(0x80000 + 68, 2);
    m.WriteU32(0x831f3300, 17);
    m.WriteU32(0x831f3304, 20);
    m.WriteU32(0x831f3300 + 4 * 32767, 5);
    auto slot = [](unsigned group) {
      return 0x60000 + 4 * (128 * group + 109 + 3);
    };
    auto run = [&](unsigned id, unsigned lo, unsigned hi) {
      s.r[3] = 0x60000;
      s.r[4] = lo;
      s.r[5] = hi;
      s.r[6] = 109;
      s.r[7] = id;
      check(battle_random_range61::Apply(0x82aa0740, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
            s.r[31] == initial.r[31]);
      return unsigned(s.r[3]);
    };
    check(run(7, 1, 9) == 9 && g.lookups == 1 && m.ReadU32(slot(2)) == 1 &&
          m.ReadU32(0x60004) == 9);
    check(run(7, 1, 9) == 3 && m.ReadU32(slot(2)) == 2);
    check(run(255, 10, 19) == 17 && m.ReadU32(slot(31)) == 1);
    m.WriteU32(slot(31), 32767);
    check(run(99, 1, 9) == 6 && !m.ReadU32(slot(31)));
    check(run(20, 1, 9) == 9 && m.ReadU32(slot(11)) == 1);
    check(run(20, 4, 4) == 4 && m.ReadU32(slot(11)) == 1);
    check(run(7, 0xffffffff, 2) == 0 && g.lookups == 3 &&
          m.ReadU32(slot(2)) == 2);
    check(!battle_random_range61::Apply(0, m, {g, native}, s));
    std::cout << "battle_random_range61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
