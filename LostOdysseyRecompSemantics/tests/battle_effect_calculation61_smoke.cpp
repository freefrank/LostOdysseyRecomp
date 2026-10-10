#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_effect_calculation61.h"
#include <iostream>
struct CalculationGuest final : manager_release_context61::GuestServices {
  unsigned defense = 5, attack = 20;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82b096e8) {
      s.r[3] = 0;
      return;
    }
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
    regions.push_back({0x82089000, 0x1000});
    regions.push_back({0x82218000, 0x1000});
    regions.push_back({0x8201f000, 0x1000});
    regions.push_back({0x821ba000, 0x1000});
    regions.push_back({0x832ae000, 0x1000});
    regions.push_back({0x8201d000, 0x1000});
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
    m.WriteU32(0x832aeb00, 0x76000);
    m.WriteU32(0x73000 + 32, 0x75000);
    m.WriteU32(0x73000 + 20, 0x42c80000);
    m.WriteU8(0x76000 + 24, 1);
    m.WriteU32(0x76000 + 12, 0x3e800000);
    m.WriteU32(0x82000fb0, 0x3f800000);
    m.WriteU32(0x82007784, 0x3f800000);
    run(0x82b20ef0);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 75 &&
          m.ReadU32(0x100000 + 4 * (116 + 3776)) == 1);
    m.WriteU32(0x90000 + 124, 0x40000000);
    run(0x82b20ef0);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 100);
    m.WriteU32(0x90000 + 124, 0);
    m.WriteU32(0x73000 + 20, 0x412e6666);
    m.WriteU32(0x73000 + 80, 0x3f800000);
    m.WriteU32(0x73000 + 76, 0x40000000);
    m.WriteU32(0x73000 + 72, 0x40400000);
    m.WriteU32(0x73000 + 56, 2);
    m.WriteU32(0x8201f9f0, 0x3f000000);
    m.WriteU32(0x821baa74, 0x3f000000);
    m.WriteU8(0x73000 + 64, 0);
    m.WriteU32(0x80000 + 232, 0);
    run(0x82b21148);
    check(std::bit_cast<float>(m.ReadU32(0x73000 + 24)) == 32);
    m.WriteU32(0x80000 + 232, 1u << 7);
    run(0x82b21148);
    check(std::bit_cast<float>(m.ReadU32(0x73000 + 24)) == 2);
    m.WriteU8(0x73000 + 36, 1);
    m.WriteU32(0x82000e1c, 0x40000000);
    run(0x82b21148);
    check(std::bit_cast<float>(m.ReadU32(0x73000 + 24)) == 64);
    m.WriteU32(0x80000 + 3 * 272 + 232, 1);
    run(0x82b21148);
    check(std::bit_cast<float>(m.ReadU32(0x73000 + 24)) == 32);
    m.WriteU8(0x73000 + 64, 1);
    run(0x82b21148);
    check(!m.ReadU32(0x73000 + 24));
    m.WriteU32(0x80000 + 76252, 2);
    m.WriteU32(0x80000 + 76264, 7);
    m.WriteU32(0x80000 + 76276, 13);
    s.r[3] = 0x76000;
    s.r[4] = 0x80000;
    s.r[5] = 2;
    s.r[6] = 0xffffffff;
    s.r[7] = 13;
    check(battle_effect_calculation61::Apply(0x82aa0890, m, {g, native}, s) &&
          s.r[3] == 1 && m.ReadU32(0x76000 + 16) == 7);
    s.r[3] = 0x76000;
    s.r[5] = 8;
    check(battle_effect_calculation61::Apply(0x82aa0890, m, {g, native}, s) &&
          s.r[3] == 0 && m.ReadU32(0x76000 + 12) == 2);
    m.WriteU32(0x832ca0cc, 0x77000);
    m.WriteU32(0x82000da4, 0x3f000000);
    for (unsigned mode = 0; mode < 5; ++mode) {
      m.WriteU32(0x77000 + 100, mode);
      s.r[4] = 0x80000;
      s.r[7] = 2;
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(10.);
      s.fpr_bits[2] = std::bit_cast<std::uint64_t>(20.);
      s.fpr_bits[3] = std::bit_cast<std::uint64_t>(0.);
      check(battle_effect_calculation61::Apply(0x82aa0e10, m, {g, native}, s));
      check(std::bit_cast<double>(s.fpr_bits[1]) == (mode == 1   ? 15
                                                     : mode == 2 ? 20
                                                                 : 10));
    }
    s.r[7] = 99;
    s.fpr_bits[3] = std::bit_cast<std::uint64_t>(7.);
    check(battle_effect_calculation61::Apply(0x82aa0e10, m, {g, native}, s) &&
          std::bit_cast<double>(s.fpr_bits[1]) == 7);
    m.WriteU32(0x832cb784, 0x76000);
    m.WriteU32(0x80000 + 76252, 5);
    m.WriteU32(0x80000 + 76276, 13);
    m.WriteU32(0x77000 + 100, 2);
    m.WriteU32(0x82000dc0, 0x41a00000);
    m.WriteU32(0x80000 + 4940, 10);
    m.WriteU8(0x73000 + 108, 0);
    m.WriteU8(0x73000 + 45, 0);
    m.WriteU8(0x73000 + 36, 0);
    run(0x82b20270);
    check(m.ReadU8(0x73000 + 36) == 1);
    m.WriteU8(0x73000 + 45, 1);
    m.WriteU8(0x73000 + 36, 9);
    run(0x82b20270);
    check(m.ReadU8(0x73000 + 36) == 9);
    m.WriteU8(0x73000 + 45, 0);
    m.WriteU8(0x73000 + 108, 1);
    m.WriteU32(0x80000 + 4940, 0);
    run(0x82b20270);
    check(m.ReadU8(0x73000 + 36) == 9);
    m.WriteU8(0x73000 + 108, 0);
    m.WriteU32(0x73000 + 68, 0x42c80000);
    m.WriteU32(0x80000 + 4828, 1);
    m.WriteU32(0x90000 + 4888, 1);
    m.WriteU32(0x80000 + 4916, 7);
    m.WriteU32(0x80000 + 76252, 2);
    m.WriteU32(0x82000de8, 0x3fc00000);
    run(0x82b20b30);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 50 &&
          m.ReadU32(0x73000 + 84) == 7 &&
          m.ReadU32(0x100000 + 4 * (116 + 3782)) == 1);
    m.WriteU32(0x90000 + 5092, 2);
    run(0x82b20b30);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 100);
    m.WriteU32(0x80000 + 76252, 8);
    m.WriteU32(0x820894f0, 0x40000000);
    run(0x82b20d38);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 100);
    m.WriteU8(0x73000 + 108, 1);
    run(0x82b20d38);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 0);
    for (unsigned i = 28; i < 32; ++i)
      check(s.fpr_bits[i] == initial.fpr_bits[i]);
    m.WriteU8(0x73000 + 108, 0);
    m.WriteU32(0x73000 + 68, 0x42c80000);
    m.WriteU32(0x80000 + 76252, 1);
    m.WriteU32(0x80000 + 4916, 2);
    m.WriteU32(0x90000 + 4880, 1);
    m.WriteU32(0x90000 + 4956, 0);
    m.WriteU32(0x90000 + 5092, 0);
    run(0x82b207a0);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 50 &&
          m.ReadU32(0x73000 + 84) == 2);
    m.WriteU32(0x90000 + 5092, 1);
    run(0x82b207a0);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 100);
    m.WriteU32(0x90000 + 4956, 0x20);
    m.WriteU32(0x73000 + 84, 99);
    m.WriteU8(0x73000 + 64, 0);
    run(0x82b207a0);
    check(m.ReadU8(0x73000 + 64) == 1 && m.ReadU32(0x73000 + 84) == 99 &&
          std::bit_cast<double>(s.fpr_bits[1]) == 0);
    m.WriteU32(0x90000 + 4956, 0x40);
    m.WriteU8(0x73000 + 64, 0);
    run(0x82b207a0);
    check(!m.ReadU8(0x73000 + 64));
    m.WriteU32(0x80000 + 4916, 0);
    m.WriteU32(0x90000 + 4956, 0x20);
    run(0x82b207a0);
    check(m.ReadU8(0x73000 + 64) == 1);
    m.WriteU8(0x73000 + 64, 0);
    m.WriteU32(0x90000 + 4956, 0x820);
    run(0x82b207a0);
    check(!m.ReadU8(0x73000 + 64));
    m.WriteU32(0x90000 + 4956, 2);
    run(0x82b207a0);
    check(m.ReadU8(0x73000 + 64) == 1);
    auto clearProperties = [&]() {
      for (unsigned bank = 0; bank < 9; ++bank) {
        m.WriteU32(0x80000 + 272 * bank + 232, 0);
        m.WriteU32(0x90000 + 272 * bank + 232, 0);
      }
    };
    clearProperties();
    m.WriteU8(0x73000 + 108, 0);
    m.WriteU8(0x73000 + 109, 0);
    m.WriteU8(0x73000 + 45, 0);
    m.WriteU32(0x73000 + 40, 0);
    m.WriteU32(0x90000 + 124, 0);
    m.WriteU32(0x90000 + 4956, 0);
    m.WriteU32(0x90000 + 7 * 272 + 232, 1u << 21);
    run(0x82b1ff20);
    check(m.ReadU32(0x73000 + 52) == 1);
    clearProperties();
    m.WriteU32(0x90000 + 4956, 4);
    run(0x82b1ff20);
    check(m.ReadU32(0x73000 + 52) == 2);
    m.WriteU32(0x80000 + 7 * 272 + 232, 1u << 23);
    run(0x82b1ff20);
    check(!m.ReadU32(0x73000 + 52));
    m.WriteU32(0x90000 + 4956, 0);
    clearProperties();
    m.WriteU32(0x90000 + 4 * 272 + 232, 1u << 11);
    m.WriteU32(0x90000 + 68, 86);
    run(0x82b1ff20);
    check(m.ReadU32(0x73000 + 52) == 3);
    clearProperties();
    m.WriteU32(0x90000 + 7 * 272 + 232, 1u << 10);
    m.WriteU32(0x90000 + 4 * (7 * 68 + 10 + 59), 100);
    run(0x82b1ff20);
    check(m.ReadU32(0x73000 + 52) == 4);
    clearProperties();
    m.WriteU32(0x90000 + 4 * 272 + 232, 1u << 13);
    m.WriteU32(0x90000 + 232, 2);
    m.WriteU32(0x90000 + 2616, 0x3f800000);
    run(0x82b1ff20);
    check(m.ReadU32(0x73000 + 52) == 5);
    clearProperties();
    m.WriteU32(0x90000 + 232, 1);
    run(0x82b1ff20);
    check(!m.ReadU32(0x73000 + 52));
    clearProperties();
    m.WriteU32(0x90000 + 4 * (7 * 68 + 8 + 59), 100);
    check(run(0x82b1fdf8));
    m.WriteU32(0x80000 + 7 * 272 + 232, 1u << 23);
    check(!run(0x82b1fdf8));
    check(!battle_effect_calculation61::Apply(0, m, {g, native}, s));
    std::cout << "battle_effect_calculation61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
