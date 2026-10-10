#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_adjustments61.h"
#include <iostream>
struct AdjustmentGuest final : manager_release_context61::GuestServices {
  unsigned inactive = 0, removed = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e308) {
      if (s.r[4] != 7)
        throw std::runtime_error("adjustment peer");
      s.r[3] = 0xa0000;
      return;
    }
    if (e == 0x82ac9000) {
      ++removed;
      return;
    }
    throw std::runtime_error("adjustment direct");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x123400 || s.r[3] != 0xa0000)
      throw std::runtime_error("adjustment virtual");
    s.r[3] = inactive;
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
    AdjustmentGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("adjustment state");
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    auto prop = [&](unsigned id, bool on) {
      auto p = 0x80000 + 272 * (id / 32) + 232, mask = 1u << (id % 32);
      m.WriteU32(p, on ? m.ReadU32(p) | mask : m.ReadU32(p) & ~mask);
    };
    auto run = [&](unsigned e, unsigned mode, unsigned variant) {
      s.r[3] = 0x70000;
      s.r[4] = 0x80000;
      s.r[5] = 0x90000;
      s.r[6] = 0x90004;
      s.r[7] = mode;
      s.r[8] = variant;
      check(battle_action_adjustments61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[25] == initial.r[25] &&
            s.r[31] == initial.r[31]);
    };
    auto reset = [&] {
      m.WriteU32(0x90000, 1);
      m.WriteU32(0x90004, 5);
    };
    auto total = [&] { return 25 * m.ReadU32(0x90000) + m.ReadU32(0x90004); };
    reset();
    run(0x82acd680, 200, 0);
    check(total() == 60);
    m.WriteU32(0x832cb778, 242);
    run(0x82acd680, 50, 0);
    check(total() == 60);
    m.WriteU32(0x832cb778, 0);
    m.WriteU32(0x80000 + 88, 1);
    m.WriteU32(0x80000 + 92, 5);
    m.WriteU32(0x80000 + 60, 4);
    m.WriteU32(0x80000 + 100, 0xc0000009);
    run(0x82acd680, 50, 1);
    check(!m.ReadU32(0x80000 + 88) && m.ReadU32(0x80000 + 92) == 15 &&
          !m.ReadU32(0x80000 + 60) && m.ReadU32(0x80000 + 100) == 0x80000009);
    for (auto entry :
         {std::pair<unsigned, unsigned>{2, 60}, {194, 45}, {162, 15}}) {
      prop(entry.first, true);
      reset();
      run(0x82acdb50, 0, 0);
      check(total() == entry.second);
      reset();
      run(0x82acdb50, 0, 1);
      check(total() == 30);
      prop(entry.first, false);
    }
    prop(2, true);
    reset();
    run(0x82acdda0, 0, 9);
    check(total() == 60);
    prop(2, false);
    prop(160, true);
    reset();
    run(0x82acd770, 0, 0);
    check(total() == 21);
    prop(192, true);
    reset();
    run(0x82acd770, 0, 0);
    check(total() == 30);
    prop(160, false);
    prop(192, false);
    prop(235, true);
    m.WriteU32(0x80000 + 4 * (11 + 399), 0);
    reset();
    run(0x82acd770, 0, 0);
    check(total() == 0);
    prop(235, false);
    prop(252, true);
    m.WriteU32(0x80000 + 4 * (28 + 535), 40);
    reset();
    run(0x82acd770, 0, 0);
    check(total() == 12);
    prop(252, false);
    prop(244, true);
    m.WriteU32(0x80000 + 4 * (20 + 567), 7);
    m.WriteU32(0xa0000, 0xa1000);
    m.WriteU32(0xa1000 + 300, 0x123400);
    reset();
    run(0x82acd770, 0, 0);
    check(total() == 15 && !g.removed);
    g.inactive = 1;
    reset();
    run(0x82acd770, 0, 0);
    check(total() == 30 && g.removed == 2);
    s.r[3] = 244;
    check(battle_action_adjustments61::Apply(0x82ac84e8, m, {g, native}, s) &&
          s.r[3] == 20);
    check(!battle_action_adjustments61::Apply(0, m, {g, native}, s));
    std::cout << "battle_action_adjustments61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
