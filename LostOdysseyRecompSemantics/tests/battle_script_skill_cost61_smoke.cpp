#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_skill_cost61.h"
#include <iostream>
struct CostGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("cost direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("cost indirect");
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
    CostGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("skill cost state");
    };
    m.WriteU32(0x83291dc0, 0x70000);
    m.WriteU32(0x83264984, 0x90000);
    m.WriteU32(0x832649c0, 0xa0000);
    constexpr unsigned skill = 0x90000 + 96 * 5 + 16,
                       item = 0xa0000 + 104 * 5 + 24;
    m.WriteU32(skill, 10);
    m.WriteU32(item, 10);
    m.WriteU32(0x80000 + 2616, 0x41200000);
    auto run = [&](unsigned cat) {
      s.r[3] = 0x80000;
      s.r[4] = cat;
      s.r[5] = 5;
      check(battle_script_skill_cost61::Apply(0x82ac9aa8, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[28] == initial.r[28] &&
            s.r[31] == initial.r[31]);
      return unsigned(s.r[3]);
    };
    check(run(2) == 1);
    m.WriteU32(skill, 11);
    check(!run(2));
    m.WriteU32(skill, 0xffffffff);
    check(run(2) == 1);
    m.WriteU32(0x83213468, 1);
    m.WriteU32(0x8321346c, 4);
    m.WriteU32(0x80000 + 272 + 232, 4);
    s.fpr_bits[0] = 123;
    check(!run(2) && s.fpr_bits[0] == 123);
    m.WriteU32(0x80000 + 272 + 232, 0);
    m.WriteU32(0x80000 + 2616, 0x7fc00000);
    check(!run(2) && run(3) == 1);
    m.WriteU32(0x80000 + 2616, 0x41200000);
    m.WriteU32(item, 11);
    check(!run(3));
    m.WriteU32(item, 10);
    check(run(3) == 1 && run(99) == 1);
    m.WriteU32(0x83213438 + 12 * 8, 0);
    m.WriteU32(0x8321343c + 12 * 8, 1u << 12);
    m.WriteU32(0x83213438 + 8, 0);
    m.WriteU32(0x8321343c + 8, 2);
    auto adjusted = [&](unsigned cost) {
      s.r[3] = 0x70000;
      s.r[4] = 0x80000;
      s.r[5] = cost;
      check(battle_script_skill_cost61::Apply(0x82ac1af0, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[28] == initial.r[28]);
      return unsigned(s.r[3]);
    };
    check(adjusted(20) == 20);
    m.WriteU32(0x80000 + 7 * 272 + 232, 1u << 12);
    check(adjusted(20) == 15 && adjusted(unsigned(-7)) == unsigned(-6));
    m.WriteU32(0x80000 + 3 * 272 + 232, 2);
    check(!adjusted(20));
    for (int id : {-1, 0, 49, 50, 99, 100, 149, 150}) {
      s.r[4] = unsigned(id);
      check(battle_script_skill_cost61::Apply(0x82b08b80, m, {g, native}, s));
      check(s.r[3] == unsigned(id >= 150   ? 9
                               : id >= 100 ? 8
                               : id >= 50  ? 7
                                           : 6));
    }
    check(!battle_script_skill_cost61::Apply(0, m, {g, native}, s));
    std::cout << "battle_script_skill_cost61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
