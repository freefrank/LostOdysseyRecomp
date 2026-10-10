#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_results61.h"
#include <iostream>
struct ResultGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("result direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("result indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x4000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ResultGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("result flags");
    };
    m.WriteU32(0x73000 + 12, 3);
    m.WriteU32(0x73000 + 24, 2);
    m.WriteU32(0x73000 + 20, 0x100000);
    m.WriteU32(0x832cb790, 0x73000);
    unsigned entries[] = {0x82b2b248, 0x82b2b270, 0x82b2b298, 0x82b2b2c0,
                          0x82b2b2e8, 0x82b2b310, 0x82b2b438, 0x82b11230};
    unsigned offsets[] = {3722, 10, 3722, 3754, 3766, 3758, 3782, 3722};
    for (unsigned i = 0; i < 8; ++i) {
      auto p = 0x100000 + 4 * (116 * 3 + 2 + offsets[i]);
      m.WriteU32(p, 99);
      m.WriteU32(p + 4, 77);
      s.r[3] = i == 7 ? 0xdeadbeef : 0x73000;
      check(battle_action_results61::Apply(entries[i], m, {g, native}, s));
      check(m.ReadU32(p) == unsigned(i != 2 && i != 6) &&
            m.ReadU32(p + 4) == 77 && s.r[1] == initial.r[1] &&
            s.lr == initial.lr && s.r[3] == 0x73000);
    }
    check(!battle_action_results61::Apply(0, m, {g, native}, s));
    std::cout << "battle_action_results61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
