#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_evaluation_gates61.h"
#include "lo_semantics/battle_action_eligibility61.h"
#include <iostream>
struct GatesGuest final : manager_release_context61::GuestServices {
  unsigned virtualResult = 0, first = 0, second = 0, calls = 0, predicate = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82b09050) {
      s.r[3] = predicate;
      return;
    }
    if (e != 0x82ac90e8 && e != 0x82ac8978 && e != 0x82aca700)
      throw std::runtime_error("gate direct");
    if (s.r[3] != 0x90000 || (s.r[4] != 10 && s.r[4] != 11))
      throw std::runtime_error("gate arguments");
    ++calls;
    s.r[3] = s.r[4] == 10 ? first : second;
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if ((e != 0x123400 && e != 0x123404) || s.r[3] != 0x90000)
      throw std::runtime_error("gate virtual");
    s.r[3] = virtualResult;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    GatesGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("gate result");
    };
    m.WriteU32(0x73004, 0x80000);
    m.WriteU32(0x73008, 0x90000);
    m.WriteU32(0x90000, 0x91000);
    m.WriteU32(0x91000 + 380, 0x123400);
    m.WriteU32(0x91000 + 292, 0x123404);
    m.WriteU32(0x73000 + 92, 10);
    m.WriteU32(0x73000 + 96, 11);
    m.WriteU32(0x73000 + 100, 20);
    m.WriteU32(0x73000 + 104, 21);
    auto run = [&](unsigned e) {
      s.r[3] = 0x73000;
      check(battle_evaluation_gates61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[29] == initial.r[29] &&
            s.r[31] == initial.r[31]);
      return m.ReadU8(0x73000 + 208);
    };
    check(!run(0x82acf878) && run(0x82b08b20));
    check(run(0x82b08b30));
    g.virtualResult = 1;
    check(!run(0x82b08b30) && run(0x82b102d8));
    g.virtualResult = 0;
    check(!run(0x82b102d8) && !run(0x82b08ab8));
    m.WriteU32(0x90000 + 64, 1);
    check(run(0x82b08ab8));
    check(!run(0x82b10618));
    g.predicate = 2;
    check(run(0x82b10618));
    g.virtualResult = 1;
    check(!run(0x82b10618));
    g.virtualResult = 0;
    check(run(0x82b0b0f0));
    m.WriteU32(0x83213438 + 8 * 18, 0);
    m.WriteU32(0x8321343c + 8 * 18, 4);
    m.WriteU32(0x80000 + 3 * 272 + 232, 4);
    check(!run(0x82b0b0f0));
    for (auto e : {0x82b0e9d0u}) {
      g.first = 0;
      g.second = 0;
      g.calls = 0;
      check(!run(e) && g.calls == 2);
      g.first = 1;
      g.calls = 0;
      check(run(e) && g.calls == 1);
      g.first = 2;
      g.second = 257;
      g.calls = 0;
      check(run(e) && g.calls == 2);
      m.WriteU32(0x73000 + 104, 0);
      g.calls = 0;
      check(!run(e) && g.calls == 1);
      m.WriteU32(0x73000 + 104, 21);
    }
    m.WriteU32(0x90000 + 272 * 10 + 232, 0);
    m.WriteU32(0x90000 + 272 * 11 + 232, 1);
    check(run(0x82b0eb68));
    m.WriteU32(0x90000 + 272 * 11 + 232, 0);
    check(!run(0x82b0eb68));
    m.WriteU32(0x90000 + 272 * 10 + 232, 4);
    check(run(0x82b0eb68));
    // Typed payload threshold gate uses real bank data.
    for (unsigned bank : {10u, 11u})
      for (unsigned bit = 0; bit < 31; ++bit)
        m.WriteU32(0x83213538 + 4 * (32 * bank + bit), 1);
    m.WriteU32(0x90000 + 272 * 10 + 232, 20);
    m.WriteU32(0x90000 + 272 * 11 + 232, 21);
    m.WriteU32(0x90000 + 4 * (680 + 2 + 59), 10);
    m.WriteU32(0x90000 + 4 * (748 + 0 + 59), 10);
    m.WriteU32(0x73000 + 108, 9);
    m.WriteU32(0x73000 + 112, 9);
    check(!run(0x82b0c430));
    m.WriteU32(0x73000 + 112, 10);
    check(run(0x82b0c430) && run(0x82b0ed68));
    g.virtualResult = 1;
    g.calls = 0;
    check(!run(0x82b0ed68) && !g.calls);
    // Dispatch a recovered leaf through the actual descriptor table.
    m.WriteU32(0x73000 + 20, 3);
    m.WriteU32(0x73000 + 48, 0xa0000);
    m.WriteU32(0xa0000 + 40, 16);
    m.WriteU32(0x73000 + 4 * (124 + 16), 0x82b08b20);
    s.r[3] = 0x73000;
    check(battle_action_eligibility61::Apply(0x82b120e0, m, {g, native}, s));
    check(m.ReadU8(0x73000 + 208) == 1);
    check(!battle_evaluation_gates61::Apply(0, m, {g, native}, s));
    std::cout << "battle_evaluation_gates61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
