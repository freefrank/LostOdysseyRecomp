#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_effect_calculation61.h"
#include <iostream>
struct CalculationGuest final : manager_release_context61::GuestServices {
  unsigned defense = 5, attack = 20;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    throw std::runtime_error("calculation direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("calculation indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x8201d000, 0x1000});
    regions.push_back({0x82218000, 0x1000});
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x831f3000, 0x21000});
    regions.push_back({0x8204f000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    CalculationGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("calculation state");
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    m.WriteU32(0x73004, 0x80000);
    m.WriteU32(0x73008, 0x90000);
    m.WriteU32(0x80000 + 64, 24);
    m.WriteU32(0x90000 + 64, 25);
    m.WriteU32(0x83264558, 0x70000);
    m.WriteU32(0x832cb790, 0x74000);
    m.WriteU32(0x74000 + 20, 0x100000);
    m.WriteU32(0x74000 + 12, 1);
    m.WriteU32(0x80000 + 2604, 0x42c80000);
    m.WriteU32(0x90000 + 2608, 0x41200000);
    m.WriteU32(0x90000 + 4 * (2 + 127), 2);
    m.WriteU32(0x90000 + 4 * (3 + 127), 3);
    m.WriteU32(0x8204fc20, 0x43480000);
    auto run = [&](unsigned e) {
      s.r[3] = 0x73000;
      check(battle_effect_calculation61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
            s.fpr_bits[31] == initial.fpr_bits[31]);
      return unsigned(s.r[3]);
    };
    run(0x82b1fb00);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 120);
    run(0x82b1fc18);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 40);
    check(run(0x82b1fcc8));
    m.WriteU8(0x73000 + 65, 1);
    run(0x82b1fb00);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 220);
    m.WriteU8(0x73000 + 65, 0);
    m.WriteU32(0x80000 + 2604, 0);
    check(!run(0x82b1fcc8));
    m.WriteU32(0x80000 + 124, 0x10000000);
    m.WriteU32(0x90000 + 68, 262);
    check(run(0x82b1fcc8));
    m.WriteU32(0x90000 + 6 * 272 + 232, 1u << 7);
    run(0x82b204c8);
    check(m.ReadU32(0x73000 + 60) == 2 &&
          m.ReadU32(0x100000 + 464 + 15100) == 2);
    m.WriteU8(0x73000 + 45, 1);
    run(0x82b204c8);
    check(!m.ReadU32(0x73000 + 60));
    m.WriteU32(0x73000 + 20, 0);
    m.WriteU8(0x73000 + 64, 1);
    run(0x82b21068);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 0 &&
          !m.ReadU32(0x73000 + 68));
    m.WriteU8(0x73000 + 64, 0);
    run(0x82b21068);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 1);
    m.WriteU32(0x8201dd2c, 0x42c80000);
    m.WriteU32(0x82000d7c, 0x3c23d70a);
    m.WriteU32(0x80000 + 2596, 0x42c80000);
    m.WriteU32(0x90000 + 2600, 0x42200000);
    run(0x82b20650);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 60);
    m.WriteU32(0x90000 + 2600, 0x43480000);
    run(0x82b20650);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 0);
    m.WriteU32(0x90000 + 4 * 127, 0xffffffff);
    m.WriteU32(0x90000 + 232, 1u << 19);
    m.WriteU32(0x90000 + 2600, 0x42c80000);
    run(0x82b1f918);
    check(s.r[3] == 110); // signed negative stat, property19's floor
    m.WriteU32(0x90000 + 3 * 272 + 232, 1u << 12);
    run(0x82b1f918);
    check(s.r[3] == 130);
    m.WriteU32(0x90000 + 4 * 127, 0);
    m.WriteU32(0x82218420, 0x40000000);
    run(0x82b1f918);
    check(s.r[3] == 200);
    check(s.fpr_bits[29] == initial.fpr_bits[29] &&
          s.fpr_bits[30] == initial.fpr_bits[30]);
    check(!battle_effect_calculation61::Apply(0, m, {g, native}, s));
    std::cout << "battle_effect_calculation61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
