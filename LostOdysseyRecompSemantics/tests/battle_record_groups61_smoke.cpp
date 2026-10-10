#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_records61.h"
#include "lo_semantics/battle_script_global_modes61.h"
#include <iostream>
struct GroupsGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x8238e2f8) {
      s.r[3] = 0x70000;
      return;
    }
    throw std::runtime_error("group direct boundary");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("group indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c3000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    GroupsGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("record groups state");
    };
    m.WriteU32(0x70000, 0x71000);
    m.WriteU32(0x70004, 5);
    for (unsigned i = 0; i < 5; ++i) {
      auto resource = 0x80000 + 0x20000 * i;
      m.WriteU32(0x71000 + 4 * i, resource);
      m.WriteU32(resource + 64, i + 1);
      m.WriteU32(resource + 14656, 0x200000 + 0x40000 * i);
      m.WriteU32(resource + 14660, i ? 1 : 2);
    }
    m.WriteU32(0x80000 + 212, 9);
    m.WriteU32(0xa0000 + 212, 9);
    m.WriteU32(0xc0000 + 212, 9);
    m.WriteU32(0xa0000 + 216, 2);
    m.WriteU32(0xc0000 + 216, 1);
    m.WriteU32(0xe0000 + 204, 7);
    m.WriteU32(0x100000 + 204, 7);
    check(battle_action_records61::Apply(0x82afd2f0, m, {g, native}, s));
    check(s.r[1] == initial.r[1] && s.r[21] == initial.r[21]);
    for (unsigned row = 0; row < 2; ++row) {
      auto p = 0x200000 + 124208 * row;
      check(m.ReadU32(p + 36) == 1 && m.ReadU32(p + 500) == 3 &&
            m.ReadU32(p + 964) == 0xffffffff);
    }
    check(m.ReadU32(0x240000 + 500) == 0xffffffff &&
          m.ReadU32(0x280000 + 500) == 0xffffffff);
    check(m.ReadU32(0x2c0000 + 500) == 5 && m.ReadU32(0x300000 + 500) == 4);
    m.WriteU32(0x832c34e0, 0x72000);
    m.WriteU8(0x72000 + 465, 0xaa);
    s.r[3] = 0x73000;
    s.r[4] = 0;
    check(battle_script_global_modes61::Apply(0x82aab870, m, {g, native}, s));
    check(m.ReadU8(0x72000 + 465) == 0xab && !m.ReadU8(0x73000 + 5737));
    s.r[4] = 257;
    check(battle_script_global_modes61::Apply(0x82aab870, m, {g, native}, s));
    check(m.ReadU8(0x72000 + 465) == 0xaa && m.ReadU8(0x73000 + 5737) == 1);
    std::cout << "battle record groups logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
