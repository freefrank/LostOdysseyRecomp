#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_property_mutation61.h"
#include <iostream>
struct MutationGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("property direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("property indirect");
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
    MutationGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("property state");
    };
    m.WriteU32(0x83213438 + 8 * 20, 0);
    m.WriteU32(0x8321343c + 8 * 20, 1u << 20);
    constexpr unsigned resource = 0x80000, flag = resource + 7 * 272 + 232,
                       first = resource + 4 * (7 * 68 + 20 + 59),
                       second = resource + 4 * (7 * 68 + 20 + 91);
    m.WriteU32(flag, (1u << 20) | 7);
    m.WriteU32(first, 5);
    m.WriteU32(second, 9);
    auto run = [&](unsigned e, unsigned id, unsigned mode) {
      s.r[3] = resource;
      s.r[4] = id;
      s.r[5] = mode;
      check(battle_property_mutation61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[29] == initial.r[29] &&
            s.r[31] == initial.r[31]);
      return unsigned(s.r[3]);
    };
    check(run(0x82ac8ee8, 244, 0) == 1 && m.ReadU32(first) == 5 &&
          m.ReadU32(second) == 9);
    check(run(0x82ac8ee8, 244, 2) == 1 && (m.ReadU32(flag) & (1u << 20)));
    check(run(0x82ac9000, 244, 0) == 1 && m.ReadU32(flag) == 7 &&
          !m.ReadU32(first) && !m.ReadU32(second));
    check(run(0x82ac9000, 244, 0) == 0);
    check(run(0x82ac8ee8, 263, 1) == 0);
    check(!battle_property_mutation61::Apply(0, m, {g, native}, s));
    std::cout << "battle_property_mutation61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
