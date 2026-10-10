#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_readiness61.h"
#include <iostream>
struct ReadinessGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("readiness direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("readiness indirect");
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
    ReadinessGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("readiness state");
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    auto run = [&](unsigned e, unsigned mode) {
      s.r[3] = 0x80000;
      s.r[4] = mode;
      check(battle_action_readiness61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[30] == initial.r[30] &&
            s.r[31] == initial.r[31]);
      return unsigned(s.r[3]);
    };
    check(!run(0x82ac9a28, 0) && !run(0x82ac9a28, 1));
    for (unsigned id : {0u, 3u, 15u, 16u, 28u, 20u, 242u, 243u, 111u}) {
      auto p = 0x80000 + 272 * (id / 32) + 232;
      m.WriteU32(p, 1u << (id % 32));
      check(run(0x8238e368, id) == 1 && run(0x82ac9a28, 0) == 1);
      check(run(0x82ac9a28, 1) == unsigned(id < 32));
      check(run(0x82ab0958, 0) ==
            unsigned(id == 0 || id == 15 || id == 3 || id == 16));
      m.WriteU32(p, 0);
    }
    m.WriteU32(0x80000 + 8 * 272 + 232, 1u << 6);
    check(run(0x8238e368, 262) == 1 && !run(0x8238e368, 263));
    for (unsigned flag : {0x10000u, 0x200000u}) {
      m.WriteU32(0x80000 + 124, flag);
      check(run(0x8238a8a0, 0) == 1 && run(0x82ac9768, 0) == 1);
    }
    m.WriteU32(0x80000 + 124, 0x80000000);
    check(!run(0x82ac9a28, 0));
    check(!battle_action_readiness61::Apply(0, m, {g, native}, s));
    std::cout << "battle_action_readiness61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
