#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_eligibility61.h"
#include <iostream>
struct EligibilityGuest final : manager_release_context61::GuestServices {
  unsigned allowed = 1, resultByte = 1, otherByte = 0, configured = 0,
           evaluated = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x82b14168) {
      if (s.r[3] != 0x73000 || s.r[4] != 0x80000 || s.r[5] != 0x90000 ||
          s.r[6] != 3 || s.r[7] != 9)
        throw std::runtime_error("eligibility configuration");
      ++configured;
      s.r[3] = allowed;
      return;
    }
    if (e == 0x82b120e0) {
      if (s.r[3] != 0x73000)
        throw std::runtime_error("eligibility receiver");
      ++evaluated;
      m.WriteU8(0x73000 + 208, resultByte);
      m.WriteU8(0x73000 + 76, otherByte);
      return;
    }
    throw std::runtime_error("eligibility direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("eligibility indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    EligibilityGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("eligibility state");
    };
    m.WriteU32(0x832ca0d8, 0x73000);
    m.WriteU32(0x832c9c54 + 44, 0x63000);
    m.WriteU32(0x63004, 0x62000);
    m.WriteU32(0x6300c, 1);
    m.WriteU32(0x62008, 25);
    m.WriteU32(0x90000 + 64, 25);
    m.WriteU32(0x83213438, 0);
    m.WriteU32(0x8321343c, 4);
    auto run = [&](unsigned kind) {
      s.r[3] = 0x70000;
      s.r[4] = 0x80000;
      s.r[5] = 0x90000;
      s.r[6] = kind;
      s.r[7] = 9;
      check(battle_action_eligibility61::Apply(0x82ad0c10, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
            s.r[31] == initial.r[31]);
      return unsigned(s.r[3]);
    };
    for (unsigned kind = 0; kind < 33; ++kind)
      check(run(kind) == unsigned(kind < 32 && kind != 17));
    m.WriteU32(0x90000 + 232, 4);
    check(!run(1) && !run(12) && run(0) == 1);
    s.r[3] = 0x90000;
    check(battle_action_eligibility61::Apply(0x8238abe0, m, {g, native}, s) &&
          s.r[3] == 4);
    m.WriteU32(0x90000 + 232, 0);
    m.WriteU16(0x70000 + 148, 32);
    m.WriteU32(0x62000 + 64, 0x4000);
    check(!run(1) && !run(3) && run(2) == 1);
    m.WriteU32(0x80000 + 68, 7);
    check(run(1) == 1 && !run(3));
    m.WriteU16(0x70000 + 148, 0);
    g.allowed = 0;
    auto evaluated = g.evaluated;
    check(!run(2) && g.evaluated == evaluated);
    g.allowed = 1;
    g.resultByte = 0;
    check(!run(2));
    g.otherByte = 1;
    check(run(2) == 1);
    check(!battle_action_eligibility61::Apply(0, m, {g, native}, s));
    std::cout << "battle_action_eligibility61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
