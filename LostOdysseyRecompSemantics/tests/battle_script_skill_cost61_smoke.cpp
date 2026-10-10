#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_skill_cost61.h"
#include <iostream>
struct CostGuest final : manager_release_context61::GuestServices {
  unsigned cost = 10, calls = 0, key = 19;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e != 0x82ac1af0 || s.r[3] != 0x70000 || s.r[4] != 0x80000 ||
        s.r[5] != key)
      throw std::runtime_error("skill cost ABI");
    ++calls;
    s.r[3] = cost;
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected cost indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x83291000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    CostGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    m.WriteU32(0x83291dc0, 0x70000);
    m.WriteU32(0x83264984, 0x90000);
    m.WriteU32(0x832649c0, 0xa0000);
    m.WriteU32(0x90000 + 96 * 5 + 16, 19);
    m.WriteU32(0xa0000 + 104 * 5 + 24, 20);
    m.WriteU32(0x80000 + 2616, 0x41200000);
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("skill cost result");
    };
    auto run = [&](unsigned category) {
      s.r[3] = 0x80000;
      s.r[4] = category;
      s.r[5] = 5;
      check(
          battle_script_skill_cost61::Apply(0x82ac9aa8, m, {guest, native}, s));
      check(s.r[1] == initial.r[1] && s.r[28] == initial.r[28] &&
            s.r[31] == initial.r[31]);
    };
    run(2);
    check(s.r[3] == 1);
    guest.cost = 11;
    run(2);
    check(!s.r[3]);
    guest.cost = unsigned(-1);
    run(2);
    check(s.r[3] == 1);
    m.WriteU32(0x83213468, 1);
    m.WriteU32(0x8321346c, 4);
    m.WriteU32(0x80000 + 272 + 232, 4);
    auto calls = guest.calls;
    run(2);
    check(!s.r[3] && guest.calls == calls);
    m.WriteU32(0x80000 + 272 + 232, 0);
    m.WriteU32(0x80000 + 2616, 0x7fc00000);
    run(2);
    check(!s.r[3]);
    guest.key = 20;
    run(3);
    check(s.r[3] == 1);
    m.WriteU32(0x80000 + 2616, 0x41200000);
    guest.cost = 11;
    run(3);
    check(!s.r[3]);
    guest.cost = 10;
    run(3);
    check(s.r[3] == 1);
    calls = guest.calls;
    run(99);
    check(s.r[3] == 1 && guest.calls == calls);
    for (int id : {-1, 0, 49, 50, 99, 100, 149, 150}) {
      s.r[4] = unsigned(id);
      check(
          battle_script_skill_cost61::Apply(0x82b08b80, m, {guest, native}, s));
      check(s.r[3] == unsigned(id >= 150   ? 9
                               : id >= 100 ? 8
                               : id >= 50  ? 7
                                           : 6));
    }
    std::cout << "battle_script_skill_cost61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
