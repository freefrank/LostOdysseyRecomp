#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_evaluation_chance61.h"
#include <iostream>
struct ChanceGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("chance direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("chance indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ChanceGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("chance state");
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    m.WriteU32(0x73004, 0x80000);
    m.WriteU32(0x73008, 0x90000);
    m.WriteU32(0x80000 + 64, 25);
    m.WriteU32(0x83264558, 0x70000);
    m.WriteU32(0x831f3300, 40);
    m.WriteU32(0x90000 + 4 * (4 + 127), 4);
    m.WriteU32(0x90000 + 4 * (4 + 195), 1);
    auto run = [&](unsigned e) {
      s.r[3] = 0x73000;
      check(battle_evaluation_chance61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
            s.fpr_bits[31] == initial.fpr_bits[31]);
      return unsigned(s.r[3]);
    };
    run(0x82b08d98);
    check(std::bit_cast<double>(s.fpr_bits[1]) == 15);
    m.WriteU32(0x73000 + 88, 0x42480000);
    check(run(0x82b08e28));  // 50 >= 40
    check(!run(0x82b08ea8)); // 50 - 15 < 40
    m.WriteU8(0x73000 + 77, 1);
    m.WriteU32(0x73000 + 88, 0xbf800000);
    check(run(0x82b08e28));
    m.WriteU32(0x73000 + 184, 1);
    check(!run(0x82b08ea8));
    m.WriteU32(0x73000 + 184, 0);
    m.WriteU32(0x90000 + 5 * 272 + 232, 8);
    check(!run(0x82b08ea8));
    m.WriteU32(0x90000 + 5 * 272 + 232, 0);
    m.WriteU32(0x90000 + 4 * 127, 10);
    m.WriteU32(0x90000 + 4 * 195, 3);
    m.WriteU32(0x90000 + 1320, 8);
    m.WriteU32(0x90000 + 4 * (3 + 331), 20);
    s.r[3] = 0x90000;
    s.r[4] = 32;
    check(battle_evaluation_chance61::Apply(0x82ac9858, m, {g, native}, s) &&
          s.r[3] == 17);
    m.WriteU32(0x90000 + 4 * (2 * 68 + 5 + 91), 99);
    s.r[3] = 0x90000;
    s.r[4] = 69;
    check(battle_evaluation_chance61::Apply(0x82ac9858, m, {g, native}, s) &&
          s.r[3] == 99);
    check(!battle_evaluation_chance61::Apply(0, m, {g, native}, s));
    std::cout << "battle_evaluation_chance61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
