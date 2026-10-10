#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_destruction61.h"
#include <iostream>
struct DestructionGuest final : manager_release_context61::GuestServices {
  std::vector<unsigned> calls;
  unsigned cleanup = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e != 0x82b7ae18 || s.r[3] != 111 || s.r[4] != 12 || s.r[5] != 3 ||
        s.r[6] != 0x2220)
      throw std::runtime_error("cleanup boundary");
    ++cleanup;
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x82298938 && e != 0x2220)
      throw std::runtime_error("destructor callback");
    calls.push_back(unsigned(s.r[3]));
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    test::GuestWindow w(cook_main_smoke::Regions);
    w.Fill(0);
    auto m = w.Memory();
    DestructionGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("destruction state");
    };
    s.r[3] = 0x100000;
    check(battle_action_destruction61::Apply(0x828ae428, m, {g, native}, s));
    check(g.calls.size() == 1024);
    unsigned n = 0;
    for (unsigned section : {14884u, 36u})
      for (int slot = 31; slot >= 0; --slot)
        for (int text = 15; text >= 0; --text)
          check(g.calls[n++] ==
                0x100000 + section + 464 * slot + 272 + 12 * text);
    check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
          s.r[31] == initial.r[31]);
    g.calls.clear();
    s.r[3] = 0x90000;
    s.r[4] = 12;
    s.r[5] = 3;
    s.r[6] = 0x2220;
    check(battle_action_destruction61::Apply(0x82b7aef0, m, {g, native}, s));
    check((g.calls == std::vector<unsigned>{0x90018, 0x9000c, 0x90000}));
    g.calls.clear();
    s.r[3] = 0x90000;
    s.r[4] = 12;
    s.r[5] = 0;
    s.r[6] = 0x2220;
    check(battle_action_destruction61::Apply(0x82b7aef0, m, {g, native}, s));
    check(g.calls.empty());
    s.r[12] = 0x90000 + 144;
    s.r[29] = 111;
    s.r[30] = 12;
    s.r[28] = 3;
    s.r[27] = 0x2220;
    m.WriteU32(0x90000 + 80, 0);
    check(battle_action_destruction61::Apply(0x82b7afa8, m, {g, native}, s));
    check(g.cleanup == 1);
    check(!battle_action_destruction61::Apply(0, m, {g, native}, s));
    std::cout << "battle_action_destruction61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
