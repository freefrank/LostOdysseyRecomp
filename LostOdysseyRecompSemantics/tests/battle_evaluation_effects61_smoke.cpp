#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_evaluation_effects61.h"
#include <iostream>
struct EffectGuest final : manager_release_context61::GuestServices {
  unsigned mode = 0, step = 0, side = 0;
  bool late = false;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("effect callback ABI");
    };
    if (e == 0x82b22948) {
      check(step++ == 0 && s.r[3] == 0x70000 &&
            m.ReadU32(0x73000 + 64) == mode);
      check(m.ReadU32(0x70004) == 0x80000 && m.ReadU32(0x70008) == 0x90000 &&
            m.ReadU32(0x70000 + 28) == 36 && m.ReadU32(0x70000 + 32) == 40 &&
            m.ReadU8(0x70000 + 45) == 1 && m.ReadU32(0x70000 + 56) == 1);
      check(m.ReadU32(0x74000 + 4) == 0x80000 &&
            m.ReadU32(0x74000 + 20) == 40 && m.ReadU8(0x70000 + 109) == 1 &&
            m.ReadU32(0x70000 + 96) == 0x42c80000);
      return;
    }
    if (e == 0x82ac8ec8) {
      check(step == 1 && s.r[3] == 0x80000 && s.r[4] == 3 && s.r[5] == 8 &&
            s.r[6] == 120 && s.r[7] == 0 && s.r[8] == 1 &&
            m.ReadU32(0x80000 + 188) == 0);
      ++side;
      return;
    }
    if (e == 0x82ac71e8 || e == 0x82ac80b8) {
      check(step == 1 && s.r[3] == 0x72000 && s.r[4] == 0x90000 &&
            s.r[5] == 120);
      side = e == 0x82ac71e8 ? 1 : 2;
      return;
    }
    throw std::runtime_error("effect direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("effect indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x832ae000, 0x1000});
    regions.push_back({0x83213000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    EffectGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("effect state");
    };
    m.WriteU32(0x832cb790, 0x74000);
    m.WriteU32(0x832c9c54 + 44, 0x63000);
    m.WriteU32(0x63004, 0x62000);
    m.WriteU32(0x6300c, 1);
    m.WriteU32(0x62008, 24);
    m.WriteU32(0x80000 + 64, 24);
    m.WriteU32(0x62000 + 64, 64);
    m.WriteU32(0x90000 + 2588, 0x42c80000);
    m.WriteU32(0x73004, 0x80000);
    m.WriteU32(0x73008, 0x90000);
    m.WriteU32(0x73000 + 36, 36);
    m.WriteU32(0x73000 + 40, 40);
    m.WriteU32(0x73000 + 120, 120);
    m.WriteU32(0x832aeb00, 0x72000);
    m.WriteU32(0x8321343c + 8 * 15, 8);
    constexpr unsigned entries[] = {
        0x82b0ce98, 0x82b0cf20, 0x82b0cf78, 0x82b0cfd0, 0x82b0d028,
        0x82b0d148, 0x82b0d1a0, 0x82b0d088, 0x82b0d210, 0x82b0d2c8};
    constexpr unsigned modes[] = {1, 2, 3, 4, 0, 5, 6, 8, 0, 9};
    for (unsigned i = 0; i < 10; ++i)
      for (unsigned choice = 0; choice < 2; ++choice) {
        g.mode = modes[i];
        g.late = i == 6 || i == 7;
        g.step = g.side = 0;
        m.WriteU32(0x832ca0cc, 0x70000);
        m.WriteU32(0x73000 + 64, 99);
        m.WriteU32(0x73000 + 108, choice);
        m.WriteU32(0x80000 + 188, 99);
        s.r[3] = 0x73000;
        check(
            battle_evaluation_effects61::Apply(entries[i], m, {g, native}, s));
        check(s.r[1] == initial.r[1] && s.r[28] == initial.r[28] &&
              s.r[31] == initial.r[31] && g.step == 1);
        if (i >= 8)
          check(g.side == choice + 1);
        if (i == 7)
          check(g.side == 1);
      }
    m.WriteU32(0x75000, 16);
    m.WriteU32(0x70000 + 64, 0xffffffff);
    s.r[3] = 0x70000;
    s.r[4] = 0x80000;
    s.r[5] = 0x90000;
    s.r[6] = 36;
    s.r[7] = 0x75000;
    s.r[8] = 7;
    s.r[9] = 258;
    check(battle_evaluation_effects61::Apply(0x82b21878, m, {g, native}, s));
    check(m.ReadU8(0x70000 + 108) == 1 && m.ReadU8(0x70000 + 45) == 2 &&
          m.ReadU32(0x70000 + 40) == 7 && m.ReadU32(0x70000 + 64) == 0xffff);
    check(!battle_evaluation_effects61::Apply(0, m, {g, native}, s));
    std::cout << "battle_evaluation_effects61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
