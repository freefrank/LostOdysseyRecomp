#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_parameters61.h"
#include <iostream>
struct ParameterGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected parameter direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("parameter indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto region :
         {test::Region{0x83213000, 0x1000}, test::Region{0x83245000, 0x1000},
          test::Region{0x83264000, 0x1000}, test::Region{0x831f3000, 0x21000}})
      regions.push_back(region);
    regions.push_back({0x832c9000, 0x4000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ParameterGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("parameter state");
    };
    m.WriteU32(0x8324570c, 0x70000);
    m.WriteU32(0x70010, 55);
    m.WriteU32(0x83264558, 0x60000);
    m.WriteU32(0x80000 + 64, 24);
    m.WriteU32(0x831f3300, 10);
    m.WriteU32(0x831f3304, 20);
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    auto run = [&](unsigned e, unsigned kind, unsigned variant) {
      s.r[3] = 0x50000;
      s.r[4] = 0x80000;
      s.r[5] = kind;
      s.r[6] = 9;
      s.r[7] = variant;
      check(battle_action_parameters61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
            s.r[31] == initial.r[31]);
    };
    m.WriteU32(0x80000 + 2612, std::bit_cast<unsigned>(10.75f));
    run(0x82b21340, 1, 0);
    check(m.ReadU32(0x70008) == 10 && m.ReadU32(0x7000c) == 160);
    run(0x82b21340, 1, 1);
    check(m.ReadU32(0x70008) == 55 && m.ReadU32(0x7000c) == 0xffffffff);
    m.WriteU32(0x80000 + 232, 4);
    m.WriteU32(0x80000 + 2612, std::bit_cast<unsigned>(20.f));
    run(0x82b21340, 1, 0);
    check(m.ReadU32(0x70008) == 24);
    m.WriteU32(0x80000 + 232, 0);
    m.WriteU32(0x80000 + 5 * 272 + 232, 2);
    m.WriteU32(0x80000 + 2612, std::bit_cast<unsigned>(-3.f));
    run(0x82b21340, 1, 0);
    check(m.ReadU32(0x70008) == 1);
    m.WriteU32(0x80000 + 5 * 272 + 232, 0);
    m.WriteU32(0x80000 + 6 * 272 + 232, 2);
    m.WriteU32(0x80000 + 2612, std::bit_cast<unsigned>(4.f));
    run(0x82b21340, 1, 0);
    check(m.ReadU32(0x70008) == 8);
    run(0x82b1f798, 4, 0);
    check(m.ReadU32(0x70008) == 1 && !m.ReadU32(0x7000c));
    run(0x82b1f798, 5, 0);
    check(m.ReadU32(0x70008) == 2 && !m.ReadU32(0x7000c));
    run(0x82b1f798, 5, 1);
    check(m.ReadU32(0x70008) == 55 && m.ReadU32(0x7000c) == 0xffffffff);
    m.WriteU32(0x83264984, 0x90000);
    m.WriteU32(0x832649c0, 0xa0000);
    m.WriteU32(0x832ca0d0, 0xb0000);
    m.WriteU32(0xb0000 + 132, 0xc0000);
    m.WriteU32(0x90000 + 9 * 96 + 20, 3);
    m.WriteU32(0x90000 + 9 * 96 + 24, 20);
    m.WriteU32(0x80000 + 2640, std::bit_cast<unsigned>(6.5f));
    m.WriteU32(0x82000a98, std::bit_cast<unsigned>(0.5f));
    run(0x82b11df0, 2, 0);
    check(m.ReadU32(0x70004) == 1 && m.ReadU32(0x70008) == 2 &&
          m.ReadU32(0x7000c) == 160);
    m.WriteU32(0xa0000 + 9 * 104 + 28, 4);
    m.WriteU32(0xa0000 + 9 * 104 + 32, 20);
    run(0x82b11df0, 3, 0);
    check(m.ReadU32(0x70004) == 1 && m.ReadU32(0x70008) == 4 &&
          m.ReadU32(0x7000c) == 170);
    run(0x82b11df0, 3, 1);
    check(m.ReadU32(0x70004) == 2 && m.ReadU32(0x70008) == 5 &&
          m.ReadU32(0x7000c) == 0xffffffff);
    m.WriteU32(0x90000 + 9 * 96 + 24, 99);
    run(0x82b11df0, 2, 0);
    check(m.ReadU32(0x70004) == 0 && m.ReadU32(0x70008) == 0 &&
          !m.ReadU32(0x7000c));
    m.WriteU32(0xc0000 + 9 * 68 + 8, 5);
    m.WriteU32(0xc0000 + 9 * 68 + 12, 99);
    run(0x82b11df0, 10, 0);
    check(m.ReadU32(0x70004) == 5 && m.ReadU32(0x70008) == 1);
    m.WriteU32(0xc0000 + 9 * 68 + 12, 18);
    run(0x82b11df0, 10, 0);
    check(m.ReadU32(0x70004) == 1 && m.ReadU32(0x70008) == 2);
    run(0x82b11df0, 11, 0);
    check(m.ReadU32(0x70004) == 0 && m.ReadU32(0x70008) == 1 &&
          m.ReadU32(0x7000c) == 75);
    m.WriteU32(0x80000 + 68, 10);
    run(0x82b11df0, 2, 0);
    check(m.ReadU32(0x70004) == 3 && m.ReadU32(0x70008) == 8);
    m.WriteU32(0x80000 + 68, 0);
    m.WriteU32(0x90000 + 9 * 96 + 24, 20);
    m.WriteU32(0x80000 + 5 * 272 + 232, 1);
    run(0x82b11df0, 2, 0);
    check(m.ReadU32(0x70004) == 0 && m.ReadU32(0x70008) == 18);
    m.WriteU32(0x80000 + 5 * 272 + 232, 0);
    m.WriteU32(0x80000 + 6 * 272 + 232, 4);
    run(0x82b11df0, 3, 0);
    check(m.ReadU32(0x70004) == 1 && m.ReadU32(0x70008) == 18);
    m.WriteU32(0x80000 + 232, 4);
    run(0x82b11df0, 11, 0);
    check(m.ReadU32(0x70004) == 0 && m.ReadU32(0x70008) == 2);
    check(!battle_action_parameters61::Apply(0, m, {g, native}, s));
    std::cout << "battle_action_parameters61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
