#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_manager_access61.h"
#include <iostream>
struct AccessGuest final : manager_release_context61::GuestServices {
  unsigned created = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e != 0x82ab01d0)
      throw std::runtime_error("access direct");
    ++created;
    s.r[3] = 0x70000;
    m.WriteU32(0x832cb788, 0x70000);
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("access indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x4000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    AccessGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("access state");
    };
    auto run = [&](unsigned e, unsigned owner = 0, unsigned id = 0) {
      s.r[3] = owner;
      s.r[4] = id;
      check(battle_manager_access61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1]);
      return unsigned(s.r[3]);
    };
    check(run(0x82380a18) == 0x70000 && run(0x82380a18) == 0x70000 &&
          g.created == 1);
    check(run(0x82389b78) == 0x832ca0e8);
    m.WriteU32(0x832ca0e8 + 20, 0x71000);
    m.WriteU32(0x832ca0e8 + 48, 0x72000);
    check(run(0x8238e2f8) == 0x71000 && run(0x82ab0110) == 0x72000);
    check(run(0x83081540, 0x832ca0e8) == 0x71000 &&
          run(0x82df7558, 0x832ca0e8) == 0x72000);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 2);
    m.WriteU32(0x71100, 0x80000);
    m.WriteU32(0x71104, 0x90000);
    m.WriteU32(0x80000 + 64, 24);
    m.WriteU32(0x90000 + 64, 25);
    check(run(0x8238e308, 0x832ca0e8, 25) == 0x90000 &&
          run(0x8238e308, 0x832ca0e8, 99) == 0);
    m.WriteU32(0x71004, 0xffffffff);
    check(run(0x8238e308, 0x832ca0e8, 24) == 0);
    m.WriteU32(0x832cb550 + 4, 0x71100);
    m.WriteU32(0x832cb550 + 8, 2);
    m.WriteU32(0x80000 + 552, 3);
    m.WriteU32(0x90000 + 552, 4);
    check(run(0x82380d30, 0, 4) == 0x90000 && run(0x82380d30, 0, 9) == 0);
    check(!battle_manager_access61::Apply(0, m, {g, native}, s));
    std::cout << "battle manager access logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
