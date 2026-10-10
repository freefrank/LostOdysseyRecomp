#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_scene_tasks61.h"
#include <iostream>
struct HandleGuest final : manager_release_context61::GuestServices {
  unsigned count = 2, status = 0, changes = 0, stops = 0, lastMode = 0,
           lastValue = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (s.r[3] != 0x71000)
      throw std::runtime_error("handle manager ABI");
    if (e == 0x82b5cf80) {
      if (s.r[4] != 3)
        throw std::runtime_error("handle filter");
      s.r[3] = count ? 1 : 0;
      return;
    }
    if (e == 0x82b5f528) {
      auto index = m.ReadU32(unsigned(s.r[4]));
      m.WriteU32(unsigned(s.r[4]), index < count ? index + 1 : 0);
      s.r[3] = 100 + index;
      return;
    }
    if (e == 0x82b5ebc8) {
      if (s.r[5] != 10)
        throw std::runtime_error("handle kind ABI");
      s.r[3] = 2;
      return;
    }
    if (e == 0x8236c728) {
      s.r[3] = status;
      return;
    }
    if (e == 0x82b62048) {
      ++changes;
      lastMode = unsigned(s.r[5]);
      lastValue = unsigned(s.r[6]);
      s.r[3] = 1;
      return;
    }
    if (e == 0x8236c7d8) {
      ++stops;
      return;
    }
    throw std::runtime_error("handle direct boundary");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("handle indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p :
         {0x832d2000u, 0x832cc000u, 0x832cb000u, 0x83213000u, 0x820c7000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    HandleGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("handle state");
    };
    m.WriteU32(0x832d268c, 0x71000);
    m.WriteU32(0x832d2810, 0x70000);
    m.WriteU32(0x70000, 102);
    m.WriteU32(0x820c7518, std::bit_cast<unsigned>(2.f));
    auto run = [&](unsigned e, unsigned arg = 0, double fade = 0.) {
      s.r[3] = arg;
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(fade);
      check(battle_scene_tasks61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
            s.fpr_bits[31] == initial.fpr_bits[31]);
      return unsigned(s.r[3]);
    };
    check(run(0x82b63828, 0x70000) == 102);
    m.WriteU32(0x70000, 999);
    check(run(0x82b63828, 0x70000) == 101);
    check(run(0x82b19cc0) == 1 && m.ReadU8(0x832cc0f8) == 1);
    g.status = 0xffffffff;
    check(run(0x82b19cc0) == 0 && !m.ReadU8(0x832cc0f8));
    run(0x82b19c00, 7, 2.);
    check(g.changes == 1 && g.lastValue == 14 && g.lastMode == 0 &&
          m.ReadU8(0x832cc0f8));
    run(0x82b19c00, 7, 3.);
    check(g.changes == 1);
    m.WriteU8(0x832cb780, 1);
    run(0x82b1a048, 0, 1.);
    check(g.changes == 1 && m.ReadU8(0x832cc0f8));
    m.WriteU8(0x832cb780, 0);
    run(0x82b1a048, 0, 1.);
    check(g.changes == 2 && g.lastMode == 2 && !m.ReadU8(0x832cc0f8));
    m.WriteU8(0x832cc0f8, 1);
    run(0x82b1a048);
    check(g.stops == 1 && !m.ReadU8(0x832cc0f8));
    g.count = 0;
    check(run(0x82b63828, 0x70000) == 0);
    run(0x82b19c00, 8);
    check(!m.ReadU8(0x832cc0f8) && g.changes == 2);
    std::cout << "battle scene handle logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
