#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_finalize61.h"
#include <iostream>
struct FinalizeGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("finalize direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("finalize indirect");
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
    FinalizeGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("finalize state");
    };
    constexpr unsigned manager = 0x70000, resource = 0x80000, actor = 0x62000,
                       state = 0x63000;
    m.WriteU32(0x832c9c54 + 44, state);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 1);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(resource + 64, 24);
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    auto parameters = [&](unsigned a, unsigned b, unsigned c) {
      m.WriteU32(manager + 4, a);
      m.WriteU32(manager + 8, b);
      m.WriteU32(manager + 12, c);
    };
    auto run = [&](unsigned reset, unsigned variant) {
      s.r[3] = manager;
      s.r[4] = resource;
      s.r[5] = reset;
      s.r[6] = variant;
      s.r[7] = 0;
      check(battle_action_finalize61::Apply(0x82acde40, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[21] == initial.r[21] &&
            s.r[31] == initial.r[31]);
    };
    parameters(2, 5, 10);
    m.WriteU32(resource + 100, 0x40000009);
    run(0, 0);
    check(m.ReadU32(resource + 88) == 2 && m.ReadU32(resource + 92) == 5 &&
          m.ReadU32(resource + 96) == 10 &&
          m.ReadU32(resource + 100) == 0x80000009);
    parameters(3, 7, 20);
    run(0, 0);
    check(m.ReadU32(resource + 88) == 2 && m.ReadU32(resource + 92) == 5);
    run(1, 0);
    check(m.ReadU32(resource + 88) == 3 && m.ReadU32(resource + 92) == 7);
    m.WriteU32(actor + 64, 0x01000000);
    m.WriteU32(resource + 88, 2);
    m.WriteU32(resource + 92, 10);
    parameters(1, 5, 9);
    run(0, 0);
    check(m.ReadU32(resource + 88) == 3 && m.ReadU32(resource + 92) == 1 &&
          m.ReadU32(manager + 8) == 1);
    m.WriteU32(resource + 88, 0);
    m.WriteU32(resource + 92, 10);
    parameters(1, 5, 9);
    run(0, 0);
    check(m.ReadU32(resource + 88) == 1 && m.ReadU32(resource + 92) == 10);
    parameters(4, 7, 8);
    run(1, 1);
    check(m.ReadU32(resource + 88) == 4 && m.ReadU32(resource + 92) == 7);
    m.WriteU32(actor + 64, 0);
    m.WriteU32(resource + 7 * 272 + 232, 1u << 11);
    m.WriteU32(resource + 4 * (7 * 68 + 11 + 59), 100);
    parameters(0, 1, 0);
    run(1, 0);
    check(m.ReadU32(resource + 88) == 14400 &&
          m.ReadU32(resource + 92) == unsigned(-359999) &&
          !m.ReadU32(resource + 96));
    m.WriteU32(resource + 7 * 272 + 232, 0);
    m.WriteU32(0x832cb778, 242);
    parameters(2, 3, 4);
    run(1, 0);
    check(!m.ReadU32(resource + 88) && (m.ReadU32(resource + 4956) & 4096));
    m.WriteU32(0x832cb778, 0);
    m.WriteU32(resource + 4956, 0);
    m.WriteU32(state + 28, 0x100000);
    run(1, 0);
    check(!m.ReadU32(resource + 88) && m.ReadU32(resource + 92) == 10 &&
          !m.ReadU32(resource + 96));
    m.WriteU32(state + 28, 0);
    m.WriteU32(actor + 64, 128);
    run(1, 0);
    check(m.ReadU32(resource + 92) == 25 && !m.ReadU32(resource + 88) &&
          !m.ReadU32(resource + 96));
    m.WriteU32(actor + 64, 4096);
    run(1, 0);
    check(!m.ReadU32(resource + 92) && !m.ReadU32(resource + 88));
    check(!battle_action_finalize61::Apply(0, m, {g, native}, s));
    std::cout << "battle_action_finalize61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
